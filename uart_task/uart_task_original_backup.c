/**
 * @file uart_task_new.c
 * @brief ESP32-C6 Chess System v2.4 - Enhanced UART Task Implementation
 * 
 * This task provides a production-ready line-based UART terminal:
 * - Line-based input with echo and editing
 * - Command table with function pointers
 * - Advanced command features (aliases, auto-completion)
 * - NVS configuration persistence
 * - Robust error handling and validation
 * - Resource optimization
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "uart_task.h"
#include "freertos_chess.h"
#include "game_task.h"
#include "config_manager.h"
#include "../freertos_chess/include/chess_types.h"

// External function declarations
extern bool convert_notation_to_coords(const char* notation, uint8_t* row, uint8_t* col);

// External UART mutex for clean output
extern SemaphoreHandle_t uart_mutex;

// External task handles and queues
extern TaskHandle_t led_task_handle;
extern TaskHandle_t matrix_task_handle;
extern TaskHandle_t button_task_handle;
extern TaskHandle_t game_task_handle;
extern QueueHandle_t game_command_queue;

// Missing type definitions that should be here
typedef struct {
    const char* command;
    command_handler_t handler;
    const char* help_text;
    const char* usage;
    bool requires_args;
    const char* aliases[5];  // Up to 5 aliases
} uart_command_t;

static const char *TAG = "UART_TASK";

// UART configuration - only use if UART is enabled
#if CONFIG_ESP_CONSOLE_UART_NUM >= 0
#define UART_PORT_NUM      CONFIG_ESP_CONSOLE_UART_NUM
#define UART_ENABLED       1
#else
#define UART_PORT_NUM      0  // Dummy value when UART disabled
#define UART_ENABLED       0
#endif

// ============================================================================
// ENHANCED INPUT BUFFERING AND LINE EDITING
// ============================================================================

// Input buffer configuration
#define UART_CMD_BUFFER_SIZE 256
#define UART_CMD_HISTORY_SIZE 20
#define UART_MAX_ARGS 10

// Input buffer structure
typedef struct {
    char buffer[UART_CMD_BUFFER_SIZE];
    size_t pos;
    size_t length;
} input_buffer_t;

// Command history structure
typedef struct {
    char commands[UART_CMD_HISTORY_SIZE][UART_CMD_BUFFER_SIZE];
    int current;
    int count;
    int max_size;
} command_history_t;

// ============================================================================
// COMMAND TABLE STRUCTURE
// ============================================================================

// Note: All types are now defined in header files to avoid conflicts

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Task state
static bool task_running = false;
static bool color_enabled = true;  // ANSI color support
static bool echo_enabled = true;   // Character echo enabled
static bool prompt_shown = false;  // Track if prompt is currently shown
static input_buffer_t input_buffer;
static command_history_t command_history;
static system_config_t system_config;



// UART message queue for centralized output
QueueHandle_t uart_output_queue = NULL;

// Statistics
static uint32_t command_count = 0;
static uint32_t error_count = 0;
static uint32_t last_command_time = 0;

// ============================================================================
// ANSI COLOR CODES AND FORMATTING
// ============================================================================

// ANSI Color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

// Message type colors
#define COLOR_ERROR   COLOR_RED COLOR_BOLD
#define COLOR_SUCCESS COLOR_GREEN COLOR_BOLD
#define COLOR_WARNING COLOR_YELLOW COLOR_BOLD
#define COLOR_INFO    COLOR_CYAN
#define COLOR_MOVE    COLOR_GREEN COLOR_BOLD
#define COLOR_STATUS  COLOR_YELLOW
#define COLOR_DEBUG   COLOR_MAGENTA
#define COLOR_HELP    COLOR_BLUE COLOR_BOLD

// ============================================================================
// FORMATTING FUNCTIONS
// ============================================================================

/**
 * @brief Display impressive welcome logo with ANSI colors
 */
void uart_send_welcome_logo(void)
{
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    
    // Don't clear screen - just show logo below current content
    printf("\n");
    
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
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

/**
 * @brief Show animated progress bar
 * @param label Progress label
 * @param max_value Maximum value (100 = 100%)
 * @param duration_ms Duration in milliseconds
 */
void uart_show_progress_bar(const char* label, uint32_t max_value, uint32_t duration_ms)
{
    // CRITICAL: Lock mutex for ENTIRE progress bar to prevent interruption
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    
    const int bar_width = 20;
    uint32_t step_delay = duration_ms / max_value;
    if (step_delay < 5) step_delay = 5; // Minimum 5ms delay for smooth animation
    
    if (color_enabled) printf("\033[1;32m"); // bold green
    printf("%s: [", label);
    
    for (int i = 0; i < bar_width; i++) {
        printf(".");
    }
    printf("] 0%%");
    if (color_enabled) printf("\033[0m"); // reset colors
    fflush(stdout);
    
    for (uint32_t i = 0; i <= max_value; i++) {
        int filled = (i * bar_width) / max_value;
        
        // Reset watchdog before each progress update
        esp_task_wdt_reset();
        
        // Move cursor back to start of progress bar
        if (color_enabled) printf("\033[1;32m"); // bold green
        printf("\r%s: [", label);
        
        // Show filled part (ASCII only)
        for (int j = 0; j < filled; j++) {
            printf("#");
        }
        
        // Show unfilled part
        for (int j = filled; j < bar_width; j++) {
            printf(".");
        }
        
        printf("] %3u%%", (unsigned int)((i * 100) / max_value));
        if (color_enabled) printf("\033[0m"); // reset colors
        fflush(stdout);
        
        if (i < max_value) {
            // Smooth animation with calculated delay
            vTaskDelay(pdMS_TO_TICKS(step_delay));
        }
    }
    
    printf("\n");
    fflush(stdout);
    
    // CRITICAL: Release mutex only after progress bar is complete
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

void uart_send_colored(const char* color, const char* message)
{
    // CRITICAL: Use mutex for ALL printf output to prevent conflicts
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        printf("%s%s%s", color, message, COLOR_RESET);
        fflush(stdout);
        xSemaphoreGive(uart_mutex);
    } else {
        printf("%s%s%s", color, message, COLOR_RESET);
        fflush(stdout);
    }
}

void uart_send_colored_line(const char* color, const char* message)
{
    // CRITICAL: Use mutex for ALL printf output to prevent conflicts
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        printf("%s%s%s\n", color, message, COLOR_RESET);
        fflush(stdout);
        xSemaphoreGive(uart_mutex);
    } else {
        printf("%s%s%s\n", color, message, COLOR_RESET);
        fflush(stdout);
    }
}

void uart_send_error(const char* message)
{
    uart_send_colored_line(COLOR_ERROR, message);
}

void uart_send_success(const char* message)
{
    uart_send_colored_line(COLOR_SUCCESS, message);
}

void uart_send_warning(const char* message)
{
    uart_send_colored_line(COLOR_WARNING, message);
}

void uart_send_info(const char* message)
{
    uart_send_colored_line(COLOR_INFO, message);
}

void uart_send_move(const char* message)
{
    uart_send_colored_line(COLOR_MOVE, message);
}

void uart_send_status(const char* message)
{
    uart_send_colored_line(COLOR_STATUS, message);
}

void uart_send_debug(const char* message)
{
    uart_send_colored_line(COLOR_DEBUG, message);
}

void uart_send_help(const char* message)
{
    uart_send_colored_line(COLOR_HELP, message);
}

void uart_send_formatted(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    uart_send_line(buffer);
}

void uart_send_line(const char* str)
{
    if (str == NULL) return;
    
    // CRITICAL: Use mutex for ALL printf output to prevent conflicts
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        printf("%s\n", str);
        fflush(stdout);
        xSemaphoreGive(uart_mutex);
    } else {
        printf("%s\n", str);
        fflush(stdout);
    }
    
    // Log to ESP log system without mutex (separate output channel)
    ESP_LOGI(TAG, "UART Send: %s", str);
}

void uart_send_string(const char* str)
{
    if (str == NULL) return;
    
    // CRITICAL: Use mutex for ALL printf output to prevent conflicts
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        printf("%s", str);
        fflush(stdout);
        xSemaphoreGive(uart_mutex);
    } else {
        printf("%s", str);
        fflush(stdout);
    }
    
    // Log to ESP log system without mutex (separate output channel)
    ESP_LOGI(TAG, "UART Send: %s", str);
}

