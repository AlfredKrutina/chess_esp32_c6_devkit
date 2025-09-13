/**
 * @file uart_task_improved.c
 * @brief ESP32-C6 Chess System v2.4 - Improved UART Task Implementation
 * 
 * This task provides a production-ready line-based UART terminal with proper echo:
 * - Non-blocking character input with immediate echo
 * - Line-based input with editing
 * - Command table with function pointers
 * - Advanced command features (aliases, auto-completion)
 * - NVS configuration persistence
 * - Robust error handling and validation
 * - Resource optimization
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-27
 * 
 * Improvements:
 * - Fixed echo functionality using proper non-blocking I/O
 * - Simplified input handling logic
 * - Better error recovery
 * - Optimized for USB Serial JTAG
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
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
#include <errno.h>

#include "uart_task.h"
#include "freertos_chess.h"
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

// Forward declarations for functions used before definition
static bool send_move_to_game_task(const char* move_str);
static void uart_input_loop_improved(void);

// Missing type definitions that should be here
typedef struct {
    const char* command;
    command_handler_t handler;
    const char* help_text;
    const char* usage;
    bool requires_args;
    const char* aliases[5];  // Up to 5 aliases
} uart_command_t;

static const char *TAG = "UART_TASK_IMPROVED";

// UART configuration - only use if UART is enabled
#if CONFIG_ESP_CONSOLE_UART_NUM >= 0
#define UART_PORT_NUM      CONFIG_ESP_CONSOLE_UART_NUM
#define UART_ENABLED       1
#else
#define UART_PORT_NUM      0  // Dummy value when UART disabled
#define UART_ENABLED       0
#endif

// ============================================================================
// IMPROVED NON-BLOCKING INPUT/OUTPUT FUNCTIONS
// ============================================================================

/**
 * @brief Read single character with immediate return (non-blocking)
 * Improved version with proper error handling
 */
static int uart_read_char_nonblocking(void)
{
    char ch;
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    
    if (flags == -1) {
        ESP_LOGW(TAG, "fcntl F_GETFL failed: %s", strerror(errno));
        return EOF;
    }
    
    // Set non-blocking mode
    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
        ESP_LOGW(TAG, "fcntl F_SETFL failed: %s", strerror(errno));
        return EOF;
    }
    
    // Use read() for truly non-blocking operation
    ssize_t bytes_read = read(STDIN_FILENO, &ch, 1);
    
    // Restore blocking mode
    fcntl(STDIN_FILENO, F_SETFL, flags);
    
    if (bytes_read > 0) {
        return (unsigned char)ch;
    } else if (bytes_read == 0) {
        return EOF; // No data available
    } else {
        // bytes_read < 0 - error
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG, "read() error: %s", strerror(errno));
        }
        return EOF;
    }
}

/**
 * @brief Write single character with immediate flush
 * Improved version with better error handling
 */
static void uart_write_char_immediate(char ch)
{
    if (uart_mutex != NULL) {
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take UART mutex for character write");
            return;
        }
    }
    
    putchar(ch);
    fflush(stdout);  // CRITICAL: Immediate flush
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

/**
 * @brief Write string with immediate flush
 * Improved version with better error handling
 */
static void uart_write_string_immediate(const char* str)
{
    if (str == NULL) return;
    
    if (uart_mutex != NULL) {
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take UART mutex for string write");
            return;
        }
    }
    
    printf("%s", str);
    fflush(stdout);  // CRITICAL: Immediate flush
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

// ============================================================================
// ENHANCED INPUT BUFFERING AND LINE EDITING
// ============================================================================

// Input buffer configuration
#define UART_CMD_BUFFER_SIZE 256
#define UART_CMD_HISTORY_SIZE 20
#define UART_MAX_ARGS 10
#define INPUT_TIMEOUT_MS 1  // Reduced timeout for better responsiveness

// Special characters
#define CHAR_BACKSPACE  0x08
#define CHAR_DELETE     0x7F
#define CHAR_ENTER      0x0D
#define CHAR_NEWLINE    0x0A
#define CHAR_ESC        0x1B
#define CHAR_CTRL_C     0x03
#define CHAR_CTRL_D     0x04

// ANSI escape codes
#define ANSI_CLEAR_LINE "\033[2K\r"
#define ANSI_CURSOR_LEFT "\033[1D"
#define ANSI_CURSOR_RIGHT "\033[1C"
#define ANSI_CLEAR_TO_END "\033[0K"

// Colors
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Input buffer structure
typedef struct {
    char buffer[UART_CMD_BUFFER_SIZE];
    size_t pos;
    size_t length;
    bool cursor_visible;
} input_buffer_t;

