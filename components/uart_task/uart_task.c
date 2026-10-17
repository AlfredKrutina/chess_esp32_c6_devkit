/**
 * @file uart_task.c
 * @brief UART Terminal Task - interactive command line for chess control
 *
 * @details
 * =============================================================================
 * WHAT THIS FILE DOES
 * =============================================================================
 *
 * This task is the chessboard "terminal". It enables control over UART:
 * 1. Command line with editing (backspace, command history)
 * 2. 50+ control commands (move, board, status, wifi, timer...)
 * 3. Colored output (ANSI colors) for clearer readability
 * 4. Debug dumps of all tasks (tasks, heap, queues...)
 * 5. WiFi configuration and management
 * 6. Test commands for LED, matrix, animations
 *
 * =============================================================================
 * HOW IT WORKS
 * =============================================================================
 *
 * STARTUP:
 * - Initialize UART (115200 baud)
 * - Create input buffer (256 characters)
 * - Show welcome logo
 * - Command prompt: "chess> _"
 *
 * MAIN LOOP:
 * while (1) {
 *     1. Read character from UART (blocking)
 *     2. Process input:
 *        - Normal character -> append to buffer
 *        - ENTER -> parse and execute command
 *        - BACKSPACE -> delete character
 *        - UP ARROW -> command history
 *     3. Execute command from the command table
 *     4. Print result
 * }
 *
 * =============================================================================
 * COMMUNICATION (FIFOS & MUTEXES)
 * =============================================================================
 *
 * QUEUES - Sending commands to other tasks:
 * - game_command_queue -> Commands for game_task (move, reset...)
 * - web_server_queue -> Commands for web server (wifi config...)
 * - LEDs are controlled by direct calls (queue was removed)
 *
 * MUTEXES - Protecting shared resources:
 * - uart_mutex -> Protects UART output (prevent overlapping prints)
 *   IMPORTANT: Always take it for printf/uart_write!
 *
 * ACCESS:
 * @code
 * xSemaphoreTake(uart_mutex, portMAX_DELAY);  // Lock
 * printf("My output\\n");                      // Write
 * xSemaphoreGive(uart_mutex);                  // Unlock
 * @endcode
 *
 * =============================================================================
 * COMMAND TABLE
 * =============================================================================
 *
 * Commands are organized in a table with function pointers:
 * - cmd_name: Command name (e.g. "move")
 * - cmd_handler: Function pointer (e.g. uart_cmd_move)
 * - cmd_help: Short help text
 * - cmd_priority: Priority (HIGH/MEDIUM/LOW)
 *
 * Command categories:
 * - Chessboard: move, board, status, reset, undo
 * - System: help, tasks, heap, version
 * - WiFi: wifi_scan, wifi_connect, wifi_status
 * - Timer: timer_start, timer_stop, timer_status
 * - LED: led_test, led_blink, anim_*
 * - Debug: squares_debug, queue_stats
 *
 * =============================================================================
 * TABLE OF CONTENTS
 * =============================================================================
 *
 * Section 1:  WDT Wrapper Functions ................... line 70
 * Section 2:  UART Driver Functions ................... line 160
 * Section 3:  Input Buffer & History .................. line 250
 * Section 4:  Command Table Definition ................ line 760
 * Section 5:  Command Handlers (50+ functions) ........ line 1800
 * Section 6:  Formatting & Output Functions ........... line 350
 * Section 7:  Main UART Task .......................... line 6200
 *
 * TIP: Ctrl+G to jump to a line
 *
 * =============================================================================
 * DEPENDENCIES
 * =============================================================================
 *
 * - game_task: We send commands (move, reset...)
 * - led_task: LED tests, animations
 * - web_server_task: WiFi configuration
 * - timer_system: Start/stop clock
 * - config_manager: Save/load configuration (NVS)
 *
 * =============================================================================
 * CRITICAL RULES
 * =============================================================================
 *
 * @warning DO NOT:
 *
 * 1. NEVER print without uart_mutex!
 *    ❌ printf("text");  // RACE - can overlap with another task
 *    ✅ xSemaphoreTake(uart_mutex, ...); printf("text"); xSemaphoreGive(...);
 *
 * 2. NEVER call blocking operations with a long timeout!
 *    ❌ xQueueSend(queue, &data, portMAX_DELAY);  // May block WDT
 *    ✅ xQueueSend(queue, &data, pdMS_TO_TICKS(1000));  // Max 1s
 *
 * 3. NEVER access other tasks' data directly!
 *    ❌ game_board[0][0] = PIECE_KING;  // Direct access - UNSAFE
 *    ✅ Use queues (game_command_queue)
 *
 * 4. ALWAYS reset WDT in long loops!
 *    ✅ uart_task_wdt_reset_safe();  // Every few seconds
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-12-22
 *
 * @note
 * - Task priority: 3 (lower than game_task)
 * - Stack size: 8KB
 * - Uses WDT (watchdog timer)
 *
 * @see game_task.c - Chess logic
 * @see led_task.c - LED control
 * @see web_server_task.c - Web interface
 */

#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "hall_i2c_squares.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "driver/uart.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
// ESP-IDF UART driver functions
// No POSIX includes needed

#include "../ble_task/include/ble_task.h"
#include "../freertos_chess/include/chess_types.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../squares_task/include/squares_task.h"
#include "../timer_system/include/timer_system.h"
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "../web_server_task/include/web_server_task.h"
#include "../web_server_task/include/board_api_auth.h"
#include "config_manager.h"
#include "esp_system.h"
#include "freertos_chess.h"
#include "task_wdt.h"
#include "game_task.h"
#include "led_mapping.h"
#include "led_task.h"
#include "uart_commands_table.h"
#include "uart_cli_panel.h"
#include "uart_parse.h"
#include "uart_task_internal.h"
#include <inttypes.h>
#include <math.h>

// External function declarations
extern bool convert_notation_to_coords(const char *notation, uint8_t *row,
                                       uint8_t *col);

// External UART mutex for clean output
extern SemaphoreHandle_t uart_mutex;

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset with WARNING instead of ERROR logging for
 * ESP_ERR_NOT_FOUND
 *
 * This function safely resets the Task Watchdog Timer. If the task is not yet
 * registered (which is normal during startup), WARNING is logged instead of ERROR.
 *
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if the task is not registered
 * (WARNING only)
 *
 * @details
 * The function is used to safely reset the watchdog timer during UART operation.
 * Prevents startup errors when the task is not yet registered.
 */
static bool s_uart_wdt_subscribed;

void uart_task_wdt_set_subscribed(bool subscribed) {
  s_uart_wdt_subscribed = subscribed;
}

esp_err_t uart_task_wdt_reset_safe(void) {
  if (!s_uart_wdt_subscribed) {
    return ESP_ERR_NOT_FOUND;
  }
  return esp_task_wdt_reset();
}

// External task handles and queues
extern TaskHandle_t led_task_handle;
extern TaskHandle_t squares_task_handle;
extern TaskHandle_t button_task_handle;
extern TaskHandle_t game_task_handle;
extern QueueHandle_t game_command_queue;
// extern QueueHandle_t led_command_queue;  //  REMOVED: Queue hell eliminated

// Component status tracking
static bool squares_component_enabled = true;
static bool led_component_enabled = true;
static bool wifi_component_enabled = false;

// UART response queue for game task responses (declared in freertos_chess.h)

// Forward declarations for functions used before definition
static void uart_task_legacy_loop(void);
static void uart_cli_arm_stdin(void);
static void uart_cli_show_prompt(void);

// Missing type definitions that should be here
// UART command structure is defined in uart_task.h

static const char *TAG = "UART_TASK";

// ============================================================================
// CHUNKED OUTPUT MACROS
// ============================================================================