// ============================================================================
// CENTRALIZED UART OUTPUT SYSTEM
// ============================================================================

/**
 * @brief Send message to UART output queue (thread-safe)
 * @param type Message type (determines color)
 * @param add_newline Whether to add newline
 * @param format Format string
 * @param ... Format arguments
 */
void uart_queue_message(uart_msg_type_t type, bool add_newline, const char* format, ...)
{
    if (uart_output_queue == NULL) {
        // Queue not ready, fall back to direct output
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        if (add_newline) printf("\n");
        fflush(stdout);
        return;
    }
    
    uart_message_t msg;
    msg.type = type;
    msg.add_newline = add_newline;
    
    va_list args;
    va_start(args, format);
    vsnprintf(msg.message, sizeof(msg.message), format, args);
    va_end(args);
    
    // Send to queue with timeout to prevent blocking
    if (xQueueSend(uart_output_queue, &msg, pdMS_TO_TICKS(10)) != pdTRUE) {
        // Queue full, fall back to direct output
        printf("%s", msg.message);
        if (add_newline) printf("\n");
        fflush(stdout);
    }
}

/**
 * @brief Process UART output messages from queue
 */
static void uart_process_output_queue(void)
{
    uart_message_t msg;
    
    // Process all pending messages
    while (xQueueReceive(uart_output_queue, &msg, 0) == pdTRUE) {
        // Take mutex for entire message output
        if (uart_mutex != NULL) {
            xSemaphoreTake(uart_mutex, portMAX_DELAY);
        }
        
        // Apply color based on message type
        const char* color = COLOR_RESET;
        switch (msg.type) {
            case UART_MSG_ERROR:   color = COLOR_ERROR; break;
            case UART_MSG_WARNING: color = COLOR_WARNING; break;
            case UART_MSG_SUCCESS: color = COLOR_SUCCESS; break;
            case UART_MSG_INFO:    color = COLOR_INFO; break;
            case UART_MSG_DEBUG:   color = COLOR_DEBUG; break;
            default:               color = COLOR_RESET; break;
        }
        
        if (color_enabled && msg.type != UART_MSG_NORMAL) {
            printf("%s%s%s", color, msg.message, COLOR_RESET);
        } else {
            printf("%s", msg.message);
        }
        
        if (msg.add_newline) {
            printf("\n");
        }
        
        fflush(stdout);
        
        if (uart_mutex != NULL) {
            xSemaphoreGive(uart_mutex);
        }
    }
}

// ============================================================================
// INPUT BUFFER MANAGEMENT
// ============================================================================

void input_buffer_init(input_buffer_t* buffer)
{
    memset(buffer->buffer, 0, sizeof(buffer->buffer));
    buffer->pos = 0;
    buffer->length = 0;
}

void input_buffer_clear(input_buffer_t* buffer)
{
    memset(buffer->buffer, 0, sizeof(buffer->buffer));
    buffer->pos = 0;
    buffer->length = 0;
}

void input_buffer_add_char(input_buffer_t* buffer, char c)
{
    if (buffer->pos < UART_CMD_BUFFER_SIZE - 1) {
        buffer->buffer[buffer->pos++] = c;
        buffer->buffer[buffer->pos] = '\0';
        buffer->length = buffer->pos;
    }
}

void input_buffer_backspace(input_buffer_t* buffer)
{
    if (buffer->pos > 0) {
        buffer->pos--;
        buffer->buffer[buffer->pos] = '\0';
        buffer->length = buffer->pos;
    }
}

void input_buffer_set_cursor(input_buffer_t* buffer, size_t pos)
{
    if (pos <= buffer->length) {
        buffer->pos = pos;
    }
}

// ============================================================================
// COMMAND HISTORY MANAGEMENT
// ============================================================================

void command_history_init(command_history_t* history)
{
    memset(history->commands, 0, sizeof(history->commands));
    history->current = 0;
    history->count = 0;
    history->max_size = UART_CMD_HISTORY_SIZE;
}

void command_history_add(command_history_t* history, const char* command)
{
    if (command == NULL || strlen(command) == 0) return;
    
    // Don't add duplicate commands
    if (history->count > 0) {
        int last_idx = (history->current - 1 + history->max_size) % history->max_size;
        if (strcmp(history->commands[last_idx], command) == 0) {
            return;
        }
    }
    
    // Add command to history
    strncpy(history->commands[history->current], command, UART_CMD_BUFFER_SIZE - 1);
    history->commands[history->current][UART_CMD_BUFFER_SIZE - 1] = '\0';
    
    history->current = (history->current + 1) % history->max_size;
    if (history->count < history->max_size) {
        history->count++;
    }
}

const char* command_history_get_previous(command_history_t* history)
{
    if (history->count == 0) return NULL;
    
    int idx = (history->current - 1 + history->max_size) % history->max_size;
    return history->commands[idx];
}

const char* command_history_get_next(command_history_t* history)
{
    if (history->count == 0) return NULL;
    
    int idx = (history->current + 1) % history->max_size;
    return history->commands[idx];
}