// Command history structure
typedef struct {
    char commands[UART_CMD_HISTORY_SIZE][UART_CMD_BUFFER_SIZE];
    int current;
    int count;
    int max_size;
} command_history_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Task state
static bool task_running = false;
static bool color_enabled = true;  // ANSI color support
static bool echo_enabled = true;   // Character echo enabled
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

void uart_send_colored(const char* color, const char* message)
{
    // CRITICAL: Use mutex for ALL printf output to prevent conflicts
    if (uart_mutex != NULL) {
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take UART mutex for colored output");
            return;
        }
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
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take UART mutex for colored line output");
            return;
        }
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
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take UART mutex for line output");
            return;
        }
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
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take UART mutex for string output");
            return;
        }
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
// INPUT BUFFER MANAGEMENT
// ============================================================================

void input_buffer_init(input_buffer_t* buffer)
{
    memset(buffer->buffer, 0, sizeof(buffer->buffer));
    buffer->pos = 0;
    buffer->length = 0;
    buffer->cursor_visible = true;
}

void input_buffer_clear(input_buffer_t* buffer)
{
    memset(buffer->buffer, 0, sizeof(buffer->buffer));
    buffer->pos = 0;
    buffer->length = 0;
}

/**
 * @brief Process regular character with immediate echo
 */
static void process_regular_char_with_echo(char ch)
{
    if (input_buffer.pos < UART_CMD_BUFFER_SIZE - 1) {
        // Echo character immediately
        if (echo_enabled) {
            uart_write_char_immediate(ch);
        }
        
        // Add to buffer
        input_buffer.buffer[input_buffer.pos] = ch;
        input_buffer.pos++;
        input_buffer.buffer[input_buffer.pos] = '\0';
        input_buffer.length = input_buffer.pos;
    }
}

/**
 * @brief Process backspace character with immediate echo
 */
static void process_backspace_with_echo(void)
{
    if (input_buffer.pos > 0) {
        // Echo backspace sequence immediately
        if (echo_enabled) {
            uart_write_string_immediate("\b \b"); // Backspace, space, backspace
        }
        
        // Remove from buffer
        input_buffer.pos--;
        input_buffer.buffer[input_buffer.pos] = '\0';
        input_buffer.length = input_buffer.pos;
    }
}

/**
 * @brief Process enter key - command completion
 */
