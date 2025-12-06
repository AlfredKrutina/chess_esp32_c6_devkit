/**
 * @file main.c
 * @brief ESP32-C6 Chess System v2.4 - Hlavni aplikace
 * 
 * Tento soubor obsahuje hlavni vstupni bod aplikace a inicializaci systemu:
 * - Vytvareni a spravu FreeRTOS tasku
 * - Inicializaci systemovych komponent
 * - Nastaveni Task Watchdog Timeru
 * - Startup sekvenci s ASCII sachovym umenim
 * - Integraci demo modu
 * - Inicializaci sachove hry
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * Funkce:
 * - 8 FreeRTOS tasku pro systemove komponenty
 * - Task Watchdog Timer pro stabilitu systemu
 * - ASCII sachove umeni welcome banner
 * - Automaticka inicializace sachove hry
 * - Demo mod s automatickymi tahy
 * - Production-ready error handling
 * 
 * @details
 * Tento soubor je hlavnim vstupnim bodem pro ESP32-C6 sachovy system.
 * Spousti se jako prvni a inicializuje vsechny potrebne komponenty.
 * System pouziva FreeRTOS pro multitasking a ma 8 hlavnich tasku:
 * - LED task: ovladani LED pasku
 * - Matrix task: skenovani 8x8 matice
 * - Button task: ovladani tlacitek
 * - UART task: komunikace pres UART
 * - Game task: logika sachove hry
 * - Animation task: LED animace
 * - Test task: testovani systemu
 * - Web server task: web rozhrani
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
// #include "matter_task.h"  // DISABLED - Matter not needed
#include "web_server_task.h"
#include "config_manager.h"
#include "game_led_animations.h"
#include "uart_commands_extended.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void show_boot_animation_and_board(void);

static const char *TAG = "MAIN";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecny reset WDT s logovanim WARNING misto ERROR pro ESP_ERR_NOT_FOUND
 * 
 * Tato funkce bezpecne resetuje Task Watchdog Timer. Pokud task neni jeste
 * registrovany (coz je normalni behem startupu), loguje se WARNING misto ERROR.
 * 
 * @return ESP_OK pokud uspesne, ESP_ERR_NOT_FOUND pokud task neni registrovany (WARNING pouze)
 */
