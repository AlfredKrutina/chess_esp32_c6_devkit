/**
 * @file main.c
 * @brief ESP32-C6 Chess System v2.4 - Main Application
 * 
 * This file contains the main application entry point and system initialization:
 * - FreeRTOS task creation and management
 * - System component initialization
 * - Task Watchdog Timer setup
 * - Startup sequence with ASCII chess art
 * - Demo mode integration
 * - Chess game initialization
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 * 
 * Features:
 * - 8 FreeRTOS tasks for system components
 * - Task Watchdog Timer for system stability
 * - ASCII chess art welcome banner
 * - Automatic chess game initialization
 * - Demo mode with automatic moves
 * - Production-ready error handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "freertos_chess.h"
#include "chess_types.h"
#include "led_task.h"
#include "matrix_task.h"
#include "button_task.h"
#include "uart_task.h"
#include "game_task.h"
#include "animation_task.h"
// #include "screen_saver_task.h"  // DISABLED to prevent LED conflicts
#include "test_task.h"
#include "matter_task.h"
#include "web_server_task.h"
#include "config_manager.h"
#include "game_led_animations.h"
#include "uart_commands_extended.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void show_boot_animation_and_board(void);

static const char *TAG = "MAIN";

// Global UART mutex for clean output
SemaphoreHandle_t uart_mutex = NULL;

// Task handles
TaskHandle_t led_task_handle = NULL;
TaskHandle_t matrix_task_handle = NULL;
TaskHandle_t button_task_handle = NULL;
TaskHandle_t uart_task_handle = NULL;
TaskHandle_t game_task_handle = NULL;
TaskHandle_t animation_task_handle = NULL;
TaskHandle_t screen_saver_task_handle = NULL;
TaskHandle_t test_task_handle = NULL;
TaskHandle_t matter_task_handle = NULL;
TaskHandle_t web_server_task_handle = NULL;
TaskHandle_t reset_button_task_handle = NULL;
TaskHandle_t promotion_button_task_handle = NULL;

// Demo mode configuration
static bool demo_mode_enabled = false;
static uint32_t demo_move_delay_ms = 3000; // Default 3 seconds

// Predefined demo moves for automatic play
static const char* DEMO_MOVES[] = {
    "e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5",
    "c2c3", "g8f6", "d2d4", "e5d4", "c3d4", "c5b4",
    "b1c3", "f6e4", "e1f1", "e4c3", "d1d3", "c3d1",
    "c4f7", "e8f7", "d3d8", "f7f8", "d8d8", "f8f7"
};

static int demo_move_index = 0;
static int demo_moves_count = sizeof(DEMO_MOVES) / sizeof(DEMO_MOVES[0]);

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize main application system components
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t main_system_init(void)
{
    ESP_LOGI(TAG, "🔧 Initializing chess system components...");
    
    // Create UART mutex for clean output
    uart_mutex = xSemaphoreCreateMutex();
    if (uart_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create UART mutex");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ UART mutex created");
    
    // Initialize FreeRTOS chess component (queues)
    ESP_LOGI(TAG, "🔄 Initializing FreeRTOS chess component...");
    esp_err_t ret = chess_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize FreeRTOS chess component: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ FreeRTOS chess component initialized");
    
    // Start FreeRTOS timers
    ESP_LOGI(TAG, "🔄 Starting FreeRTOS timers...");
    ret = chess_start_timers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ FreeRTOS timers started successfully");
    
    // LED system will be initialized automatically when LED task starts
    
    // Verify all queues are available
    if (game_command_queue == NULL) {
        ESP_LOGE(TAG, "Game command queue not available");
        return ESP_FAIL;
    }
    // LED command queue removed - using direct LED calls
    ESP_LOGI(TAG, "LED system using direct calls (no queue)");
    if (matrix_command_queue == NULL) {
        ESP_LOGE(TAG, "Matrix command queue not available");
        return ESP_FAIL;
    }
    if (button_event_queue == NULL) {
        ESP_LOGE(TAG, "Button event queue not available");
        return ESP_FAIL;
    }
    
    // Verify UART queues
    if (uart_command_queue == NULL) {
        ESP_LOGE(TAG, "UART command queue not available");
        return ESP_FAIL;
    }
    if (uart_response_queue == NULL) {
        ESP_LOGE(TAG, "UART response queue not available");
        return ESP_FAIL;
    }
    
    // Verify Matter queues
    if (matter_command_queue == NULL || matter_status_queue == NULL) {
        ESP_LOGE(TAG, "Matter queues not available");
        return ESP_FAIL;
    }
    
    // Verify Web server queues
    if (web_command_queue == NULL || web_server_status_queue == NULL) {
        ESP_LOGE(TAG, "Web server queues not available");
        return ESP_FAIL;
    }
    
    // Verify Test queues
    if (test_command_queue == NULL) {
        ESP_LOGE(TAG, "Test command queue not available");
        return ESP_FAIL;
    }
    
    // Verify Animation queues
    if (animation_command_queue == NULL || animation_status_queue == NULL) {
        ESP_LOGE(TAG, "Animation queues not available");
        return ESP_FAIL;
    }
    
    // Verify Screen saver queues
    if (screen_saver_command_queue == NULL || screen_saver_status_queue == NULL) {
        ESP_LOGE(TAG, "Screen saver queues not available");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ All system queues verified");
    
    // Initialize endgame animation system
    ESP_LOGI(TAG, "🔄 Initializing endgame animation system...");
    ret = init_endgame_animation_system();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize endgame animation system: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ Endgame animation system initialized");

    // Register extended UART commands
    ESP_LOGI(TAG, "🔄 Registering extended UART commands...");
    ret = register_extended_uart_commands();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register extended UART commands: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ Extended UART commands registered");
    
    return ESP_OK;
}

// ============================================================================
// STARTUP SEQUENCE FUNCTIONS
// ============================================================================

/**
 * @brief Initialize chess game and send new game command
 */