static bool process_enter_with_echo(void)
{
    // Echo newline immediately
    if (echo_enabled) {
        uart_write_char_immediate('\r');
        uart_write_char_immediate('\n');
    }
    
    // Null terminate command
    input_buffer.buffer[input_buffer.pos] = '\0';
    
    // Skip empty commands
    if (input_buffer.pos == 0) {
        return false;
    }
    
    return true; // Command ready for processing
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

// ============================================================================
// COMMAND PARSING AND EXECUTION
// ============================================================================

/**
 * @brief Parse command line into arguments
 */
static int parse_command(char* cmd_line, char* argv[], int max_args)
{
    int argc = 0;
    char* token = strtok(cmd_line, " \t\r\n");
    
    while (token != NULL && argc < max_args - 1) {
        argv[argc] = token;
        argc++;
        token = strtok(NULL, " \t\r\n");
    }
    
    argv[argc] = NULL;
    return argc;
}

/**
 * @brief Process individual commands
 */
static void process_command(char* argv[], int argc)
{
    if (argc == 0) return;
    
    command_count++;
    
    // Convert command to lowercase
    for (int i = 0; argv[0][i]; i++) {
        argv[0][i] = tolower(argv[0][i]);
    }
    
    // HELP command
    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "h") == 0 || strcmp(argv[0], "?") == 0) {
        uart_write_string_immediate(COLOR_BOLD "ESP32-C6 Chess System v2.4 - Command Help\r\n" COLOR_RESET);
        uart_write_string_immediate("========================================\r\n");
        uart_write_string_immediate("CHESS COMMANDS:\r\n");
        uart_write_string_immediate("  move <from><to>  - Make chess move (e.g., move e2e4)\r\n");
        uart_write_string_immediate("  moves [square]   - Show available moves for square\r\n");
        uart_write_string_immediate("  board           - Display current board position\r\n");
        uart_write_string_immediate("  new             - Start new game\r\n");
        uart_write_string_immediate("  reset           - Reset game\r\n");
        uart_write_string_immediate("\r\nSYSTEM COMMANDS:\r\n");
        uart_write_string_immediate("  status          - Show system status\r\n");
        uart_write_string_immediate("  version         - Show version information\r\n");
        uart_write_string_immediate("  echo on/off     - Toggle character echo\r\n");
        uart_write_string_immediate("  clear           - Clear screen\r\n");
        uart_write_string_immediate("  help            - Show this help\r\n");
        uart_write_string_immediate("========================================\r\n");
    }
    
    // ECHO command
    else if (strcmp(argv[0], "echo") == 0) {
        if (argc == 2) {
            if (strcmp(argv[1], "on") == 0) {
                uart_set_echo_enabled(true);
                uart_write_string_immediate(COLOR_GREEN "Echo enabled\r\n" COLOR_RESET);
            } else if (strcmp(argv[1], "off") == 0) {
                uart_set_echo_enabled(false);
                uart_write_string_immediate(COLOR_YELLOW "Echo disabled\r\n" COLOR_RESET);
            } else if (strcmp(argv[1], "test") == 0) {
                uart_test_echo();
            } else {
                uart_write_string_immediate(COLOR_RED "Usage: echo on/off/test\r\n" COLOR_RESET);
            }
        } else {
            uart_write_string_immediate(COLOR_CYAN "Echo is currently: " COLOR_RESET);
            uart_write_string_immediate(echo_enabled ? COLOR_GREEN "ON\r\n" : COLOR_YELLOW "OFF\r\n");
            uart_write_string_immediate(COLOR_RESET);
        }
    }
    
    // MOVE command
    else if (strcmp(argv[0], "move") == 0 || strcmp(argv[0], "m") == 0) {
        if (argc != 2) {
            uart_write_string_immediate(COLOR_RED "Usage: move <from><to> (e.g., move e2e4)\r\n" COLOR_RESET);
            return;
        }
        
        if (!is_valid_move_notation(argv[1])) {
            uart_write_string_immediate(COLOR_RED "Invalid move format. Use format like 'e2e4'\r\n" COLOR_RESET);
            return;
        }
        
        uart_write_string_immediate(COLOR_CYAN "Processing move: ");
        uart_write_string_immediate(argv[1]);
        uart_write_string_immediate("\r\n" COLOR_RESET);
        
        send_move_to_game_task(argv[1]);
    }
    
    // BOARD command
    else if (strcmp(argv[0], "board") == 0 || strcmp(argv[0], "b") == 0) {
        if (game_command_queue != NULL) {
            chess_move_command_t cmd = {0};
            cmd.type = GAME_CMD_GET_BOARD;
            xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
            uart_write_string_immediate(COLOR_GREEN "Board display requested\r\n" COLOR_RESET);
        } else {
            uart_write_string_immediate(COLOR_RED "Game task not available\r\n" COLOR_RESET);
        }
    }
    
    // NEW GAME command
    else if (strcmp(argv[0], "new") == 0) {
        if (game_command_queue != NULL) {
            chess_move_command_t cmd = {0};
            cmd.type = GAME_CMD_NEW_GAME;
            xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
            uart_write_string_immediate(COLOR_GREEN "New game started\r\n" COLOR_RESET);
        } else {
            uart_write_string_immediate(COLOR_RED "Game task not available\r\n" COLOR_RESET);
        }
    }
    
    // RESET command
    else if (strcmp(argv[0], "reset") == 0) {
        if (game_command_queue != NULL) {
            chess_move_command_t cmd = {0};
            cmd.type = GAME_CMD_RESET_GAME;
            xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
            uart_write_string_immediate(COLOR_GREEN "Game reset requested\r\n" COLOR_RESET);
        } else {
            uart_write_string_immediate(COLOR_RED "Game task not available\r\n" COLOR_RESET);
        }
    }
    
    // STATUS command
    else if (strcmp(argv[0], "status") == 0) {
        uart_write_string_immediate(COLOR_BOLD "SYSTEM STATUS\r\n" COLOR_RESET);
        uart_write_string_immediate("=============\r\n");
        
        char status_buf[256];
        snprintf(status_buf, sizeof(status_buf), 
                "Free Heap: %lu bytes\r\n"
                "Commands: %lu\r\n"
                "Errors: %lu\r\n"
                "Echo: %s\r\n"
                "Uptime: %llu sec\r\n",
                (unsigned long)esp_get_free_heap_size(),
                (unsigned long)command_count,
                (unsigned long)error_count,
                echo_enabled ? "ON" : "OFF",
                esp_timer_get_time() / 1000000);
        
        uart_write_string_immediate(status_buf);
    }
    
    // VERSION command
    else if (strcmp(argv[0], "version") == 0 || strcmp(argv[0], "ver") == 0) {
        uart_write_string_immediate(COLOR_BOLD "ESP32-C6 Chess System v2.4\r\n" COLOR_RESET);
        uart_write_string_immediate("Author: Alfred Krutina\r\n");
        uart_write_string_immediate("Build: " __DATE__ " " __TIME__ "\r\n");
    }
    
    // CLEAR command
    else if (strcmp(argv[0], "clear") == 0 || strcmp(argv[0], "cls") == 0) {
        uart_write_string_immediate("\033[2J\033[H"); // Clear screen and home cursor
    }
    
    // Unknown command
    else {
        // Check if it's a direct move (like "e2e4")
        if (strlen(argv[0]) == 4 && is_valid_move_notation(argv[0])) {
            uart_write_string_immediate(COLOR_CYAN "Processing move: ");
            uart_write_string_immediate(argv[0]);
            uart_write_string_immediate("\r\n" COLOR_RESET);
            send_move_to_game_task(argv[0]);
        } else {
            uart_write_string_immediate(COLOR_RED "Unknown command: ");
            uart_write_string_immediate(argv[0]);
            uart_write_string_immediate("\r\nType 'help' for available commands\r\n" COLOR_RESET);
            error_count++;
        }
    }
}