static esp_err_t main_task_wdt_reset_safe(void) {
    esp_err_t ret = esp_task_wdt_reset();
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // Log as WARNING instead of ERROR - task not registered yet
        ESP_LOGW(TAG, "WDT reset: task not registered yet (this is normal during startup)");
        return ESP_OK; // Treat as success for our purposes
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

/** @brief Globalni UART mutex pro cisty vystup */
SemaphoreHandle_t uart_mutex = NULL;

/** @brief Handle pro LED task */
TaskHandle_t led_task_handle = NULL;
/** @brief Handle pro Matrix task */
TaskHandle_t matrix_task_handle = NULL;
/** @brief Handle pro Button task */
TaskHandle_t button_task_handle = NULL;
/** @brief Handle pro UART task */
TaskHandle_t uart_task_handle = NULL;
/** @brief Handle pro Game task */
TaskHandle_t game_task_handle = NULL;
/** @brief Handle pro Animation task */
TaskHandle_t animation_task_handle = NULL;
/** @brief Handle pro Screen Saver task */
TaskHandle_t screen_saver_task_handle = NULL;
/** @brief Handle pro Test task */
TaskHandle_t test_task_handle = NULL;
// TaskHandle_t matter_task_handle = NULL;  // DISABLED - Matter not needed
/** @brief Handle pro Web Server task */
TaskHandle_t web_server_task_handle = NULL;
/** @brief Handle pro Reset Button task */
TaskHandle_t reset_button_task_handle = NULL;
/** @brief Handle pro Promotion Button task */
TaskHandle_t promotion_button_task_handle = NULL;

/** @brief Konfigurace demo modu - je demo mod zapnuty */
/** @brief Konfigurace demo modu - je demo mod zapnuty */
static volatile bool demo_mode_enabled = false;
/** @brief Zpozdeni mezi demo tahy v milisekundach - dynamicky meneno */
static uint32_t current_demo_delay_ms = 3000; 

// Forward declaration
void demo_report_activity(void);

/** @brief Preddefinovane demo tahy pro automaticke hrani */
static const char* DEMO_MOVES[] = {
    "e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5",
    "c2c3", "g8f6", "d2d4", "e5d4", "c3d4", "c5b4",
    "b1c3", "f6e4", "e1f1", "e4c3", "d1d3", "c3d1",
    "c4f7", "e8f7", "d3d8", "f7f8", "d8d8", "f8f7"
};

/** @brief Aktualni index v demo tahach */
static int demo_move_index = 0;
/** @brief Celkovy pocet demo tahu */
static int demo_moves_count = sizeof(DEMO_MOVES) / sizeof(DEMO_MOVES[0]);

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Inicializuje hlavni systemove komponenty aplikace
 * 
 * Tato funkce inicializuje vsechny systemove komponenty potrebne pro chod
 * sachoveho systemu. Vytvari mutexy, inicializuje FreeRTOS chess komponentu,
 * spousti timery a overuje dostupnost vsech front.
 * 
 * @return ESP_OK pri uspechu, ESP_FAIL pri chybe
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
    
    // DISABLED: Verify Matter queues - Matter not needed
    /*
    if (matter_command_queue == NULL || matter_status_queue == NULL) {
        ESP_LOGE(TAG, "Matter queues not available");
        return ESP_FAIL;
    }
    */
    
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
 * @brief Inicializuje sachovou hru a posle prikaz pro novou hru
 * 
 * Tato funkce spusti novou sachovou hru odeslanim prikazu GAME_CMD_NEW_GAME
 * do game tasku. Po spusteni hry aktualizuje dostupnost tlacitek LED.
 * 
 * @details
 * Funkce vytvori novou sachovou hru odeslanim prikazu do game tasku.
 * Po uspesnem spusteni hry aktualizuje LED feedback pro tlacitka,
 * aby uzivatel vedel ktera tlacitka jsou dostupna.
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
 * @brief Prepina demo mod zapnuto/vypnuto
 * 
 * Tato funkce umoznuje zapnout nebo vypnout demo mod, ktery automaticky
 * hraje preddefinovane tahy. Uzivatel muze zapnout demo mod prikazem
 * "DEMO ON" a vypnout prikazem "DEMO OFF".
 * 
 * @param enabled true pro zapnuti, false pro vypnuti
 * 
 * @details
 * Demo mod umoznuje automaticke hrani preddefinovanych tahu.
 * Kdyz je zapnuty, system automaticky hraje tahy z pole DEMO_MOVES.
 * Kdyz je vypnuty, uzivatel muze hrat manualne.
 */
void toggle_demo_mode(bool enabled)
{
    demo_mode_enabled = enabled;
    
    if (enabled) {
        ESP_LOGI(TAG, "🤖 SCREENSAVER MODE ENABLED");
        ESP_LOGI(TAG, "Automatic play will start with variable speed (0.7s - 4s)");
        ESP_LOGI(TAG, "Touch the board to interrupt!");
    } else {
        ESP_LOGI(TAG, "🤖 SCREENSAVER MODE DISABLED");
        ESP_LOGI(TAG, " returned to manual control");
    }
}

/**
 * @brief Report activity on the board (interrupt demo mode)
 * Called from matrix_task when any change is detected
 */
void demo_report_activity(void)
{
    if (demo_mode_enabled) {
        demo_mode_enabled = false;
        ESP_LOGW(TAG, "✋ DEMO INTERRUPTED by user activity!");
        // Optional: Send feedback to Web/UART if needed
    }
}

/**
 * @brief Get demo mode status
 * @return true if enabled, false otherwise
 */
bool is_demo_mode_enabled(void)
{
    return demo_mode_enabled;
}

/**
 * @brief Vykona jeden demo tah
 * 
 * Tato funkce vykona jeden tah z preddefinovane sekvence demo tahu.
 * Tahy jsou ulozeny v poli DEMO_MOVES a jsou hrany postupne.
 * Po dokonceni vsech tahu se sekvence resetuje.
 * 
 * @details
 * Funkce vezme aktualni tah z pole DEMO_MOVES a posle ho do game tasku.
 * Tahy jsou ve formatu "e2e4" (z pozice e2 na pozici e4).
 * Po dokonceni vsech tahu se index resetuje na 0.
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
        
        // For demo/screensaver, we want valid moves only, but we don't assume physical sync
        // The game task should handle this command visually
        
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG, "❌ Failed to send GAME_CMD_MAKE_MOVE");
        } else {
            // ESP_LOGI(TAG, "✅ Demo move sent: %c%c -> %c%c", move[0], move[1], move[2], move[3]);
        }
    }
    
    demo_move_index++;
    
    // Reset demo moves if we've played all of them
    if (demo_move_index >= demo_moves_count) {
        ESP_LOGI(TAG, "🤖 Demo game complete! Restarting...");
        demo_move_index = 0; // Reset for next demo
        
        // Optional: Reset board here via GAME_CMD_NEW_GAME if we want perfect loop
        // But the previous implementation just looped moves which might be invalid if board state isn't reset
        // For screensaver visualization, maybe we want to reset
        
        // Send reset command
        if (game_command_queue != NULL) {
             chess_move_command_t cmd = { .type = GAME_CMD_NEW_GAME };
             xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
        }
    }
}

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Inicializuje konzoli a UART
 * 
 * Tato funkce inicializuje NVS flash, konzoli a UART pro komunikaci.
 * Pouziva USB Serial JTAG konzoli, takze neni potreba externi UART.
 * 
 * @details
 * Funkce inicializuje NVS flash pamet pro ulozeni konfigurace,
 * nastavi konzoli pro prijimani prikazu a registruje help prikaz.
 * Vsechna komunikace probiha pres USB Serial JTAG.
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
 * @brief Vytvori vsechny systemove tasky
 * 
 * Tato funkce vytvori vsechny FreeRTOS tasky potrebne pro chod systemu.
 * Kazdy task ma svoji prioritu a velikost stacku. Po vytvoreni tasku
 * se zobrazi boot animace a inicializuje se sachova hra.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 * 
 * @details
 * Funkce vytvori 8 hlavnich tasku:
 * - LED task: ovladani LED pasku
 * - Matrix task: skenovani 8x8 matice
 * - Button task: ovladani tlacitek
 * - UART task: komunikace pres UART
 * - Game task: logika sachove hry
 * - Animation task: LED animace
 * - Test task: testovani systemu
 * - Web server task: web rozhrani
 * 
 * Po vytvoreni vsech tasku se zobrazi boot animace a inicializuje hra.
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
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ LED task created successfully (%dKB stack) - will self-register with TWDT", LED_TASK_STACK_SIZE / 1024);
    
    // Create Matrix task
    result = xTaskCreate(
        (TaskFunction_t)matrix_task_start, "matrix_task", MATRIX_TASK_STACK_SIZE, NULL,
        MATRIX_TASK_PRIORITY, &matrix_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Matrix task");
        return ESP_FAIL;
    }
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ Matrix task created successfully (%dKB stack) - will self-register with TWDT", MATRIX_TASK_STACK_SIZE / 1024);
    
    // Create Button task
    result = xTaskCreate(
        (TaskFunction_t)button_task_start, "button_task", BUTTON_TASK_STACK_SIZE, NULL,
        BUTTON_TASK_PRIORITY, &button_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Button task");
        return ESP_FAIL;
    }
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ Button task created successfully (%dKB stack) - will self-register with TWDT", BUTTON_TASK_STACK_SIZE / 1024);
    
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
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ UART task created successfully (%dKB stack) - suspended until after boot animation, will self-register with TWDT", UART_TASK_STACK_SIZE / 1024);
    
    // Create Game task
    result = xTaskCreate(
        (TaskFunction_t)game_task_start, "game_task", GAME_TASK_STACK_SIZE, NULL,
        GAME_TASK_PRIORITY, &game_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Game task");
        return ESP_FAIL;
    }
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ Game task created successfully (%dKB stack) - will self-register with TWDT", GAME_TASK_STACK_SIZE / 1024);
    
    // Create Animation task
    result = xTaskCreate(
        (TaskFunction_t)animation_task_start, "animation_task", ANIMATION_TASK_STACK_SIZE, NULL,
        ANIMATION_TASK_PRIORITY, &animation_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Animation task");
        return ESP_FAIL;
    }
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ Animation task created successfully (%dKB stack) - will self-register with TWDT", ANIMATION_TASK_STACK_SIZE / 1024);
    
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
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ Test task created successfully (%dKB stack) - will self-register with TWDT", TEST_TASK_STACK_SIZE / 1024);
    
    // DISABLED: Create Matter task - Matter not needed
    /*
    result = xTaskCreate(
        (TaskFunction_t)matter_task_start, "matter_task", MATTER_TASK_STACK_SIZE, NULL,
        MATTER_TASK_PRIORITY, &matter_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Matter task");
        return ESP_FAIL;
    }
    
    ret = esp_task_wdt_add(matter_task_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Warning: Matter task WDT registration failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "✓ Matter task created successfully (%dKB stack) and registered with TWDT", MATTER_TASK_STACK_SIZE / 1024);
    */
    
    // Create Web Server task
    result = xTaskCreate(
        (TaskFunction_t)web_server_task_start, "web_server_task", WEB_SERVER_TASK_STACK_SIZE, NULL,
        WEB_SERVER_TASK_PRIORITY, &web_server_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Web Server task");
        return ESP_FAIL;
    }
    
    // Task will register itself with TWDT internally
    ESP_LOGI(TAG, "✓ Web Server task created successfully (%dKB stack) - will self-register with TWDT", WEB_SERVER_TASK_STACK_SIZE / 1024);
    
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
 * @brief Zobrazi centralizovanou boot animaci a sachovnici
 * 
 * Tato funkce je volana po inicializaci vsech tasku, aby se zabranilo
 * duplicitnimu renderovani a zajistil se plynuly timing animace.
 * Zobrazi ASCII sachove umeni, progress bar a navod k pouziti.
 * 
 * @details
 * Funkce zobrazi pekny ASCII banner s nazvem systemu, progress bar
 * s animaci nacitani a navod k pouziti systemu. Vsechno je barevne
 * a plynule animovane pro lepsi uzivatelsky zazitek.
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
    // int current_message = 0; // Unused variable removed
    
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
        
        // ✅ Spustit LED boot animaci podle progress
        led_boot_animation_step((uint8_t)progress);
        
        // CRITICAL: Reset watchdog timer during loading (only if registered)
        esp_err_t wdt_ret = main_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        if (i < total_steps) {
            vTaskDelay(pdMS_TO_TICKS(step_delay));
        }
    }
    
    // ✅ Po dokončení boot animace - fade out
    led_boot_animation_fade_out();
    
    printf("\n\033[1;32m✓ Chess Engine Ready!\033[0m\n\n"); // Success message
    
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