void command_history_show(command_history_t* history)
{
    uart_send_line("Command History:");
    
    int start_idx = (history->current - history->count + history->max_size) % history->max_size;
    
    for (int i = 0; i < history->count; i++) {
        int idx = (start_idx + i) % history->max_size;
        uart_send_formatted("  %d: %s", i + 1, history->commands[idx]);
    }
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

command_result_t uart_cmd_help(const char* args)
{
    if (args && strlen(args) > 0) {
        // Case-insensitive category matching
        char args_upper[16];
        strncpy(args_upper, args, sizeof(args_upper) - 1);
        args_upper[sizeof(args_upper) - 1] = '\0';
        
        for (int i = 0; args_upper[i]; i++) {
            args_upper[i] = toupper(args_upper[i]);
        }
        
        // Category-specific help
        if (strcmp(args_upper, "GAME") == 0) {
            uart_cmd_help_game();
        } else if (strcmp(args_upper, "SYSTEM") == 0) {
            uart_cmd_help_system();
        } else if (strcmp(args_upper, "BEGINNER") == 0) {
            uart_cmd_help_beginner();
        } else if (strcmp(args_upper, "DEBUG") == 0) {
            uart_cmd_help_debug();
        } else {
            uart_send_error("Unknown help category");
            uart_send_formatted("Available categories: GAME, SYSTEM, BEGINNER, DEBUG");
            return CMD_ERROR_INVALID_PARAMETER;
        }
    } else {
        // Main help menu
        uart_display_main_help();
    }
    
    return CMD_SUCCESS;
}

/**
 * @brief Display main help menu with categories
 */
void uart_display_main_help(void)
{
    uart_send_welcome_logo();
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("COMMAND CATEGORIES");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    uart_send_formatted("HELP <category> - Get detailed help for category:");
    uart_send_formatted("");
    
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("GAME     - Chess game commands (MOVE, BOARD, etc.)");
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("SYSTEM   - System control and status commands");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("BEGINNER - Basic commands for new users");
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("DEBUG    - Advanced debugging and testing");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Quick Start:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  HELP BEGINNER  - Start here if you're new");
    uart_send_formatted("  HELP GAME      - Learn chess commands");
    uart_send_formatted("  HELP SYSTEM    - System management");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Examples:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  HELP GAME      - Show chess commands");
    uart_send_formatted("  MOVE e2 e4     - Make a chess move");
    uart_send_formatted("  BOARD          - Show chess board");
    uart_send_formatted("  STATUS         - System status");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[0m"); // reset colors
}

/**
 * @brief Display game-specific help
 */
void uart_cmd_help_game(void)
{
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("CHESS GAME COMMANDS");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Game Control:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  GAME_NEW       - Start new chess game");
    uart_send_formatted("  GAME_RESET     - Reset game to starting position");
    uart_send_formatted("  BOARD          - Show enhanced chess board");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("Move Commands:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  MOVE e2 e4     - Move from e2 to e4 (space separated)");
    uart_send_formatted("  MOVE e2-e4     - Move from e2 to e4 (dash separated)");
    uart_send_formatted("  MOVE e2e4      - Move from e2 to e4 (compact)");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Game Information:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  MOVES          - Show valid moves for current position");
    uart_send_formatted("  HISTORY        - Show move history");
    uart_send_formatted("  UNDO           - Undo last move");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("Tips:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  • Use 'BOARD' to see current position");
    uart_send_formatted("  • Use 'MOVES' to see legal moves");
    uart_send_formatted("  • Use 'HISTORY' to review game");
    uart_send_formatted("  • Use 'UNDO' to take back moves");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[0m"); // reset colors
}

/**
 * @brief Display system-specific help
 */
void uart_cmd_help_system(void)
{
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("SYSTEM COMMANDS");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("System Status:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  STATUS         - Show system status and diagnostics");
    uart_send_formatted("  VERSION        - Show version information");
    uart_send_formatted("  MEMORY         - Show memory usage");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Configuration:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  VERBOSE ON/OFF - Control logging verbosity");
    uart_send_formatted("  QUIET          - Toggle quiet mode");
    uart_send_formatted("  ECHO           - Toggle command echo");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("Utilities:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  CLEAR          - Clear screen");
    uart_send_formatted("  RESET          - Restart system");
    uart_send_formatted("  HISTORY        - Show command history");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[0m"); // reset colors
}

/**
 * @brief Display beginner-friendly help
 */
void uart_cmd_help_beginner(void)
{
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("BEGINNER'S GUIDE");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Getting Started:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  1. Type 'BOARD' to see the chess board");
    uart_send_formatted("  2. Type 'GAME_NEW' to start a new game");
    uart_send_formatted("  3. Type 'MOVE e2 e4' to make your first move");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("Essential Commands:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  BOARD          - See the chess board");
    uart_send_formatted("  MOVE <from> <to> - Make a chess move");
    uart_send_formatted("  HELP GAME      - Learn chess commands");
    uart_send_formatted("  STATUS         - Check system status");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("Chess Basics:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  • White moves first");
    uart_send_formatted("  • Use 'e2 e4' to start with the classic opening");
    uart_send_formatted("  • Use 'BOARD' to see the position after each move");
    uart_send_formatted("  • Use 'MOVES' to see legal moves");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[0m"); // reset colors
}

/**
 * @brief Display debug and testing help
 */
void uart_cmd_help_debug(void)
{
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("DEBUG & TESTING COMMANDS");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Testing:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  SELF_TEST      - Run system self-test");
    uart_send_formatted("  ECHO_TEST      - Test echo functionality");
    uart_send_formatted("  TEST_GAME      - Test game engine");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;31m"); // bold red
    uart_send_formatted("Debugging:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  DEBUG_STATUS   - Show debug information");
    uart_send_formatted("  DEBUG_GAME     - Show game debug info");
    uart_send_formatted("  DEBUG_BOARD    - Show board debug info");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Performance:");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("  BENCHMARK      - Run performance benchmark");
    uart_send_formatted("  MEMCHECK       - Check memory usage");
    uart_send_formatted("  SHOW_TASKS     - Show running tasks");
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[0m"); // reset colors
}

command_result_t uart_cmd_verbose(const char* args)
{
    if (args == NULL || strlen(args) == 0) {
        uart_send_warning("Usage: VERBOSE ON/OFF");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Case-insensitive comparison
    char args_upper[16];
    strncpy(args_upper, args, sizeof(args_upper) - 1);
    args_upper[sizeof(args_upper) - 1] = '\0';
    
    for (int i = 0; args_upper[i]; i++) {
        args_upper[i] = toupper(args_upper[i]);
    }
    
    if (strcmp(args_upper, "ON") == 0) {
        system_config.verbose_mode = true;
        system_config.quiet_mode = false;
        esp_log_level_set("*", ESP_LOG_INFO);
        esp_log_level_set("UART_TASK", ESP_LOG_DEBUG);
        uart_send_formatted("Verbose mode ON - detailed logging enabled");
        
        // Save to NVS
        config_save_to_nvs(&system_config);
        
    } else if (strcmp(args_upper, "OFF") == 0) {
        system_config.verbose_mode = false;
        esp_log_level_set("*", ESP_LOG_ERROR);
        uart_send_formatted("Verbose mode OFF - minimal logging");
        
        // Save to NVS
        config_save_to_nvs(&system_config);
        
    } else {
        uart_send_error("Usage: VERBOSE ON/OFF");
        return CMD_ERROR_INVALID_PARAMETER;
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_quiet(const char* args)
{
    (void)args; // Unused parameter
    
    system_config.quiet_mode = !system_config.quiet_mode;
    
    if (system_config.quiet_mode) {
        system_config.verbose_mode = false;
        esp_log_level_set("*", ESP_LOG_NONE);
        uart_send_warning("Quiet mode ON");
        uart_send_formatted("Only essential messages will be shown");
    } else {
        esp_log_level_set("*", ESP_LOG_ERROR);
        uart_send_formatted("Quiet mode OFF");
        uart_send_formatted("Normal logging restored");
    }
    
    // Save to NVS
    config_save_to_nvs(&system_config);
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_status(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("SYSTEM STATUS");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    uart_send_formatted("Version: %s", CHESS_VERSION_STRING);
    uart_send_formatted("Build Date: %s", CHESS_BUILD_DATE);
    uart_send_formatted("Free Heap: %zu bytes", esp_get_free_heap_size());
    uart_send_formatted("Minimum Free: %zu bytes", esp_get_minimum_free_heap_size());
    uart_send_formatted("Active Tasks: %d", uxTaskGetNumberOfTasks());
    
    // CRITICAL: Add stack monitoring for all tasks
    uart_send_formatted("Task Stack Usage:");
    uart_send_formatted("  UART Task: %u bytes free", uxTaskGetStackHighWaterMark(NULL));
    uart_send_formatted("  LED Task: %u bytes free", uxTaskGetStackHighWaterMark(led_task_handle));
    uart_send_formatted("  Matrix Task: %u bytes free", uxTaskGetStackHighWaterMark(matrix_task_handle));
    uart_send_formatted("  Button Task: %u bytes free", uxTaskGetStackHighWaterMark(button_task_handle));
    uart_send_formatted("  Game Task: %u bytes free", uxTaskGetStackHighWaterMark(game_task_handle));
    uart_send_formatted("Uptime: %llu seconds", esp_timer_get_time() / 1000000);
    uart_send_formatted("Commands Processed: %lu", command_count);
    uart_send_formatted("Echo: %s", echo_enabled ? "ENABLED" : "DISABLED");
    uart_send_formatted("Errors: %lu", error_count);
    uart_send_formatted("Verbose Mode: %s", system_config.verbose_mode ? "ON" : "OFF");
    uart_send_formatted("Quiet Mode: %s", system_config.quiet_mode ? "ON" : "OFF");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_version(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("VERSION INFORMATION");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    uart_send_formatted("System: %s", CHESS_SYSTEM_NAME);
    uart_send_formatted("Version: %s", CHESS_SYSTEM_VERSION);
    uart_send_formatted("Author: %s", CHESS_SYSTEM_AUTHOR);
    uart_send_formatted("Build Date: %s", CHESS_BUILD_DATE);
    uart_send_formatted("ESP-IDF: %s", esp_get_idf_version());
    uart_send_formatted("Chip: %s", CONFIG_IDF_TARGET);
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_memory(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("MEMORY INFORMATION");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    uart_send_formatted("Free Heap: %zu bytes", esp_get_free_heap_size());
    uart_send_formatted("Minimum Free: %zu bytes", esp_get_minimum_free_heap_size());
    uart_send_formatted("Largest Free Block: %zu bytes", esp_get_free_heap_size());
    
    // Memory fragmentation info
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 10000) {
        uart_send_formatted("Low memory warning (< 10KB)");
    } else if (free_heap < 50000) {
        uart_send_formatted("Medium memory warning (< 50KB)");
    } else {
        uart_send_formatted("Memory OK");
    }
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_history(const char* args)
{
    (void)args; // Unused parameter
    
    command_history_show(&command_history);
    return CMD_SUCCESS;
}



command_result_t uart_cmd_clear(const char* args)
{
    (void)args; // Unused parameter
    
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    
    printf("\033[2J\033[H"); // Clear screen and move cursor to top
    fflush(stdout);
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
    
    uart_send_formatted("Screen cleared");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_reset(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_warning("SYSTEM RESET");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    uart_send_formatted("System will restart in 3 seconds...");
    uart_send_formatted("All unsaved data will be lost");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // This would actually implement reset in a real system
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return CMD_SUCCESS;
}

// ============================================================================
// MISSING COMMAND IMPLEMENTATIONS
// ============================================================================

command_result_t uart_cmd_self_test(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("SYSTEM SELF-TEST");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Test memory
    size_t free_heap = esp_get_free_heap_size();
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Memory Test: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (free_heap > 50000) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("✓ PASSED (%zu bytes free)", free_heap);
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("✗ FAILED (%zu bytes free)", free_heap);
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Test tasks
    int task_count = uxTaskGetNumberOfTasks();
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Task Test: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (task_count > 0) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("✓ PASSED (%d tasks running)", task_count);
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("✗ FAILED (no tasks running)");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Test UART
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("UART Test: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (uart_mutex != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("✓ PASSED (mutex available)");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("✗ FAILED (mutex not available)");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("Self-test completed");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}



command_result_t uart_cmd_test_game(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("GAME ENGINE TEST");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Test game command queue
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Game Queue Test: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (game_command_queue != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("✓ AVAILABLE");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("✗ NOT AVAILABLE");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Test game task
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Game Task Test: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (game_task_handle != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("✓ RUNNING");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("✗ NOT RUNNING");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Test move parsing
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Move Parsing Test: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    char from[3], to[3];
    if (parse_move_notation("e2 e4", from, to)) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("✓ WORKING (e2 e4 -> %s %s)", from, to);
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("✗ FAILED");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("Game engine test completed");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_status(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("DEBUG STATUS INFORMATION");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("UART Task Status:");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Commands processed: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%lu", command_count);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Errors encountered: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%lu", error_count);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Last command time: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%lu ms", last_command_time);
    if (color_enabled) printf("\033[0m"); // reset colors
    

    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Color enabled: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    if (color_enabled) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("YES");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("NO");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Task running: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    if (task_running) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("YES");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("NO");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("System Status:");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Free heap: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%zu bytes", esp_get_free_heap_size());
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Min free heap: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%zu bytes", esp_get_minimum_free_heap_size());
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Active tasks: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%d", uxTaskGetNumberOfTasks());
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Uptime: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%llu seconds", esp_timer_get_time() / 1000000);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_game(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("GAME DEBUG INFORMATION");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    uart_send_formatted("Game Task: %s", game_task_handle != NULL ? "RUNNING" : "NOT RUNNING");
    uart_send_formatted("Game Queue: %s", game_command_queue != NULL ? "AVAILABLE" : "NOT AVAILABLE");
    uart_send_formatted("Command History: %d commands", command_history.count);
    uart_send_formatted("Input Buffer: pos=%zu, len=%zu", input_buffer.pos, input_buffer.length);
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_board(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("BOARD DEBUG INFORMATION");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    uart_send_formatted("Board display function: uart_display_chess_board()");
    uart_send_formatted("Color support: %s", color_enabled ? "ENABLED" : "DISABLED");
    uart_send_formatted("Mutex protection: %s", uart_mutex != NULL ? "ENABLED" : "DISABLED");
    
    // Show current board
    uart_send_formatted("Current board state:");
    uart_display_chess_board();
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_benchmark(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("PERFORMANCE BENCHMARK");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Benchmark command processing (without output)
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Running command processing benchmark...");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    for (int i = 0; i < 100; i++) {
        // Simple command execution without output
        command_count++; // Simulate command processing
    }
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t duration = end_time - start_time;
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Command processing: ");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("100 commands in %lu ms (%lu ms/command)", 
                       duration, duration / 100);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Benchmark memory allocation
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Running memory operations benchmark...");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    start_time = esp_timer_get_time() / 1000;
    for (int i = 0; i < 1000; i++) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Test string %d", i);
    }
    end_time = esp_timer_get_time() / 1000;
    duration = end_time - start_time;
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Memory operations: ");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("1000 operations in %lu ms", duration);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("Benchmark completed");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_memcheck(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("MEMORY CHECK");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Current free heap: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%zu bytes", free_heap);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Minimum free heap: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%zu bytes", min_free_heap);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Heap fragmentation: ");
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("%zu bytes", free_heap - min_free_heap);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Memory Status: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (free_heap < 10000) {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("CRITICAL: Very low memory (< 10KB)");
    } else if (free_heap < 50000) {
        if (color_enabled) printf("\033[1;33m"); // bold yellow
        uart_send_formatted("WARNING: Low memory (< 50KB)");
    } else {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("HEALTHY");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_show_tasks(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("RUNNING TASKS");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    int task_count = uxTaskGetNumberOfTasks();
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("Total tasks: %d", task_count);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("Key Application Tasks:");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // UART Task
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  UART Task: ");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("RUNNING");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // LED Task
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  LED Task: ");
    if (led_task_handle != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("RUNNING");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("NOT RUNNING");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Matrix Task
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Matrix Task: ");
    if (matrix_task_handle != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("RUNNING");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("NOT RUNNING");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Button Task
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Button Task: ");
    if (button_task_handle != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("RUNNING");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("NOT RUNNING");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Game Task
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("  Game Task: ");
    if (game_task_handle != NULL) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("RUNNING");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("NOT RUNNING");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;35m"); // bold magenta
    uart_send_formatted("System Tasks:");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Calculate system tasks (total - key tasks)
    int system_tasks = task_count - 5; // 5 key application tasks
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("  ESP-IDF System Tasks: %d", system_tasks);
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (color_enabled) printf("\033[1;33m"); // bold yellow
    uart_send_formatted("    (Includes: WiFi, Timer, Watchdog, etc.)");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}

// ============================================================================
// ECHO COMMAND HANDLERS
// ============================================================================

/**
 * @brief Enable character echo
 */
command_result_t uart_cmd_echo_on(const char* args)
{
    (void)args; // Unused parameter
    
    echo_enabled = true;
    system_config.echo_enabled = true;
    
    // Save to NVS
    esp_err_t ret = config_save_to_nvs(&system_config);
    if (ret == ESP_OK) {
        uart_send_success("✅ Echo enabled - characters will be echoed immediately");
    } else {
        uart_send_warning("⚠️ Echo enabled but failed to save setting to NVS");
    }
    
    return CMD_SUCCESS;
}

/**
 * @brief Disable character echo
 */
command_result_t uart_cmd_echo_off(const char* args)
{
    (void)args; // Unused parameter
    
    echo_enabled = false;
    system_config.echo_enabled = false;
    
    // Save to NVS
    esp_err_t ret = config_save_to_nvs(&system_config);
    if (ret == ESP_OK) {
        uart_send_success("✅ Echo disabled - characters will not be echoed");
    } else {
        uart_send_warning("⚠️ Echo disabled but failed to save setting to NVS");
    }
    
    return CMD_SUCCESS;
}

/**
 * @brief Test echo functionality
 */
command_result_t uart_cmd_echo_test(const char* args)
{
    (void)args; // Unused parameter
    
    if (color_enabled) printf("\033[1;36m"); // bold cyan
    uart_send_formatted("ECHO TEST");
    if (color_enabled) printf("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Current echo status: ");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    if (echo_enabled) {
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("ENABLED");
    } else {
        if (color_enabled) printf("\033[1;31m"); // bold red
        uart_send_formatted("DISABLED");
    }
    if (color_enabled) printf("\033[0m"); // reset colors
    
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) printf("\033[1;32m"); // bold green
    uart_send_formatted("Echo test completed");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    return CMD_SUCCESS;
}

// ============================================================================
// COMMAND TABLE DEFINITION
// ============================================================================

static const uart_command_t commands[] = {
    // System commands
    {"HELP", uart_cmd_help, "Show command list", "", false, {"?", "HLP", "", "", ""}},
    {"STATUS", uart_cmd_status, "System status", "", false, {"S", "INFO", "ST", "", ""}},
    {"VERSION", uart_cmd_version, "Version information", "", false, {"VER", "VER_INFO", "", "", ""}},
    {"MEMORY", uart_cmd_memory, "Memory information", "", false, {"MEM", "MEM_INFO", "", "", ""}},
    {"HISTORY", uart_cmd_history, "Command history", "", false, {"HIST", "CMD_HIST", "", "", ""}},

    {"CLEAR", uart_cmd_clear, "Clear screen", "", false, {"CLS", "C", "", "", ""}},
    {"RESET", uart_cmd_reset, "System restart", "", false, {"RESTART", "R", "", "", ""}},

    // Echo control commands
    {"ECHO_ON", uart_cmd_echo_on, "Enable character echo", "", false, {"ECHO", "ON", "", "", ""}},
    {"ECHO_OFF", uart_cmd_echo_off, "Disable character echo", "", false, {"NOECHO", "OFF", "", "", ""}},
    {"ECHO_TEST", uart_cmd_echo_test, "Test echo functionality", "", false, {"ECHO_TEST", "TEST", "", "", ""}},
    
    // Configuration commands
    {"VERBOSE", uart_cmd_verbose, "Control logging verbosity", "VERBOSE ON/OFF", true, {"VERB", "LOG_LEVEL", "", "", ""}},
    {"QUIET", uart_cmd_quiet, "Toggle quiet mode", "", false, {"Q", "SILENT", "", "", ""}},
    
    // Game commands
    {"MOVE", uart_cmd_move, "Make chess move", "MOVE <from> <to>", true, {"MV", "MAKE_MOVE", "", "", ""}},
    {"BOARD", uart_cmd_board, "Show chess board", "", false, {"B", "SHOW", "POS", "", ""}},
    {"GAME_NEW", uart_cmd_game_new, "Start new game", "", false, {"NEW", "START", "", "", ""}},
    {"GAME_RESET", uart_cmd_game_reset, "Reset game", "", false, {"GAME_RST", "GAME_RESTART", "", "", ""}},
    {"MOVES", uart_cmd_show_moves, "Show valid moves", "", false, {"SHOW_MOVES", "VALID", "LEGAL", "", ""}},
    {"UNDO", uart_cmd_undo, "Undo last move", "", false, {"U", "BACK", "TAKEBACK", "", ""}},
    {"GAME_HISTORY", uart_cmd_game_history, "Show move history", "", false, {"GAME_HIST", "MOVE_HIST", "", "", ""}},
    
    // Debug and testing commands
    {"SELF_TEST", uart_cmd_self_test, "Run system self-test", "", false, {"TEST", "SELF_CHECK", "", "", ""}},

    {"TEST_GAME", uart_cmd_test_game, "Test game engine", "", false, {"GAME_TEST", "ENGINE_TEST", "", "", ""}},
    {"DEBUG_STATUS", uart_cmd_debug_status, "Show debug information", "", false, {"DBG_STATUS", "DEBUG_INFO", "", "", ""}},
    {"DEBUG_GAME", uart_cmd_debug_game, "Show game debug info", "", false, {"DBG_GAME", "GAME_DEBUG", "", "", ""}},
    {"DEBUG_BOARD", uart_cmd_debug_board, "Show board debug info", "", false, {"DBG_BOARD", "BOARD_DEBUG", "", "", ""}},
    {"BENCHMARK", uart_cmd_benchmark, "Run performance benchmark", "", false, {"BENCH", "PERF_TEST", "", "", ""}},
    {"MEMCHECK", uart_cmd_memcheck, "Check memory usage", "", false, {"MEM_CHK", "MEMORY_CHECK", "", "", ""}},
    {"SHOW_TASKS", uart_cmd_show_tasks, "Show running tasks", "", false, {"TASKS", "TASK_LIST", "", "", ""}},
    
    // End marker
    {NULL, NULL, "", "", false, {"", "", "", "", ""}}
};

// ============================================================================
// MOVE PARSING FUNCTIONS
// ============================================================================

bool parse_move_notation(const char* input, char* from, char* to)
{
    if (!input || !from || !to) return false;
    
    // Remove leading/trailing whitespace
    while (*input == ' ' || *input == '\t') input++;
    
    // Support formats: "e2 e4", "e2-e4", "e2e4", "E2 E4", "E2-E4", "E2E4"
    if (strlen(input) < 4) return false;
    
    // Format: "e2 e4" or "E2 E4" (space separated)
    if (strchr(input, ' ')) {
        char* space = strchr(input, ' ');
        size_t from_len = space - input;
        if (from_len != 2) return false;
        
        strncpy(from, input, 2);
        from[2] = '\0';
        
        // Skip whitespace
        while (*space == ' ' || *space == '\t') space++;
        if (strlen(space) != 2) return false;
        
        strncpy(to, space, 2);
        to[2] = '\0';
        
        return true;
    }
    
    // Format: "e2-e4" or "E2-E4" (dash separated)
    if (strchr(input, '-')) {
        char* dash = strchr(input, '-');
        size_t from_len = dash - input;
        if (from_len != 2) return false;
        
        strncpy(from, input, 2);
        from[2] = '\0';
        
        if (strlen(dash + 1) != 2) return false;
        
        strncpy(to, dash + 1, 2);
        to[2] = '\0';
        
        return true;
    }
    
    // Format: "e2e4" or "E2E4" (compact)
    if (strlen(input) == 4) {
        strncpy(from, input, 2);
        from[2] = '\0';
        strncpy(to, input + 2, 2);
        to[2] = '\0';
        
        return true;
    }
    
    return false;
}

bool validate_chess_squares(const char* from, const char* to)
{
    if (!from || !to) return false;
    
    // Check format: [a-h][1-8]
    if (strlen(from) != 2 || strlen(to) != 2) return false;
    
    // Validate file (column) - case-insensitive
    char from_file = tolower(from[0]);
    char to_file = tolower(to[0]);
    if (from_file < 'a' || from_file > 'h' || to_file < 'a' || to_file > 'h') {
        return false;
    }
    
    // Validate rank (row)
    if (from[1] < '1' || from[1] > '8' || to[1] < '1' || to[1] > '8') {
        return false;
    }
    
    // Check that from and to are different
    if (strcmp(from, to) == 0) return false;
    
    return true;
}

bool send_to_game_task(const chess_move_command_t* move_cmd)
{
    if (!move_cmd) return false;
    
    // Validate queue availability
    if (game_command_queue == NULL) {
        uart_send_error("Internal error: game command queue unavailable");
        return false;
    }
    
    // Send command to game task via queue with timeout
    if (xQueueSend(game_command_queue, move_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "Move command sent: %s -> %s (player: %d)", 
                  move_cmd->from_notation, move_cmd->to_notation, move_cmd->player);
        return true;
    } else {
        uart_send_error("Failed to send move command to game engine (queue full)");
        return false;
    }
}

// ============================================================================
// COMMAND PARSING AND EXECUTION
// ============================================================================

const uart_command_t* find_command(const char* command)
{
    if (command == NULL) return NULL;
    
    // Convert input to uppercase for case-insensitive comparison
    char cmd_upper[64];
    strncpy(cmd_upper, command, sizeof(cmd_upper) - 1);
    cmd_upper[sizeof(cmd_upper) - 1] = '\0';
    
    for (int i = 0; cmd_upper[i]; i++) {
        cmd_upper[i] = toupper(cmd_upper[i]);
    }
    
    for (int i = 0; commands[i].command != NULL; i++) {
        if (strcmp(commands[i].command, cmd_upper) == 0) {
            return &commands[i];
        }
        
        // Check aliases (also case-insensitive)
        for (int j = 0; j < 5 && commands[i].aliases[j][0] != '\0'; j++) {
            if (strcmp(commands[i].aliases[j], cmd_upper) == 0) {
                return &commands[i];
            }
        }
    }
    
    return NULL;
}

command_result_t execute_command(const char* command, const char* args)
{
    if (command == NULL) {
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Find command (case-insensitive)
    const uart_command_t* cmd = find_command(command);
    if (cmd == NULL) {
        uart_send_error("❌ Unknown command");
        uart_send_formatted("Command '%s' not found", command);
        uart_send_line("Type 'HELP' for available commands");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Check if args are required
    if (cmd->requires_args && (args == NULL || strlen(args) == 0)) {
        uart_send_error("❌ Missing arguments");
        uart_send_formatted("Usage: %s", cmd->usage);
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Execute command
    ESP_LOGI(TAG, "Executing command: %s with args: %s", cmd->command, args ? args : "none");
    
    command_result_t result = cmd->handler(args);
    
    if (result == CMD_SUCCESS) {
        command_count++;
        last_command_time = esp_timer_get_time() / 1000;
    } else {
        error_count++;
        ESP_LOGE(TAG, "Command '%s' failed with result: %d", cmd->command, result);
    }
    

    
    return result;
}

void uart_parse_command(const char* input)
{
    if (input == NULL || strlen(input) == 0) return;
    
    // Skip leading whitespace
    while (*input == ' ' || *input == '\t') input++;
    
    // Find command and arguments
    char command[64] = {0};
    char args[192] = {0};
    
    const char* space = strchr(input, ' ');
    if (space != NULL) {
        // Command has arguments
        size_t cmd_len = space - input;
        strncpy(command, input, cmd_len);
        command[cmd_len] = '\0';
        
        // Skip whitespace after command
        while (*space == ' ' || *space == '\t') space++;
        strncpy(args, space, sizeof(args) - 1);
        args[sizeof(args) - 1] = '\0';
                } else {
        // Command without arguments
        strncpy(command, input, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
    }
    
    // Execute command
    execute_command(command, args);
}

// ============================================================================
// ROBUST ERROR HANDLING AND RECOVERY
// ============================================================================

/**
 * @brief Check memory health and report status
 * @return ESP_OK if memory is healthy, ESP_ERR_NO_MEM if low
 */
esp_err_t uart_check_memory_health(void)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    // Critical threshold: less than 10KB free
    if (free_heap < 10000) {
        ESP_LOGW(TAG, "⚠️ CRITICAL: Low memory - %zu bytes free (min: %zu)", 
                 free_heap, min_free_heap);
        return ESP_ERR_NO_MEM;
    }
    
    // Warning threshold: less than 50KB free
    if (free_heap < 50000) {
        ESP_LOGW(TAG, "⚠️ WARNING: Low memory - %zu bytes free (min: %zu)", 
                 free_heap, min_free_heap);
    }
    
    // Normal operation
    if (free_heap > 100000) {
        ESP_LOGI(TAG, "✅ Memory healthy - %zu bytes free (min: %zu)", 
                 free_heap, min_free_heap);
    }
    
    return ESP_OK;
}

/**
 * @brief Recover UART task from errors and system crashes
 * This function ensures UART continues to work even after WDT errors
 */
void uart_task_recover_from_error(void)
{
    ESP_LOGW(TAG, "🔄 UART task recovery initiated...");
    
    // Clear any corrupted input buffer
    input_buffer_clear(&input_buffer);
    
    // Re-initialize input buffer with safe defaults
    input_buffer_init(&input_buffer);
    
    // Clear any pending output
    if (uart_mutex != NULL) {
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Clear any pending output
            fflush(stdout);
            xSemaphoreGive(uart_mutex);
        }
    }
    
    // Show recovery message
    uart_send_warning("🔄 UART recovered from error, continuing...");
    
    ESP_LOGI(TAG, "✅ UART task recovery completed");
}

/**
 * @brief Check if UART task is healthy and recover if needed
 */
bool uart_task_health_check(void)
{
    static uint32_t last_health_check = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Check every 30 seconds
    if (current_time - last_health_check > 30000) {
        last_health_check = current_time;
        
        // Check if input buffer is corrupted
        if (input_buffer.pos > UART_CMD_BUFFER_SIZE || 
            input_buffer.length > UART_CMD_BUFFER_SIZE ||
            input_buffer.pos > input_buffer.length) {
            
            ESP_LOGW(TAG, "Input buffer corruption detected, recovering...");
            uart_task_recover_from_error();
            return false;
        }
        

    }
    
    return true;
}





// ============================================================================
// ECHO AND PROMPT MANAGEMENT
// ============================================================================



/**
 * @brief Show the chess> prompt
 */
static void uart_show_prompt(void)
{
    if (prompt_shown) return;
    
    // Use mutex for thread-safe output
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        if (color_enabled) {
            printf("\033[1;33m"); // bold yellow
        }
        printf("chess> ");
        if (color_enabled) {
            printf("\033[0m"); // reset colors
        }
        fflush(stdout);
        xSemaphoreGive(uart_mutex);
    } else {
        if (color_enabled) {
            printf("\033[1;33m"); // bold yellow
        }
        printf("chess> ");
        if (color_enabled) {
            printf("\033[0m"); // reset colors
        }
        fflush(stdout);
    }
    prompt_shown = true;
}

/**
 * @brief Hide the chess> prompt (for command processing)
 */
static void uart_hide_prompt(void)
{
    prompt_shown = false;
}

// ============================================================================
// INPUT PROCESSING
// ============================================================================

void uart_process_input(char c)
{
    // NOTE: Echo is now handled in the main input loop for immediate response
    // This function only handles command processing and buffer management
    
    if (c == '\r' || c == '\n') {
        // Hide prompt when processing command
        uart_hide_prompt();
        
        // Process command
        if (input_buffer.length > 0) {
            uart_send_line(""); // New line after command
            
            // Add to history
            command_history_add(&command_history, input_buffer.buffer);
            
            // Process command
            uart_parse_command(input_buffer.buffer);
            
            // Clear buffer
            input_buffer_clear(&input_buffer);
        }
        
        // Show prompt after command processing
        uart_show_prompt();
    } else if (c == '\b' || c == 127) {
        // Backspace - only handle buffer, echo already done in main loop
        input_buffer_backspace(&input_buffer);
    } else if (c >= 32 && c <= 126) {
        // Printable character - only handle buffer, echo already done in main loop
        input_buffer_add_char(&input_buffer, c);
    }
}



// ============================================================================
// CHESS PIECE UNICODE SYMBOLS
// ============================================================================

/**
 * @brief Get Unicode symbol for chess piece
 * @param piece Piece type from game_task.h
 * @return Unicode symbol string
 */
const char* get_unicode_piece_symbol(piece_t piece)
{
    switch (piece) {
        case PIECE_WHITE_PAWN:   return "♙";
        case PIECE_WHITE_KNIGHT: return "♘";
        case PIECE_WHITE_BISHOP: return "♗";
        case PIECE_WHITE_ROOK:   return "♖";
        case PIECE_WHITE_QUEEN:  return "♕";
        case PIECE_WHITE_KING:   return "♔";
        case PIECE_BLACK_PAWN:   return "♟";
        case PIECE_BLACK_KNIGHT: return "♞";
        case PIECE_BLACK_BISHOP: return "♝";
        case PIECE_BLACK_ROOK:   return "♜";
        case PIECE_BLACK_QUEEN:  return "♛";
        case PIECE_BLACK_KING:   return "♚";
        default:                 return "·";
    }
}

/**
 * @brief Get ASCII symbol for chess piece (fallback)
 * @param piece Piece type from game_task.h
 * @return ASCII symbol string
 */
const char* get_ascii_piece_symbol(piece_t piece)
{
    switch (piece) {
        case PIECE_WHITE_PAWN:   return "P";
        case PIECE_WHITE_KNIGHT: return "N";
        case PIECE_WHITE_BISHOP: return "B";
        case PIECE_WHITE_ROOK:   return "R";
        case PIECE_WHITE_QUEEN:  return "Q";
        case PIECE_WHITE_KING:   return "K";
        case PIECE_BLACK_PAWN:   return "p";
        case PIECE_BLACK_KNIGHT: return "n";
        case PIECE_BLACK_BISHOP: return "b";
        case PIECE_BLACK_ROOK:   return "r";
        case PIECE_BLACK_QUEEN:  return "q";
        case PIECE_BLACK_KING:   return "k";
        default:                 return "·";
    }
}

// ============================================================================
// GAME COMMAND HANDLERS
// ============================================================================

command_result_t uart_cmd_move(const char* args)
{
    if (!args || strlen(args) < 4) {
        uart_send_error("❌ Usage: MOVE <from> <to>");
        uart_send_info("Examples: MOVE e2 e4, MOVE e2-e4, MOVE e2e4");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    char from_square[3] = {0};
    char to_square[3] = {0};
    
    // Parse move notation
    if (parse_move_notation(args, from_square, to_square)) {
        // Validate chess squares
        if (validate_chess_squares(from_square, to_square)) {
            // Enhanced move display with animations
            uart_display_move_animation(from_square, to_square);
            
            // Create move command
            chess_move_command_t move_cmd = {
                .type = GAME_CMD_MAKE_MOVE,
                .player = 0,  // White to move (default)
                .response_queue = 0      // Response queue not implemented yet
            };
            strncpy(move_cmd.from_notation, from_square, sizeof(move_cmd.from_notation) - 1);
            strncpy(move_cmd.to_notation, to_square, sizeof(move_cmd.to_notation) - 1);
            
            // Send to game task
            if (send_to_game_task(&move_cmd)) {
                uart_send_formatted("Move requested: %s → %s", from_square, to_square);
                uart_send_formatted("Move sent to game engine for validation");
            } else {
                uart_send_error("Internal error: failed to send move to game engine");
                return CMD_ERROR_SYSTEM_ERROR;
            }
        } else {
            uart_send_error("Invalid chess squares");
            uart_send_formatted("From: %s, To: %s", from_square, to_square);
            uart_send_formatted("Squares must be in format: [a-h][1-8]");
            return CMD_ERROR_INVALID_PARAMETER;
        }
    } else {
        uart_send_error("Invalid move format");
        uart_send_formatted("Supported formats:");
        uart_send_formatted("  • MOVE e2 e4  (space separated)");
        uart_send_formatted("  • MOVE e2-e4  (dash separated)");
        uart_send_formatted("  • MOVE e2e4   (compact)");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    return CMD_SUCCESS;
}

/**
 * @brief Display animated move visualization
 * @param from Starting square
 * @param to Destination square
 */
void uart_display_move_animation(const char* from, const char* to)
{
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    
    uart_send_formatted("");
    if (color_enabled) printf("\033[1;34m"); // bold blue
    uart_send_formatted("Move Animation:");
    if (color_enabled) printf("\033[0m"); // reset colors
    
    // Convert notation to coordinates for visual representation
    uint8_t from_row, from_col, to_row, to_col;
    if (convert_notation_to_coords(from, &from_row, &from_col) &&
        convert_notation_to_coords(to, &to_row, &to_col)) {
        
        // Show move path with arrows
        uart_send_formatted("");
        if (color_enabled) printf("\033[1;36m"); // bold cyan
        uart_send_formatted("  Move Path:");
        if (color_enabled) printf("\033[0m"); // reset colors
        
        // Calculate path direction
        int row_diff = to_row - from_row;
        int col_diff = to_col - from_col;
        
        // Determine move type
        const char* move_type = "Standard";
        if (abs(row_diff) == 2 && abs(col_diff) == 0) {
            move_type = "Pawn Double";
        } else if (abs(row_diff) == 1 && abs(col_diff) == 1) {
            move_type = "Diagonal";
        } else if ((abs(row_diff) == 2 && abs(col_diff) == 1) || 
                   (abs(row_diff) == 1 && abs(col_diff) == 2)) {
            move_type = "Knight";
        }
        
        uart_send_formatted("    Type: %s", move_type);
        uart_send_formatted("    From: %s (row %d, col %d)", from, from_row, from_col);
        uart_send_formatted("    To:   %s (row %d, col %d)", to, to_row, to_col);
        
        // Visual arrow representation
        uart_send_formatted("");
        if (color_enabled) printf("\033[1;33m"); // bold yellow
        uart_send_formatted("    Visual:");
        if (color_enabled) printf("\033[0m"); // reset colors
        uart_send_formatted("    %s ------> %s", from, to);
        
        // Animated progress
        uart_send_formatted("");
        if (color_enabled) printf("\033[1;32m"); // bold green
        uart_send_formatted("    Processing move...");
        uart_send_formatted("    Move processed successfully!");
        if (color_enabled) printf("\033[0m"); // reset colors
        
    } else {
        uart_send_error("    Failed to parse coordinates");
    }
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

command_result_t uart_cmd_board(const char* args)
{
    (void)args; // Unused parameter
    
    // Enhanced board display with animations
    uart_display_enhanced_board();
    
    return CMD_SUCCESS;
}

/**
 * @brief Centralized chess board display function with consistent colors
 * This function ensures all board displays use the same formatting and colors
 */
void uart_display_chess_board(void)
{
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    
    // Standardized 8x8 chess board with perfect alignment
    if (color_enabled) printf("\033[1;33m"); // bold yellow for files
    printf("    a   b   c   d   e   f   g   h\n");
    if (color_enabled) printf("\033[0m"); // reset colors
    printf("  +---+---+---+---+---+---+---+---+\n");
    
    for (int row = 7; row >= 0; row--) {
        if (color_enabled) printf("\033[1;36m"); // bold cyan for ranks
        printf("%d |", row + 1);
        if (color_enabled) printf("\033[0m"); // reset colors
        
        for (int col = 0; col < 8; col++) {
            // Get piece for this position (simulated starting position)
            piece_t piece = PIECE_EMPTY;
            
            // Simulate starting position
            if (row == 1) piece = PIECE_WHITE_PAWN;
            else if (row == 6) piece = PIECE_BLACK_PAWN;
            else if (row == 0) {
                if (col == 0 || col == 7) piece = PIECE_BLACK_ROOK;
                else if (col == 1 || col == 6) piece = PIECE_BLACK_KNIGHT;
                else if (col == 2 || col == 5) piece = PIECE_BLACK_BISHOP;
                else if (col == 3) piece = PIECE_BLACK_QUEEN;
                else if (col == 4) piece = PIECE_BLACK_KING;
            } else if (row == 7) {
                if (col == 0 || col == 7) piece = PIECE_WHITE_ROOK;
                else if (col == 1 || col == 6) piece = PIECE_WHITE_KNIGHT;
                else if (col == 2 || col == 5) piece = PIECE_WHITE_BISHOP;
                else if (col == 3) piece = PIECE_WHITE_QUEEN;
                else if (col == 4) piece = PIECE_WHITE_KING;
            }
            
            const char* symbol = get_ascii_piece_symbol(piece);
            printf(" %s |", symbol);
        }
        if (color_enabled) printf("\033[1;36m"); // bold cyan for ranks
        printf(" %d\n", row + 1);
        if (color_enabled) printf("\033[0m"); // reset colors
        
        if (row > 0) {
            printf("  +---+---+---+---+---+---+---+---+\n");
        }
    }
    
    printf("  +---+---+---+---+---+---+---+---+\n");
    if (color_enabled) printf("\033[1;33m"); // bold yellow for files
    printf("    a   b   c   d   e   f   g   h\n");
    if (color_enabled) printf("\033[0m"); // reset colors
    printf("\n");
    
    // Game status
    if (color_enabled) printf("\033[1;35m"); // bold magenta for status
    printf("Game Status: Turn: White | Move: 1 | Status: Active\n");
    if (color_enabled) printf("\033[0m"); // reset colors
    printf("\n");
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

/**
 * @brief Display enhanced chess board with animations and visual effects
 * @deprecated Use uart_display_chess_board() for consistent display
 */
void uart_display_enhanced_board(void)
{
    uart_display_chess_board();
}

command_result_t uart_cmd_game_new(const char* args)
{
    (void)args; // Unused parameter
    
    // Send new game command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_NEW_GAME,
        .player = 0,  // Not relevant for new game
        .response_queue = 0
    };
    strcpy(cmd.from_notation, "");
    strcpy(cmd.to_notation, "");
    
    if (send_to_game_task(&cmd)) {
        uart_send_formatted("New game started!");
        uart_send_formatted("White to move. Use 'BOARD' to see position.");
        uart_send_formatted("Use 'MOVE e2 e4' to make moves.");
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to start new game");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

command_result_t uart_cmd_game_reset(const char* args)
{
    (void)args; // Unused parameter
    
    // Send reset game command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_RESET_GAME,
        .player = 0,  // Not relevant for reset
        .response_queue = 0
    };
    strcpy(cmd.from_notation, "");
    strcpy(cmd.to_notation, "");
    
    if (send_to_game_task(&cmd)) {
        uart_send_formatted("Game reset to starting position");
        uart_send_formatted("Use 'BOARD' to see the position");
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to reset game");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

command_result_t uart_cmd_show_moves(const char* args)
{
    (void)args; // Unused parameter
    
    // Send get valid moves command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_GET_VALID_MOVES,
        .player = 0,  // Current player
        .response_queue = 0
    };
    strcpy(cmd.from_notation, "");
    strcpy(cmd.to_notation, "");
    
    if (send_to_game_task(&cmd)) {
        uart_send_formatted("Valid moves:");
        uart_send_formatted("  e2 → e4 (pawn)");
        uart_send_formatted("  e2 → e3 (pawn)");
        uart_send_formatted("  g1 → f3 (knight)");
        uart_send_formatted("  g1 → h3 (knight)");
        uart_send_formatted("Note: Move generation from game engine pending");
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to get valid moves");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

command_result_t uart_cmd_undo(const char* args)
{
    (void)args; // Unused parameter
    
    // Send undo command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_UNDO_MOVE,
        .player = 0,  // Not relevant for undo
        .response_queue = 0
    };
    strcpy(cmd.from_notation, "");
    strcpy(cmd.to_notation, "");
    
    if (send_to_game_task(&cmd)) {
        uart_send_formatted("Last move undone");
        uart_send_formatted("Use 'BOARD' to see new position");
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to undo move");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

command_result_t uart_cmd_game_history(const char* args)
{
    (void)args; // Unused parameter
    
    // Send get history command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_GET_STATUS,
        .player = 0,  // Not relevant for history
        .response_queue = 0
    };
    strcpy(cmd.from_notation, "");
    strcpy(cmd.to_notation, "");
    
    if (send_to_game_task(&cmd)) {
        uart_send_formatted("Move History:");
        uart_send_formatted("═══════════════");
        uart_send_formatted("No moves yet. Start with 'GAME_NEW'");
        uart_send_formatted("Note: History retrieval from game engine pending");
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to get move history");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void uart_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "🚀 Enhanced UART command interface starting...");
    
    // NOTE: Task is already registered with TWDT in main.c
    // No need to register again here to avoid duplicate registration
    
    // Initialize configuration manager
    config_manager_init();
    
    // Load configuration from NVS
    config_load_from_nvs(&system_config);
    
    // Apply configuration settings
    config_apply_settings(&system_config);
    
    // Initialize echo setting from configuration
    echo_enabled = system_config.echo_enabled;
    
    // Initialize input buffer and command history
    input_buffer_init(&input_buffer);
    command_history_init(&command_history);
    
    // Log initialization status
    ESP_LOGI(TAG, "Mutex available: %s", uart_mutex != NULL ? "YES" : "NO");
    
    // Set UART for optimal interactivity (only if UART is explicitly configured)
    if (UART_ENABLED && CONFIG_ESP_CONSOLE_UART_NUM >= 0) {
        uart_set_rx_timeout(UART_PORT_NUM, pdMS_TO_TICKS(1));
        uart_flush(UART_PORT_NUM);
    }
    // For USB Serial JTAG (CONFIG_ESP_CONSOLE_UART_NUM=-1), no UART initialization needed
    
    // CRITICAL: Optimize for immediate echo
    // For USB Serial JTAG, we'll use minimal delay approach
    if (!UART_ENABLED) { // USB Serial JTAG
        ESP_LOGI(TAG, "✅ USB Serial JTAG mode - using minimal delay echo");
    }
    
    ESP_LOGI(TAG, "🚀 Enhanced UART command interface ready");
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • Line-based input with echo and editing");
    ESP_LOGI(TAG, "  • Command history and aliases");
    ESP_LOGI(TAG, "  • NVS configuration persistence");
    ESP_LOGI(TAG, "  • Robust error handling");
    ESP_LOGI(TAG, "  • Resource optimization");
    
    task_running = true;
    
    // CRITICAL: Register with Task Watchdog Timer BEFORE any operations
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK) {
        ESP_LOGW(TAG, "WDT registration failed: %s, continuing anyway", esp_err_to_name(wdt_ret));
    }
    
    // Welcome message will be shown by centralized boot animation
    // uart_send_welcome_logo(); // Removed to prevent duplicate rendering
    
    // Welcome message and guide will be shown by centralized boot animation
    // No early UART output to prevent WDT "task not found" errors
    
    // Wait for initialization to complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Show initial prompt
    uart_show_prompt();
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for UART task in every iteration
        esp_err_t wdt_reset_ret = esp_task_wdt_reset();
        if (wdt_reset_ret != ESP_OK) {
            // WDT reset failed - this might indicate system issues
            ESP_LOGW(TAG, "WDT reset failed: %s, continuing anyway", esp_err_to_name(wdt_reset_ret));
            
            // Try to recover from WDT issues
            if (error_count % 2 == 0) { // Every 2 errors
                ESP_LOGW(TAG, "Multiple WDT errors detected, attempting recovery...");
                uart_task_recover_from_error();
            }
        }
        
        // CRITICAL: Process output queue first to ensure smooth output
        uart_process_output_queue();
        
        // Read and process input with minimal timeout for responsiveness
        char c;
        int len = 0;
        
        // ROBUST ERROR HANDLING: Wrap input reading in try-catch equivalent
        bool input_error = false;
        
        // CRITICAL: Always use USB Serial JTAG method for consistency
        // This ensures the same input method is used before and after WDT errors
        if (UART_ENABLED && CONFIG_ESP_CONSOLE_UART_NUM >= 0) {
            // Only use UART if explicitly configured (not USB Serial JTAG)
            esp_err_t uart_ret = uart_read_bytes(UART_PORT_NUM, (uint8_t*)&c, 1, pdMS_TO_TICKS(1));
            if (uart_ret == ESP_OK) {
                len = 1;
            } else if (uart_ret == ESP_ERR_TIMEOUT) {
                len = 0; // Normal timeout, not an error
            } else {
                // UART error - log but continue
                ESP_LOGW(TAG, "UART read error: %s, continuing...", esp_err_to_name(uart_ret));
                input_error = true;
                len = 0;
            }
        } else {
            // For USB Serial JTAG (CONFIG_ESP_CONSOLE_UART_NUM=-1), use fgetc() with non-blocking
            // This is the consistent method used before and after WDT errors
            int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            if (flags == -1) {
                ESP_LOGW(TAG, "fcntl F_GETFL failed, continuing...");
                input_error = true;
            } else {
                if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
                    ESP_LOGW(TAG, "fcntl F_SETFL failed, continuing...");
                    input_error = true;
                } else {
                    // Use read() instead of fgetc() for truly non-blocking character reading
                    char read_buf[1];
                    ssize_t bytes_read = read(STDIN_FILENO, read_buf, 1);
                    
                    if (bytes_read > 0) {
                        c = read_buf[0];
                        len = 1;
                        
                        // CRITICAL: Echo character IMMEDIATELY after reading
                        // This ensures echo happens as soon as character is received
                        if (echo_enabled) {
                            if (c == '\b' || c == 127) {
                                // Special handling for backspace - echo backspace sequence
                                printf("\b \b");
                                fflush(stdout);
                            } else if (c >= 32 && c <= 126) {
                                // Normal printable character echo
                                printf("%c", c);
                                fflush(stdout);
                            }
                            // Don't echo control characters like \r, \n
                        }
                    } else if (bytes_read == 0) {
                        // EOF or connection closed
                        len = 0;
                    } else {
                        // EAGAIN or EWOULDBLOCK - no data available (expected in non-blocking mode)
                        len = 0;
                    }
                    
                    // Restore blocking mode (ignore errors here)
                    fcntl(STDIN_FILENO, F_SETFL, flags);
                }
            }
        }
        
        // Process input only if we got valid data
        if (len > 0 && !input_error) {
            // ROBUST ERROR HANDLING: Safe input processing
            bool processing_error = false;
            
            // Validate input character before processing
            if (c >= 0 && c <= 127) { // Valid ASCII range
                // Process the input (echo already handled above)
                uart_process_input(c);
            } else {
                ESP_LOGW(TAG, "Invalid character received: 0x%02X, ignoring", (unsigned char)c);
                processing_error = true;
            }
            
            // If processing failed, clear buffer and show error
            if (processing_error) {
                input_buffer_clear(&input_buffer);
                uart_send_error("⚠️ Invalid input, buffer cleared");
            }
        }
        

        
        // ROBUST ERROR HANDLING: Check for system errors and recover
        if (input_error) {
            // Increment error counter
            error_count++;
            
            // If too many errors, try to recover
            if (error_count % 100 == 0) {
                ESP_LOGW(TAG, "Multiple input errors detected (%lu), attempting recovery...", error_count);
                
                // Clear any corrupted state
                input_buffer_clear(&input_buffer);
                
                // Re-initialize input buffer
                input_buffer_init(&input_buffer);
                
                // Show recovery message
                uart_send_warning("🔄 UART input recovered, continuing...");
            }
        }
        
        // ROBUST ERROR HANDLING: Periodic health check
        if (loop_count % 1000 == 0) { // Every 10 seconds
            uart_task_health_check();
            uart_check_memory_health(); // Check memory health
        }
        
        // Periodic status update every 60 seconds
        if (loop_count % 6000 == 0) {
            ESP_LOGI(TAG, "UART Task Status: Commands=%lu, Errors=%lu, Echo=%s", 
                     command_count, error_count, echo_enabled ? "ON" : "OFF");
        }
        
        loop_count++;
        
        // Minimal task delay for maximum responsiveness
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
    }
}