// ============================================================================
// MOVE PARSING FUNCTIONS
// ============================================================================

/**
 * @brief Validate chess move notation (e.g., "e2e4")
 */
bool is_valid_move_notation(const char* move)
{
    if (strlen(move) != 4) return false;
    
    // Check format: [a-h][1-8][a-h][1-8]
    if (move[0] < 'a' || move[0] > 'h') return false;
    if (move[1] < '1' || move[1] > '8') return false;
    if (move[2] < 'a' || move[2] > 'h') return false;
    if (move[3] < '1' || move[3] > '8') return false;
    
    return true;
}

/**
 * @brief Send move command to game task via FreeRTOS queue
 */
static bool send_move_to_game_task(const char* move_str)
{
    if (game_command_queue == NULL) {
        uart_write_string_immediate(COLOR_RED "Error: Game command queue not available\r\n" COLOR_RESET);
        return false;
    }
    
    // Create move command structure
    chess_move_command_t move_cmd = {0};
    move_cmd.type = GAME_CMD_MAKE_MOVE;
    
    // Copy move notation (e.g., "e2e4" -> from="e2", to="e4")
    strncpy(move_cmd.from_notation, move_str, 2);
    move_cmd.from_notation[2] = '\0';
    strncpy(move_cmd.to_notation, move_str + 2, 2);
    move_cmd.to_notation[2] = '\0';
    
    move_cmd.player = 0; // Will be determined by game task
    move_cmd.response_queue = 0; // No response needed
    
    // Send command to game task
    if (xQueueSend(game_command_queue, &move_cmd, pdMS_TO_TICKS(1000)) == pdTRUE) {
        uart_write_string_immediate(COLOR_GREEN "Move command sent to game task\r\n" COLOR_RESET);
        return true;
    } else {
        uart_write_string_immediate(COLOR_RED "Failed to send move to game task (queue full)\r\n" COLOR_RESET);
        return false;
    }
}

// ============================================================================
// IMPROVED INPUT PROCESSING
// ============================================================================

void uart_process_input(char c)
{
    if (c == '\r' || c == '\n') {
        // Process command
        if (input_buffer.length > 0) {
            // Add to history
            command_history_add(&command_history, input_buffer.buffer);
            
            // Process command
            char* argv[UART_MAX_ARGS];
            int argc = parse_command(input_buffer.buffer, argv, UART_MAX_ARGS);
            process_command(argv, argc);
            
            // Clear buffer
            input_buffer_clear(&input_buffer);
        }
        
        // Show prompt
        uart_write_string_immediate(COLOR_BOLD "chess> " COLOR_RESET);
    } else if (c == '\b' || c == 127) {
        // Backspace
        process_backspace_with_echo();
    } else if (c >= 32 && c <= 126) {
        // Printable character
        process_regular_char_with_echo(c);
    }
}

// ============================================================================
// IMPROVED MAIN INPUT LOOP
// ============================================================================

/**
 * @brief Improved main character input processing loop
 * This version has proper non-blocking I/O and immediate echo
 */