/**
 * @brief Hlavni funkce aplikace
 * 
 * Tato funkce je hlavnim vstupnim bodem aplikace. Inicializuje system,
 * vytvori tasky a spusti hlavni smycku aplikace. Obsahuje error handling
 * a safe mode pro pripady chyb.
 * 
 * @details
 * Funkce je volana jako prvni po spusteni ESP32. Inicializuje Task Watchdog
 * Timer, vytvori vsechny systemove tasky a spusti hlavni smycku aplikace.
 * Obsahuje error handling a safe mode pro pripady chyb pri inicializaci.
 * 
 * Hlavni smycka:
 * - Resetuje watchdog timer
 * - Loguje system status kazdych 60 sekund
 * - Zpracovava demo mod pokud je zapnuty
 * - Ceka 1 sekundu mezi iteracemi
 */
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
    main_task_wdt_reset_safe();
    
    // PŘIDAT: Vrátit normální WDT timeout po inicializaci - OPTIMALIZOVÁNO pro web server
    twdt_config.timeout_ms = 8000;  // Zvýšeno na 8 sekund pro web server
    esp_task_wdt_reconfigure(&twdt_config);  // Použít reconfigure místo init
    
    // Main task is already registered with TWDT at the beginning of app_main()
    ESP_LOGI(TAG, "✓ Main task already registered with Task Watchdog Timer");
    
    // Wait for tasks to initialize (startup banner now handled by UART task)
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Main application loop
    // uint32_t loop_count = 0; // Unused variable removed
    uint32_t last_status_time = 0;
    uint32_t last_demo_move_time = 0; // Track time in ms instead of seconds for finer granularity
    
    ESP_LOGI(TAG, "🎯 Main application loop started");
    
    for (;;) {
        // CRITICAL: Reset watchdog for main task in every iteration
        main_task_wdt_reset_safe();
        
        // Get current time in ms
        uint32_t current_time_ms = esp_timer_get_time() / 1000;
        uint32_t current_time_sec = current_time_ms / 1000;
        
        // Periodic system status logging (respects quiet_mode and verbose_mode)
        if (current_time_sec - last_status_time >= 60) { // Every 60 seconds
            ESP_LOGI(TAG, "🔄 System Status: Uptime=%lu s, FreeHeap=%zu bytes, Tasks=%d", 
                     current_time_sec, esp_get_free_heap_size(), uxTaskGetNumberOfTasks());
            
            // LOGS REMOVED FOR BREVITY
            last_status_time = current_time_sec;
        }
        
        // Demo mode processing
        if (demo_mode_enabled) {
            if (current_time_ms - last_demo_move_time >= current_demo_delay_ms) {
                execute_demo_move();
                last_demo_move_time = current_time_ms;
                
                // Calculate next random delay (700ms - 4000ms)
                current_demo_delay_ms = 700 + (esp_random() % 3301);
                // ESP_LOGI(TAG, "Next move in %lu ms", current_demo_delay_ms);
            }
        }
        
        // Short delay to allow frequent checks (interruption, etc.) but not starve WDT
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}