void initialize_chess_game(void)
{
    ESP_LOGI(TAG, "🎯 Starting new chess game...");
    
    // Send new game command to game task
    if (game_command_queue != NULL) {
        chess_move_command_t cmd = { 0 };
        cmd.type = GAME_CMD_NEW_GAME;
        // from_notation a to_notation nechte prázdné
        
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG, "❌ Failed to send GAME_CMD_NEW_GAME");
        } else {
            ESP_LOGI(TAG, "✅ New game command sent");
        }
    } else {
        ESP_LOGE(TAG, "❌ Game command queue not available");
    }
    
    // ✅ CRITICAL FIX: Update button LED availability after game starts
    extern void led_update_button_availability_from_game(void);
    led_update_button_availability_from_game();
    
    ESP_LOGI(TAG, "🎯 Game ready! White to move.");
    ESP_LOGI(TAG, "💡 Type 'HELP' for available commands");
    ESP_LOGI(TAG, "💡 Type 'DEMO ON' to enable automatic play");
}

/**
 * @brief Toggle demo mode on/off
 * @param enabled true to enable, false to disable
 */
void toggle_demo_mode(bool enabled)
{
    demo_mode_enabled = enabled;
    
    if (enabled) {
        ESP_LOGI(TAG, "🤖 DEMO MODE ENABLED");
        ESP_LOGI(TAG, "Automatic play mode is now active");
        ESP_LOGI(TAG, "Moves will be played automatically");
        ESP_LOGI(TAG, "Type 'DEMO OFF' to stop automatic play");
    } else {
        ESP_LOGI(TAG, "🤖 DEMO MODE DISABLED");
        ESP_LOGI(TAG, "Manual play mode is now active");
        ESP_LOGI(TAG, "You can now make moves manually");
    }
}

/**
 * @brief Execute one demo move
 */
void execute_demo_move(void)
{
    if (!demo_mode_enabled || demo_move_index >= demo_moves_count) {
        return;
    }
    
    const char* move = DEMO_MOVES[demo_move_index];
    ESP_LOGI(TAG, "🤖 Demo move %d/%d: %s", demo_move_index + 1, demo_moves_count, move);
    
    // Send move command to game task
    if (strlen(move) == 4 && game_command_queue != NULL) {
        chess_move_command_t cmd = { 0 };
        cmd.type = GAME_CMD_MAKE_MOVE;
        strncpy(cmd.from_notation, move, 2);
        strncpy(cmd.to_notation, move + 2, 2);
        
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG, "❌ Failed to send GAME_CMD_MAKE_MOVE");
        } else {
            ESP_LOGI(TAG, "✅ Demo move sent: %c%c -> %c%c", move[0], move[1], move[2], move[3]);
        }
    }
    
    demo_move_index++;
    
    // Reset demo moves if we've played all of them
    if (demo_move_index >= demo_moves_count) {
        ESP_LOGI(TAG, "🤖 Demo game complete! %d moves played", demo_moves_count);
        demo_move_index = 0; // Reset for next demo
    }
}

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize console and UART
 */