// Universal chunked printf macro by design
#define CHUNKED_PRINTF(format, ...)                                            \
  do {                                                                         \
    printf(format, ##__VA_ARGS__);                                             \
    fflush(stdout);                                                            \
    SAFE_WDT_RESET();                                                          \
    vTaskDelay(pdMS_TO_TICKS(1));                                              \
  } while (0)

// Optimized constants for ESP32-C6
#define CHUNK_DELAY_MS 2                     // Minimal delay
#define MAX_CHUNK_SIZE UART_MESSAGE_TEXT_MAX /* uart_queue_message.h */
#define STACK_SAFETY_LIMIT 512               // Minimum free stack

// UART configuration - only use if UART is enabled
#if CONFIG_ESP_CONSOLE_UART_NUM >= 0
#define UART_PORT_NUM CONFIG_ESP_CONSOLE_UART_NUM
#define UART_ENABLED 1
#else
#define UART_PORT_NUM 0 // Dummy value when UART disabled
#define UART_ENABLED 0
#endif
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024
// UART_QUEUE_SIZE is now defined in freertos_chess.h

// ============================================================================
// ESP-IDF UART DRIVER FUNCTIONS
// ============================================================================

// Forward declarations
static void uart_fputs(const char *str);
void uart_send_line(const char *str);

/**
 * @brief Replace fputs with ESP-IDF UART driver
 *
 * This function sends a string via UART. It uses the ESP-IDF UART driver
 * or USB Serial JTAG depending on configuration.
 *
 * @param str String to send via UART
 *
 * @details
 * The function automatically detects whether the UART is enabled and uses it
 * the corresponding method for sending data.
 */
static void uart_fputs(const char *str) {
  if (UART_ENABLED) {
    uart_write_bytes(UART_PORT_NUM, str, strlen(str));
  } else {
    // For USB Serial JTAG, use printf
    printf("%s", str);
  }
}

/**
 * @brief Writes one character with immediate flush
 *
 * This function writes one character to the UART with an immediate flush.
 * Uses ESP-IDF UART driver or USB Serial JTAG depending on configuration.
 *
 * @param ch Character to write
 *
 * @details
 * The function uses a mutex for safe operations from multiple fibers and automatically
 * detects if UART is enabled.
 */
void uart_write_char_immediate(char ch) {
  if (uart_mutex != NULL) {
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
  }

  if (UART_ENABLED) {
    uart_write_bytes(UART_PORT_NUM, &ch, 1);
  } else {
    // For USB Serial JTAG, use putchar
    putchar(ch);
  }

  if (uart_mutex != NULL) {
    xSemaphoreGive(uart_mutex);
  }
}

static void uart_cli_arm_stdin(void) {
#if !UART_ENABLED
  setvbuf(stdin, NULL, _IONBF, 0);
  int fd = fileno(stdin);
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    ESP_LOGW(TAG, "stdin non-blocking setup failed");
  }
#endif
}

static void uart_cli_show_prompt(void) {
  if (UART_ENABLED) {
    if (uart_mutex != NULL) {
      if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uart_write_bytes(UART_PORT_NUM, "chess> ", 7);
        xSemaphoreGive(uart_mutex);
      }
    } else {
      uart_write_bytes(UART_PORT_NUM, "chess> ", 7);
    }
  } else {
    printf("chess> ");
    fflush(stdout);
  }
}

/**
 * @brief Writes a string with immediate flush.
 */
void uart_write_string_immediate(const char *str) {
  if (uart_mutex != NULL) {
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
  }

  if (UART_ENABLED) {
    uart_write_bytes(UART_PORT_NUM, str, strlen(str));
  } else {
    // For USB Serial JTAG, use printf
    printf("%s", str);
  }

  if (uart_mutex != NULL) {
    xSemaphoreGive(uart_mutex);
  }
}

// ============================================================================
// ENHANCED INPUT BUFFERING AND LINE EDITING
// ============================================================================

/* Memory: previously 20×256 = 5120 B + input; now 8×192 + 192 ≈ 1728 B (~3.4 KiB
 * BSS savings) */
#define UART_CMD_BUFFER_SIZE 192
#define UART_CMD_HISTORY_SIZE 8
#define UART_MAX_ARGS 10
#define INPUT_TIMEOUT_MS 100

// Special characters
#define CHAR_BACKSPACE 0x08
#define CHAR_DELETE 0x7F
#define CHAR_ENTER 0x0D
#define CHAR_NEWLINE 0x0A
#define CHAR_ESC 0x1B
#define CHAR_CTRL_C 0x03
#define CHAR_CTRL_D 0x04

// ANSI escape codes
#define ANSI_CLEAR_LINE "\033[2K\r"
#define ANSI_CURSOR_LEFT "\033[1D"
#define ANSI_CURSOR_RIGHT "\033[1C"
#define ANSI_CLEAR_TO_END "\033[0K"

// Colors
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD "\033[1m"

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
// COMMAND TABLE STRUCTURE
// ============================================================================

// Note: All types are now defined in header files to avoid conflicts

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Task state
static bool task_running = false;
bool color_enabled = true; // ANSI color support
static input_buffer_t input_buffer;
static command_history_t command_history;
static system_config_t system_config;

// Arrow key navigation state
typedef enum {
  ESC_STATE_NONE,
  ESC_STATE_ESC,    // ESC character accepted
  ESC_STATE_BRACKET // ESC[ accepted, waiting for final character
} esc_state_t;

static esc_state_t esc_state = ESC_STATE_NONE;
static int history_navigation_index = -1; // -1 = nejsme v navigaci

// UART message queue for centralized output
QueueHandle_t uart_output_queue = NULL;

// Statistics
uint32_t command_count = 0;
uint32_t error_count = 0;
uint32_t last_command_time = 0;

// ============================================================================
// ANSI COLOR CODES AND FORMATTING
// ============================================================================

// ANSI Color codes for terminal output (see uart_task_internal.h)

// ============================================================================
// FORMATTING FUNCTIONS
// ============================================================================

/**
 * @brief Display an impressive welcome logo with ANSI colors
 */
/**
 * @brief Send welcome logo via UART
 *
 * This function follows the nice ASCII logo of the system via UART.
 * The logo contains the name of the system and is colored.
 *
 * @details
 * The function uses a mutex for thread-safe operations and reinforcement
 * ASCII art logo with color formatting for a better look.
 */