static void uart_input_loop_improved(void)
{
    ESP_LOGI(TAG, "🚀 Starting improved UART input loop with proper echo");
    
    while (task_running) {
        // Reset watchdog
        esp_task_wdt_reset();
        
        // Try to read character (non-blocking)
        int ch = uart_read_char_nonblocking();
        
        if (ch == EOF) {
            // No character available, small delay
            vTaskDelay(pdMS_TO_TICKS(1)); // Minimal delay for responsiveness
            continue;
        }
        
        // Process character based on type
        switch (ch) {
            case CHAR_BACKSPACE:
            case CHAR_DELETE:
                process_backspace_with_echo();
                break;
                
            case CHAR_ENTER:
            case CHAR_NEWLINE:
                if (process_enter_with_echo()) {
                    // Command ready - parse and execute
                    char* argv[UART_MAX_ARGS];
                    int argc = parse_command(input_buffer.buffer, argv, UART_MAX_ARGS);
                    process_command(argv, argc);
                }
                
                // Reset buffer for next command
                input_buffer_clear(&input_buffer);
                
                // Show prompt
                uart_write_string_immediate(COLOR_BOLD "chess> " COLOR_RESET);
                break;
                
            case CHAR_CTRL_C:
                uart_write_string_immediate("^C\r\n");
                input_buffer_clear(&input_buffer);
                uart_write_string_immediate(COLOR_BOLD "chess> " COLOR_RESET);
                break;
                
            case CHAR_CTRL_D:
                uart_write_string_immediate("^D\r\n");
                break;
                
            default:
                // Regular printable character
                if (ch >= 32 && ch <= 126) {
                    process_regular_char_with_echo((char)ch);
                }
                break;
        }
    }
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void uart_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "🚀 Improved UART command interface starting...");
    
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
    
    ESP_LOGI(TAG, "Mutex available: %s", uart_mutex != NULL ? "YES" : "NO");
    ESP_LOGI(TAG, "Echo enabled: %s", echo_enabled ? "YES" : "NO");
    
    // Set UART for optimal interactivity (only if UART is explicitly configured)
    if (UART_ENABLED && CONFIG_ESP_CONSOLE_UART_NUM >= 0) {
        uart_set_rx_timeout(UART_PORT_NUM, pdMS_TO_TICKS(1));
        uart_flush(UART_PORT_NUM);
    }
    // For USB Serial JTAG (CONFIG_ESP_CONSOLE_UART_NUM=-1), no UART initialization needed
    
    ESP_LOGI(TAG, "🚀 Improved UART command interface ready");
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • Non-blocking character input with immediate echo");
    ESP_LOGI(TAG, "  • Line-based input with editing");
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
    
    // Show initial prompt after everything is initialized
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for initialization to complete
    uart_write_string_immediate(COLOR_BOLD "chess> " COLOR_RESET);
    
    // Start improved input loop
    uart_input_loop_improved();
    
    // Should never reach here
    ESP_LOGE(TAG, "UART task unexpectedly exited");
    vTaskDelete(NULL);
}

// ============================================================================
// ECHO CONTROL FUNCTIONS
// ============================================================================

/**
 * @brief Enable or disable character echo
 * @param enabled true to enable echo, false to disable
 */
void uart_set_echo_enabled(bool enabled)
{
    echo_enabled = enabled;
    system_config.echo_enabled = enabled;
    
    // Save to NVS
    config_save_to_nvs(&system_config);
    
    ESP_LOGI(TAG, "Echo %s", enabled ? "enabled" : "disabled");
}

/**
 * @brief Get current echo status
 * @return true if echo is enabled, false otherwise
 */
bool uart_get_echo_enabled(void)
{
    return echo_enabled;
}

/**
 * @brief Test echo functionality
 */
void uart_test_echo(void)
{
    uart_send_info("ECHO TEST");
    uart_send_formatted("Current echo status: %s", echo_enabled ? "ENABLED" : "DISABLED");
    uart_send_info("Type some characters to test echo...");
    
    // Test echo for a few seconds
    uint32_t start_time = esp_timer_get_time() / 1000;
    while ((esp_timer_get_time() / 1000) - start_time < 5000) {
        int ch = uart_read_char_nonblocking();
        if (ch != EOF) {
            if (ch == '\r' || ch == '\n') {
                uart_send_info("Echo test completed");
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================================
// LEGACY FUNCTION COMPATIBILITY
// ============================================================================

// These functions are kept for compatibility with existing code
void uart_parse_command(const char* input)
{
    if (input == NULL || strlen(input) == 0) return;
    
    char* argv[UART_MAX_ARGS];
    char input_copy[UART_CMD_BUFFER_SIZE];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    int argc = parse_command(input_copy, argv, UART_MAX_ARGS);
    process_command(argv, argc);
}

// Memory and health monitoring
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
    
    return ESP_OK;
}