static void init_console(void)
{
    ESP_LOGI(TAG, "Initializing console...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize console (USB Serial JTAG only - no UART needed)
    ESP_LOGI(TAG, "Using USB Serial JTAG console - no UART initialization needed");
    
    // Initialize console
    esp_console_config_t console_config = {
        .max_cmdline_args = 8,
        .max_cmdline_length = 256,
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));
    
    // Register help command
    esp_console_register_help_command();
    
    ESP_LOGI(TAG, "Console initialized successfully");
}

/**
 * @brief Create all system tasks
 * @return ESP_OK on success, error code on failure
 */
esp_err_t create_system_tasks(void)
{
    ESP_LOGI(TAG, "Creating system tasks...");
    
    // Create LED task
    BaseType_t result = xTaskCreate(
        (TaskFunction_t)led_task_start, "led_task", LED_TASK_STACK_SIZE, NULL,
        LED_TASK_PRIORITY, &led_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register LED task with Task Watchdog Timer
    esp_err_t ret = esp_task_wdt_add(led_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: LED task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    ESP_LOGI(TAG, "✓ LED task created successfully (%dKB stack) and registered with TWDT", LED_TASK_STACK_SIZE / 1024);
    
    // Create Matrix task
    result = xTaskCreate(
        (TaskFunction_t)matrix_task_start, "matrix_task", MATRIX_TASK_STACK_SIZE, NULL,
        MATRIX_TASK_PRIORITY, &matrix_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Matrix task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Matrix task with Task Watchdog Timer
    ret = esp_task_wdt_add(matrix_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Matrix task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Matrix task created successfully (%dKB stack) and registered with TWDT", MATRIX_TASK_STACK_SIZE / 1024);
    
    // Create Button task
    result = xTaskCreate(
        (TaskFunction_t)button_task_start, "button_task", BUTTON_TASK_STACK_SIZE, NULL,
        BUTTON_TASK_PRIORITY, &button_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Button task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Button task with Task Watchdog Timer
    ret = esp_task_wdt_add(button_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Button task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Button task created successfully (%dKB stack) and registered with TWDT", BUTTON_TASK_STACK_SIZE / 1024);
    
    // Create UART task (but suspend it until after boot animation)
    result = xTaskCreate(
        (TaskFunction_t)uart_task_start, "uart_task", UART_TASK_STACK_SIZE, NULL,
        UART_TASK_PRIORITY, &uart_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART task");
        return ESP_FAIL;
    }
    
    // Suspend UART task until after boot animation
    vTaskSuspend(uart_task_handle);
    
    // CRITICAL: Register UART task with Task Watchdog Timer
    ret = esp_task_wdt_add(uart_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: UART task WDT registration failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ UART task registered with TWDT");
    }
    ESP_LOGI(TAG, "✓ UART task created successfully (%dKB stack) - suspended until after boot animation", UART_TASK_STACK_SIZE / 1024);
    
    // Create Game task
    result = xTaskCreate(
        (TaskFunction_t)game_task_start, "game_task", GAME_TASK_STACK_SIZE, NULL,
        GAME_TASK_PRIORITY, &game_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Game task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Game task with Task Watchdog Timer
    ret = esp_task_wdt_add(game_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Game task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Game task created successfully (%dKB stack) and registered with TWDT", GAME_TASK_STACK_SIZE / 1024);
    
    // Create Animation task
    result = xTaskCreate(
        (TaskFunction_t)animation_task_start, "animation_task", ANIMATION_TASK_STACK_SIZE, NULL,
        ANIMATION_TASK_PRIORITY, &animation_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Animation task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Animation task with Task Watchdog Timer
    ret = esp_task_wdt_add(animation_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Animation task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Animation task created successfully (%dKB stack) and registered with TWDT", ANIMATION_TASK_STACK_SIZE / 1024);
    
    // Create Screen Saver task - DISABLED to prevent LED conflicts
    // result = xTaskCreate(
    //     (TaskFunction_t)screen_saver_task_start, "screen_saver_task", SCREEN_SAVER_TASK_STACK_SIZE, NULL,
    //     SCREEN_SAVER_TASK_PRIORITY, &screen_saver_task_handle
    // );
    
    // if (result != pdPASS) {
    //     ESP_LOGE(TAG, "Failed to create Screen Saver task");
    //     return ESP_FAIL;
    // }
    
    // CRITICAL: Register Screen Saver task with Task Watchdog Timer - DISABLED
    // ret = esp_task_wdt_add(screen_saver_task_handle);
    // if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
    //     ESP_LOGW(TAG, "Warning: Screen Saver task WDT registration failed: %s", esp_err_to_name(ret));
    //     // Continue anyway - task will still work
    // }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    
    ESP_LOGI(TAG, "✓ Screen Saver task DISABLED to prevent LED conflicts");
    
    // Create Test task
    result = xTaskCreate(
        (TaskFunction_t)test_task_start, "test_task", TEST_TASK_STACK_SIZE, NULL,
        TEST_TASK_PRIORITY, &test_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Test task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Test task with Task Watchdog Timer
    ret = esp_task_wdt_add(test_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Test task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Test task created successfully (%dKB stack) and registered with TWDT", TEST_TASK_STACK_SIZE / 1024);
    
    // Create Matter task
    result = xTaskCreate(
        (TaskFunction_t)matter_task_start, "matter_task", MATTER_TASK_STACK_SIZE, NULL,
        MATTER_TASK_PRIORITY, &matter_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Matter task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Matter task with Task Watchdog Timer
    ret = esp_task_wdt_add(matter_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Matter task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Matter task created successfully (%dKB stack) and registered with TWDT", MATTER_TASK_STACK_SIZE / 1024);
    
    // Create Web Server task
    result = xTaskCreate(
        (TaskFunction_t)web_server_task_start, "web_server_task", WEB_SERVER_TASK_STACK_SIZE, NULL,
        WEB_SERVER_TASK_PRIORITY, &web_server_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Web Server task");
        return ESP_FAIL;
    }
    
    // CRITICAL: Register Web Server task with Task Watchdog Timer
    ret = esp_task_wdt_add(web_server_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Web Server task WDT registration failed: %s", esp_err_to_name(ret));
        // Continue anyway - task will still work
    }
    // Suppress ESP_ERR_INVALID_ARG without logging (task already registered)
    ESP_LOGI(TAG, "✓ Web Server task created successfully (%dKB stack) and registered with TWDT", WEB_SERVER_TASK_STACK_SIZE / 1024);
    
    ESP_LOGI(TAG, "All system tasks created successfully");
    
    // Wait for all tasks to initialize before showing boot animation
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Show centralized boot animation and chess board
    show_boot_animation_and_board();
    
    // Initialize chess game after boot animation
    initialize_chess_game();
    
    // Resume UART task now that boot animation is complete
    vTaskResume(uart_task_handle);
    ESP_LOGI(TAG, "✅ UART task resumed after boot animation");
    
    return ESP_OK;
}

// ============================================================================
// CENTRALIZED BOOT ANIMATION AND BOARD DISPLAY
// ============================================================================

/**
 * @brief Show centralized boot animation and chess board
 * This function is called after all tasks are initialized to avoid
 * duplicate rendering and ensure smooth animation timing
 */
void show_boot_animation_and_board(void)
{
    ESP_LOGI(TAG, "🎬 Starting centralized boot animation...");
    
    // Clear screen and show welcome logo
    printf("\033[2J\033[H");
    
    // Greek-inspired CZECHMAT banner with ANSI colors
    // New ASCII art logo - using fputs to avoid printf formatting issues
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m............................................................\x1b[34m:=*+-\x1b[0m...............................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................................................\x1b[34m:=#%@@%*=-=+#@@@%*=:\x1b[0m.....................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m..............................................\x1b[34m-=*%@@%*=-=*%@%@=*@%@%*=-+#%@@%*=-\x1b[0m..............................................\x1b[0m\n", stdout);
    fputs("\x1b[0m......................................\x1b[34m:-+#@@@%+--+#%@%+@+#@@%@%%@%@@-*@=@@%#=-=*%@@@#+-:\x1b[0m......................................\x1b[0m\n", stdout);
    fputs("\x1b[0m...............................\x1b[34m:-+%@@@#+--*%@@*@=*@*@@@#=\x1b[0m...........\x1b[34m:+%@@%+@:#@*@@%+--+%@@@%+-:\x1b[0m...............................\x1b[0m\n", stdout);
    fputs("\x1b[0m........................\x1b[34m:-*@@@@#-:=#@@*@*+@+@@@%+:\x1b[0m.........................\x1b[34m-*@@@%+@:@@#@@#-:=#@@@@#-:\x1b[0m........................\x1b[0m\n", stdout);
    fputs("\x1b[0m....................\x1b[34m%@@@@**#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%@@@@#\x1b[0m....................\x1b[0m\n", stdout);
    fputs("\x1b[0m....................\x1b[34m%@#################################################################################%@#\x1b[0m....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[34m:%@=@+#@+@##@=@#%@+@*#@+@#%@=@*#@+@#*@+@#*@+@%*@+@%=@=%@+@**@=@%+@+#@=@%=@+#@+%%=@+:\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m......................\x1b[34m#@==============================================================================@+\x1b[0m......................\x1b[0m\n", stdout);
    fputs("\x1b[0m.......................\x1b[34m##==========@\x1b[0m:::::::::::::::::::::::::::::::::::::::::::::::::::::\x1b[34m*@==========@+\x1b[0m........................\x1b[0m\n", stdout);
    fputs("\x1b[0m........................\x1b[34m:@*******%@:\x1b[0m.\x1b[34m:%%%%%%%%%%%%%%%%%%%%%--#@@#.+%%%%%%%%%%%%%%%%%%%%*\x1b[0m..\x1b[34m-@#******%%\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m-@#+%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%%%=:+@@=\x1b[0m..:::::::::::::::::::\x1b[37m@%\x1b[0m....\x1b[34m@+#+*%*@:\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m=@#=%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%#--:*@@@@+-*-\x1b[0m.................\x1b[37m@%\x1b[0m....\x1b[34m@+#+*%*@-\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m=%#=%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%#.%@@@@@@@@%:\x1b[0m.................\x1b[37m@%\x1b[0m...\x1b[34m:%**+*%+@-\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m=%#-%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%#-@@@@@@@@:\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m-%**+*#+@-\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m+#%-%:%:@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%#-########-\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m=%**+*#+@=\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m**%-%:%:@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%:#%%%##%%%*\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m+#**+*#*%=\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m#+%:%:%-%:\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%*::@@@@@%\x1b[0m....................\x1b[37m@%\x1b[0m...\x1b[34m*##*+***%+\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m#=%:%:#-%:\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%%%.%@@@@*\x1b[0m....................\x1b[37m@%\x1b[0m...\x1b[34m#*#++***#+\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m%:%:%:#=%=\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%%#:@@@@@%\x1b[0m....................\x1b[37m@%\x1b[0m...\x1b[34m%*#++*+***\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m%:%:%:#=#+\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%%-*@@@@@@-\x1b[0m...................\x1b[37m@%\x1b[0m...\x1b[34m%+%++*+#+#\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m@:%:%:#+#*\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%#:=%%%%%%%%:\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m@+%++*+#=#\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.........................\x1b[34m@:%:%:#+*#\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%-=%@%%%%%%%%-\x1b[0m.................\x1b[37m@%\x1b[0m...\x1b[34m@=%=+*=#-%\x1b[0m.........................\x1b[0m\n", stdout);
    fputs("\x1b[0m.......................\x1b[34m:@*++++++++%#.-@@%%%%%%%%%%%%%%%.%@@@@@@@@@@@@#\x1b[0m................\x1b[37m@%\x1b[0m..\x1b[34m@*++++++++%%\x1b[0m........................\x1b[0m\n", stdout);
    fputs("\x1b[0m......................\x1b[34m=@=----------*@-@@@@@@@@@@@@@@@@@:*############=:@@@@@@@@@@@@@@@@%-@=----------=@:\x1b[0m.......................\x1b[0m\n", stdout);
    fputs("\x1b[0m....................\x1b[34m*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@=\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m................................................................................\x1b[37m@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m=@@@@@:+@@@@@@..@@@@@+..%@@@@@.-@%...+@%..@@#...=@@:...=@@-.=@@@@@@%-@@@@@-\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m%@+....:...:@@:..@@....-@@:...:::@#...=@#..@@@#.*@@@:..:%@@@:...@@:..:@@\x1b[0m......\x1b[37m@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m@@:.......=@%....@@%%%.+@#......:@@%%%%@#.:@*+@@@:%@-..+@.*@#...@@:..:@@#@*\x1b[0m...\x1b[37m@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m+@%:..-*.=@%..:=.@@...*:@@=...+-:@#...=@#.=@=.+@:.#@=.=@#**%@+..@@:..:@@...=\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m:*%@@#.=%%%%%%:-%%%%%*..-#@@%+.#%%:..#%#:#%=.....#%*:%%-..*%%=-%%+..=%%%%%=\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m##--------------------------------------------------------------------------------@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m#%================================================================================@+\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.....................\x1b[37m+##################################################################################-\x1b[0m.....................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    fputs("\x1b[0m.............................................................................................................................\x1b[0m\n", stdout);
    
    // Show boot progress with smooth animation and status messages
    printf("\033[1;32m"); // bold green
    printf("Initializing Chess Engine...\n");
    
    const int bar_width = 50;  // Longer bar for better visibility
    const int total_steps = 200; // More steps for smoother animation
    const int step_delay = 25; // 25ms per step for ultra-smooth animation
    
    // Status messages with progress thresholds
    const char* status_messages[] = {
        "Starting system...",
        "Creating tasks...",
        "Initializing GPIO...",
        "Setting up matrix...",
        "Configuring LEDs...",
        "Loading chess engine...",
        "Preparing board...",
        "System ready!"
    };
    const int num_messages = sizeof(status_messages) / sizeof(status_messages[0]);
    
    for (int i = 0; i <= total_steps; i++) {
        int progress = (i * 100) / total_steps;
        int filled = (i * bar_width) / total_steps;
        
        // Update status message based on progress
        int message_index = (progress * num_messages) / 100;
        if (message_index >= num_messages) message_index = num_messages - 1;
        
        // Clear current line and redraw
        printf("\rBooting: [");
        for (int j = 0; j < bar_width; j++) {
            if (j < filled) {
                printf("\033[1;32m█\033[0m"); // Bright green
            } else {
                printf("\033[2;37m░\033[0m"); // Dim gray
            }
        }
        printf("] %3d%% - %s", progress, status_messages[message_index]);
        fflush(stdout);
        
        // ✅ NOVÉ: LED boot animace krok - simultánně s UART animací
        led_boot_animation_step(progress);
        
        // CRITICAL: Reset watchdog timer during loading (only if registered)
        esp_err_t wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        if (i < total_steps) {
            vTaskDelay(pdMS_TO_TICKS(step_delay));
        }
    }
    
    printf("\n\033[1;32m✓ Chess Engine Ready!\033[0m\n\n"); // Success message
    
    // ✅ NOVÉ: LED boot animace fade out - postupně ztlumí LED
    ESP_LOGI(TAG, "🌟 Starting LED fade out animation...");
    led_boot_animation_fade_out();
    
    // Chess board will be displayed by game task after initialization
    ESP_LOGI(TAG, "🎯 Chess board will be displayed by game task...");
    
    // Show game guide after board
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("📋 CHESS GAME GUIDE - Type commands to play:\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("📋 Basic Commands:\n");
    printf("  • move e2e4    - Move piece from e2 to e4\n");
    printf("  • help         - Show all available commands\n");
    printf("  • board        - Display current board\n");
    printf("  • status       - Show game status\n");
    printf("  • reset        - Start new game\n");
    printf("\n");
    printf("🎯 Quick Start: Type 'move e2e4' to make your first move!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    ESP_LOGI(TAG, "✅ Boot animation and board display completed");
}

// ============================================================================
// MAIN APPLICATION FUNCTION
// ============================================================================

void app_main(void)
{
    ESP_LOGI(TAG, "🎯 ESP32-C6 Chess System v2.4 starting...");
    
    // PŘIDAT: Zvýšení WDT timeout pro inicializaci
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000,  // 10 sekund pro init - OPTIMALIZOVÁNO pro web server
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    // CRITICAL: Use reconfigure instead of init to avoid "TWDT already initialized" error
    esp_err_t ret = esp_task_wdt_reconfigure(&twdt_config);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "TWDT already initialized, skipping reconfiguration");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure TWDT: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "TWDT configured with 15s timeout");
    }
    
    // CRITICAL: Add main task to Task Watchdog Timer BEFORE any initialization
    ret = esp_task_wdt_add(NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to add main task to TWDT: %s", esp_err_to_name(ret));
        return;
    } else {
        ESP_LOGI(TAG, "✅ Main task registered with Task Watchdog Timer");
    }
    
    // Initialize console and UART FIRST - before any system initialization
    // This ensures that safe mode can output error messages
    ESP_LOGI(TAG, "🔄 Initializing console and UART...");
    init_console();
    ESP_LOGI(TAG, "✅ Console and UART initialized successfully");
    
    // System initialization with error recovery
    ret = main_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ System init failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "🔄 Entering safe mode - basic UART only");
        
        // Safe mode - jen UART pro debugging
        while(1) {
            ESP_LOGI(TAG, "💔 Safe mode: Init failed, system halted");
            // Note: No watchdog reset in safe mode - task not registered
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    
    // Task Watchdog Timer is automatically initialized by ESP-IDF
    // (CONFIG_ESP_TASK_WDT_INIT=y in sdkconfig)
    ESP_LOGI(TAG, "Task Watchdog Timer initialized automatically by ESP-IDF");
    
    // Create system tasks
    ret = create_system_tasks();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Task creation failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "🔄 Entering safe mode - basic UART only");
        
        // Safe mode - jen UART pro debugging
        while(1) {
            ESP_LOGI(TAG, "💔 Safe mode: Task creation failed, system halted");
            // Note: No watchdog reset in safe mode - task not registered
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    
    // CRITICAL: Reset watchdog AFTER task creation
    esp_task_wdt_reset();
    
    // PŘIDAT: Vrátit normální WDT timeout po inicializaci - OPTIMALIZOVÁNO pro web server
    twdt_config.timeout_ms = 8000;  // Zvýšeno na 8 sekund pro web server
    esp_task_wdt_reconfigure(&twdt_config);  // Použít reconfigure místo init
    
    // Main task is already registered with TWDT at the beginning of app_main()
    ESP_LOGI(TAG, "✓ Main task already registered with Task Watchdog Timer");
    
    // Wait for tasks to initialize (startup banner now handled by UART task)
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Main application loop
    uint32_t last_status_time = 0;
    
    ESP_LOGI(TAG, "🎯 Main application loop started");
    
    for (;;) {
        // CRITICAL: Reset watchdog for main task in every iteration
        esp_task_wdt_reset();
        
        // Periodic system status logging (respects quiet_mode and verbose_mode)
        uint32_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
        if (current_time - last_status_time >= 60) { // Every 60 seconds
            ESP_LOGI(TAG, "🔄 System Status: Uptime=%lus, FreeHeap=%zu bytes, Tasks=%d", 
                     current_time, esp_get_free_heap_size(), uxTaskGetNumberOfTasks());
            
            // DEBUG MONITORING - Stack usage monitoring pro web server optimalizaci
            ESP_LOGI(TAG, "📊 Stack Usage:");
            if (led_task_handle) ESP_LOGI(TAG, "  LED: %d bytes free", uxTaskGetStackHighWaterMark(led_task_handle));
            if (matrix_task_handle) ESP_LOGI(TAG, "  Matrix: %d bytes free", uxTaskGetStackHighWaterMark(matrix_task_handle));
            if (button_task_handle) ESP_LOGI(TAG, "  Button: %d bytes free", uxTaskGetStackHighWaterMark(button_task_handle));
            if (uart_task_handle) ESP_LOGI(TAG, "  UART: %d bytes free", uxTaskGetStackHighWaterMark(uart_task_handle));
            if (game_task_handle) ESP_LOGI(TAG, "  Game: %d bytes free", uxTaskGetStackHighWaterMark(game_task_handle));
            if (web_server_task_handle) ESP_LOGI(TAG, "  Web Server: %d bytes free", uxTaskGetStackHighWaterMark(web_server_task_handle));
            
            last_status_time = current_time;
        }
        
        // Demo mode processing
        if (demo_mode_enabled) {
            static uint32_t last_demo_time = 0;
            if (current_time - last_demo_time >= (demo_move_delay_ms / 1000)) {
                execute_demo_move();
                last_demo_time = current_time;
            }
        }
        
        // CRITICAL: Main task delay - must be present for watchdog safety
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay - CRITICAL for watchdog
    }
}