void uart_send_welcome_logo(void) {
  if (uart_mutex != NULL) {
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
  }

  // Don't clear screen - just show logo below current content
  uart_write_string_immediate("\n");

  // New ASCII art logo - using ESP-IDF UART driver
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs("\x1b[0m.........................................................."
             "..\x1b[34m:=*+-\x1b[0m..........................................."
             "....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................................................\x1b["
             "34m:=#%@@%*=-=+#@@@%*=:\x1b[0m..................................."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m..............................................\x1b[34m-=*%"
             "@@%*=-=*%@%@=*@%@%*=-+#%@@%*=-\x1b[0m............................"
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m......................................\x1b[34m:-+#@@@%+--+"
             "#%@%+@+#@@%@%%@%@@-*@=@@%#=-=*%@@@#+-:\x1b[0m...................."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m...............................\x1b[34m:-+%@@@#+--*%@@*@=*"
             "@*@@@#=\x1b[0m...........\x1b[34m:+%@@%+@:#@*@@%+--+%@@@%+-:\x1b["
             "0m...............................\x1b[0m\n");
  uart_fputs("\x1b[0m........................\x1b[34m:-*@@@@#-:=#@@*@*+@+@@@%+:"
             "\x1b[0m.........................\x1b[34m-*@@@%+@:@@#@@#-:=#@@@@#-"
             ":\x1b[0m........................\x1b[0m\n");
  uart_fputs("\x1b[0m....................\x1b[34m%@@@@**#@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%@@@@#\x1b["
             "0m....................\x1b[0m\n");
  uart_fputs("\x1b[0m....................\x1b[34m%@############################"
             "#####################################################%@#\x1b[0m.."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[34m:%@=@+#@+@##@=@#%@+@*#@+@#%@="
             "@*#@+@#*@+@#*@+@%*@+@%=@=%@+@**@=@%+@+#@=@%=@+#@+%%=@+:\x1b[0m..."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m......................\x1b[34m#@=========================="
             "====================================================@+\x1b[0m...."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m.......................\x1b[34m##==========@\x1b[0m:::::::"
             "::::::::::::::::::::::::::::::::::::::::::::::\x1b[34m*@========="
             "=@+\x1b[0m........................\x1b[0m\n");
  uart_fputs("\x1b[0m........................\x1b[34m:@*******%@:\x1b[0m.\x1b["
             "34m:%%%%%%%%%%%%%%%%%%%%%--#@@#.+%%%%%%%%%%%%%%%%%%%%*\x1b[0m.."
             "\x1b[34m-@#******%%\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m-@#+%:%.@\x1b[0m...\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%%%=:+@@=\x1b[0m..:::::::::::::::::::\x1b[37m@%\x1b[0m...."
      "\x1b[34m@+#+*%*@:\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m=@#=%:%.@\x1b[0m...\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%#--:*@@@@+-*-\x1b[0m.................\x1b[37m@%\x1b[0m...."
      "\x1b[34m@+#+*%*@-\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m=%#=%:%.@\x1b[0m...\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%#.%@@@@@@@@%:\x1b[0m.................\x1b[37m@%\x1b[0m..."
      "\x1b[34m:%**+*%+@-\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m=%#-%:%.@\x1b[0m...\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%#-@@@@@@@@:\x1b[0m..................\x1b[37m@%\x1b[0m..."
      "\x1b[34m-%**+*#+@-\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m+#%-%:%:@\x1b[0m...\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%#-########-\x1b[0m..................\x1b[37m@%\x1b[0m..."
      "\x1b[34m=%**+*#+@=\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m**%-%:%:@\x1b[0m...\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%:#%%%##%%%*\x1b[0m..................\x1b[37m@%\x1b[0m..."
      "\x1b[34m+#**+*#*%=\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m#+%:%:%-%:\x1b[0m..\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%*::@@@@@%\x1b[0m....................\x1b[37m@%\x1b[0m..."
      "\x1b[34m*##*+***%+\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m#=%:%:#-%:\x1b[0m..\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%%%.%@@@@*\x1b[0m....................\x1b[37m@%\x1b[0m..."
      "\x1b[34m#*#++***#+\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m%:%:%:#=%=\x1b[0m..\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%%#:@@@@@%\x1b[0m....................\x1b[37m@%\x1b[0m..."
      "\x1b[34m%*#++*+***\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m%:%:%:#=#+\x1b[0m..\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%%%-*@@@@@@-\x1b[0m...................\x1b[37m@%\x1b[0m..."
      "\x1b[34m%+%++*+#+#\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m@:%:%:#+#*\x1b[0m..\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%#:=%%%%%%%%:\x1b[0m..................\x1b[37m@%\x1b[0m..."
      "\x1b[34m@+%++*+#=#\x1b[0m.........................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m.........................\x1b[34m@:%:%:#+*#\x1b[0m..\x1b[34m-@@%%"
      "%%%%%%%%%%%%%%-=%@%%%%%%%%-\x1b[0m.................\x1b[37m@%\x1b[0m..."
      "\x1b[34m@=%=+*=#-%\x1b[0m.........................\x1b[0m\n");
  uart_fputs("\x1b[0m.......................\x1b[34m:@*++++++++%#.-@@%%%%%%%%%%"
             "%%%%%.%@@@@@@@@@@@@#\x1b[0m................\x1b[37m@%\x1b[0m.."
             "\x1b[34m@*++++++++%%\x1b[0m........................\x1b[0m\n");
  uart_fputs("\x1b[0m......................\x1b[34m=@=----------*@-@@@@@@@@@@@@"
             "@@@@@:*############=:@@@@@@@@@@@@@@@@%-@=----------=@:\x1b[0m...."
             "...................\x1b[0m\n");
  uart_fputs("\x1b[0m....................\x1b[34m*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@=\x1b[0m.."
             "...................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...................."
             "............................................................\x1b["
             "37m@+\x1b[0m.....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m=@@@@@:+@"
             "@@@@@..@@@@@+..%@@@@@.-@%...+@%..@@#...=@@:...=@@-.=@@@@@@%-@@@@@"
             "-\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m%@+....:.."
             ".:@@:..@@....-@@:...:::@#...=@#..@@@#.*@@@:..:%@@@:...@@:..:@@"
             "\x1b[0m......\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m@@:......."
             "=@%....@@%%%.+@#......:@@%%%%@#.:@*+@@@:%@-..+@.*@#...@@:..:@@#@*"
             "\x1b[0m...\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m+@%:..-*.="
             "@%..:=.@@...*:@@=...+-:@#...=@#.=@=.+@:.#@=.=@#**%@+..@@:..:@@..."
             "=\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m:*%@@#.=%"
             "%%%%%:-%%%%%*..-#@@%+.#%%:..#%#:#%=.....#%*:%%-..*%%=-%%+..=%%%%%"
             "=\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m##---------------------------"
             "-----------------------------------------------------@+\x1b[0m..."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m#%==========================="
             "=====================================================@+\x1b[0m..."
             "..................\x1b[0m\n");
  uart_fputs("\x1b[0m.....................\x1b[37m+############################"
             "######################################################-\x1b[0m..."
             "..................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");
  uart_fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n");

  if (uart_mutex != NULL) {
    xSemaphoreGive(uart_mutex);
  }
}

/**
 * @brief Show an animated progress bar
 *
 * This function will display an animated progress bar with a label and percentages.
 * Progress bar is colorful and smoothly animated.
 *
 * @param label Progress bar label
 * @param max_value Maximum value (100 = 100%)
 * @param duration_ms The duration in milliseconds
 *
 * @details
 * The function creates a nice animated progress bar with color formatting.
 * Uses mutex for thread-safe operations and smoothly animates progress.
 */
void uart_show_progress_bar(const char *label, uint32_t max_value,
                            uint32_t duration_ms) {
  // Locking the mutex for the entire progress bar operation
  if (uart_mutex != NULL) {
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
  }

  const int bar_width = 20;
  uint32_t step_delay = duration_ms / max_value;
  if (step_delay < 5)
    step_delay = 5; // Minimum 5ms delay for smooth animation

  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s: [", label);
  uart_write_string_immediate(buffer);

  for (int i = 0; i < bar_width; i++) {
    uart_write_string_immediate(".");
  }
  uart_write_string_immediate("] 0%");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors

  for (uint32_t i = 0; i <= max_value; i++) {
    int filled = (i * bar_width) / max_value;

    // Reset watchdog before each progress update (only if registered)
    esp_err_t wdt_ret = uart_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    // Move cursor back to start of progress bar
    if (color_enabled)
      uart_write_string_immediate("\033[1;32m"); // bold green
    uart_write_string_immediate("\r");
    snprintf(buffer, sizeof(buffer), "%s: [", label);
    uart_write_string_immediate(buffer);

    // Show filled part (ASCII only)
    for (int j = 0; j < filled; j++) {
      uart_write_string_immediate("#");
    }

    // Show unfilled part
    for (int j = filled; j < bar_width; j++) {
      uart_write_string_immediate(".");
    }

    snprintf(buffer, sizeof(buffer), "] %3u%%",
             (unsigned int)((i * 100) / max_value));
    uart_write_string_immediate(buffer);
    if (color_enabled)
      uart_write_string_immediate("\033[0m"); // reset colors

    if (i < max_value) {
      // Smooth animation with calculated delay
      vTaskDelay(pdMS_TO_TICKS(step_delay));
    }
  }

  uart_write_string_immediate("\n");

  // Release the mutex only after the progress bar is completed
  if (uart_mutex != NULL) {
    xSemaphoreGive(uart_mutex);
  }
}

/**
 * @brief Send colored text via UART
 *
 * This function sends text with color formatting via UART.
 * Uses ANSI escape sequences for colors.
 *
 * @param color ANSI escape sequence for color
 * @param message The text to send
 *
 * @details
 * The function automatically adds a color reset to the end of the text
 * and uses a mutex for thread-safe operations.
 */
void uart_send_colored(const char *color, const char *message) {
  // Use ESP-IDF UART driver with mutex
  if (UART_ENABLED) {
    if (uart_mutex != NULL) {
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      char buffer[512];
      snprintf(buffer, sizeof(buffer), "%s%s%s", color, message, COLOR_RESET);
      uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
      xSemaphoreGive(uart_mutex);
    } else {
      char buffer[512];
      snprintf(buffer, sizeof(buffer), "%s%s%s", color, message, COLOR_RESET);
      uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
    }
  } else {
    // For USB Serial JTAG, use printf
    printf("%s%s%s", color, message, COLOR_RESET);
  }
}

/**
 * @brief Send colored text with a new line via UART
 *
 * This function sends text with color formatting and a new line via UART.
 * Uses ANSI escape sequences for colors.
 *
 * @param color ANSI escape sequence for color
 * @param message The text to send
 *
 * @details
 * The function automatically adds new lines and resets the color to the end of the text
 * and uses a mutex for thread-safe operations.
 */
void uart_send_colored_line(const char *color, const char *message) {
  // Use ESP-IDF UART driver with mutex
  if (UART_ENABLED) {
    if (uart_mutex != NULL) {
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      char buffer[512];
      snprintf(buffer, sizeof(buffer), "%s%s%s\n", color, message, COLOR_RESET);
      uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
      xSemaphoreGive(uart_mutex);
    } else {
      char buffer[512];
      snprintf(buffer, sizeof(buffer), "%s%s%s\n", color, message, COLOR_RESET);
      uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
    }
  } else {
    // For USB Serial JTAG, use printf
    printf("%s%s%s\n", color, message, COLOR_RESET);
  }
}

/**
 * @brief Send an error message via UART
 *
 * This function will send an error message with red color via UART.
 *
 * @param message The error message to send
 */
void uart_send_error(const char *message) {
  uart_send_colored_line(COLOR_ERROR, message);
}

/**
 * @brief Send success message via UART
 *
 * This function will send a successful message with green color via UART.
 *
 * @param message The successful message to send
 */
void uart_send_success(const char *message) {
  uart_send_colored_line(COLOR_SUCCESS, message);
}

/**
 * @brief Send warning message via UART
 *
 * This function will send a yellow warning message via UART.
 *
 * @param message Warning right to send
 */
void uart_send_warning(const char *message) {
  uart_send_colored_line(COLOR_WARNING, message);
}

/**
 * @brief Send information message via UART
 *
 * This function sends an information message with blue color via UART.
 *
 * @param message Informational message to send
 */
void uart_send_info(const char *message) {
  uart_send_colored_line(COLOR_INFO, message);
}

/**
 * @brief Send pull message via UART
 *
 * This function sends a pull message with a special color via UART.
 *
 * @param message The move message to send
 */
void uart_send_move(const char *message) {
  uart_send_colored_line(COLOR_MOVE, message);
}

/**
 * @brief Send status message via UART
 *
 * This function sends a status message with a special color via UART.
 *
 * @param message Status right to send
 */
void uart_send_status(const char *message) {
  uart_send_colored_line(COLOR_STATUS, message);
}

/**
 * @brief Send debug message via UART
 *
 * This function sends a debug message with a special color via UART.
 *
 * @param message Debug right to send
 */
void uart_send_debug(const char *message) {
  uart_send_colored_line(COLOR_DEBUG, message);
}

/**
 * @brief Send help message via UART
 *
 * This function sends a help message with a special color via UART.
 *
 * @param message Help right to send
 */
void uart_send_help(const char *message) {
  uart_send_colored_line(COLOR_HELP, message);
}

/**
 * @brief Send a formatted message via UART
 *
 * This function sends a formatted message with printf-style formatting via UART.
 *
 * @param format Format string for printf
 * @param ... Arguments for the format string
 *
 * @details
 * The function uses vsnprintf for safe formatting and automatically
 * adds a newline to the end of the message.
 */
void uart_send_formatted(const char *format, ...) {
  va_list args;
  va_start(args, format);

  char buffer[512];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  uart_send_line(buffer);
}

/**
 * @brief Send lines of text via UART
 *
 * This function appends lines of text with a new line via UART.
 * Uses mutex for thread-safe operations.
 *
 * @param str String to send
 *
 * @details
 * The function automatically adds new lines to the end of the text
 * and uses short mutex timeouts to prevent WDT problems.
 */
void uart_send_line(const char *str) {
  if (str == NULL)
    return;

  // Use ESP-IDF UART driver with mutex
  if (UART_ENABLED) {
    if (uart_mutex != NULL) {
      // Use shorter timeout to prevent WDT issues
      if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uart_write_bytes(UART_PORT_NUM, str, strlen(str));
        uart_write_bytes(UART_PORT_NUM, "\n", 1);
        xSemaphoreGive(uart_mutex);
      } else {
        // Fallback to printf if mutex timeout
        printf("%s\n", str);
      }
    } else {
      uart_write_bytes(UART_PORT_NUM, str, strlen(str));
      uart_write_bytes(UART_PORT_NUM, "\n", 1);
    }
  } else {
    // For USB Serial JTAG, use printf
    printf("%s\n", str);
  }

  // Log to ESP log system without mutex (separate output channel)
  ESP_LOGI(TAG, "UART Send: %s", str);
}

void uart_send_string(const char *str) {
  if (str == NULL)
    return;

  // Use ESP-IDF UART driver with mutex
  if (UART_ENABLED) {
    if (uart_mutex != NULL) {
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      uart_write_bytes(UART_PORT_NUM, str, strlen(str));
      xSemaphoreGive(uart_mutex);
    } else {
      uart_write_bytes(UART_PORT_NUM, str, strlen(str));
    }
  } else {
    // For USB Serial JTAG, use printf
    printf("%s", str);
  }

  // Log to ESP log system without mutex (separate output channel)
  ESP_LOGI(TAG, "UART Send: %s", str);
}

// ============================================================================
// CENTRALIZED UART OUTPUT SYSTEM
// ============================================================================

/**
 * @brief Send a message to the UART output queue (safe from multiple fibers)
 * @param type Message type (determines color)
 * @param add_newline Whether to add a newline
 * @param format Format string
 * @param ... Format arguments
 */
void uart_queue_message(uart_msg_type_t type, bool add_newline,
                        const char *format, ...) {
  if (uart_output_queue == NULL) {
    // Queue not ready, fall back to direct output
    va_list args;
    va_start(args, format);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (UART_ENABLED) {
      uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
      if (add_newline)
        uart_write_bytes(UART_PORT_NUM, "\n", 1);
    } else {
      printf("%s", buffer);
      if (add_newline)
        printf("\n");
    }
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
    if (UART_ENABLED) {
      uart_write_bytes(UART_PORT_NUM, msg.message, strlen(msg.message));
      if (add_newline)
        uart_write_bytes(UART_PORT_NUM, "\n", 1);
    } else {
      printf("%s", msg.message);
      if (add_newline)
        printf("\n");
    }
  }
}

/**
 * @brief Process UART output messages from queue
 */
static void uart_process_output_queue(void) {
  uart_message_t msg;

  // Process all pending messages
  while (xQueueReceive(uart_output_queue, &msg, 0) == pdTRUE) {
    // Take mutex for entire message output
    if (uart_mutex != NULL) {
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }

    // Apply color based on message type
    const char *color = COLOR_RESET;
    switch (msg.type) {
    case UART_MSG_ERROR:
      color = COLOR_ERROR;
      break;
    case UART_MSG_WARNING:
      color = COLOR_WARNING;
      break;
    case UART_MSG_SUCCESS:
      color = COLOR_SUCCESS;
      break;
    case UART_MSG_INFO:
      color = COLOR_INFO;
      break;
    case UART_MSG_DEBUG:
      color = COLOR_DEBUG;
      break;
    default:
      color = COLOR_RESET;
      break;
    }

    if (UART_ENABLED) {
      if (color_enabled && msg.type != UART_MSG_NORMAL) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%s%s%s", color, msg.message,
                 COLOR_RESET);
        uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
      } else {
        uart_write_bytes(UART_PORT_NUM, msg.message, strlen(msg.message));
      }

      if (msg.add_newline) {
        uart_write_bytes(UART_PORT_NUM, "\n", 1);
      }
    } else {
      // For USB Serial JTAG, use printf
      if (color_enabled && msg.type != UART_MSG_NORMAL) {
        printf("%s%s%s", color, msg.message, COLOR_RESET);
      } else {
        printf("%s", msg.message);
      }

      if (msg.add_newline) {
        printf("\n");
      }
    }

    // No need for fflush with ESP-IDF UART driver

    if (uart_mutex != NULL) {
      xSemaphoreGive(uart_mutex);
    }
  }
}

// ============================================================================
// INPUT BUFFER MANAGEMENT
// ============================================================================

void input_buffer_init(input_buffer_t *buffer) {
  memset(buffer->buffer, 0, sizeof(buffer->buffer));
  buffer->pos = 0;
  buffer->length = 0;
  buffer->cursor_visible = true;
}

void input_buffer_clear(input_buffer_t *buffer) {
  memset(buffer->buffer, 0, sizeof(buffer->buffer));
  buffer->pos = 0;
  buffer->length = 0;
}

void input_buffer_add_char(input_buffer_t *buffer, char c) {
  if (buffer->pos < UART_CMD_BUFFER_SIZE - 1) {
    buffer->buffer[buffer->pos++] = c;
    buffer->buffer[buffer->pos] = '\0';
    buffer->length = buffer->pos;
  }
}

void input_buffer_backspace(input_buffer_t *buffer) {
  if (buffer->pos > 0) {
    // Backspace handled by terminal

    buffer->pos--;
    buffer->buffer[buffer->pos] = '\0';
    buffer->length = buffer->pos;
  }
}

void input_buffer_set_cursor(input_buffer_t *buffer, size_t pos) {
  if (pos <= buffer->length) {
    buffer->pos = pos;

    // Prompt handled by terminal
    {
      char prompt_line[UART_CMD_BUFFER_SIZE + 8];
      snprintf(prompt_line, sizeof(prompt_line), "\r%s", buffer->buffer);
      if (UART_ENABLED) {
        uart_write_bytes(UART_PORT_NUM, prompt_line, strlen(prompt_line));

        // Move cursor to current position
        for (size_t i = buffer->pos; i < buffer->length; i++) {
          uart_write_bytes(UART_PORT_NUM, " ", 1);
        }
        for (size_t i = buffer->pos; i < buffer->length; i++) {
          uart_write_bytes(UART_PORT_NUM, "\b", 1);
        }
      } else {
        printf("%s", prompt_line);

        // Move cursor to current position
        for (size_t i = buffer->pos; i < buffer->length; i++) {
          printf(" ");
        }
        for (size_t i = buffer->pos; i < buffer->length; i++) {
          printf("\b");
        }
      }
    }
  }
}

// ============================================================================
// COMMAND HISTORY MANAGEMENT
// ============================================================================

void command_history_init(command_history_t *history) {
  memset(history->commands, 0, sizeof(history->commands));
  history->current = 0;
  history->count = 0;
  history->max_size = UART_CMD_HISTORY_SIZE;
}

void command_history_add(command_history_t *history, const char *command) {
  if (command == NULL || strlen(command) == 0)
    return;

  // Don't add duplicate commands
  if (history->count > 0) {
    int last_idx =
        (history->current - 1 + history->max_size) % history->max_size;
    if (strcmp(history->commands[last_idx], command) == 0) {
      return;
    }
  }

  // Add command to history
  strncpy(history->commands[history->current], command,
          UART_CMD_BUFFER_SIZE - 1);
  history->commands[history->current][UART_CMD_BUFFER_SIZE - 1] = '\0';

  history->current = (history->current + 1) % history->max_size;
  if (history->count < history->max_size) {
    history->count++;
  }
}

const char *command_history_get_previous(command_history_t *history) {
  if (history->count == 0)
    return NULL;

  int idx = (history->current - 1 + history->max_size) % history->max_size;
  return history->commands[idx];
}

const char *command_history_get_next(command_history_t *history) {
  if (history->count == 0)
    return NULL;

  int idx = (history->current + 1) % history->max_size;
  return history->commands[idx];
}

void command_history_show(command_history_t *history) {
  uart_send_line("Command History:");

  int start_idx = (history->current - history->count + history->max_size) %
                  history->max_size;

  for (int i = 0; i < history->count; i++) {
    int idx = (start_idx + i) % history->max_size;
    uart_send_formatted("  %d: %s", i + 1, history->commands[idx]);
  }
}

// ============================================================================
// ARROW KEY NAVIGATION FUNCTIONS
// ============================================================================

/**
 * @brief Refresh input display in terminal
 */
static void refresh_input_display(void) {
  // Clear the current line
  if (UART_ENABLED) {
    if (uart_mutex != NULL) {
      xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(100));
    }
    uart_write_bytes(UART_PORT_NUM, ANSI_CLEAR_LINE, strlen(ANSI_CLEAR_LINE));
    // Zobrazit prompt
    if (color_enabled) {
      uart_write_bytes(UART_PORT_NUM, "\033[1;33m", 7);
    }
    // Zobrazit input buffer
    uart_write_bytes(UART_PORT_NUM, input_buffer.buffer, input_buffer.length);
    if (color_enabled) {
      uart_write_bytes(UART_PORT_NUM, "\033[0m", 4);
    }
    if (uart_mutex != NULL) {
      xSemaphoreGive(uart_mutex);
    }
  } else {
    printf(ANSI_CLEAR_LINE);
    if (color_enabled) {
      printf("\033[1;33m");
    }
    printf("%s", input_buffer.buffer);
    if (color_enabled) {
      printf("\033[0m");
    }
    fflush(stdout);
  }
}

/**
 * @brief Handle Arrow Up - show previous command from history
 */
static void handle_arrow_up(void) {
  if (command_history.count == 0) {
    return; // No history
  }

  // If we are at the beginning of the navigation, we start from the last command
  if (history_navigation_index < 0) {
    history_navigation_index = command_history.count - 1;
  } else if (history_navigation_index > 0) {
    history_navigation_index--;
  }

  // Get command from history (circular buffer)
  // The most recent command is at position (current - 1) % max_size
  // Older commands are at (current - 2) % max_size, etc.
  int start_idx = (command_history.current - command_history.count +
                   command_history.max_size) %
                  command_history.max_size;
  int idx = (start_idx + history_navigation_index) % command_history.max_size;

  const char *cmd = command_history.commands[idx];
  if (cmd == NULL || strlen(cmd) == 0) {
    return;
  }

  // Display the command in the input buffer
  input_buffer_clear(&input_buffer);
  strncpy(input_buffer.buffer, cmd, UART_CMD_BUFFER_SIZE - 1);
  input_buffer.buffer[UART_CMD_BUFFER_SIZE - 1] = '\0';
  input_buffer.length = strlen(input_buffer.buffer);
  input_buffer.pos = input_buffer.length;

  // Refresh the display in the terminal
  refresh_input_display();
}

/**
 * @brief Handle Arrow Down - show next command from history
 */
static void handle_arrow_down(void) {
  if (command_history.count == 0) {
    return; // No history
  }

  if (history_navigation_index < 0) {
    return; // We're at the end
  }

  history_navigation_index++;

  if (history_navigation_index >= command_history.count) {
    // We are at the end of history - clear the input
    history_navigation_index = -1;
    input_buffer_clear(&input_buffer);
    refresh_input_display();
    return;
  }

  // Retrieve command from history
  int start_idx = (command_history.current - command_history.count +
                   command_history.max_size) %
                  command_history.max_size;
  int idx = (start_idx + history_navigation_index) % command_history.max_size;

  const char *cmd = command_history.commands[idx];
  if (cmd == NULL || strlen(cmd) == 0) {
    return;
  }

  // Display the command in the input buffer
  input_buffer_clear(&input_buffer);
  strncpy(input_buffer.buffer, cmd, UART_CMD_BUFFER_SIZE - 1);
  input_buffer.buffer[UART_CMD_BUFFER_SIZE - 1] = '\0';
  input_buffer.length = strlen(input_buffer.buffer);
  input_buffer.pos = input_buffer.length;

  // Refresh the display in the terminal
  refresh_input_display();
}

// ============================================================================
// GENIUS COMPATIBILITY FUNCTIONS
// ============================================================================

/**
 * @brief Simple compatibility - no complex response system needed
 * All responses are handled directly via printf in game_task
 */

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// Arrow key navigation functions
static void handle_arrow_up(void);
static void handle_arrow_down(void);
static void refresh_input_display(void);

// High Priority Commands
command_result_t uart_cmd_ledtest(const char *args);

// Demo mode commands

// Component Control Commands

// Endgame Commands

// Advantage Graph Functions

// Animation test commands
// Puzzle test command removed

// Endgame Animation Style Commands

// Help functions

// Puzzle Commands
// Puzzle level commands removed

// Endgame animation control

// System Commands

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

command_result_t uart_cmd_help(const char *args) {
  (void)args;
  uart_display_main_help();
  return CMD_SUCCESS;
}

static const char *uart_log_level_name(uint8_t level) {
  switch (level) {
  case ESP_LOG_NONE:
    return "none";
  case ESP_LOG_ERROR:
    return "error";
  case ESP_LOG_WARN:
    return "warn";
  case ESP_LOG_INFO:
    return "info";
  case ESP_LOG_DEBUG:
    return "debug";
  case ESP_LOG_VERBOSE:
    return "verbose";
  default:
    return "?";
  }
}

command_result_t uart_cmd_log(const char *args) {
  if (args == NULL || args[0] == '\0') {
    uart_send_formatted("Log level: %s (%u) quiet=%s verbose=%s",
                        uart_log_level_name(system_config.log_level),
                        system_config.log_level,
                        system_config.quiet_mode ? "ON" : "OFF",
                        system_config.verbose_mode ? "ON" : "OFF");
    uart_send_line("LOG none|error|warn|info|debug|verbose");
    return CMD_SUCCESS;
  }

  uint8_t level;
  if (strcasecmp(args, "none") == 0) {
    level = ESP_LOG_NONE;
  } else if (strcasecmp(args, "error") == 0) {
    level = ESP_LOG_ERROR;
  } else if (strcasecmp(args, "warn") == 0 || strcasecmp(args, "warning") == 0) {
    level = ESP_LOG_WARN;
  } else if (strcasecmp(args, "info") == 0) {
    level = ESP_LOG_INFO;
  } else if (strcasecmp(args, "debug") == 0) {
    level = ESP_LOG_DEBUG;
  } else if (strcasecmp(args, "verbose") == 0) {
    level = ESP_LOG_VERBOSE;
  } else {
    uart_send_line("LOG none|error|warn|info|debug|verbose");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  system_config.log_level = level;
  system_config.quiet_mode = (level == ESP_LOG_NONE);
  system_config.verbose_mode = false;
  if (config_apply_settings(&system_config) != ESP_OK) {
    uart_send_line("Log level apply failed");
    return CMD_ERROR_SYSTEM_ERROR;
  }
  if (config_save_to_nvs(&system_config) != ESP_OK) {
    uart_send_line("Log level saved in RAM only");
  }
  uart_send_formatted("Log level set to %s", uart_log_level_name(level));
  return CMD_SUCCESS;
}

/**
 * @brief Shows the main help menu with categories
 */
void uart_display_main_help(void) {
  uart_send_line("Commands:");
  for (int i = 0; uart_commands[i].name != NULL; i++) {
    uart_send_formatted("  %-8s %s", uart_commands[i].name,
                        uart_commands[i].description);
  }
}

/**
 * @brief Zobrazi help specificky pro hru
 */

/**
 * @brief Zobrazi help specificky pro system
 */

/**
 * @brief Zobrazi help pro zacatecniky
 */

/**
 * @brief Display help for the web interface
 */

/**
 * @brief CZECHMATE and Bluetooth LE mobile applications — users + developers
 */

/**
 * @brief Zobrazi help pro debug a testovani
 */


command_result_t uart_cmd_status(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("SYSTEM STATUS");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  uart_send_formatted("Version: %s", CHESS_VERSION_STRING);
  uart_send_formatted("Build Date: %s", CHESS_BUILD_DATE);
  uart_send_formatted("Free Heap: %zu bytes", esp_get_free_heap_size());
  uart_send_formatted("Minimum Free: %zu bytes",
                      esp_get_minimum_free_heap_size());
  uart_send_formatted("Active Tasks: %d", uxTaskGetNumberOfTasks());

  // Stack trace for all tasks
  uart_send_formatted("Task Stack Usage:");
  uart_send_formatted("  UART Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(NULL));
  uart_send_formatted("  LED Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(led_task_handle));
  uart_send_formatted("  Matrix Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(squares_task_handle));
  uart_send_formatted("  Button Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(button_task_handle));
  uart_send_formatted("  Game Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(game_task_handle));
  uart_send_formatted("Uptime: %llu seconds", esp_timer_get_time() / 1000000);
  uart_send_formatted("Commands Processed: %lu", command_count);
  uart_send_formatted("Errors: %lu", error_count);
  uart_send_formatted("Verbose Mode: %s",
                      system_config.verbose_mode ? "ON" : "OFF");
  uart_send_formatted("Quiet Mode: %s",
                      system_config.quiet_mode ? "ON" : "OFF");

  // Component Status
  uart_send_formatted("");
  uart_send_formatted("🔧 Component Status:");
  uart_send_formatted("  Matrix Scanner: %s",
                      squares_component_enabled ? "ENABLED" : "DISABLED");
  uart_send_formatted("  LED Control: %s",
                      led_component_enabled ? "ENABLED" : "DISABLED");
  uart_send_formatted("  WiFi: %s",
                      wifi_component_enabled ? "ENABLED" : "DISABLED");
  uart_send_formatted("  UART: %s", "ENABLED");        // Always enabled
  uart_send_formatted("  Game Engine: %s", "ENABLED"); // Always enabled

  uart_send_formatted("");
  uart_send_formatted("📊 GAME STATISTICS:");
  uart_send_formatted("  Total Games: %" PRIu32, game_get_total_games());
  uart_send_formatted("  White Wins: %" PRIu32, game_get_white_wins());
  uart_send_formatted("  Black Wins: %" PRIu32, game_get_black_wins());
  uart_send_formatted("  Draws: %" PRIu32, game_get_draws());
  uart_send_formatted("  Current Game State: %s", game_get_game_state_string());
  uart_send_formatted("  Move Count: %" PRIu32, game_get_move_count());
  uart_send_formatted("  Current Player: %s",
                      game_get_current_player() == PLAYER_WHITE ? "White"
                                                                : "Black");

  uart_send_formatted("");
  uart_send_formatted("💡 Use 'COMPONENT_OFF <name>' or 'COMPONENT_ON <name>' "
                      "to control components");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}


command_result_t uart_cmd_clear(const char *args) {
  (void)args; // Unused parameter

  // Reset watchdog timer before UART operations
  SAFE_WDT_RESET();

  // Simple clear without mutex to avoid WDT issues
  uart_write_string_immediate(
      "\033[2J\033[H"); // Clear screen and move cursor to top
  uart_write_string_immediate("Screen cleared\r\n");

  // Reset watchdog timer after UART operations
  SAFE_WDT_RESET();

  return CMD_SUCCESS;
}

// ============================================================================
// DEMO MODE COMMANDS
// ============================================================================

/**
 * @brief Toggle demo/screensaver mode
 */

/**
 * @brief Show demo mode status
 */

command_result_t uart_cmd_reset(const char *args) {
  (void)args; // Unused parameter

  uart_send_warning("SYSTEM RESET");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  uart_send_formatted("System will restart in 3 seconds...");
  uart_send_formatted("All unsaved data will be lost");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // This would actually implement reset in a real system
  vTaskDelay(pdMS_TO_TICKS(3000));
  esp_restart();

  return CMD_SUCCESS;
}

// ============================================================================
// STARTING POSITION CHECK COMMANDS
// ============================================================================

/**
 * @brief Toggle starting position check (hlidani postaveni figurek)
 */


// ============================================================================
// HIGH PRIORITY COMMANDS
// ============================================================================

/**
 * @brief Show the evaluation of the position
 */

/**
 * @brief Test all LEDs
 */
command_result_t uart_cmd_ledtest(const char *args) {
  (void)args;
  SAFE_WDT_RESET();

  static const uint8_t colors[][3] = {
      {255, 0, 0},     /* red */
      {0, 255, 0},     /* green */
      {0, 0, 255},     /* blue */
      {255, 255, 0},   /* yellow */
      {255, 255, 255}, /* white */
      {0, 0, 0},       /* off */
  };

  uart_send_line("LED test");

  for (size_t c = 0; c < sizeof(colors) / sizeof(colors[0]); c++) {
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      led_set_pixel_safe((uint8_t)i, colors[c][0], colors[c][1], colors[c][2]);
    }
    SAFE_WDT_RESET();
    vTaskDelay(pdMS_TO_TICKS(400));
    SAFE_WDT_RESET();
  }

  led_clear_board_only();
  uart_send_line("LED test done");
  return CMD_SUCCESS;
}

static bool s_hall_stream;
static int64_t s_hall_next_us;

static void uart_hall_print_once(void) {
  unsigned n = hall_i2c_squares_segment_count();
  if (n == 0) {
    uart_send_line("hall: build has no I2C Hall");
    return;
  }
  for (unsigned seg = 0; seg < n; seg++) {
    uint8_t addr = 0;
    uint16_t raw[32];
    esp_err_t err = hall_i2c_squares_read_segment_raw((uint8_t)seg, &addr, raw);
    char line[192];
    if (err != ESP_OK) {
      snprintf(line, sizeof(line), "seg%u 0x%02x FAIL %s", seg, addr,
               esp_err_to_name(err));
    } else {
      int pos = snprintf(line, sizeof(line), "seg%u 0x%02x f0=%u/%u |d|", seg,
                         addr, raw[0], raw[1]);
      for (unsigned field = 0; field < 16 && pos > 0 && pos < (int)sizeof(line) - 8;
           field++) {
        uint16_t a = raw[field * 2];
        uint16_t b = raw[field * 2 + 1];
        unsigned d = (a > b) ? (unsigned)(a - b) : (unsigned)(b - a);
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %u", d);
      }
    }
    uart_send_line(line);
    SAFE_WDT_RESET();
  }
}

command_result_t uart_cmd_hall(const char *args) {
  (void)args;
  s_hall_stream = true;
  s_hall_next_us = esp_timer_get_time();
  uart_send_line("Hall stream on. Type STOP to end.");
  uart_hall_print_once();
  s_hall_next_us = esp_timer_get_time() + 500000;
  return CMD_SUCCESS;
}

command_result_t uart_cmd_hall_stop(const char *args) {
  (void)args;
  s_hall_stream = false;
  uart_send_line("Hall stream off");
  return CMD_SUCCESS;
}

/**
 * @brief Check memory health and report status
 * @return ESP_OK if memory is healthy, ESP_ERR_NO_MEM if low
 */
esp_err_t uart_check_memory_health(void) {
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();

  /* ESP32-C6 + WiFi + BLE + HTTP: usually ~15-30 KiB free — don't take it as a bug
   */
  enum {
    heap_critical = 5120,
    heap_warning = 12288,
  };
  static uint32_t last_warn_ms;
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

  if (free_heap < (size_t)heap_critical) {
    ESP_LOGW(TAG, "⚠️ CRITICAL: Low memory - %zu bytes free (min: %zu)",
             free_heap, min_free_heap);
    return ESP_ERR_NO_MEM;
  }

  if (free_heap < (size_t)heap_warning) {
    if (now_ms - last_warn_ms > 60000) {
      last_warn_ms = now_ms;
      ESP_LOGW(TAG, "⚠️ WARNING: Low memory - %zu bytes free (min: %zu)",
               free_heap, min_free_heap);
    }
  }

  return ESP_OK;
}

/**
 * @brief Recover UART task from errors and system crashes
 * This function ensures UART continues to work even after WDT errors
 */
void uart_task_recover_from_error(void) {
  ESP_LOGW(TAG, "🔄 UART task recovery initiated...");

  // Immediate reset of the watchdog timer to prevent further timeouts
  SAFE_WDT_RESET();

  // Clear any corrupted input buffer
  input_buffer_clear(&input_buffer);

  // Re-initialize input buffer with safe defaults
  input_buffer_init(&input_buffer);

  // Clear any pending output with timeout protection
  if (uart_mutex != NULL) {
    if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      // Clear any pending output
      // No need for fflush with ESP-IDF UART driver
      xSemaphoreGive(uart_mutex);
    } else {
      ESP_LOGW(TAG, "Mutex timeout during recovery, continuing anyway");
    }
  }

  // CRITICAL: Reset WDT before any output operations
  SAFE_WDT_RESET();

  // Show recovery message with timeout protection
  uart_send_warning(
      "🔄 UART recovered from WDT error, console is responsive again");
  uart_send_warning("💡 You can now continue typing commands normally");
  // Prompt removed

  // Final watchdog timer reset
  SAFE_WDT_RESET();

  ESP_LOGI(TAG, "✅ UART task recovery completed");
}

/**
 * @brief Check if UART task is healthy and recover if needed
 */
bool uart_task_health_check(void) {
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
// INPUT PROCESSING
// ============================================================================

void uart_process_input(char c) {
  // ARROW KEY SUPPORT: Handling of ANSI escape sequences for arrows
  if (esc_state == ESC_STATE_NONE && c == CHAR_ESC) {
    esc_state = ESC_STATE_ESC;
    return; // We are waiting for the next sign
  }

  if (esc_state == ESC_STATE_ESC) {
    if (c == '[') {
      esc_state = ESC_STATE_BRACKET;
      return; // We are waiting for the final sign
    } else {
      // Invalid escape sequence, reset and continue normal processing
      esc_state = ESC_STATE_NONE;
    }
  }

  if (esc_state == ESC_STATE_BRACKET) {
    esc_state = ESC_STATE_NONE; // Resetovat stav

    if (c == 'A') {
      // Arrow Up - previous command from history
      handle_arrow_up();
      return;
    } else if (c == 'B') {
      // Arrow Down - next command from history
      handle_arrow_down();
      return;
    }
    // Other escape sequence - ignore
    return;
  }

  /* USB Serial JTAG sends CR+LF. Swallow the LF after CR so the prompt
   * is printed once. */
  static bool swallow_lf = false;
  if (swallow_lf && c == '\n') {
    swallow_lf = false;
    return;
  }
  swallow_lf = false;

  if (c == '\r' || c == '\n') {
    if (c == '\r') {
      swallow_lf = true;
    }
    // Process command
    if (input_buffer.length > 0) {
      uart_send_line(""); // New line after command

      // Add to history
      command_history_add(&command_history, input_buffer.buffer);

      // Process command
      uart_parse_command(input_buffer.buffer);

      // Clear buffer
      input_buffer_clear(&input_buffer);

      // Reset the navigation index when the command is entered
      history_navigation_index = -1;
    }

    uart_cli_show_prompt();
  } else if (c == '\b' || c == 127) {
    if (input_buffer.length > 0) {
      input_buffer_backspace(&input_buffer);
      uart_write_string_immediate("\b \b");
      fflush(stdout);
    }
  } else if (c >= 32 && c <= 126) {
    // Printable character
    input_buffer_add_char(&input_buffer, c);
    uart_write_char_immediate(c);
    fflush(stdout);
  }
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void uart_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "🚀 Enhanced UART command interface starting...");

  // Registrace tasku s Task Watchdog Timer
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  s_uart_wdt_subscribed = (wdt_ret == ESP_OK || wdt_ret == ESP_ERR_INVALID_ARG);
  if (!s_uart_wdt_subscribed) {
    ESP_LOGE(TAG, "Failed to register UART task with TWDT: %s",
             esp_err_to_name(wdt_ret));
  } else {
    ESP_LOGI(TAG, "✅ UART task registered with TWDT");
  }

  // Initialize configuration manager
  config_manager_init();

  // Load configuration from NVS
  config_load_from_nvs(&system_config);

  // Apply configuration settings
  config_apply_settings(&system_config);
  game_set_led_guidance_level(system_config.led_guidance_level);

  // Initialize input buffer and command history
  input_buffer_init(&input_buffer);
  command_history_init(&command_history);

  // UART response queue is created in freertos_chess.c
  if (uart_response_queue == NULL) {
    ESP_LOGE(TAG, "UART response queue not available");
  } else {
    ESP_LOGI(TAG, "UART response queue available");
  }

  ESP_LOGI(TAG, "Mutex available: %s", uart_mutex != NULL ? "YES" : "NO");

  // Initialize ESP-IDF UART driver (only if UART is explicitly configured)
  if (UART_ENABLED) {
    ESP_LOGI(TAG, "Initializing UART driver...");

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2,
                                        UART_BUF_SIZE * 2, UART_QUEUE_SIZE,
                                        NULL, 0));

    // Set optimal interactivity
    uart_set_rx_timeout(UART_PORT_NUM, pdMS_TO_TICKS(1));
    uart_flush(UART_PORT_NUM);

    ESP_LOGI(TAG, "UART driver initialized successfully");
  } else {
    ESP_LOGI(
        TAG,
        "Using USB Serial JTAG console - no UART driver initialization needed");
  }

  ESP_LOGI(TAG, "🚀 Enhanced UART command interface ready");
  ESP_LOGI(TAG, "Features:");
  ESP_LOGI(TAG, "  • Radkovy vstup s editaci");
  ESP_LOGI(TAG, "  • Command history and aliases");
  ESP_LOGI(TAG, "  • NVS configuration persistence");
  ESP_LOGI(TAG, "• Robust error handling");
  ESP_LOGI(TAG, "  • Resource optimization");

  task_running = true;

  // WDT already registered above

  // Welcome message will be shown by centralized boot animation
  // uart_send_welcome_logo(); // Removed to prevent duplicate rendering

  // Welcome message and guide will be shown by centralized boot animation
  // No early UART output to prevent WDT "task not found" errors

  uart_cli_arm_stdin();

  // Show initial prompt after everything is initialized
  vTaskDelay(pdMS_TO_TICKS(100));
  uart_cli_show_prompt();

  // Start main input loop using original logic
  uart_task_legacy_loop();

  // Should never reach here
  ESP_LOGE(TAG, "UART task unexpectedly exited");
  vTaskDelete(NULL);
}

// Legacy main loop (kept for compatibility)
void uart_task_legacy_loop(void) {
  uint32_t loop_count = 0;

  for (;;) {
    // Reset the watchdog timer for the UART task in each iteration
    esp_err_t wdt_reset_ret = uart_task_wdt_reset_safe();
    if (wdt_reset_ret != ESP_OK && wdt_reset_ret != ESP_ERR_NOT_FOUND) {
      // WDT reset failed - this might indicate system issues
    }

    // Output queue processing first for smooth output
    uart_process_output_queue();

    // Read and process input with minimal timeout for responsiveness
    char c;
    int len = 0;

    // ROBUST ERROR HANDLING: Wrap input reading in try-catch equivalent
    bool input_error = false;

    // Always use the USB Serial JTAG method for consistency
    // This ensures the same input method is used before and after WDT errors
    if (UART_ENABLED) {
      // Only use UART if explicitly configured (not USB Serial JTAG)
      esp_err_t uart_ret =
          uart_read_bytes(UART_PORT_NUM, (uint8_t *)&c, 1, pdMS_TO_TICKS(1));
      if (uart_ret == ESP_OK) {
        len = 1;
      } else if (uart_ret == ESP_ERR_TIMEOUT) {
        len = 0; // Normal timeout, not an error
      } else {
        // UART error - log but continue
        ESP_LOGW(TAG, "UART read error: %s, continuing...",
                 esp_err_to_name(uart_ret));
        input_error = true;
        len = 0;
      }
    } else {
      // For USB Serial JTAG (CONFIG_ESP_CONSOLE_UART_NUM=-1), use getchar
      // This is the consistent method used before and after WDT errors
      // Using a non-blocking approach to prevent WDT timeouts

      // Kontrola recovery módu po WDT
      static bool wdt_recovery_mode = false;
      static uint32_t wdt_recovery_start = 0;

      // Check if we need to enter recovery mode
      if (error_count > 0 && !wdt_recovery_mode) {
        wdt_recovery_mode = true;
        wdt_recovery_start = esp_timer_get_time() / 1000;
        ESP_LOGW(TAG, "Entering WDT recovery mode");
      }

      // Exit recovery mode after 10 seconds
      if (wdt_recovery_mode &&
          (esp_timer_get_time() / 1000 - wdt_recovery_start) > 10000) {
        wdt_recovery_mode = false;
        error_count = 0; // Reset error count
        ESP_LOGI(TAG, "Exiting WDT recovery mode");
      }

      // In recovery mode, use more conservative approach
      if (wdt_recovery_mode) {
        // Use shorter delays and more frequent WDT resets
        vTaskDelay(pdMS_TO_TICKS(1));
        SAFE_WDT_RESET();
      }

      int ch = getchar();
      if (ch >= 0) {
        c = (char)ch;
        len = 1;
      } else {
        /* Non-blocking stdin sets EOF on EAGAIN; clear it or the next
         * getchar() never reads again. */
        clearerr(stdin);
        len = 0;
      }
    }

    // Process input only if we got valid data
    if (len > 0 && !input_error) {
      // ROBUST ERROR HANDLING: Safe input processing
      bool processing_error = false;

      // Validate input character before processing
      if (c <=
          127) { // Valid ASCII range (c is unsigned, so >= 0 is always true)
        // Process the input
        uart_process_input(c);
      } else {
        ESP_LOGW(TAG, "Invalid character received: 0x%02X, ignoring",
                 (unsigned char)c);
        processing_error = true;
      }

      // If processing failed, clear buffer and show error
      if (processing_error) {
        input_buffer_clear(&input_buffer);
        uart_send_error("⚠️ Invalid input, buffer cleared");
        // Prompt removed
      }
    }

    // ROBUST ERROR HANDLING: Check for system errors and recover
    if (input_error) {
      // Increment error counter
      error_count++;

      // If too many errors, try to recover
      if (error_count % 100 == 0) {
        ESP_LOGW(TAG,
                 "Multiple input errors detected (%lu), attempting recovery...",
                 error_count);

        // Clear any corrupted state
        input_buffer_clear(&input_buffer);

        // Re-initialize input buffer
        input_buffer_init(&input_buffer);

        // Show recovery message
        uart_send_warning("🔄 UART input recovered, continuing...");
        // Prompt removed
      }
    }

    /* Memory/log: check ~1x/30s (1ms tick × 30000), not every second */
    if (loop_count % 30000 == 0) {
      uart_task_health_check();
      uart_check_memory_health();
    }

    /* Line status: at 1ms loop = every ~6s (earlier comment "60s"
     * was inaccurate) */
    if (loop_count % 6000 == 0) {
      ESP_LOGI(TAG, "UART Task Status: Commands=%lu, Errors=%lu", command_count,
               error_count);
    }

    if (s_hall_stream) {
      int64_t now = esp_timer_get_time();
      if (now >= s_hall_next_us) {
        uart_hall_print_once();
        s_hall_next_us = esp_timer_get_time() + 500000;
      }
    }

    loop_count++;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/**
 * @brief Display LED board states in terminal
 */

// ============================================================================
// CHUNKED OUTPUT FUNCTIONS
// ============================================================================

// ============================================================================
// UNIVERSAL CHUNKED UART FUNCTIONS
// ============================================================================

// ============================================================================
// ADVANTAGE GRAPH IMPLEMENTATION
// ============================================================================

/**
 * @brief Display chess.com style advantage graph
 * @param move_count Total number of moves in the game
 * @param white_wins True if white won, false if black won
 */

// ============================================================================
// ANIMATION TEST COMMANDS
// ============================================================================


// Endgame Animation Style Commands


// Puzzle removal test command removed

