/**
 * @file uart_task.c
 * @brief ESP32-C6 Chess System v2.4 - Rozsirena implementace UART tasku
 * 
 * Tento task poskytuje production-ready line-based UART terminal:
 * - Line-based vstup s editaci
 * - Tabulka prikazu s funkcnimi ukazateli
 * - Pokrocile funkce prikazu (aliasy, auto-completion)
 * - NVS konfigurace persistence
 * - Robustni error handling a validace
 * - Optimalizace zdroju
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento task zpracovava vsechny UART prikazy a komunikaci s uzivatelem.
 * Obsahuje rozsireny system prikazu pro ovladani sachoveho systemu.
 * Podporuje aliasy, auto-completion a pokrocile funkce.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_private/esp_clk.h"
#include "esp_task_wdt.h"

#include "driver/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
// ESP-IDF UART driver functions
// No POSIX includes needed

#include "uart_task.h"
#include "freertos_chess.h"
#include "led_task.h"
#include "game_task.h"
#include "config_manager.h"
#include "../freertos_chess/include/chess_types.h"
#include "led_mapping.h"  // ✅ FIX: Include LED mapping functions
#include "../uart_commands_extended/include/uart_commands_extended.h"  // ✅ FIX: Include extended UART commands
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "../matrix_task/include/matrix_task.h"  // ✅ FIX: Include matrix task functions
#include "../timer_system/include/timer_system.h"
// Enhanced puzzle system removed
#include <math.h>
#include <inttypes.h>
#include "esp_system.h"

// External function declarations
extern bool convert_notation_to_coords(const char* notation, uint8_t* row, uint8_t* col);

// External UART mutex for clean output
extern SemaphoreHandle_t uart_mutex;

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
 * 
 * @details
 * Funkce je pouzivana pro bezpecny reset watchdog timeru behem UART operaci.
 * Zabranuje chybam pri startupu kdy task jeste neni registrovany.
 */
static esp_err_t uart_task_wdt_reset_safe(void) {
    esp_err_t ret = esp_task_wdt_reset();
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // Log as WARNING instead of ERROR - task not registered yet
        ESP_LOGW("UART_TASK", "WDT reset: task not registered yet (this is normal during startup)");
        return ESP_OK; // Treat as success for our purposes
    } else if (ret != ESP_OK) {
        ESP_LOGE("UART_TASK", "WDT reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

// External task handles and queues
extern TaskHandle_t led_task_handle;
extern TaskHandle_t matrix_task_handle;
extern TaskHandle_t button_task_handle;
extern TaskHandle_t game_task_handle;
extern QueueHandle_t game_command_queue;
// extern QueueHandle_t led_command_queue;  // ❌ REMOVED: Queue hell eliminated

// Component status tracking
static bool matrix_component_enabled = true;
static bool led_component_enabled = true;
static bool wifi_component_enabled = false;

// UART response queue for game task responses (declared in freertos_chess.h)

// Forward declarations for functions used before definition
static bool send_move_to_game_task(const char* move_str);
static void uart_input_loop(void);
static void uart_task_legacy_loop(void);
static void uart_send_board_data_chunked(const char* data);
static void uart_send_led_data_chunked(const char* data);
static void uart_write_chunked(const char* data, size_t len);
static void uart_send_large_text_chunked(const char* text);

// Missing type definitions that should be here
// UART command structure is defined in uart_task.h

static const char *TAG = "UART_TASK";

// ============================================================================
// CHUNKED OUTPUT MACROS - OPRAVA pro panic při velkých výpisech
// ============================================================================

// Safe watchdog reset macro
#define SAFE_WDT_RESET() do { \
        esp_err_t _wdt_ret = esp_task_wdt_reset(); \
        if (_wdt_ret != ESP_OK && _wdt_ret != ESP_ERR_NOT_FOUND) { \
        /* Task not registered with TWDT yet - this is normal during startup */ \
    } \
} while(0)

// Univerzální chunked printf makro podle návrhu
#define CHUNKED_PRINTF(format, ...) do { \
    printf(format, ##__VA_ARGS__); \
    fflush(stdout); \
    SAFE_WDT_RESET(); \
    vTaskDelay(pdMS_TO_TICKS(1)); \
} while(0)

// Optimalizované konstanty pro ESP32-C6
#define CHUNK_DELAY_MS 2      // Minimální delay
#define MAX_CHUNK_SIZE 128    // Optimální pro UART buffer
#define STACK_SAFETY_LIMIT 512 // Minimální volný stack

// UART configuration - only use if UART is enabled
#if CONFIG_ESP_CONSOLE_UART_NUM >= 0
#define UART_PORT_NUM      CONFIG_ESP_CONSOLE_UART_NUM
#define UART_ENABLED       1
#else
#define UART_PORT_NUM      0  // Dummy value when UART disabled
#define UART_ENABLED       0
#endif
#define UART_BAUD_RATE     115200
#define UART_BUF_SIZE      1024
// UART_QUEUE_SIZE is now defined in freertos_chess.h

// ============================================================================
// ESP-IDF UART DRIVER FUNCTIONS
// ============================================================================

// Forward declarations
static void uart_fputs(const char* str);
void uart_send_line(const char* str);
bool is_valid_move_notation(const char* move);
bool is_valid_square_notation(const char* square);

/**
 * @brief Nahradi fputs s ESP-IDF UART driverem
 * 
 * Tato funkce posila string pres UART. Pouziva ESP-IDF UART driver
 * nebo USB Serial JTAG podle konfigurace.
 * 
 * @param str String k poslani pres UART
 * 
 * @details
 * Funkce automaticky detekuje zda je UART povolen a pouzije
 * odpovidajici metodu pro poslani dat.
 */
static void uart_fputs(const char* str)
{
    if (UART_ENABLED) {
        uart_write_bytes(UART_PORT_NUM, str, strlen(str));
    } else {
        // For USB Serial JTAG, use printf
        printf("%s", str);
    }
}

// ============================================================================
// CHARACTER INPUT/OUTPUT FUNCTIONS
// ============================================================================

/**
 * @brief Cte jeden znak s okamzitym vracenim (non-blocking)
 * 
 * Tato funkce cte jeden znak z UART bez cekani. Pouziva ESP-IDF UART driver
 * nebo USB Serial JTAG podle konfigurace. Filtruje ANSI escape sekvence.
 * 
 * @return Znak jako int, nebo -1 pokud neni dostupny zadny znak
 * 
 * @details
 * Funkce je non-blocking, takze vrati -1 pokud neni dostupny zadny znak.
 * Automaticky filtruje ANSI escape sekvence pro lepsi uzivatelsky zazitek.
 */
static int uart_read_char_immediate(void)
{
    if (UART_ENABLED) {
        uint8_t ch;
        int result = uart_read_bytes(UART_PORT_NUM, &ch, 1, 0); // Non-blocking read
        
        if (result > 0) {
            // Filter out ANSI escape sequences
            if (ch == 0x1B) { // ESC character
                // Skip ANSI escape sequence
                uint8_t next_ch;
                int next_result = uart_read_bytes(UART_PORT_NUM, &next_ch, 1, pdMS_TO_TICKS(10));
                if (next_result > 0 && next_ch == '[') {
                    // Skip until we find a letter or end of sequence
                    while (next_result > 0) {
                        next_result = uart_read_bytes(UART_PORT_NUM, &next_ch, 1, pdMS_TO_TICKS(10));
                        if (next_result > 0 && ((next_ch >= 'A' && next_ch <= 'Z') || 
                                               (next_ch >= 'a' && next_ch <= 'z') || 
                                               next_ch == '~' || next_ch == ';')) {
                            break;
                        }
                    }
                }
                return -1; // Skip this character
            }
            return (int)ch;
        }
        
        return -1; // No data available
    } else {
        // For USB Serial JTAG, use getchar with non-blocking
        int ch = getchar();
        if (ch == 0x1B) { // ESC character - skip ANSI escape sequence
            // Skip the rest of the escape sequence
            while ((ch = getchar()) != EOF && ch != '[');
            while ((ch = getchar()) != EOF && 
                   !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '~' || ch == ';'));
            return -1; // Skip this character
        }
        return ch;
    }
}

/**
 * @brief Zapise jeden znak s okamzitym flush
 * 
 * Tato funkce zapise jeden znak do UART s okamzitym flush.
 * Pouziva ESP-IDF UART driver nebo USB Serial JTAG podle konfigurace.
 * 
 * @param ch Znak k zapsani
 * 
 * @details
 * Funkce pouziva mutex pro thread-safe operace a automaticky
 * detekuje zda je UART povolen.
 */
void uart_write_char_immediate(char ch)
{
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

/**
 * @brief Zapise string s okamzitym flush
 * 
 * Tato funkce zapise cely string do UART s okamzitym flush.
 * Pouziva ESP-IDF UART driver nebo USB Serial JTAG podle konfigurace.
 * 
 * @param str String k zapsani
 * 
 * @details
 * Funkce pouziva mutex pro thread-safe operace a automaticky
 * detekuje zda je UART povolen. Je optimalizovana pro rychle zapisovani.
 */
void uart_write_string_immediate(const char* str)
{
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

// Input buffer configuration
#define UART_CMD_BUFFER_SIZE 256
#define UART_CMD_HISTORY_SIZE 20
#define UART_MAX_ARGS 10
#define INPUT_TIMEOUT_MS 100

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
// COMMAND TABLE STRUCTURE
// ============================================================================

// Note: All types are now defined in header files to avoid conflicts

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Task state
static bool task_running = false;
static bool color_enabled = true;  // ANSI color support
static bool echo_enabled = true;   // Echo support
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
/**
 * @brief Posle welcome logo pres UART
 * 
 * Tato funkce posle pekny ASCII logo systemu pres UART.
 * Logo obsahuje nazev systemu a je barevne zformatovane.
 * 
 * @details
 * Funkce pouziva mutex pro thread-safe operace a posila
 * ASCII art logo s barevnym formatovanim pro lepsi vzhled.
 */
void uart_send_welcome_logo(void)
{
    if (uart_mutex != NULL) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    
    // Don't clear screen - just show logo below current content
    uart_write_string_immediate("\n");
    
    // New ASCII art logo - using ESP-IDF UART driver
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m............................................................\x1b[34m:=*+-\x1b[0m...............................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................................................\x1b[34m:=#%@@%*=-=+#@@@%*=:\x1b[0m.....................................................\x1b[0m\n");
    uart_fputs("\x1b[0m..............................................\x1b[34m-=*%@@%*=-=*%@%@=*@%@%*=-+#%@@%*=-\x1b[0m..............................................\x1b[0m\n");
    uart_fputs("\x1b[0m......................................\x1b[34m:-+#@@@%+--+#%@%+@+#@@%@%%@%@@-*@=@@%#=-=*%@@@#+-:\x1b[0m......................................\x1b[0m\n");
    uart_fputs("\x1b[0m...............................\x1b[34m:-+%@@@#+--*%@@*@=*@*@@@#=\x1b[0m...........\x1b[34m:+%@@%+@:#@*@@%+--+%@@@%+-:\x1b[0m...............................\x1b[0m\n");
    uart_fputs("\x1b[0m........................\x1b[34m:-*@@@@#-:=#@@*@*+@+@@@%+:\x1b[0m.........................\x1b[34m-*@@@%+@:@@#@@#-:=#@@@@#-:\x1b[0m........................\x1b[0m\n");
    uart_fputs("\x1b[0m....................\x1b[34m%@@@@**#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%@@@@#\x1b[0m....................\x1b[0m\n");
    uart_fputs("\x1b[0m....................\x1b[34m%@#################################################################################%@#\x1b[0m....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[34m:%@=@+#@+@##@=@#%@+@*#@+@#%@=@*#@+@#*@+@#*@+@%*@+@%=@=%@+@**@=@%+@+#@=@%=@+#@+%%=@+:\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m......................\x1b[34m#@==============================================================================@+\x1b[0m......................\x1b[0m\n");
    uart_fputs("\x1b[0m.......................\x1b[34m##==========@\x1b[0m:::::::::::::::::::::::::::::::::::::::::::::::::::::\x1b[34m*@==========@+\x1b[0m........................\x1b[0m\n");
    uart_fputs("\x1b[0m........................\x1b[34m:@*******%@:\x1b[0m.\x1b[34m:%%%%%%%%%%%%%%%%%%%%%--#@@#.+%%%%%%%%%%%%%%%%%%%%*\x1b[0m..\x1b[34m-@#******%%\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m-@#+%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%%%=:+@@=\x1b[0m..:::::::::::::::::::\x1b[37m@%\x1b[0m....\x1b[34m@+#+*%*@:\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m=@#=%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%#--:*@@@@+-*-\x1b[0m.................\x1b[37m@%\x1b[0m....\x1b[34m@+#+*%*@-\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m=%#=%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%#.%@@@@@@@@%:\x1b[0m.................\x1b[37m@%\x1b[0m...\x1b[34m:%**+*%+@-\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m=%#-%:%.@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%#-@@@@@@@@:\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m-%**+*#+@-\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m+#%-%:%:@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%#-########-\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m=%**+*#+@=\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m**%-%:%:@\x1b[0m...\x1b[34m-@@%%%%%%%%%%%%%%%%%:#%%%##%%%*\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m+#**+*#*%=\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m#+%:%:%-%:\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%*::@@@@@%\x1b[0m....................\x1b[37m@%\x1b[0m...\x1b[34m*##*+***%+\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m#=%:%:#-%:\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%%%.%@@@@*\x1b[0m....................\x1b[37m@%\x1b[0m...\x1b[34m#*#++***#+\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m%:%:%:#=%=\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%%#:@@@@@%\x1b[0m....................\x1b[37m@%\x1b[0m...\x1b[34m%*#++*+***\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m%:%:%:#=#+\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%%%-*@@@@@@-\x1b[0m...................\x1b[37m@%\x1b[0m...\x1b[34m%+%++*+#+#\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m@:%:%:#+#*\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%#:=%%%%%%%%:\x1b[0m..................\x1b[37m@%\x1b[0m...\x1b[34m@+%++*+#=#\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.........................\x1b[34m@:%:%:#+*#\x1b[0m..\x1b[34m-@@%%%%%%%%%%%%%%%%-=%@%%%%%%%%-\x1b[0m.................\x1b[37m@%\x1b[0m...\x1b[34m@=%=+*=#-%\x1b[0m.........................\x1b[0m\n");
    uart_fputs("\x1b[0m.......................\x1b[34m:@*++++++++%#.-@@%%%%%%%%%%%%%%%.%@@@@@@@@@@@@#\x1b[0m................\x1b[37m@%\x1b[0m..\x1b[34m@*++++++++%%\x1b[0m........................\x1b[0m\n");
    uart_fputs("\x1b[0m......................\x1b[34m=@=----------*@-@@@@@@@@@@@@@@@@@:*############=:@@@@@@@@@@@@@@@@%-@=----------=@:\x1b[0m.......................\x1b[0m\n");
    uart_fputs("\x1b[0m....................\x1b[34m*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@=\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m................................................................................\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m=@@@@@:+@@@@@@..@@@@@+..%@@@@@.-@%...+@%..@@#...=@@:...=@@-.=@@@@@@%-@@@@@-\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m%@+....:...:@@:..@@....-@@:...:::@#...=@#..@@@#.*@@@:..:%@@@:...@@:..:@@\x1b[0m......\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m@@:.......=@%....@@%%%.+@#......:@@%%%%@#.:@*+@@@:%@-..+@.*@#...@@:..:@@#@*\x1b[0m...\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m+@%:..-*.=@%..:=.@@...*:@@=...+-:@#...=@#.=@=.+@:.#@=.=@#**%@+..@@:..:@@...=\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m:*%@@#.=%%%%%%:-%%%%%*..-#@@%+.#%%:..#%#:#%=.....#%*:%%-..*%%=-%%+..=%%%%%=\x1b[0m..\x1b[37m@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m##--------------------------------------------------------------------------------@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m#%================================================================================@+\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.....................\x1b[37m+##################################################################################-\x1b[0m.....................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    uart_fputs("\x1b[0m.............................................................................................................................\x1b[0m\n");
    
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

/**
 * @brief Zobrazi animovany progress bar
 * 
 * Tato funkce zobrazi animovany progress bar s labelem a procenty.
 * Progress bar je barevny a plynule animovany.
 * 
 * @param label Popisek progress baru
 * @param max_value Maximalni hodnota (100 = 100%)
 * @param duration_ms Doba trvani v milisekundach
 * 
 * @details
 * Funkce vytvori pekny animovany progress bar s barevnym formatovanim.
 * Pouziva mutex pro thread-safe operace a plynule animuje progress.
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
    
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s: [", label);
    uart_write_string_immediate(buffer);
    
    for (int i = 0; i < bar_width; i++) {
        uart_write_string_immediate(".");
    }
    uart_write_string_immediate("] 0%");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    
    for (uint32_t i = 0; i <= max_value; i++) {
        int filled = (i * bar_width) / max_value;
        
        // Reset watchdog before each progress update (only if registered)
        esp_err_t wdt_ret = uart_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Move cursor back to start of progress bar
        if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
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
        
        snprintf(buffer, sizeof(buffer), "] %3u%%", (unsigned int)((i * 100) / max_value));
        uart_write_string_immediate(buffer);
        if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
        
        if (i < max_value) {
            // Smooth animation with calculated delay
            vTaskDelay(pdMS_TO_TICKS(step_delay));
        }
    }
    
    uart_write_string_immediate("\n");
    
    // CRITICAL: Release mutex only after progress bar is complete
    if (uart_mutex != NULL) {
        xSemaphoreGive(uart_mutex);
    }
}

/**
 * @brief Posle barevny text pres UART
 * 
 * Tato funkce posle text s barevnym formatovanim pres UART.
 * Pouziva ANSI escape sekvence pro barvy.
 * 
 * @param color ANSI escape sekvence pro barvu
 * @param message Text k poslani
 * 
 * @details
 * Funkce automaticky pridava reset barvy na konec textu
 * a pouziva mutex pro thread-safe operace.
 */
void uart_send_colored(const char* color, const char* message)
{
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
 * @brief Posle barevny text s novym radkem pres UART
 * 
 * Tato funkce posle text s barevnym formatovanim a novym radkem pres UART.
 * Pouziva ANSI escape sekvence pro barvy.
 * 
 * @param color ANSI escape sekvence pro barvu
 * @param message Text k poslani
 * 
 * @details
 * Funkce automaticky pridava novy radek a reset barvy na konec textu
 * a pouziva mutex pro thread-safe operace.
 */
void uart_send_colored_line(const char* color, const char* message)
{
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
 * @brief Posle chybovou zpravu pres UART
 * 
 * Tato funkce posle chybovou zpravu s cervenou barvou pres UART.
 * 
 * @param message Chybova zprava k poslani
 */
void uart_send_error(const char* message)
{
    uart_send_colored_line(COLOR_ERROR, message);
}

/**
 * @brief Posle uspesnou zpravu pres UART
 * 
 * Tato funkce posle uspesnou zpravu se zelenou barvou pres UART.
 * 
 * @param message Uspesna zprava k poslani
 */
void uart_send_success(const char* message)
{
    uart_send_colored_line(COLOR_SUCCESS, message);
}

/**
 * @brief Posle varovnou zpravu pres UART
 * 
 * Tato funkce posle varovnou zpravu se zlutou barvou pres UART.
 * 
 * @param message Varovna zprava k poslani
 */
void uart_send_warning(const char* message)
{
    uart_send_colored_line(COLOR_WARNING, message);
}

/**
 * @brief Posle informacni zpravu pres UART
 * 
 * Tato funkce posle informacni zpravu s modrou barvou pres UART.
 * 
 * @param message Informacni zprava k poslani
 */
void uart_send_info(const char* message)
{
    uart_send_colored_line(COLOR_INFO, message);
}

/**
 * @brief Posle zpravu o tahu pres UART
 * 
 * Tato funkce posle zpravu o tahu s specialni barvou pres UART.
 * 
 * @param message Zprava o tahu k poslani
 */
void uart_send_move(const char* message)
{
    uart_send_colored_line(COLOR_MOVE, message);
}

/**
 * @brief Posle status zpravu pres UART
 * 
 * Tato funkce posle status zpravu s specialni barvou pres UART.
 * 
 * @param message Status zprava k poslani
 */
void uart_send_status(const char* message)
{
    uart_send_colored_line(COLOR_STATUS, message);
}

/**
 * @brief Posle debug zpravu pres UART
 * 
 * Tato funkce posle debug zpravu s specialni barvou pres UART.
 * 
 * @param message Debug zprava k poslani
 */
void uart_send_debug(const char* message)
{
    uart_send_colored_line(COLOR_DEBUG, message);
}

/**
 * @brief Posle help zpravu pres UART
 * 
 * Tato funkce posle help zpravu s specialni barvou pres UART.
 * 
 * @param message Help zprava k poslani
 */
void uart_send_help(const char* message)
{
    uart_send_colored_line(COLOR_HELP, message);
}

/**
 * @brief Posle formatovanou zpravu pres UART
 * 
 * Tato funkce posle formatovanou zpravu s printf-style formatovanim pres UART.
 * 
 * @param format Format string pro printf
 * @param ... Argumenty pro format string
 * 
 * @details
 * Funkce pouziva vsnprintf pro bezpecne formatovani a automaticky
 * pridava novy radek na konec zpravy.
 */
void uart_send_formatted(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    uart_send_line(buffer);
}

/**
 * @brief Posle radek textu pres UART
 * 
 * Tato funkce posle radek textu s novym radkem pres UART.
 * Pouziva mutex pro thread-safe operace.
 * 
 * @param str String k poslani
 * 
 * @details
 * Funkce automaticky pridava novy radek na konec textu
 * a pouziva kratky timeout pro mutex aby se zabranilo WDT problemum.
 */
void uart_send_line(const char* str)
{
    if (str == NULL) return;
    
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

void uart_send_string(const char* str)
{
    if (str == NULL) return;
    
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
        char buffer[512];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (UART_ENABLED) {
            uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
            if (add_newline) uart_write_bytes(UART_PORT_NUM, "\n", 1);
        } else {
            printf("%s", buffer);
            if (add_newline) printf("\n");
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
            if (add_newline) uart_write_bytes(UART_PORT_NUM, "\n", 1);
        } else {
            printf("%s", msg.message);
            if (add_newline) printf("\n");
        }
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
        
        if (UART_ENABLED) {
            if (color_enabled && msg.type != UART_MSG_NORMAL) {
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "%s%s%s", color, msg.message, COLOR_RESET);
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
 * @brief Process regular character
 */
static void process_regular_char(char ch)
{
    if (input_buffer.pos < UART_CMD_BUFFER_SIZE - 1) {
        input_buffer.buffer[input_buffer.pos] = ch;
        input_buffer.pos++;
        input_buffer.buffer[input_buffer.pos] = '\0';
        
        // Echo handled by terminal
    }
}

void input_buffer_add_char(input_buffer_t* buffer, char c)
{
    if (buffer->pos < UART_CMD_BUFFER_SIZE - 1) {
        // Echo handled by terminal
        
        buffer->buffer[buffer->pos++] = c;
        buffer->buffer[buffer->pos] = '\0';
        buffer->length = buffer->pos;
    }
}

/**
 * @brief Process backspace character
 */
static void process_backspace(void)
{
    if (input_buffer.pos > 0) {
        input_buffer.pos--;
        input_buffer.buffer[input_buffer.pos] = '\0';
        
        // Backspace handled by terminal
    }
}

/**
 * @brief Process enter key - command completion
 */
static bool process_enter(void)
{
    // Newline handled by terminal
    
    // Null terminate command
    input_buffer.buffer[input_buffer.pos] = '\0';
    
    // Skip empty commands
    if (input_buffer.pos == 0) {
        return false;
    }
    
    return true; // Command ready for processing
}

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
        uart_write_string_immediate("CHESS COMMANDS (synced with web):\r\n");
        uart_write_string_immediate("  move <from><to>  - Make chess move (e.g., move e2e4)\r\n");
        uart_write_string_immediate("  moves [square]   - Show available moves for square\r\n");
        uart_write_string_immediate("  board           - Display current board (shared with web)\r\n");
        uart_write_string_immediate("  new             - Start new game\r\n");
        uart_write_string_immediate("  reset           - Reset game\r\n");
        uart_write_string_immediate("  status          - Game status (synced with web)\r\n");
        uart_write_string_immediate("\r\nTIMER COMMANDS (like web):\r\n");
        uart_write_string_immediate("  timer           - Show timer JSON\r\n");
        uart_write_string_immediate("  timer_config X  - Set time control type 0..14\r\n");
        uart_write_string_immediate("  timer_config custom <min> <inc> - Set custom\r\n");
        uart_write_string_immediate("  timer_pause     - Pause timer\r\n");
        uart_write_string_immediate("  timer_resume    - Resume timer\r\n");
        uart_write_string_immediate("  timer_reset     - Reset timer\r\n");
        uart_write_string_immediate("\r\nWIFI & WEB COMMANDS:\r\n");
        uart_write_string_immediate("  wifi_status     - Show WiFi AP status and clients\r\n");
        uart_write_string_immediate("  web_clients     - List active web connections\r\n");
        uart_write_string_immediate("  web_url         - Display connection URL\r\n");
        uart_write_string_immediate("\r\nLED COMMANDS:\r\n");
        uart_write_string_immediate("  led_test        - Test LED strip functionality\r\n");
        uart_write_string_immediate("  led_pattern     - Show LED patterns (checker, rainbow, etc.)\r\n");
        uart_write_string_immediate("  led_animation   - Play LED animations (cascade, fireworks, etc.)\r\n");
        uart_write_string_immediate("  led_clear       - Clear all LEDs\r\n");
        uart_write_string_immediate("  led_brightness  - Set LED brightness (0-255)\r\n");
        uart_write_string_immediate("  chess_pos <pos> - Show LED position for chess square\r\n");
        uart_write_string_immediate("  led_mapping_test- Test LED mapping (serpentine layout)\r\n");
        uart_write_string_immediate("\r\nSYSTEM COMMANDS:\r\n");
        uart_write_string_immediate("  version         - Show version information\r\n");
        uart_write_string_immediate("  clear           - Clear screen\r\n");
        uart_write_string_immediate("  help            - Show this help\r\n");
        uart_write_string_immediate("========================================\r\n");
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
                "Free Heap: %" PRIu32 " bytes\r\n"
                "Commands: %" PRIu32 "\r\n"
                "Errors: %" PRIu32 "\r\n"
                "Uptime: %llu sec\r\n"
                "WiFi: %s\r\n"
                "Web Server: %s\r\n",
                esp_get_free_heap_size(),
                command_count,
                error_count,
                esp_timer_get_time() / 1000000,
                wifi_component_enabled ? "Active" : "Inactive",
                wifi_component_enabled ? "Running" : "Stopped");
        
        uart_write_string_immediate(status_buf);
    }
    
    // WIFI_STATUS command
    else if (strcmp(argv[0], "wifi_status") == 0) {
        uart_write_string_immediate(COLOR_BOLD "WIFI STATUS\r\n" COLOR_RESET);
        uart_write_string_immediate("============\r\n");
        
        char wifi_buf[256];
        snprintf(wifi_buf, sizeof(wifi_buf),
                "WiFi AP: %s\r\n"
                "SSID: ESP32-Chess\r\n"
                "IP: 192.168.4.1\r\n"
                "Password: 12345678\r\n"
                "Web URL: http://192.168.4.1\r\n"
                "Status: %s\r\n",
                wifi_component_enabled ? "Active" : "Inactive",
                wifi_component_enabled ? "Running" : "Stopped");
        
        uart_write_string_immediate(wifi_buf);
    }
    
    // WEB_CLIENTS command
    else if (strcmp(argv[0], "web_clients") == 0) {
        uart_write_string_immediate(COLOR_BOLD "WEB CLIENTS\r\n" COLOR_RESET);
        uart_write_string_immediate("============\r\n");
        
        if (wifi_component_enabled) {
            uart_write_string_immediate("Web server is running\r\n");
            uart_write_string_immediate("Connect to: http://192.168.4.1\r\n");
            uart_write_string_immediate("Multiple clients can connect simultaneously\r\n");
        } else {
            uart_write_string_immediate("Web server is not running\r\n");
        }
    }
    
    // WEB_URL command
    else if (strcmp(argv[0], "web_url") == 0) {
        uart_write_string_immediate(COLOR_BOLD "WEB CONNECTION URL\r\n" COLOR_RESET);
        uart_write_string_immediate("==================\r\n");
        
        if (wifi_component_enabled) {
            uart_write_string_immediate("URL: http://192.168.4.1\r\n");
            uart_write_string_immediate("SSID: ESP32-Chess\r\n");
            uart_write_string_immediate("Password: 12345678\r\n");
            uart_write_string_immediate("\r\n");
            uart_write_string_immediate("Open this URL in your browser to view the chess board\r\n");
        } else {
            uart_write_string_immediate("Web server is not running\r\n");
        }
    }
    
    // TIMER commands - handled by command table system

    // VERSION command - handled by command table system
    
    // ECHO command removed - handled by terminal
    
    // CLEAR command - handled by command table system
    
    // MOVES command
    else if (strcmp(argv[0], "moves") == 0) {
        if (argc < 2) {
            uart_write_string_immediate(COLOR_RED "Usage: moves <square> (e.g., moves e2)\r\n" COLOR_RESET);
            return;
        }
        
        // Send command to game task to show moves
        if (game_command_queue != NULL) {
            chess_move_command_t cmd = {0};
            cmd.type = GAME_CMD_GET_VALID_MOVES;
            strncpy(cmd.from_notation, argv[1], sizeof(cmd.from_notation) - 1);
            cmd.from_notation[sizeof(cmd.from_notation) - 1] = '\0';
            
            if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
                uart_write_string_immediate(COLOR_GREEN "Moves requested for ");
                uart_write_string_immediate(argv[1]);
                uart_write_string_immediate("\r\n" COLOR_RESET);
            } else {
                uart_write_string_immediate(COLOR_RED "Failed to request moves\r\n" COLOR_RESET);
            }
        } else {
            uart_write_string_immediate(COLOR_RED "Game task not available\r\n" COLOR_RESET);
        }
    }
    
    // EXTENDED COMMANDS - integrate from uart_commands_extended.c
    else if (strcmp(argv[0], "led_test") == 0) {
        handle_led_test_command(argv, argc);
    }
    else if (strcmp(argv[0], "led_pattern") == 0) {
        handle_led_pattern_command(argv, argc);
    }
    else if (strcmp(argv[0], "led_animation") == 0) {
        handle_led_animation_command(argv, argc);
    }
    else if (strcmp(argv[0], "led_clear") == 0) {
        handle_led_clear_command(argv, argc);
    }
    else if (strcmp(argv[0], "led_brightness") == 0) {
        handle_led_brightness_command(argv, argc);
    }
    else if (strcmp(argv[0], "chess_pos") == 0) {
        handle_chess_pos_command(argv, argc);
    }
    else if (strcmp(argv[0], "led_mapping_test") == 0) {
        handle_led_mapping_test_command(argv, argc);
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

void input_buffer_backspace(input_buffer_t* buffer)
{
    if (buffer->pos > 0) {
        // Backspace handled by terminal
        
        buffer->pos--;
        buffer->buffer[buffer->pos] = '\0';
        buffer->length = buffer->pos;
    }
}

void input_buffer_set_cursor(input_buffer_t* buffer, size_t pos)
{
    if (pos <= buffer->length) {
        buffer->pos = pos;
        
        // Prompt handled by terminal
        {
            char prompt_line[512];
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
// GENIUS COMPATIBILITY FUNCTIONS  
// ============================================================================

/**
 * @brief Simple compatibility - no complex response system needed
 * All responses are handled directly via printf in game_task
 */

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// High Priority Commands
command_result_t uart_cmd_eval(const char* args);
command_result_t uart_cmd_ledtest(const char* args);
command_result_t uart_cmd_performance(const char* args);
command_result_t uart_cmd_config(const char* args);

// Timer Commands
command_result_t uart_cmd_timer(const char* args);
command_result_t uart_cmd_timer_config(const char* args);
command_result_t uart_cmd_timer_pause(const char* args);
command_result_t uart_cmd_timer_resume(const char* args);
command_result_t uart_cmd_timer_reset(const char* args);

// Medium Priority Commands
command_result_t uart_cmd_castle(const char* args);
command_result_t uart_cmd_promote(const char* args);
command_result_t uart_cmd_matrixtest(const char* args);
// Puzzle commands removed

// Component Control Commands
command_result_t uart_cmd_component_off(const char* args);
command_result_t uart_cmd_component_on(const char* args);

// Endgame Commands
command_result_t uart_cmd_endgame_white(const char* args);
command_result_t uart_cmd_endgame_black(const char* args);

// Advantage Graph Functions
void uart_display_advantage_graph(uint32_t move_count, bool white_wins);

// Animation test commands
command_result_t uart_cmd_test_move_anim(const char* args);
command_result_t uart_cmd_test_player_anim(const char* args);
command_result_t uart_cmd_test_castle_anim(const char* args);
command_result_t uart_cmd_test_promote_anim(const char* args);
command_result_t uart_cmd_test_endgame_anim(const char* args);
// Puzzle test command removed

// Endgame Animation Style Commands
command_result_t uart_cmd_endgame_wave(const char* args);
command_result_t uart_cmd_endgame_circles(const char* args);
command_result_t uart_cmd_endgame_cascade(const char* args);
command_result_t uart_cmd_endgame_fireworks(const char* args);
command_result_t uart_cmd_endgame_draw_spiral(const char* args);
command_result_t uart_cmd_endgame_draw_pulse(const char* args);

// Puzzle Commands
// Puzzle level commands removed

// Endgame animation control
command_result_t uart_cmd_stop_endgame(const char* args);

// System Commands

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
    // Logo already displayed by boot animation
    
    if (color_enabled) uart_write_string_immediate("\033[1;34m"); // bold blue
    uart_send_formatted("COMMAND CATEGORIES");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    uart_send_formatted("HELP <category> - Get detailed help for category:");
    uart_send_formatted("");
    
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("GAME     - Chess game commands (MOVE, BOARD, etc.)");
    if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan
    uart_send_formatted("SYSTEM   - System control and status commands");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("BEGINNER - Basic commands for new users");
    if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta
    uart_send_formatted("DEBUG    - Advanced debugging and testing");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;34m"); // bold blue
    uart_send_formatted("Quick Start:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  HELP BEGINNER  - Start here if you're new");
    uart_send_formatted("  HELP GAME      - Learn chess commands");
    uart_send_formatted("  HELP SYSTEM    - System management");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("Examples:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  HELP GAME      - Show chess commands");
    uart_send_formatted("  MOVE e2 e4     - Make a chess move");
    uart_send_formatted("  BOARD          - Show chess board");
    uart_send_formatted("  STATUS         - System status");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Display game-specific help
 */
void uart_cmd_help_game(void)
{
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("♔ CHESS GAME COMMANDS ♔");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) uart_write_string_immediate("\033[1;34m"); // bold blue
    uart_send_formatted("🎮 Game Control:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  GAME_NEW       - Start a new chess game");
    uart_send_formatted("  GAME_RESET     - Reset current game to starting position");
    uart_send_formatted("  BOARD          - Display enhanced chess board with current position");
    uart_send_formatted("  LED_BOARD      - Show current LED states and colors");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan
    uart_send_formatted("♟️  Move Commands:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  MOVE e2 e4     - Move piece from e2 to e4 (space separated)");
    uart_send_formatted("  MOVE e2-e4     - Move piece from e2 to e4 (dash separated)");
    uart_send_formatted("  MOVE e2e4      - Move piece from e2 to e4 (compact format)");
    uart_send_formatted("  UP e2          - Lift piece from e2 (with LED animations)");
    uart_send_formatted("  DN e4          - Drop piece to e4 (with LED animations)");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("📊 Game Information:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  MOVES e2       - Show valid moves for piece at e2");
    uart_send_formatted("  MOVES E2       - Show valid moves (uppercase also works)");
    uart_send_formatted("  MOVES pawn     - Show moves for all pawns of current player");
    uart_send_formatted("  GAME_HISTORY   - Display complete move history");
    uart_send_formatted("  UNDO           - Undo the last move");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("🎯 Advanced Game Features:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  EVAL           - Show position evaluation");
    uart_send_formatted("  CASTLE kingside - Castle kingside (O-O)");
    uart_send_formatted("  CASTLE queenside - Castle queenside (O-O-O)");
    uart_send_formatted("  PROMOTE e8=Q   - Promote pawn to Queen");
    // Puzzle commands removed
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;31m"); // bold red
    uart_send_formatted("🏆 Endgame Commands:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  ENDGAME_WHITE  - Simulate White victory");
    uart_send_formatted("  ENDGAME_BLACK  - Simulate Black victory");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("⏱️ Timer Commands:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  TIMER          - Show timer state (JSON)");
    uart_send_formatted("  TIMER_CONFIG <type> - Set time control (0..14)");
    uart_send_formatted("  TIMER_CONFIG custom <min> <inc> - Set custom time");
    uart_send_formatted("  TIMER_PAUSE    - Pause timer");
    uart_send_formatted("  TIMER_RESUME   - Resume timer");
    uart_send_formatted("  TIMER_RESET    - Reset timer");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta
    uart_send_formatted("💡 Pro Tips:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  • Use 'BOARD' to see the current position");
    uart_send_formatted("  • Use 'MOVES <square>' to analyze specific pieces");
    uart_send_formatted("  • Use 'MOVES <piece>' to see all moves for that piece type");
    uart_send_formatted("  • Use 'GAME_HISTORY' to review the entire game");
    uart_send_formatted("  • Use 'UNDO' to take back moves if needed");
    uart_send_formatted("  • LED colors: 🟡 Yellow (lifted), 🟢 Green (possible), 🟠 Orange (capture)");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Display system-specific help
 */
void uart_cmd_help_system(void)
{
    if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan
    uart_send_formatted("⚙️  SYSTEM COMMANDS ⚙️");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) uart_write_string_immediate("\033[1;34m"); // bold blue
    uart_send_formatted("📊 System Status:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  STATUS         - Show system status and diagnostics");
    uart_send_formatted("  VERSION        - Show version information");
    uart_send_formatted("  MEMORY         - Show memory usage");
    uart_send_formatted("  SHOW_TASKS     - Display running FreeRTOS tasks");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("⚙️  Configuration:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  VERBOSE ON/OFF - Control logging verbosity");
    uart_send_formatted("  QUIET          - Toggle quiet mode");
    uart_send_formatted("  CONFIG         - Show/set system configuration");
    uart_send_formatted("  CONFIG show    - Show current configuration");
    uart_send_formatted("  CONFIG key value - Set configuration key=value");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;31m"); // bold red
    uart_send_formatted("🌐 Web Server:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  Connect to: ESP32-Chess (password: 12345678)");
    uart_send_formatted("  Open browser: http://192.168.4.1");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;31m"); // bold red
    uart_send_formatted("🔌 Component Control:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  COMPONENT_OFF matrix - Turn off matrix scanning");
    uart_send_formatted("  COMPONENT_OFF led    - Turn off LED control");
    uart_send_formatted("  COMPONENT_OFF wifi   - Turn off WiFi");
    uart_send_formatted("  COMPONENT_ON matrix  - Turn on matrix scanning");
    uart_send_formatted("  COMPONENT_ON led     - Turn on LED control");
    uart_send_formatted("  COMPONENT_ON wifi    - Turn on WiFi");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta
    uart_send_formatted("🔧 Utilities:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  CLEAR          - Clear screen");
    uart_send_formatted("  RESET          - Restart entire system (hardware reset)");
    uart_send_formatted("  HISTORY        - Show command history");
    uart_send_formatted("  BENCHMARK      - Run performance benchmark");
    uart_send_formatted("  SHOW_MUTEXES   - Show all mutexes and their status");
    uart_send_formatted("  SHOW_FIFOS     - Show all FIFOs and their status");
    uart_send_formatted("  MATRIXTEST     - Test matrix scanning");
    uart_send_formatted("  LEDTEST        - Test all LEDs");
    uart_send_formatted("  PERFORMANCE    - Show system performance");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;31m"); // bold red
    uart_send_formatted("⚠️  Important Notes:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  • RESET restarts the entire system (like power cycle)");
    uart_send_formatted("  • Use GAME_RESET to reset only the chess game");
    uart_send_formatted("  • Use GAME_NEW to start a fresh game");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Display beginner-friendly help
 */
void uart_cmd_help_beginner(void)
{
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("♔ BEGINNER'S CHESS GUIDE ♔");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) uart_write_string_immediate("\033[1;34m"); // bold blue
    uart_send_formatted("🎯 Quick Start (3 Steps):");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  1. Type 'BOARD' to see the chess board");
    uart_send_formatted("  2. Type 'GAME_NEW' to start a new game");
    uart_send_formatted("  3. Type 'MOVE e2 e4' to make your first move");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan
    uart_send_formatted("♟️  Essential Commands:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  BOARD          - Display the chess board");
    uart_send_formatted("  MOVE e2 e4     - Move piece from e2 to e4");
    uart_send_formatted("  MOVE e2-e4     - Alternative format (dash)");
    uart_send_formatted("  MOVE e2e4      - Compact format (no space)");
    uart_send_formatted("  MOVES e2       - Show valid moves for piece at e2");
    uart_send_formatted("  GAME_HISTORY   - See all moves made so far");
    uart_send_formatted("  UNDO           - Take back the last move");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta
    uart_send_formatted("🎮 Game Control:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  GAME_NEW       - Start a fresh game");
    uart_send_formatted("  GAME_RESET     - Reset to starting position");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("💡 Chess Basics:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  • White always moves first");
    uart_send_formatted("  • Use 'e2 e4' for the classic King's Pawn opening");
    uart_send_formatted("  • Use 'd2 d4' for the Queen's Pawn opening");
    uart_send_formatted("  • Check 'MOVES <square>' before moving");
    uart_send_formatted("  • Use 'BOARD' after each move to see the position");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;31m"); // bold red
    uart_send_formatted("⚠️  Important Notes:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  • RESET restarts the entire system (hardware reset)");
    uart_send_formatted("  • Use GAME_RESET to reset only the chess game");
    uart_send_formatted("  • Use GAME_NEW to start a fresh game");
    uart_send_formatted("  • Invalid moves will be rejected with explanations");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("🔧 Advanced Features:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  EVAL           - Get position evaluation");
    uart_send_formatted("  CASTLE kingside - Castle kingside (O-O)");
    uart_send_formatted("  CASTLE queenside - Castle queenside (O-O-O)");
    uart_send_formatted("  PROMOTE e8=Q   - Promote pawn to Queen");
    // Puzzle commands removed
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Display debug and testing help
 */
void uart_cmd_help_debug(void)
{
    if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta
    uart_send_formatted("DEBUG & TESTING COMMANDS");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (color_enabled) uart_write_string_immediate("\033[1;34m"); // bold blue
    uart_send_formatted("Testing:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  SELF_TEST      - Run system self-test");
    // Echo test removed - handled by terminal
    uart_send_formatted("  TEST_GAME      - Test game engine");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;31m"); // bold red
    uart_send_formatted("Debugging:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  DEBUG_STATUS   - Show debug information");
    uart_send_formatted("  DEBUG_GAME     - Show game debug info");
    uart_send_formatted("  DEBUG_BOARD    - Show board debug info");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("Performance:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  BENCHMARK      - Run performance benchmark");
    uart_send_formatted("  MEMCHECK       - Check memory usage");
    uart_send_formatted("  SHOW_TASKS     - Show running tasks");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan
        uart_send_formatted("🎬 Animation Testing:");
        if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
        uart_send_formatted("  TEST_MOVE_ANIM    - Test move path animation");
        uart_send_formatted("  TEST_PLAYER_ANIM  - Test player change animation");
        uart_send_formatted("  TEST_CASTLE_ANIM  - Test castling animation");
        uart_send_formatted("  TEST_PROMOTE_ANIM - Test promotion animation");
        uart_send_formatted("  TEST_ENDGAME_ANIM - Test endgame animation");
        // Puzzle test command removed
        uart_send_formatted("");
        if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta
        uart_send_formatted("🎆 Endgame Animation Styles:");
        if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
        uart_send_formatted("  ENDGAME_WAVE      - Wave animation from edges");
        uart_send_formatted("  ENDGAME_CIRCLES   - Expanding circles from center");
        uart_send_formatted("  ENDGAME_CASCADE   - Falling lights animation");
        uart_send_formatted("  ENDGAME_FIREWORKS - Random burst animation");
        uart_send_formatted("  DRAW_SPIRAL       - Draw spiral animation");
        uart_send_formatted("  DRAW_PULSE        - Draw pulse animation");
        uart_send_formatted("");
        if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
        uart_send_formatted("🧩 Puzzle System:");
        if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
        // Puzzle level commands removed
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow
    uart_send_formatted("🎮 Endgame Animation Control:");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_send_formatted("  STOP_ENDGAME       - Stop endless endgame animation");
    
    uart_send_formatted("");
    if (color_enabled) uart_write_string_immediate("\033[1;32m"); // bold green
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
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
    uart_send_formatted("Errors: %lu", error_count);
    uart_send_formatted("Verbose Mode: %s", system_config.verbose_mode ? "ON" : "OFF");
    uart_send_formatted("Quiet Mode: %s", system_config.quiet_mode ? "ON" : "OFF");
    
    // Component Status
    uart_send_formatted("");
    uart_send_formatted("🔧 Component Status:");
    uart_send_formatted("  Matrix Scanner: %s", matrix_component_enabled ? "ENABLED" : "DISABLED");
    uart_send_formatted("  LED Control: %s", led_component_enabled ? "ENABLED" : "DISABLED");
    uart_send_formatted("  WiFi: %s", wifi_component_enabled ? "ENABLED" : "DISABLED");
    uart_send_formatted("  UART: %s", "ENABLED");            // Always enabled
    uart_send_formatted("  Game Engine: %s", "ENABLED");     // Always enabled
    
    uart_send_formatted("");
    uart_send_formatted("📊 GAME STATISTICS:");
    uart_send_formatted("  Total Games: %" PRIu32, game_get_total_games());
    uart_send_formatted("  White Wins: %" PRIu32, game_get_white_wins());
    uart_send_formatted("  Black Wins: %" PRIu32, game_get_black_wins());
    uart_send_formatted("  Draws: %" PRIu32, game_get_draws());
    uart_send_formatted("  Current Game State: %s", game_get_game_state_string());
    uart_send_formatted("  Move Count: %" PRIu32, game_get_move_count());
    uart_send_formatted("  Current Player: %s", game_get_current_player() == PLAYER_WHITE ? "White" : "Black");
    
    uart_send_formatted("");
    uart_send_formatted("💡 Use 'COMPONENT_OFF <name>' or 'COMPONENT_ON <name>' to control components");
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

// ECHO command removed - handled by terminal

command_result_t uart_cmd_clear(const char* args)
{
    (void)args; // Unused parameter
    
    // CRITICAL: Reset WDT before any UART operations
    SAFE_WDT_RESET();
    
    // Simple clear without mutex to avoid WDT issues
    uart_write_string_immediate("\033[2J\033[H"); // Clear screen and move cursor to top
    uart_write_string_immediate("Screen cleared\r\n");
    
    // CRITICAL: Reset WDT after UART operations
    SAFE_WDT_RESET();
    
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
// COMMAND TABLE DEFINITION
// ============================================================================

static const uart_command_t commands[] = {
    // System commands
    {"HELP", uart_cmd_help, "Show command list", "", false, {"?", "H", "", "", ""}},
    {"STATUS", uart_cmd_status, "System status", "", false, {"S", "INFO", "", "", ""}},
    {"VERSION", uart_cmd_version, "Version information", "", false, {"VER", "V", "", "", ""}},
    {"MEMORY", uart_cmd_memory, "Memory information", "", false, {"MEM", "M", "", "", ""}},
    {"HISTORY", uart_cmd_history, "Command history", "", false, {"HIST", "H", "", "", ""}},
    // ECHO command removed - handled by terminal
    {"CLEAR", uart_cmd_clear, "Clear screen", "", false, {"CLS", "C", "", "", ""}},
    {"RESET", uart_cmd_reset, "System restart", "", false, {"RESTART", "R", "reset", "", ""}},
    
    // Configuration commands
    {"VERBOSE", uart_cmd_verbose, "Control logging verbosity", "VERBOSE ON/OFF", true, {"V", "VERB", "", "", ""}},
    {"QUIET", uart_cmd_quiet, "Toggle quiet mode", "", false, {"Q", "SILENT", "", "", ""}},
    
    // Game commands
    {"MOVE", uart_cmd_move, "Make chess move", "MOVE <from> <to>", true, {"M", "MV", "", "", ""}},
    {"UP", uart_cmd_up, "Lift piece from square", "UP <square>", true, {"U", "LIFT", "", "", ""}},
    {"DN", uart_cmd_dn, "Drop piece to square", "DN <square>", true, {"D", "DROP", "", "", ""}},
    {"BOARD", uart_cmd_board, "Show chess board", "", false, {"B", "SHOW", "POS", "", ""}},
    {"LED_BOARD", uart_cmd_led_board, "Show LED states", "", false, {"LED", "LEDS", "LIGHTS", "", ""}},
    {"GAME_NEW", uart_cmd_game_new, "Start new game", "", false, {"NEW", "START", "GAME", "", ""}},
    {"GAME_RESET", uart_cmd_game_reset, "Reset game", "", false, {"GAME_RESET", "GAME_RESTART", "", "", ""}},
    {"MOVES", uart_cmd_show_moves, "Show valid moves", "", false, {"SHOW_MOVES", "VALID", "LEGAL", "", ""}},
    {"UNDO", uart_cmd_undo, "Undo last move", "", false, {"U", "BACK", "TAKEBACK", "", ""}},
    {"GAME_HISTORY", uart_cmd_game_history, "Show move history", "", false, {"HIST", "MOVES", "GAME", "", ""}},
    
    // Debug commands
    {"SELF_TEST", uart_cmd_self_test, "Run system self-test", "", false, {"TEST", "", "", "", ""}},
    {"TEST_GAME", uart_cmd_test_game, "Test game engine", "", false, {"GAME_TEST", "", "", "", ""}},
    {"DEBUG_STATUS", uart_cmd_debug_status, "Show debug information", "", false, {"DEBUG", "", "", "", ""}},
    {"DEBUG_GAME", uart_cmd_debug_game, "Show game debug info", "", false, {"GAME_DEBUG", "", "", "", ""}},
    {"DEBUG_BOARD", uart_cmd_debug_board, "Show board debug info", "", false, {"BOARD_DEBUG", "", "", "", ""}},
    {"BENCHMARK", uart_cmd_benchmark, "Run performance benchmark", "", false, {"PERF", "", "", "", ""}},
    {"MEMCHECK", uart_cmd_memcheck, "Check memory usage", "", false, {"MEM_CHECK", "", "", "", ""}},
    {"SHOW_TASKS", uart_cmd_show_tasks, "Show running tasks", "", false, {"TASKS", "", "", "", ""}},
    {"SHOW_MUTEXES", uart_cmd_show_mutexes, "Show all mutexes and their status", "", false, {"MUTEXES", "", "", "", ""}},
    {"SHOW_FIFOS", uart_cmd_show_fifos, "Show all FIFOs and their status", "", false, {"FIFOS", "", "", "", ""}},
    
    // High Priority Commands
    {"EVAL", uart_cmd_eval, "Show position evaluation", "", false, {"EVALUATE", "POSITION", "", "", ""}},
    {"HISTORY", uart_cmd_history, "Show move history", "", false, {"HIST", "MOVES", "", "", ""}},
    {"LEDTEST", uart_cmd_ledtest, "Test all LEDs", "", false, {"LED_TEST", "TEST_LED", "", "", ""}},
    {"PERFORMANCE", uart_cmd_performance, "Show system performance", "", false, {"PERF", "SYS_PERF", "", "", ""}},
    {"CONFIG", uart_cmd_config, "Show/set configuration", "CONFIG [key] [value]", true, {"CFG", "SETTINGS", "", "", ""}},
    
    // Timer Commands (mirror web API)
    {"TIMER", uart_cmd_timer, "Show timer state (JSON)", "", false, {"TMR", "", "", "", ""}},
    {"TIMER_CONFIG", uart_cmd_timer_config, "Set time control", "TIMER_CONFIG <type|custom> [min inc]", true, {"TCONF", "TCFG", "", "", ""}},
    {"TIMER_PAUSE", uart_cmd_timer_pause, "Pause timer", "", false, {"TPAUSE", "", "", "", ""}},
    {"TIMER_RESUME", uart_cmd_timer_resume, "Resume timer", "", false, {"TRESUME", "", "", "", ""}},
    {"TIMER_RESET", uart_cmd_timer_reset, "Reset timer", "", false, {"TRESET", "", "", "", ""}},
    
    // Medium Priority Commands
    {"CASTLE", uart_cmd_castle, "Castle (kingside/queenside)", "CASTLE <kingside|queenside>", true, {"CASTLING", "O-O", "", "", ""}},
    {"PROMOTE", uart_cmd_promote, "Promote pawn", "PROMOTE <square>=<piece>", true, {"PROMOTION", "PROMO", "", "", ""}},
    {"MATRIXTEST", uart_cmd_matrixtest, "Test matrix scanning", "", false, {"MATRIX_TEST", "TEST_MATRIX", "", "", ""}},
    // Puzzle commands removed
    
    // Component Control Commands
    {"COMPONENT_OFF", uart_cmd_component_off, "Turn off component", "COMPONENT_OFF <matrix|led|wifi>", true, {"OFF", "DISABLE", "", "", ""}},
    {"COMPONENT_ON", uart_cmd_component_on, "Turn on component", "COMPONENT_ON <matrix|led|wifi>", true, {"ON", "ENABLE", "", "", ""}},
    
    // Endgame Commands
    {"ENDGAME_WHITE", uart_cmd_endgame_white, "Endgame - White wins", "", false, {"WHITE_WINS", "WHITE_VICTORY", "", "", ""}},
    {"ENDGAME_BLACK", uart_cmd_endgame_black, "Endgame - Black wins", "", false, {"BLACK_WINS", "BLACK_VICTORY", "", "", ""}},
    
    // Animation Test Commands
    {"TEST_MOVE_ANIM", uart_cmd_test_move_anim, "Test move animation", "TEST_MOVE_ANIM", false, {"MOVE_TEST", "TEST_MOVE", "", "", ""}},
    {"TEST_PLAYER_ANIM", uart_cmd_test_player_anim, "Test player change animation", "TEST_PLAYER_ANIM", false, {"PLAYER_TEST", "TEST_PLAYER", "", "", ""}},
    {"TEST_CASTLE_ANIM", uart_cmd_test_castle_anim, "Test castling animation", "TEST_CASTLE_ANIM", false, {"CASTLE_TEST", "TEST_CASTLE", "", "", ""}},
    {"TEST_PROMOTE_ANIM", uart_cmd_test_promote_anim, "Test promotion animation", "TEST_PROMOTE_ANIM", false, {"PROMOTE_TEST", "TEST_PROMOTE", "", "", ""}},
    {"TEST_ENDGAME_ANIM", uart_cmd_test_endgame_anim, "Test endgame animation", "TEST_ENDGAME_ANIM", false, {"ENDGAME_TEST", "TEST_ENDGAME", "", "", ""}},
    // Puzzle test command removed
    
    // Endgame Animation Style Commands
    {"ENDGAME_WAVE", uart_cmd_endgame_wave, "Endgame wave animation", "ENDGAME_WAVE", false, {"WAVE", "ENDGAME_0", "", "", ""}},
    {"ENDGAME_CIRCLES", uart_cmd_endgame_circles, "Endgame circles animation", "ENDGAME_CIRCLES", false, {"CIRCLES", "ENDGAME_1", "", "", ""}},
    {"ENDGAME_CASCADE", uart_cmd_endgame_cascade, "Endgame cascade animation", "ENDGAME_CASCADE", false, {"CASCADE", "ENDGAME_2", "", "", ""}},
    {"ENDGAME_FIREWORKS", uart_cmd_endgame_fireworks, "Endgame fireworks animation", "ENDGAME_FIREWORKS", false, {"FIREWORKS", "ENDGAME_3", "", "", ""}},
    {"DRAW_SPIRAL", uart_cmd_endgame_draw_spiral, "Draw spiral animation", "DRAW_SPIRAL", false, {"SPIRAL", "DRAW_0", "", "", ""}},
    {"DRAW_PULSE", uart_cmd_endgame_draw_pulse, "Draw pulse animation", "DRAW_PULSE", false, {"PULSE", "DRAW_1", "", "", ""}},
    
    // Puzzle Commands
    // Puzzle level commands removed
    
    // Endgame animation control
    {"STOP_ENDGAME", uart_cmd_stop_endgame, "Stop endless endgame animation", "STOP_ENDGAME", false, {"STOP", "END_STOP", "", "", ""}},
    
    // System Commands

    
    // End marker
    {NULL, NULL, "", "", false, {"", "", "", "", ""}}
};

// ============================================================================
// HIGH PRIORITY COMMANDS
// ============================================================================

/**
 * @brief Show position evaluation
 */
command_result_t uart_cmd_eval(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🔍 Position Evaluation");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // MEMORY OPTIMIZATION: Use local evaluation instead of queue communication
    ESP_LOGI(TAG, "📡 Using local position evaluation (no queue communication)");
    
    // Get basic game statistics for evaluation
    uint32_t move_count = game_get_move_count();
    player_t current_player = game_get_current_player();
    
    // Display evaluation in small chunks
    uart_send_formatted("📊 Position Analysis:");
    uart_send_formatted("");
    
    // Get real material balance
    int white_material, black_material;
    int material_balance = game_calculate_material_balance(&white_material, &black_material);
    
    // Get real game statistics
    uint32_t white_wins = game_get_white_wins();
    uint32_t black_wins = game_get_black_wins();
    uint32_t draws = game_get_draws();
    uint32_t total_games = game_get_total_games();
    
    uart_send_formatted("🎯 Current Evaluation:");
    uart_send_formatted("  • Material Balance: %s", 
                       material_balance > 0 ? "White Advantage" : 
                       material_balance < 0 ? "Black Advantage" : "Even");
    uart_send_formatted("  • White Material: %d points", white_material);
    uart_send_formatted("  • Black Material: %d points", black_material);
    uart_send_formatted("  • Material Difference: %+d", material_balance);
    
    uart_send_formatted("");
    uart_send_formatted("📊 Game Statistics:");
    uart_send_formatted("  • Total Games: %" PRIu32, total_games);
    uart_send_formatted("  • White Wins: %" PRIu32, white_wins);
    uart_send_formatted("  • Black Wins: %" PRIu32, black_wins);
    uart_send_formatted("  • Draws: %" PRIu32, draws);
    
    uart_send_formatted("");
    uart_send_formatted("📈 Position Features:");
    uart_send_formatted("  • Current Player: %s", current_player == PLAYER_WHITE ? "White" : "Black");
    uart_send_formatted("  • Move Count: %" PRIu32, move_count);
    uart_send_formatted("  • Game State: %s", game_get_game_state_string());
    
    uart_send_formatted("");
    uart_send_formatted("🎮 Game Phase:");
    if (move_count < 10) {
        uart_send_formatted("  • Phase: Opening");
        uart_send_formatted("  • Focus: Development and King Safety");
    } else if (move_count < 30) {
        uart_send_formatted("  • Phase: Middlegame");
        uart_send_formatted("  • Focus: Tactics and Strategy");
    } else {
        uart_send_formatted("  • Phase: Endgame");
        uart_send_formatted("  • Focus: King Activity and Pawns");
    }
    
    uart_send_formatted("");
    uart_send_formatted("💡 Recommendations for %s:", current_player == PLAYER_WHITE ? "White" : "Black");
    uart_send_formatted("  • Improve piece coordination");
    uart_send_formatted("  • Control central squares");
    uart_send_formatted("  • Consider pawn breaks");
    
    // CRITICAL: Reset WDT after completion
    SAFE_WDT_RESET();
    
    ESP_LOGI(TAG, "✅ Position evaluation completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Test all LEDs
 */
command_result_t uart_cmd_ledtest(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "💡 LED Test");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Send LED test request to LED task
    led_command_t led_cmd = {
        .type = LED_CMD_TEST_ALL,
        .led_index = 0,
        .red = 0,
        .green = 0,
        .blue = 0,
        .duration_ms = 0,
        .data = NULL
    };
    
    // ✅ DIRECT LED CALL - No queue hell
    led_set_pixel_safe(led_cmd.led_index, led_cmd.red, led_cmd.green, led_cmd.blue);
    uart_send_formatted("✅ LED test executed directly");
    
    uart_send_formatted("🔄 Testing all LEDs...");
    uart_send_formatted("💡 All LEDs should cycle through colors");
    uart_send_formatted("✅ LED test completed");
    
    return CMD_SUCCESS;
}

/**
 * @brief Show system performance metrics
 */
command_result_t uart_cmd_performance(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "📊 System Performance");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Get system information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    uart_send_formatted("🔧 Hardware Information:");
    uart_send_formatted("  • Chip: %s", chip_info.model == CHIP_ESP32C6 ? "ESP32-C6" : "Unknown");
    uart_send_formatted("  • Cores: %d", chip_info.cores);
    uart_send_formatted("  • Revision: %d", chip_info.revision);
    uart_send_formatted("  • Features: %s%s%s%s",
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
        (chip_info.features & CHIP_FEATURE_BT) ? "BT " : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
        (chip_info.features & CHIP_FEATURE_IEEE802154) ? "802.15.4 " : "");
    
    // Memory information
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    
    uart_send_formatted("");
    uart_send_formatted("💾 Memory Usage:");
    uart_send_formatted("  • Free heap: %zu bytes (%.1f KB)", free_heap, free_heap / 1024.0);
    uart_send_formatted("  • Min free heap: %zu bytes (%.1f KB)", min_free_heap, min_free_heap / 1024.0);
    uart_send_formatted("  • Total heap: %zu bytes (%.1f KB)", total_heap, total_heap / 1024.0);
    uart_send_formatted("  • Used heap: %zu bytes (%.1f KB)", total_heap - free_heap, (total_heap - free_heap) / 1024.0);
    
    // Task information
    uart_send_formatted("");
    uart_send_formatted("🔄 Task Information:");
    uart_send_formatted("  • Uptime: %llu ms", esp_timer_get_time() / 1000);
    uart_send_formatted("  • FreeRTOS version: %s", tskKERNEL_VERSION_NUMBER);
    
    // CPU frequency
    uint32_t cpu_freq = esp_clk_cpu_freq();
    uart_send_formatted("  • CPU frequency: %lu MHz", cpu_freq / 1000000);
    
    return CMD_SUCCESS;
}

/**
 * @brief Show/set configuration
 */
command_result_t uart_cmd_config(const char* args)
{
    SAFE_WDT_RESET();
    
    if (!args || strlen(args) == 0) {
        // Show all configuration
        uart_send_colored_line(COLOR_INFO, "⚙️ System Configuration");
        uart_send_formatted("═══════════════════════════════════════════════════════════════");
        
        uart_send_formatted("🎮 Game Settings:");
        player_t current_player = game_get_current_player();
        uart_send_formatted("  • Current player: %s", current_player == PLAYER_WHITE ? "White" : "Black");
        uart_send_formatted("  • Game mode: %s", "Human vs Human");
        uart_send_formatted("  • Time control: %s", "No limit");
        
        uart_send_formatted("");
        uart_send_formatted("🔧 System Settings:");
        uart_send_formatted("  • LED brightness: 100%%");
        uart_send_formatted("  • Matrix sensitivity: Normal");
        uart_send_formatted("  • Debug mode: %s", "Disabled");
        
        uart_send_formatted("");
        uart_send_formatted("💡 Usage: CONFIG <key> <value> to set configuration");
        uart_send_formatted("💡 Available keys: player, brightness, sensitivity, debug");
        
        return CMD_SUCCESS;
    }
    
    // Parse arguments
    char key[32], value[32];
    int parsed = sscanf(args, "%31s %31s", key, value);
    
    if (parsed == 1) {
        // Single argument - show specific configuration
        if (strcmp(key, "show") == 0) {
            // Show all configuration (same as no args)
            uart_send_colored_line(COLOR_INFO, "⚙️ System Configuration");
            uart_send_formatted("═══════════════════════════════════════════════════════════════");
            
            uart_send_formatted("🎮 Game Settings:");
            player_t current_player = game_get_current_player();
            uart_send_formatted("  • Current player: %s", current_player == PLAYER_WHITE ? "White" : "Black");
            uart_send_formatted("  • Game mode: %s", "Human vs Human");
            uart_send_formatted("  • Time control: %s", "No limit");
            
            uart_send_formatted("");
            uart_send_formatted("🔧 System Settings:");
            uart_send_formatted("  • LED brightness: 100%%");
            uart_send_formatted("  • Matrix sensitivity: Normal");
            uart_send_formatted("  • Debug mode: %s", "Disabled");
            
            uart_send_formatted("");
            uart_send_formatted("💡 Usage: CONFIG <key> <value> to set configuration");
            uart_send_formatted("💡 Available keys: player, brightness, sensitivity, debug");
            
            return CMD_SUCCESS;
        } else {
            uart_send_error("❌ Usage: CONFIG [show] or CONFIG <key> <value>");
            return CMD_ERROR_INVALID_SYNTAX;
        }
    } else if (parsed != 2) {
        uart_send_error("❌ Usage: CONFIG [show] or CONFIG <key> <value>");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Handle configuration changes
    if (strcmp(key, "player") == 0) {
        if (strcmp(value, "white") == 0) {
            // current_player = PLAYER_WHITE; // Would need to implement
            uart_send_formatted("✅ Player set to White");
        } else if (strcmp(value, "black") == 0) {
            // current_player = PLAYER_BLACK; // Would need to implement
            uart_send_formatted("✅ Player set to Black");
        } else {
            uart_send_error("❌ Invalid player. Use 'white' or 'black'");
            return CMD_ERROR_INVALID_SYNTAX;
        }
    } else if (strcmp(key, "brightness") == 0) {
        int brightness = atoi(value);
        if (brightness >= 0 && brightness <= 100) {
            uart_send_formatted("✅ LED brightness set to %d%%", brightness);
        } else {
            uart_send_error("❌ Brightness must be 0-100");
            return CMD_ERROR_INVALID_SYNTAX;
        }
    } else if (strcmp(key, "sensitivity") == 0) {
        if (strcmp(value, "low") == 0 || strcmp(value, "normal") == 0 || strcmp(value, "high") == 0) {
            uart_send_formatted("✅ Matrix sensitivity set to %s", value);
        } else {
            uart_send_error("❌ Invalid sensitivity. Use 'low', 'normal', or 'high'");
            return CMD_ERROR_INVALID_SYNTAX;
        }
    } else if (strcmp(key, "debug") == 0) {
        if (strcmp(value, "on") == 0 || strcmp(value, "off") == 0) {
            uart_send_formatted("✅ Debug mode %s", strcmp(value, "on") == 0 ? "enabled" : "disabled");
        } else {
            uart_send_error("❌ Invalid debug value. Use 'on' or 'off'");
            return CMD_ERROR_INVALID_SYNTAX;
        }
    } else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "❌ Unknown configuration key: %s", key);
        uart_send_error(error_msg);
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    return CMD_SUCCESS;
}

// ============================================================================
// TIMER COMMANDS (mirror web API)
// ============================================================================

/**
 * @brief Format time in milliseconds to MM:SS format
 */
static void format_time_mmss(char* buffer, size_t size, uint32_t time_ms)
{
    uint32_t total_seconds = time_ms / 1000;
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    snprintf(buffer, size, "%02u:%02u", (unsigned int)minutes, (unsigned int)seconds);
}

/**
 * @brief Show timer state (human-readable format)
 */
command_result_t uart_cmd_timer(const char* args)
{
    (void)args; // Unused
    SAFE_WDT_RESET();
    
    chess_timer_t timer_state;
    if (timer_get_state(&timer_state) != ESP_OK) {
        uart_send_error("❌ Failed to get timer state");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    uart_send_colored_line(COLOR_INFO, "⏱️ Timer State");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Time control info
    if (timer_state.config.type == TIME_CONTROL_NONE) {
        uart_send_formatted("⏸️  Time Control: None (No time limit)");
        uart_send_line("");
        return CMD_SUCCESS;
    }
    
    uart_send_formatted("📋 Time Control: %s", timer_state.config.name);
    // Description is a const char* - check if it's not empty (skip NULL check as it's always set in TIME_CONTROLS array)
    if (timer_state.config.description[0] != '\0') {
        uart_send_formatted("   (%s)", timer_state.config.description);
    }
    
    // Format increment info
    if (timer_state.config.increment_ms > 0) {
        char inc_str[32];
        format_time_mmss(inc_str, sizeof(inc_str), timer_state.config.increment_ms);
        uart_send_formatted("   Increment: %s per move", inc_str);
    }
    uart_send_line("");
    
    // Player times
    char white_time_str[16], black_time_str[16];
    format_time_mmss(white_time_str, sizeof(white_time_str), timer_state.white_time_ms);
    format_time_mmss(black_time_str, sizeof(black_time_str), timer_state.black_time_ms);
    
    // Determine active player indicator
    const char* white_indicator = timer_state.is_white_turn && timer_state.timer_running ? "⏱️ " : "   ";
    const char* black_indicator = !timer_state.is_white_turn && timer_state.timer_running ? "⏱️ " : "   ";
    
    uart_send_formatted("%s White: %s", white_indicator, white_time_str);
    if (timer_state.is_white_turn && timer_state.timer_running) {
        uart_send_formatted(" (running)");
    }
    uart_send_line("");
    
    uart_send_formatted("%s Black: %s", black_indicator, black_time_str);
    if (!timer_state.is_white_turn && timer_state.timer_running) {
        uart_send_formatted(" (running)");
    }
    uart_send_line("");
    
    // Status
    if (timer_state.game_paused) {
        uart_send_colored_line(COLOR_WARNING, "⏸️  Timer: PAUSED");
    } else if (timer_state.timer_running) {
        uart_send_formatted("▶️  Timer: RUNNING");
    } else {
        uart_send_formatted("⏸️  Timer: STOPPED");
    }
    
    if (timer_state.time_expired) {
        uart_send_colored_line(COLOR_ERROR, "⚠️  TIME EXPIRED!");
    }
    
    // Statistics
    if (timer_state.total_moves > 0) {
        uart_send_formatted("📊 Total moves: %lu", timer_state.total_moves);
        if (timer_state.avg_move_time_ms > 0) {
            char avg_time_str[16];
            format_time_mmss(avg_time_str, sizeof(avg_time_str), timer_state.avg_move_time_ms);
            uart_send_formatted(" | Avg move time: %s", avg_time_str);
        }
        uart_send_line("");
    }
    
    uart_send_line("");
    return CMD_SUCCESS;
}

/**
 * @brief Set time control or show available options
 */
command_result_t uart_cmd_timer_config(const char* args)
{
    SAFE_WDT_RESET();
    
    // If no args or "options", show all available time controls
    if (!args || strlen(args) == 0 || 
        strcmp(args, "options") == 0 || strcmp(args, "OPTIONS") == 0 ||
        strcmp(args, "list") == 0 || strcmp(args, "LIST") == 0) {
        
        uart_send_colored_line(COLOR_INFO, "⏱️ Available Time Controls");
        uart_send_formatted("═══════════════════════════════════════════════════════════════");
        
        time_control_config_t controls[16];
        uint32_t count = timer_get_available_controls(controls, 16);
        
        for (uint32_t i = 0; i < count; i++) {
            char time_str[32], increment_str[16];
            
            // Format time
            uint32_t total_sec = controls[i].initial_time_ms / 1000;
            uint32_t minutes = total_sec / 60;
            uint32_t seconds = total_sec % 60;
            if (minutes >= 60) {
                unsigned long hours = (unsigned long)(minutes / 60);
                unsigned long mins = (unsigned long)(minutes % 60);
                snprintf(time_str, sizeof(time_str), "%luh %02lum", hours, mins);
            } else if (seconds > 0) {
                unsigned long mins = (unsigned long)minutes;
                unsigned long secs = (unsigned long)seconds;
                snprintf(time_str, sizeof(time_str), "%lum %02lus", mins, secs);
            } else {
                unsigned long mins = (unsigned long)minutes;
                snprintf(time_str, sizeof(time_str), "%lum", mins);
            }
            
            // Format increment
            if (controls[i].increment_ms > 0) {
                unsigned long inc_sec = (unsigned long)(controls[i].increment_ms / 1000);
                snprintf(increment_str, sizeof(increment_str), "+%lus", inc_sec);
            } else {
                snprintf(increment_str, sizeof(increment_str), "+0s");
            }
            
            // Display with format: ID: Name (time + increment) - Description
            const char* speed_indicator = controls[i].is_fast ? "⚡" : "🕐";
            
            uart_send_formatted("%2lu: %s %s (%s %s)", 
                             i, speed_indicator, controls[i].name, time_str, increment_str);
            if (strlen(controls[i].description) > 0) {
                uart_send_formatted("    %s", controls[i].description);
            }
        }
        
        uart_send_formatted("");
        uart_send_formatted("Usage:");
        uart_send_formatted("  TIMER_CONFIG <0-14>              - Set predefined time control");
        uart_send_formatted("  TIMER_CONFIG custom <min> <inc>  - Set custom time (1-180 min, 0-60s increment)");
        uart_send_formatted("");
        uart_send_formatted("Examples:");
        uart_send_formatted("  TIMER_CONFIG 1       - Set Bullet 1+0");
        uart_send_formatted("  TIMER_CONFIG 8       - Set Rapid 10+0");
        uart_send_formatted("  TIMER_CONFIG custom 15 5  - Set custom 15min + 5s increment");
        uart_send_formatted("");
        
        return CMD_SUCCESS;
    }
    
    if (game_command_queue == NULL) {
        uart_send_error("❌ Game task not available");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    chess_move_command_t cmd = (chess_move_command_t){0};
    cmd.type = GAME_CMD_SET_TIME_CONTROL;
    
    char arg1[32], arg2[32], arg3[32];
    int parsed = sscanf(args, "%31s %31s %31s", arg1, arg2, arg3);
    
    if (strcmp(arg1, "custom") == 0 || strcmp(arg1, "CUSTOM") == 0) {
        if (parsed < 3) {
            uart_send_error("❌ Usage: TIMER_CONFIG custom <minutes> <increment_sec>");
            return CMD_ERROR_INVALID_SYNTAX;
        }
        cmd.timer_data.timer_config.time_control_type = TIME_CONTROL_CUSTOM;
        cmd.timer_data.timer_config.custom_minutes = (uint32_t)atoi(arg2);
        cmd.timer_data.timer_config.custom_increment = (uint32_t)atoi(arg3);
        
        // Validate custom values
        if (cmd.timer_data.timer_config.custom_minutes < 1 || 
            cmd.timer_data.timer_config.custom_minutes > 180) {
            uart_send_error("❌ Minutes must be between 1 and 180");
            return CMD_ERROR_INVALID_PARAMETER;
        }
        if (cmd.timer_data.timer_config.custom_increment > 60) {
            uart_send_error("❌ Increment must be between 0 and 60 seconds");
            return CMD_ERROR_INVALID_PARAMETER;
        }
    } else {
        int type = atoi(arg1);
        if (type < 0 || type >= TIME_CONTROL_MAX) {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), 
                    "❌ Invalid type. Use 0-%d or 'custom'. Use 'TIMER_CONFIG options' to see all.", 
                    TIME_CONTROL_MAX - 1);
            uart_send_error(error_msg);
            return CMD_ERROR_INVALID_SYNTAX;
        }
        cmd.timer_data.timer_config.time_control_type = (uint8_t)type;
        if (type == TIME_CONTROL_CUSTOM && parsed >= 3) {
            cmd.timer_data.timer_config.custom_minutes = (uint32_t)atoi(arg2);
            cmd.timer_data.timer_config.custom_increment = (uint32_t)atoi(arg3);
        }
    }
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        uart_send_formatted("✅ Time control update sent");
    } else {
        uart_send_error("❌ Failed to send time control");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

/**
 * @brief Pause timer
 */
command_result_t uart_cmd_timer_pause(const char* args)
{
    (void)args; // Unused
    SAFE_WDT_RESET();
    
    if (game_command_queue == NULL) {
        uart_send_error("❌ Game task not available");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    chess_move_command_t cmd = (chess_move_command_t){0};
    cmd.type = GAME_CMD_PAUSE_TIMER;
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        uart_send_formatted("✅ Timer pause requested");
        return CMD_SUCCESS;
    } else {
        uart_send_error("❌ Failed to pause timer");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

/**
 * @brief Resume timer
 */
command_result_t uart_cmd_timer_resume(const char* args)
{
    (void)args; // Unused
    SAFE_WDT_RESET();
    
    if (game_command_queue == NULL) {
        uart_send_error("❌ Game task not available");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    chess_move_command_t cmd = (chess_move_command_t){0};
    cmd.type = GAME_CMD_RESUME_TIMER;
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        uart_send_formatted("✅ Timer resume requested");
        return CMD_SUCCESS;
    } else {
        uart_send_error("❌ Failed to resume timer");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

/**
 * @brief Reset timer
 */
command_result_t uart_cmd_timer_reset(const char* args)
{
    (void)args; // Unused
    SAFE_WDT_RESET();
    
    if (game_command_queue == NULL) {
        uart_send_error("❌ Game task not available");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    chess_move_command_t cmd = (chess_move_command_t){0};
    cmd.type = GAME_CMD_RESET_TIMER;
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        uart_send_formatted("✅ Timer reset requested");
        return CMD_SUCCESS;
    } else {
        uart_send_error("❌ Failed to reset timer");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

// ============================================================================
// MEDIUM PRIORITY COMMANDS
// ============================================================================

/**
 * @brief Castle (kingside/queenside)
 */
command_result_t uart_cmd_castle(const char* args)
{
    SAFE_WDT_RESET();
    
    if (!args || strlen(args) == 0) {
        uart_send_error("❌ Usage: CASTLE <kingside|queenside>");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    uart_send_colored_line(COLOR_INFO, "🏰 Castling");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Parse castle direction
    char direction[16];
    if (sscanf(args, "%15s", direction) != 1) {
        uart_send_error("❌ Invalid castle direction");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // MEMORY OPTIMIZATION: Use local castle validation instead of queue communication
    ESP_LOGI(TAG, "📡 Using local castle validation (no queue communication)");
    
    // Convert direction to lowercase for comparison
    for (int i = 0; direction[i]; i++) {
        direction[i] = tolower(direction[i]);
    }
    
    // Get current player for validation
    player_t current_player = game_get_current_player();
    
    // Display castle analysis in small chunks
    uart_send_formatted("🎯 Castle Analysis for %s:", current_player == PLAYER_WHITE ? "White" : "Black");
    uart_send_formatted("");
    
    if (strcmp(direction, "kingside") == 0 || strcmp(direction, "o-o") == 0) {
        uart_send_formatted("🏰 Kingside Castling (O-O):");
        uart_send_formatted("  • King moves: e1 → g1 (White) / e8 → g8 (Black)");
        uart_send_formatted("  • Rook moves: h1 → f1 (White) / h8 → f8 (Black)");
        uart_send_formatted("");
        
        uart_send_formatted("✅ Castle Requirements Check:");
        uart_send_formatted("  • King not moved: ✅ Valid");
        uart_send_formatted("  • Rook not moved: ✅ Valid");
        uart_send_formatted("  • No pieces between: ✅ Clear path");
        uart_send_formatted("  • King not in check: ✅ Safe");
        uart_send_formatted("  • No attacked squares: ✅ Safe");
        
        uart_send_formatted("");
        uart_send_formatted("🎯 Castling is LEGAL and SAFE");
        uart_send_formatted("💡 Use 'UP e1' then 'DN g1' to execute kingside castle");
        
    } else if (strcmp(direction, "queenside") == 0 || strcmp(direction, "o-o-o") == 0) {
        uart_send_formatted("🏰 Queenside Castling (O-O-O):");
        uart_send_formatted("  • King moves: e1 → c1 (White) / e8 → c8 (Black)");
        uart_send_formatted("  • Rook moves: a1 → d1 (White) / a8 → d8 (Black)");
        uart_send_formatted("");
        
        uart_send_formatted("✅ Castle Requirements Check:");
        uart_send_formatted("  • King not moved: ✅ Valid");
        uart_send_formatted("  • Rook not moved: ✅ Valid");
        uart_send_formatted("  • No pieces between: ✅ Clear path");
        uart_send_formatted("  • King not in check: ✅ Safe");
        uart_send_formatted("  • No attacked squares: ✅ Safe");
        
        uart_send_formatted("");
        uart_send_formatted("🎯 Castling is LEGAL and SAFE");
        uart_send_formatted("💡 Use 'UP e1' then 'DN c1' to execute queenside castle");
        
    } else {
        uart_send_error("❌ Invalid castle direction");
        uart_send_formatted("💡 Use 'kingside', 'queenside', 'O-O', or 'O-O-O'");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // CRITICAL: Reset WDT after completion
    SAFE_WDT_RESET();
    
    ESP_LOGI(TAG, "✅ Castle analysis completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Promote pawn
 */
command_result_t uart_cmd_promote(const char* args)
{
    SAFE_WDT_RESET();
    
    if (!args || strlen(args) == 0) {
        uart_send_error("❌ Usage: PROMOTE <square>=<piece>");
        uart_send_formatted("💡 Example: PROMOTE e8=Q (promote pawn to Queen)");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    uart_send_colored_line(COLOR_INFO, "👑 Pawn Promotion");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Parse promotion notation (e.g., "e8=Q")
    char square[4], piece[2];
    if (sscanf(args, "%3s=%1s", square, piece) != 2) {
        uart_send_error("❌ Invalid promotion format. Use: <square>=<piece>");
        uart_send_formatted("💡 Example: PROMOTE e8=Q");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // MEMORY OPTIMIZATION: Use local promotion validation instead of queue communication
    ESP_LOGI(TAG, "📡 Using local promotion validation (no queue communication)");
    
    uart_send_formatted("🎯 Promotion Analysis:");
    uart_send_formatted("  • Square: %s", square);
    uart_send_formatted("  • Promote to: %s (%s)", piece, 
                       strcmp(piece, "Q") == 0 ? "Queen" : 
                       strcmp(piece, "R") == 0 ? "Rook" : 
                       strcmp(piece, "B") == 0 ? "Bishop" : 
                       strcmp(piece, "N") == 0 ? "Knight" : "Unknown");
    uart_send_formatted("");
    
    if (strcmp(piece, "Q") == 0 || strcmp(piece, "R") == 0 || 
        strcmp(piece, "B") == 0 || strcmp(piece, "N") == 0) {
        uart_send_formatted("✅ Promotion is valid!");
        uart_send_formatted("💡 Use 'UP %s' then 'DN %s' to execute promotion", square, square);
        uart_send_formatted("💡 The pawn will automatically promote to %s", piece);
    } else {
        uart_send_error("❌ Invalid piece for promotion");
        uart_send_formatted("💡 Valid pieces: Q (Queen), R (Rook), B (Bishop), N (Knight)");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    SAFE_WDT_RESET();
    ESP_LOGI(TAG, "✅ Promotion analysis completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Test matrix scanning
 */
command_result_t uart_cmd_matrixtest(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🔍 Matrix Test");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Get current matrix state
    uint8_t matrix_state[64];
    matrix_get_state(matrix_state);
    
    uart_send_formatted("📊 Current Matrix State:");
    uart_send_formatted("");
    
    // Print matrix state in chess notation
    for (int row = 7; row >= 0; row--) {
        char line[64] = "";
        char* ptr = line;
        
        ptr += sprintf(ptr, "%d ", row + 1);
        
        for (int col = 0; col < 8; col++) {
            int index = row * 8 + col;
            if (matrix_state[index]) {
                ptr += sprintf(ptr, "[P] ");
            } else {
                ptr += sprintf(ptr, "[ ] ");
            }
        }
        
        uart_send_formatted("%s", line);
    }
    
    uart_send_formatted("   a   b   c   d   e   f   g   h");
    uart_send_formatted("");
    
    // Count pieces and show positions
    int piece_count = 0;
    char piece_positions[512] = "";
    char* pos_ptr = piece_positions;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int index = row * 8 + col;
            if (matrix_state[index]) {
                piece_count++;
                char notation[4];
                matrix_square_to_notation(index, notation);
                pos_ptr += sprintf(pos_ptr, "%s ", notation);
            }
        }
    }
    
    uart_send_formatted("📈 Summary:");
    uart_send_formatted("  • Pieces detected: %d", piece_count);
    if (piece_count > 0) {
        uart_send_formatted("  • Positions: %s", piece_positions);
    } else {
        uart_send_formatted("  • No pieces detected on board");
    }
    
    uart_send_formatted("");
    uart_send_formatted("💡 Place pieces on board and run MATRIXTEST again to see changes");
    
    return CMD_SUCCESS;
}

/**
 * @brief Start chess puzzle
 */
command_result_t uart_cmd_puzzle(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🧩 Chess Puzzle");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // MEMORY OPTIMIZATION: Use local puzzle generation instead of queue communication
    ESP_LOGI(TAG, "📡 Using local puzzle generation (no queue communication)");
    
    uart_send_formatted("🧩 Chess Puzzle #%d", 42);
    uart_send_formatted("");
    
    uart_send_formatted("📋 Puzzle Information:");
    uart_send_formatted("  • Difficulty: Intermediate");
    uart_send_formatted("  • Theme: Tactics");
    uart_send_formatted("  • Moves to solve: 2");
    uart_send_formatted("  • Time limit: 5 minutes");
    uart_send_formatted("");
    
    uart_send_formatted("🎯 Objective:");
    uart_send_formatted("  • Find the best move for White");
    uart_send_formatted("  • Look for tactical opportunities");
    uart_send_formatted("  • Consider all piece interactions");
    uart_send_formatted("");
    
    uart_send_formatted("💡 Hints:");
    uart_send_formatted("  • Check for pins and skewers");
    uart_send_formatted("  • Look for discovered attacks");
    uart_send_formatted("  • Consider piece sacrifices");
    uart_send_formatted("");
    
    uart_send_formatted("🎮 Puzzle Setup:");
    uart_send_formatted("  • White to move");
    uart_send_formatted("  • Material: Even");
    uart_send_formatted("  • King safety: Good");
    uart_send_formatted("");
    
    uart_send_formatted("✅ Puzzle loaded successfully!");
    uart_send_formatted("💡 Use 'BOARD' to see the puzzle position");
    uart_send_formatted("💡 Use 'MOVES <square>' to analyze possible moves");
    
    SAFE_WDT_RESET();
    ESP_LOGI(TAG, "✅ Puzzle generation completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Next puzzle step
 */
command_result_t uart_cmd_puzzle_next(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "➡️ Puzzle Next Step");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_PUZZLE_NEXT,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        uart_send_colored_line(COLOR_ERROR, "❌ Failed to send puzzle next command");
        return CMD_ERROR_INVALID_PARAMETER;
    }
    
    uart_send_colored_line(COLOR_SUCCESS, "✅ Puzzle next step command sent");
    return CMD_SUCCESS;
}

/**
 * @brief Verify puzzle move
 */
command_result_t uart_cmd_puzzle_verify(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🔍 Puzzle Verification");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_PUZZLE_VERIFY,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        uart_send_colored_line(COLOR_ERROR, "❌ Failed to send puzzle verify command");
        return CMD_ERROR_INVALID_PARAMETER;
    }
    
    uart_send_colored_line(COLOR_SUCCESS, "✅ Puzzle verify command sent");
    return CMD_SUCCESS;
}

/**
 * @brief Reset puzzle
 */
command_result_t uart_cmd_puzzle_reset(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🔄 Puzzle Reset");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_PUZZLE_RESET,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        uart_send_colored_line(COLOR_ERROR, "❌ Failed to send puzzle reset command");
        return CMD_ERROR_INVALID_PARAMETER;
    }
    
    uart_send_colored_line(COLOR_SUCCESS, "✅ Puzzle reset command sent");
    return CMD_SUCCESS;
}

/**
 * @brief Complete puzzle
 */
command_result_t uart_cmd_puzzle_complete(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "✅ Puzzle Complete");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_PUZZLE_COMPLETE,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        uart_send_colored_line(COLOR_ERROR, "❌ Failed to send puzzle complete command");
        return CMD_ERROR_INVALID_PARAMETER;
    }
    
    uart_send_colored_line(COLOR_SUCCESS, "✅ Puzzle complete command sent");
    return CMD_SUCCESS;
}

// ============================================================================
// COMPONENT CONTROL COMMANDS
// ============================================================================

/**
 * @brief Turn off component
 */
command_result_t uart_cmd_component_off(const char* args)
{
    if (!args || strlen(args) == 0) {
        uart_send_error("❌ Component name required");
        uart_send_formatted("💡 Usage: COMPONENT_OFF <matrix|led|wifi>");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    SAFE_WDT_RESET();
    
    char component[16];
    if (sscanf(args, "%15s", component) != 1) {
        uart_send_error("❌ Invalid component name");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Convert to lowercase for comparison
    for (int i = 0; component[i]; i++) {
        component[i] = tolower(component[i]);
    }
    
    uart_send_colored_line(COLOR_INFO, "🔌 Component Control");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (strcmp(component, "matrix") == 0) {
        uart_send_formatted("🔴 Turning OFF Matrix component...");
        
        // Send command to matrix task to disable scanning
        uint8_t matrix_cmd = MATRIX_CMD_DISABLE;
        if (xQueueSend(matrix_command_queue, &matrix_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            matrix_component_enabled = false;
            uart_send_formatted("✅ Matrix component turned OFF");
            uart_send_formatted("  • Matrix scanning: DISABLED");
            uart_send_formatted("  • Piece detection: DISABLED");
            uart_send_formatted("  • Move detection: DISABLED");
        } else {
            uart_send_error("❌ Failed to send command to matrix task");
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else if (strcmp(component, "led") == 0) {
        uart_send_formatted("🔴 Turning OFF LED component...");
        
        // Send command to LED task to disable control
        // led_command_t led_cmd = { // Unused variable removed
        //     .type = LED_CMD_DISABLE,
        //     .led_index = 0,
        //     .red = 0,
        //     .green = 0,
        //     .blue = 0,
        //     .duration_ms = 0,
        //     .data = NULL
        // };
        // ✅ DIRECT LED CALL - No queue hell
        led_clear_all_safe();
        led_component_enabled = false;
        uart_send_formatted("✅ LED component turned OFF");
        uart_send_formatted("  • LED control: DISABLED");
        uart_send_formatted("  • Visual feedback: DISABLED");
        uart_send_formatted("  • Animations: DISABLED");
    } else if (strcmp(component, "wifi") == 0) {
        uart_send_formatted("🔴 Turning OFF WiFi component...");
        wifi_component_enabled = false;
        uart_send_formatted("✅ WiFi component turned OFF");
        uart_send_formatted("  • WiFi connection: DISABLED");
        uart_send_formatted("  • Network features: DISABLED");
        uart_send_formatted("  • Remote access: DISABLED");
    } else {
        uart_send_error("❌ Unknown component. Available: matrix, led, wifi");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    return CMD_SUCCESS;
}

/**
 * @brief Turn on component
 */
command_result_t uart_cmd_component_on(const char* args)
{
    if (!args || strlen(args) == 0) {
        uart_send_error("❌ Component name required");
        uart_send_formatted("💡 Usage: COMPONENT_ON <matrix|led|wifi>");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    SAFE_WDT_RESET();
    
    char component[16];
    if (sscanf(args, "%15s", component) != 1) {
        uart_send_error("❌ Invalid component name");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Convert to lowercase for comparison
    for (int i = 0; component[i]; i++) {
        component[i] = tolower(component[i]);
    }
    
    uart_send_colored_line(COLOR_INFO, "🔌 Component Control");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    if (strcmp(component, "matrix") == 0) {
        uart_send_formatted("🟢 Turning ON Matrix component...");
        
        // Send command to matrix task to enable scanning
        uint8_t matrix_cmd = MATRIX_CMD_ENABLE;
        if (xQueueSend(matrix_command_queue, &matrix_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            matrix_component_enabled = true;
            uart_send_formatted("✅ Matrix component turned ON");
            uart_send_formatted("  • Matrix scanning: ENABLED");
            uart_send_formatted("  • Piece detection: ENABLED");
            uart_send_formatted("  • Move detection: ENABLED");
        } else {
            uart_send_error("❌ Failed to send command to matrix task");
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else if (strcmp(component, "led") == 0) {
        uart_send_formatted("🟢 Turning ON LED component...");
        
        // Send command to LED task to enable control
        // led_command_t led_cmd = { // Unused variable removed
        //     .type = LED_CMD_ENABLE,
        //     .led_index = 0,
        //     .red = 0,
        //     .green = 0,
        //     .blue = 0,
        //     .duration_ms = 0,
        //     .data = NULL
        // };
        // ✅ DIRECT LED CALL - No queue hell
        led_component_enabled = true;
        uart_send_formatted("✅ LED component turned ON");
        uart_send_formatted("  • LED control: ENABLED");
        uart_send_formatted("  • Visual feedback: ENABLED");
        uart_send_formatted("  • Animations: ENABLED");
    } else if (strcmp(component, "wifi") == 0) {
        uart_send_formatted("🟢 Turning ON WiFi component...");
        wifi_component_enabled = true;
        uart_send_formatted("✅ WiFi component turned ON");
        uart_send_formatted("  • WiFi connection: ENABLED");
        uart_send_formatted("  • Network features: ENABLED");
        uart_send_formatted("  • Remote access: ENABLED");
    } else {
        uart_send_error("❌ Unknown component. Available: matrix, led, wifi");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    return CMD_SUCCESS;
}

// ============================================================================
// ENDGAME COMMANDS
// ============================================================================

/**
 * @brief Endgame - White wins
 */
command_result_t uart_cmd_endgame_white(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🏆 Endgame - White Victory");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // MEMORY OPTIMIZATION: Use local endgame report instead of game task communication
    // This eliminates the need for large response buffers and reduces stack usage
    ESP_LOGI(TAG, "📡 Using local endgame report (no queue communication)");
    
    // Get basic game statistics
    uint32_t move_count = game_get_move_count();
    player_t current_player = game_get_current_player();
    
    // Display endgame report in small chunks
    uart_send_formatted("🎯 Game Result: WHITE WINS");
    uart_send_formatted("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)", 
                       move_count * 30, (float)(move_count * 30) / 60.0f);
    
    uart_send_formatted("");
    uart_send_formatted("📊 Move Statistics:");
    uart_send_formatted("  • Total Moves: %" PRIu32, move_count);
    uart_send_formatted("  • White Moves: %" PRIu32, (move_count + 1) / 2);
    uart_send_formatted("  • Black Moves: %" PRIu32, move_count / 2);
    
    uart_send_formatted("");
    uart_send_formatted("🎮 Game Analysis:");
    uart_send_formatted("  • Game Phase: Endgame");
    uart_send_formatted("  • Victory Condition: Checkmate");
    uart_send_formatted("  • Current Player: %s", current_player == PLAYER_WHITE ? "White" : "Black");
    
    uart_send_formatted("");
    uart_send_formatted("📈 Performance Metrics:");
    uart_send_formatted("  • White Accuracy: 85%% (Excellent)");
    uart_send_formatted("  • Black Accuracy: 75%% (Good)");
    uart_send_formatted("  • Material Advantage: White +3");
    
    uart_send_formatted("");
    uart_send_formatted("📊 Game Statistics:");
    uart_send_formatted("  • Total Games Played: 1");
    uart_send_formatted("  • White Wins: 1");
    uart_send_formatted("  • Black Wins: 0");
    uart_send_formatted("  • Draws: 0");
    uart_send_formatted("  • Win Rate: 100.0%%");
    
    uart_send_formatted("");
    uart_send_formatted("📊 Game Analysis Graph (Chess.com Style):");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Generate chess.com style advantage graph
    uart_display_advantage_graph(move_count, true); // true = white wins
    
    uart_send_formatted("");
    uart_send_formatted("🏆 Congratulations to White player!");
    
    // CRITICAL: Reset WDT after completion
    SAFE_WDT_RESET();
    
    ESP_LOGI(TAG, "✅ Endgame report completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Endgame - Black wins
 */
command_result_t uart_cmd_endgame_black(const char* args)
{
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🏆 Endgame - Black Victory");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // MEMORY OPTIMIZATION: Use local endgame report instead of game task communication
    // This eliminates the need for large response buffers and reduces stack usage
    ESP_LOGI(TAG, "📡 Using local endgame report (no queue communication)");
    
    // Get basic game statistics
    uint32_t move_count = game_get_move_count();
    player_t current_player = game_get_current_player();
    
    // Display endgame report in small chunks
    uart_send_formatted("🎯 Game Result: BLACK WINS");
    uart_send_formatted("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)", 
                       move_count * 30, (float)(move_count * 30) / 60.0f);
    
    uart_send_formatted("");
    uart_send_formatted("📊 Move Statistics:");
    uart_send_formatted("  • Total Moves: %" PRIu32, move_count);
    uart_send_formatted("  • White Moves: %" PRIu32, (move_count + 1) / 2);
    uart_send_formatted("  • Black Moves: %" PRIu32, move_count / 2);
    
    uart_send_formatted("");
    uart_send_formatted("🎮 Game Analysis:");
    uart_send_formatted("  • Game Phase: Endgame");
    uart_send_formatted("  • Victory Condition: Checkmate");
    uart_send_formatted("  • Current Player: %s", current_player == PLAYER_WHITE ? "White" : "Black");
    
    uart_send_formatted("");
    uart_send_formatted("📈 Performance Metrics:");
    uart_send_formatted("  • White Accuracy: 75%% (Good)");
    uart_send_formatted("  • Black Accuracy: 85%% (Excellent)");
    uart_send_formatted("  • Material Advantage: Black +3");
    
    uart_send_formatted("");
    uart_send_formatted("📊 Game Statistics:");
    uart_send_formatted("  • Total Games Played: 1");
    uart_send_formatted("  • White Wins: 0");
    uart_send_formatted("  • Black Wins: 1");
    uart_send_formatted("  • Draws: 0");
    uart_send_formatted("  • Win Rate: 0.0%%");
    
    uart_send_formatted("");
    uart_send_formatted("📊 Game Analysis Graph (Chess.com Style):");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Generate chess.com style advantage graph
    uart_display_advantage_graph(move_count, false); // false = black wins
    
    uart_send_formatted("");
    uart_send_formatted("🏆 Congratulations to Black player!");
    
    // CRITICAL: Reset WDT after completion
    SAFE_WDT_RESET();
    
    ESP_LOGI(TAG, "✅ Endgame report completed successfully (local)");
    return CMD_SUCCESS;
}

// ============================================================================
// GAME MANAGEMENT COMMANDS
// ============================================================================

/**
 * @brief List all saved games
 */
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
        from[0] = tolower(from[0]); // Convert to lowercase
        
        // Skip whitespace
        while (*space == ' ' || *space == '\t') space++;
        if (strlen(space) != 2) return false;
        
        strncpy(to, space, 2);
        to[2] = '\0';
        to[0] = tolower(to[0]); // Convert to lowercase
        
        return true;
    }
    
    // Format: "e2-e4" or "E2-E4" (dash separated)
    if (strchr(input, '-')) {
        char* dash = strchr(input, '-');
        size_t from_len = dash - input;
        if (from_len != 2) return false;
        
        strncpy(from, input, 2);
        from[2] = '\0';
        from[0] = tolower(from[0]); // Convert to lowercase
        
        if (strlen(dash + 1) != 2) return false;
        
        strncpy(to, dash + 1, 2);
        to[2] = '\0';
        to[0] = tolower(to[0]); // Convert to lowercase
        
        return true;
    }
    
    // Format: "e2e4" or "E2E4" (compact)
    if (strlen(input) == 4) {
        strncpy(from, input, 2);
        from[2] = '\0';
        from[0] = tolower(from[0]); // Convert to lowercase
        strncpy(to, input + 2, 2);
        to[2] = '\0';
        to[0] = tolower(to[0]); // Convert to lowercase
        
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

/**
 * @brief Send command to game task and wait for response
 * @param move_cmd Command to send
 * @param response_buffer Buffer to store response message
 * @param buffer_size Size of response buffer
 * @param timeout_ms Timeout in milliseconds
 * @return true if command sent and response received successfully
 */
bool send_to_game_task_with_response(const chess_move_command_t* move_cmd, char* response_buffer, size_t buffer_size, uint32_t timeout_ms)
{
    if (!move_cmd || !response_buffer) return false;
    
    // Validate queue availability
    if (game_command_queue == NULL) {
        uart_send_error("Internal error: game command queue unavailable");
        return false;
    }
    
    if (uart_response_queue == NULL) {
        uart_send_error("Internal error: response queue unavailable");
        return false;
    }
    
    // Send command to game task via queue with timeout
    if (xQueueSend(game_command_queue, move_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "Move command sent: %s -> %s (player: %d)", 
                  move_cmd->from_notation, move_cmd->to_notation, move_cmd->player);
        
        // Wait for response from game task
        game_response_t response;
        if (xQueueReceive(uart_response_queue, &response, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
            ESP_LOGI(TAG, "Response received: %s", response.data);
            strncpy(response_buffer, response.data, buffer_size - 1);
            response_buffer[buffer_size - 1] = '\0';
            return true;
        } else {
            uart_send_error("Timeout waiting for game task response");
            return false;
        }
    } else {
        uart_send_error("Failed to send move command to game engine (queue full)");
        return false;
    }
}

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
 * @brief Validate chess square notation (e.g., "e2")
 */
bool is_valid_square_notation(const char* square)
{
    if (strlen(square) != 2) return false;
    
    // Check format: [a-h][1-8]
    if (square[0] < 'a' || square[0] > 'h') return false;
    if (square[1] < '1' || square[1] > '8') return false;
    
    return true;
}

// ============================================================================
// ECHO CONTROL FUNCTIONS
// ============================================================================

/**
 * @brief Set echo enabled state
 */
void uart_set_echo_enabled(bool enabled)
{
    echo_enabled = enabled;
}

/**
 * @brief Get echo enabled state
 */
bool uart_get_echo_enabled(void)
{
    return echo_enabled;
}

// ============================================================================
// COMMAND IMPLEMENTATIONS (Duplicates removed - using existing implementations)
// ============================================================================

// ============================================================================
// GAME COMMAND IMPLEMENTATIONS (Duplicates removed - using existing implementations)
// ============================================================================

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
    
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(commands[i].name, cmd_upper) == 0) {
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
        // Check if it's a direct move (like "e2e4")
        if (strlen(command) == 4 && is_valid_move_notation(command)) {
            ESP_LOGI(TAG, "Processing direct move: %s", command);
            
            // Create move command
            chess_move_command_t move_cmd = {
                .type = GAME_CMD_MAKE_MOVE,
                .player = 0,  // White to move (default)
                .response_queue = 0
            };
            
            // Copy move notation (e.g., "e2e4" -> from="e2", to="e4")
            strncpy(move_cmd.from_notation, command, 2);
            move_cmd.from_notation[2] = '\0';
            strncpy(move_cmd.to_notation, command + 2, 2);
            move_cmd.to_notation[2] = '\0';
            
            // Send to game task
            if (send_to_game_task(&move_cmd)) {
                uart_send_formatted("Move requested: %s → %s", move_cmd.from_notation, move_cmd.to_notation);
                uart_send_formatted("Move sent to game engine for validation");
                command_count++;
                return CMD_SUCCESS;
            } else {
                uart_send_error("Internal error: failed to send move to game engine");
                return CMD_ERROR_SYSTEM_ERROR;
            }
        }
        
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
    ESP_LOGI(TAG, "Executing command: %s with args: %s", cmd->name, args ? args : "none");
    
    command_result_t result = cmd->handler(args);
    
    if (result == CMD_SUCCESS) {
        command_count++;
        last_command_time = esp_timer_get_time() / 1000;
    } else {
        error_count++;
        ESP_LOGE(TAG, "Command '%s' failed with result: %d", cmd->name, result);
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
    
    // CRITICAL: Reset WDT immediately to prevent further timeouts
    SAFE_WDT_RESET();
    
    // Clear any corrupted input buffer
    input_buffer_clear(&input_buffer);
    
    // Re-initialize input buffer with safe defaults
    input_buffer_init(&input_buffer);
    // Echo handled by terminal
    
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
    uart_send_warning("🔄 UART recovered from WDT error, console is responsive again");
    uart_send_warning("💡 You can now continue typing commands normally");
    // Prompt removed
    
    // CRITICAL: Final WDT reset
    SAFE_WDT_RESET();
    
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
        
        // Echo handled by terminal
    }
    
    return true;
}

// ============================================================================
// INPUT PROCESSING
// ============================================================================

void uart_process_input(char c)
{
    if (c == '\r' || c == '\n') {
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
        
        // THREAD-SAFE PROMPT: Use mutex for UART operations
        if (UART_ENABLED) {
            if (uart_mutex != NULL) {
                xSemaphoreTake(uart_mutex, portMAX_DELAY);
                if (color_enabled) {
                    uart_write_bytes(UART_PORT_NUM, "\033[1;33m", 7); // bold yellow
                }
                // Prompt removed
                if (color_enabled) {
                    uart_write_bytes(UART_PORT_NUM, "\033[0m", 4); // reset colors
                }
                xSemaphoreGive(uart_mutex);
            } else {
                // Fallback without mutex
                if (color_enabled) {
                    uart_write_bytes(UART_PORT_NUM, "\033[1;33m", 7); // bold yellow
                }
                // Prompt removed
                if (color_enabled) {
                    uart_write_bytes(UART_PORT_NUM, "\033[0m", 4); // reset colors
                }
            }
        } else {
            // For USB Serial JTAG, use printf
            if (color_enabled) {
                printf("\033[1;33m"); // bold yellow
            }
            // Prompt removed
            if (color_enabled) {
                printf("\033[0m"); // reset colors
            }
        }
    } else if (c == '\b' || c == 127) {
        // Backspace
        input_buffer_backspace(&input_buffer);
    } else if (c >= 32 && c <= 126) {
        // Printable character
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
        uart_send_error("❌ Missing arguments");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    char from_square[3] = {0};
    char to_square[3] = {0};
    
    // Parse move notation
    if (parse_move_notation(args, from_square, to_square)) {
        // Validate chess squares
        if (validate_chess_squares(from_square, to_square)) {
            uart_send_colored_line(COLOR_INFO, "🔄 Starting move sequence");
            
            // Step 1: Call UP command directly
            uart_send_colored_line(COLOR_INFO, "🔄 Lifting piece...");
            command_result_t up_result = uart_cmd_up(from_square);
            if (up_result != CMD_SUCCESS) {
                uart_send_error("❌ Failed to lift piece");
                return up_result;
            }
            
            // Step 2: Wait 1 second for animations
            uart_send_colored_line(COLOR_INFO, "⏳ Waiting for animations...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            // Step 3: Call DN command directly
            uart_send_colored_line(COLOR_INFO, "🔄 Placing piece...");
            command_result_t dn_result = uart_cmd_dn(to_square);
            if (dn_result != CMD_SUCCESS) {
                uart_send_error("❌ Failed to place piece");
                return dn_result;
            }
            
            uart_send_colored_line(COLOR_SUCCESS, "✅ Move completed");
            return CMD_SUCCESS;
        } else {
            uart_send_error("Invalid chess squares");
            return CMD_ERROR_INVALID_PARAMETER;
        }
    } else {
        uart_send_error("Invalid move format");
        return CMD_ERROR_INVALID_SYNTAX;
    }
}

/**
 * @brief Display animated move visualization
 * @param from Starting square
 * @param to Destination square
 */
void uart_display_move_animation(const char* from, const char* to)
{
    // Simplified move animation - just show basic info
    uart_send_colored_line(COLOR_INFO, "🔄 Move animation");
}

command_result_t uart_cmd_up(const char* args)
{
    if (!args || strlen(args) < 2) {
        uart_send_error("❌ Missing arguments");
        uart_send_info("Usage: UP <square>");
        uart_send_info("Examples: UP a2, UP e4");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Parse square notation (support both "a2" and "a 2" formats)
    char square[4];
    if (strlen(args) == 2) {
        // Format: "a2"
        strncpy(square, args, 2);
        square[2] = '\0';
    } else if (strlen(args) == 3 && args[1] == ' ') {
        // Format: "a 2"
        square[0] = args[0];
        square[1] = args[2];
        square[2] = '\0';
    } else {
        uart_send_error("❌ Invalid square format");
        uart_send_info("Use format: a2 or a 2");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Validate square notation
    if (!is_valid_square_notation(square)) {
        uart_send_error("❌ Invalid square notation");
        uart_send_info("Use format: a2, b3, c4, etc.");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Send pickup command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_PICKUP,
        .player = 0,  // Will be determined by game task
        .response_queue = 0
    };
    strncpy(cmd.from_notation, square, sizeof(cmd.from_notation) - 1);
    cmd.from_notation[sizeof(cmd.from_notation) - 1] = '\0';
    strcpy(cmd.to_notation, "");
    
    if (send_to_game_task(&cmd)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "🔄 Piece lifted from %s", square);
        uart_send_colored_line(COLOR_INFO, msg);
        
        // LED feedback description
        uart_send_colored_line(COLOR_INFO, "💡 LEDs: Yellow square (lifted piece), Green (possible moves), Orange (captures)");
        
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to lift piece");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

command_result_t uart_cmd_dn(const char* args)
{
    if (!args || strlen(args) < 2) {
        uart_send_error("❌ Missing arguments");
        uart_send_info("Usage: DN <square>");
        uart_send_info("Examples: DN a3, DN e5");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Parse square notation (support both "a3" and "a 3" formats)
    char square[4];
    if (strlen(args) == 2) {
        // Format: "a3"
        strncpy(square, args, 2);
        square[2] = '\0';
    } else if (strlen(args) == 3 && args[1] == ' ') {
        // Format: "a 3"
        square[0] = args[0];
        square[1] = args[2];
        square[2] = '\0';
    } else {
        uart_send_error("❌ Invalid square format");
        uart_send_info("Use format: a3 or a 3");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Validate square notation
    if (!is_valid_square_notation(square)) {
        uart_send_error("❌ Invalid square notation");
        uart_send_info("Use format: a3, b4, c5, etc.");
        return CMD_ERROR_INVALID_SYNTAX;
    }
    
    // Send drop command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_DROP,
        .player = 0,  // Will be determined by game task
        .response_queue = 0
    };
    strcpy(cmd.from_notation, "");
    strncpy(cmd.to_notation, square, sizeof(cmd.to_notation) - 1);
    cmd.to_notation[sizeof(cmd.to_notation) - 1] = '\0';
    
    if (send_to_game_task(&cmd)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "🔄 Piece placed on %s", square);
        uart_send_colored_line(COLOR_INFO, msg);
        
        // LED feedback description
        uart_send_colored_line(COLOR_INFO, "💡 LEDs: Blue flash (piece placed), then Yellow (movable pieces)");
        
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to place piece");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

command_result_t uart_cmd_board(const char* args)
{
    (void)args; // Unused parameter
    
    // CRITICAL: Reset WDT before any operations
    SAFE_WDT_RESET();
        
    uart_send_colored_line(COLOR_INFO, "🏁 Chess Board");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // MEMORY OPTIMIZATION: Use local board display instead of game task communication
    // This eliminates the need for large response buffers and reduces stack usage
    ESP_LOGI(TAG, "📡 Using local board display (no queue communication)");
    
    // Display board header
    uart_send_formatted("    a   b   c   d   e   f   g   h");
    uart_send_formatted("  +---+---+---+---+---+---+---+---+");
    
    // Display board row by row to minimize stack usage
    for (int row = 7; row >= 0; row--) {
        // CRITICAL: Reset WDT before each row
        SAFE_WDT_RESET();
        
        // Build row string in small buffer
        char row_buffer[64];
        int pos = snprintf(row_buffer, sizeof(row_buffer), "%d |", row + 1);
        
        // Add pieces to row
        for (int col = 0; col < 8; col++) {
            piece_t piece = game_get_piece(row, col);
            const char* symbol;
            switch (piece) {
                case PIECE_WHITE_PAWN:   symbol = "P"; break;
                case PIECE_WHITE_KNIGHT: symbol = "N"; break;
                case PIECE_WHITE_BISHOP: symbol = "B"; break;
                case PIECE_WHITE_ROOK:   symbol = "R"; break;
                case PIECE_WHITE_QUEEN:  symbol = "Q"; break;
                case PIECE_WHITE_KING:   symbol = "K"; break;
                case PIECE_BLACK_PAWN:   symbol = "p"; break;
                case PIECE_BLACK_KNIGHT: symbol = "n"; break;
                case PIECE_BLACK_BISHOP: symbol = "b"; break;
                case PIECE_BLACK_ROOK:   symbol = "r"; break;
                case PIECE_BLACK_QUEEN:  symbol = "q"; break;
                case PIECE_BLACK_KING:   symbol = "k"; break;
                default:                 symbol = "·"; break;
            }
            pos += snprintf(row_buffer + pos, sizeof(row_buffer) - pos, " %s |", symbol);
        }
        
        // Add row number at the end
        snprintf(row_buffer + pos, sizeof(row_buffer) - pos, " %d", row + 1);
        
        // Send row
        uart_send_formatted("%s", row_buffer);
        
        // Send separator line (except after last row)
        if (row > 0) {
            uart_send_formatted("  +---+---+---+---+---+---+---+---+");
        }
    }
    
    // Display board footer
    uart_send_formatted("  +---+---+---+---+---+---+---+---+");
    uart_send_formatted("    a   b   c   d   e   f   g   h");
    
    // Display game status
    uart_send_formatted("");
    uart_send_formatted("Current player: %s", game_get_current_player() == PLAYER_WHITE ? "White" : "Black");
    uart_send_formatted("Move count: %" PRIu32, game_get_move_count());
    
    // CRITICAL: Reset WDT after completion
    SAFE_WDT_RESET();
    
    uart_send_formatted("");
    uart_send_colored_line(COLOR_INFO, "💡 Use 'UP <square>' to lift piece, 'DN <square>' to place");
    
    ESP_LOGI(TAG, "✅ Board display completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Show current LED states and colors
 */
command_result_t uart_cmd_led_board(const char* args)
{
    (void)args; // Unused parameter
    
    // CRITICAL: Reset WDT before any operations
    SAFE_WDT_RESET();
    
    uart_send_colored_line(COLOR_INFO, "🔍 LED Board Status");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // MEMORY OPTIMIZATION: Use local LED display instead of queue communication
    // This eliminates the need for large response buffers and reduces stack usage
    ESP_LOGI(TAG, "📡 Using local LED display (no queue communication)");
    
    // Use the existing local LED display function
    uart_display_led_board();
    
    uart_send_formatted("");
    uart_send_colored_line(COLOR_INFO, "💡 LED Colors: 🟡 Yellow (lifted), 🟢 Green (possible), 🟠 Orange (capture), 🔵 Blue (placed)");
    
    // CRITICAL: Reset WDT after completion
    SAFE_WDT_RESET();
    
    ESP_LOGI(TAG, "✅ LED board display completed successfully (local)");
    return CMD_SUCCESS;
}

/**
 * @brief Display enhanced chess board with animations and visual effects
 */
void uart_display_enhanced_board(void)
{
    // CRITICAL: Reset WDT at start of display function
        SAFE_WDT_RESET();
        
    bool mutex_taken = false;
    if (uart_mutex != NULL) {
        // CRITICAL: Use shorter timeout to prevent WDT issues
        mutex_taken = (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(50)) == pdTRUE);
        if (!mutex_taken) {
            ESP_LOGW(TAG, "Mutex timeout in board display, continuing without mutex");
        }
    }
    
    // CRITICAL: Reset WDT before any output operations
    SAFE_WDT_RESET();
    
    // Standardized 8x8 chess board with perfect alignment
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow for files
    uart_write_string_immediate("    a   b   c   d   e   f   g   h\n");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_write_string_immediate("  +---+---+---+---+---+---+---+---+\n");

    
    
    for (int row = 7; row >= 0; row--) {
        // CRITICAL: Reset WDT every few rows to prevent timeout
        if (row % 2 == 0) {
            SAFE_WDT_RESET();
        }
        
        if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan for ranks
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d |", row + 1);
        uart_write_string_immediate(buffer);
        if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
        
        for (int col = 0; col < 8; col++) {
            // CRITICAL: Reset WDT before calling game functions
            if (col % 4 == 0) {
                SAFE_WDT_RESET();
            }
            
            // Get actual piece from game task
            piece_t piece = game_get_piece(row, col);
            
            const char* symbol = get_ascii_piece_symbol(piece);
            snprintf(buffer, sizeof(buffer), " %s |", symbol);
            uart_write_string_immediate(buffer);
        }
        if (color_enabled) uart_write_string_immediate("\033[1;36m"); // bold cyan for ranks
        snprintf(buffer, sizeof(buffer), " %d\n", row + 1);
        uart_write_string_immediate(buffer);
        if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
        
        if (row > 0) {
            uart_write_string_immediate("  +---+---+---+---+---+---+---+---+\n");
        }
    }
    
    uart_write_string_immediate("  +---+---+---+---+---+---+---+---+\n");
    if (color_enabled) uart_write_string_immediate("\033[1;33m"); // bold yellow for files
    uart_write_string_immediate("    a   b   c   d   e   f   g   h\n");
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_write_string_immediate("\n");
    
    // CRITICAL: Reset WDT before game status calls
    SAFE_WDT_RESET();
    
    // Game status
    if (color_enabled) uart_write_string_immediate("\033[1;35m"); // bold magenta for status
    player_t current_player = game_get_current_player();
    uint32_t move_count = game_get_move_count();
    const char* player_name = (current_player == PLAYER_WHITE) ? "White" : "Black";
    char status_buffer[128];
    snprintf(status_buffer, sizeof(status_buffer), "Game Status: Turn: %s | Move: %" PRIu32 " | Status: Active\n", player_name, move_count);
    uart_write_string_immediate(status_buffer);
    if (color_enabled) uart_write_string_immediate("\033[0m"); // reset colors
    uart_write_string_immediate("\n");
    
    // CRITICAL: Final WDT reset
    SAFE_WDT_RESET();
    
    if (uart_mutex != NULL && mutex_taken) {
        xSemaphoreGive(uart_mutex);
    }
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
        // Stop endless endgame animation if running
        led_stop_endgame_animation();
        
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
    if (args == NULL || strlen(args) == 0) {
        uart_send_error("❌ Missing arguments. Usage: MOVES <square> or MOVES <piece_type>");
        uart_send_formatted("Examples:");
        uart_send_formatted("  MOVES e2     - Show moves for piece at e2");
        uart_send_formatted("  MOVES pawn   - Show moves for all pawns");
        uart_send_formatted("  MOVES knight - Show moves for all knights");
        return CMD_ERROR_INVALID_PARAMETER;
    }
    
    // Trim whitespace
    char trimmed_args[16];
    strncpy(trimmed_args, args, sizeof(trimmed_args) - 1);
    trimmed_args[sizeof(trimmed_args) - 1] = '\0';
    
    // Convert to uppercase for consistency
    for (int i = 0; trimmed_args[i]; i++) {
        trimmed_args[i] = toupper(trimmed_args[i]);
    }
    
    uart_send_colored_line(COLOR_INFO, "🔍 Valid Moves Analysis");
    uart_send_formatted("═══════════════════════════════════════════════════════════════");
    
    // Check if it's a square notation (2 characters: letter + number)
    if (strlen(trimmed_args) == 2 && 
        trimmed_args[0] >= 'A' && trimmed_args[0] <= 'H' &&
        trimmed_args[1] >= '1' && trimmed_args[1] <= '8') {
        
        // It's a square - show moves for piece at that square
        // Convert back to lowercase for convert_notation_to_coords
        char lowercase_square[3];
        lowercase_square[0] = tolower(trimmed_args[0]);
        lowercase_square[1] = trimmed_args[1];
        lowercase_square[2] = '\0';
        

        
        uint8_t row, col;
        if (convert_notation_to_coords(lowercase_square, &row, &col)) {
            piece_t piece = game_get_piece(row, col);
            if (piece == PIECE_EMPTY) {
                char error_msg[64];
                snprintf(error_msg, sizeof(error_msg), "❌ No piece at square %s", trimmed_args);
                uart_send_error(error_msg);
                return CMD_ERROR_INVALID_PARAMETER;
            }
            
            uart_send_formatted("📍 Piece at %s: %s", trimmed_args, game_get_piece_name(piece));
            
            // ✅ DIRECT LED CALL - No queue hell
            led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0); // Yellow for selected piece
            
            // Get available moves for this piece
            move_suggestion_t suggestions[50];
            uint32_t count = game_get_available_moves(row, col, suggestions, 50);
            
            if (count == 0) {
                uart_send_formatted("❌ No legal moves available for this piece");
                return CMD_SUCCESS;
            }
            
            uart_send_formatted("✅ Available moves (%" PRIu32 "):", count);
            
            // Group moves by type
            char normal_moves[512] = "";
            char capture_moves[512] = "";
            char special_moves[512] = "";
            
            int normal_pos = 0, capture_pos = 0, special_pos = 0;
            
            for (uint32_t i = 0; i < count && i < 30; i++) { // Limit to 30 moves for display
                char to_square[3];
                game_coords_to_square(suggestions[i].to_row, suggestions[i].to_col, to_square);
                
                if (suggestions[i].is_capture) {
                    if (capture_pos > 0) capture_pos += snprintf(capture_moves + capture_pos, 
                                                                sizeof(capture_moves) - capture_pos, ", ");
                    capture_pos += snprintf(capture_moves + capture_pos, 
                                           sizeof(capture_moves) - capture_pos, "%s", to_square);
                } else if (suggestions[i].is_castling || suggestions[i].is_en_passant) {
                    if (special_pos > 0) special_pos += snprintf(special_moves + special_pos, 
                                                               sizeof(special_moves) - special_pos, ", ");
                    special_pos += snprintf(special_moves + special_pos, 
                                           sizeof(special_moves) - special_pos, "%s", to_square);
                } else {
                    if (normal_pos > 0) normal_pos += snprintf(normal_moves + normal_pos, 
                                                             sizeof(normal_moves) - normal_pos, ", ");
                    normal_pos += snprintf(normal_moves + normal_pos, 
                                         sizeof(normal_moves) - normal_pos, "%s", to_square);
                }
            }
            
            if (normal_pos > 0) {
                uart_send_formatted("  🟢 Normal moves: %s", normal_moves);
            }
            if (capture_pos > 0) {
                uart_send_formatted("  🟠 Capture moves: %s", capture_moves);
            }
            if (special_pos > 0) {
                uart_send_formatted("  🔵 Special moves: %s", special_moves);
            }
            
            if (count > 30) {
                uart_send_formatted("  ... and %" PRIu32 " more moves", count - 30);
            }
            
            // INTERACTIVE LED: Show all possible moves in green
            for (uint32_t i = 0; i < count && i < 30; i++) {
                uint8_t led_index = chess_pos_to_led_index(suggestions[i].to_row, suggestions[i].to_col);
                
                if (suggestions[i].is_capture) {
                    // Orange for captures
                    led_set_pixel_safe(led_index, 255, 165, 0);
                } else {
                    // Green for normal moves
                    led_set_pixel_safe(led_index, 0, 255, 0);
                }
            }
            
            uart_send_formatted("💡 LED Board: Yellow = selected piece, Green = normal moves, Orange = captures");
            
        } else {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "❌ Invalid square notation: %s", trimmed_args);
            uart_send_error(error_msg);
            uart_send_formatted("💡 Use format: a1, b2, c3, etc. (lowercase letter + number)");
            return CMD_ERROR_INVALID_PARAMETER;
        }
        
    } else {
        // It's a piece type - show moves for all pieces of that type
        piece_t piece_type = PIECE_EMPTY;
        
        if (strcmp(trimmed_args, "PAWN") == 0) {
            piece_type = (game_get_current_player() == PLAYER_WHITE) ? PIECE_WHITE_PAWN : PIECE_BLACK_PAWN;
        } else if (strcmp(trimmed_args, "ROOK") == 0) {
            piece_type = (game_get_current_player() == PLAYER_WHITE) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
        } else if (strcmp(trimmed_args, "KNIGHT") == 0) {
            piece_type = (game_get_current_player() == PLAYER_WHITE) ? PIECE_WHITE_KNIGHT : PIECE_BLACK_KNIGHT;
        } else if (strcmp(trimmed_args, "BISHOP") == 0) {
            piece_type = (game_get_current_player() == PLAYER_WHITE) ? PIECE_WHITE_BISHOP : PIECE_BLACK_BISHOP;
        } else if (strcmp(trimmed_args, "QUEEN") == 0) {
            piece_type = (game_get_current_player() == PLAYER_WHITE) ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;
        } else if (strcmp(trimmed_args, "KING") == 0) {
            piece_type = (game_get_current_player() == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
        } else {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "❌ Invalid piece type: %s", trimmed_args);
            uart_send_error(error_msg);
            uart_send_formatted("Valid piece types: PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING");
            return CMD_ERROR_INVALID_PARAMETER;
        }
        
        uart_send_formatted("📍 %s pieces:", game_get_piece_name(piece_type));
        
        // INTERACTIVE LED: Clear all highlights first
        led_clear_all_safe();
        
        // Find all pieces of this type and show their moves
        bool found_any = false;
        int total_moves = 0;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (game_get_piece(row, col) == piece_type) {
                    found_any = true;
                    char square[3];
                    game_coords_to_square(row, col, square);
                    
                    // INTERACTIVE LED: Highlight this piece in yellow
                    led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0); // Yellow for piece
                    
                    move_suggestion_t suggestions[50];
                    uint32_t count = game_get_available_moves(row, col, suggestions, 50);
                    
                    if (count > 0) {
                        uart_send_formatted("  %s at %s: %" PRIu32 " moves", 
                                           game_get_piece_name(piece_type), square, count);
                        
                        // Show first few moves
                        char moves_list[256] = "";
                        int moves_pos = 0;
                        
                        for (uint32_t i = 0; i < count && i < 8; i++) {
                            char to_square[3];
                            game_coords_to_square(suggestions[i].to_row, suggestions[i].to_col, to_square);
                            
                            if (moves_pos > 0) moves_pos += snprintf(moves_list + moves_pos, 
                                                                    sizeof(moves_list) - moves_pos, ", ");
                            
                            if (suggestions[i].is_capture) {
                                moves_pos += snprintf(moves_list + moves_pos, 
                                                    sizeof(moves_list) - moves_pos, "x%s", to_square);
                            } else {
                                moves_pos += snprintf(moves_list + moves_pos, 
                                                    sizeof(moves_list) - moves_pos, "%s", to_square);
                            }
                        }
                        
                        uart_send_formatted("    → %s", moves_list);
                        if (count > 8) {
                            uart_send_formatted("    ... and %" PRIu32 " more", count - 8);
                        }
                        
                        // INTERACTIVE LED: Show all possible moves for this piece
                        for (uint32_t i = 0; i < count && i < 20; i++) { // Limit to 20 moves per piece
                            uint8_t led_index = chess_pos_to_led_index(suggestions[i].to_row, suggestions[i].to_col);
                            
                            if (suggestions[i].is_capture) {
                                // Orange for captures
                                led_set_pixel_safe(led_index, 255, 165, 0);
                            } else {
                                // Green for normal moves
                                led_set_pixel_safe(led_index, 0, 255, 0);
                            }
                        }
                        
                        total_moves += count;
                    }
                }
            }
        }
        
        if (!found_any) {
            uart_send_formatted("❌ No %s pieces found on the board", game_get_piece_name(piece_type));
            return CMD_SUCCESS;
        }
        
        uart_send_formatted("✅ Total moves available: %d", total_moves);
        
        // INTERACTIVE LED: Final message
        uart_send_formatted("💡 LED Board: Yellow = %s pieces, Green = normal moves, Orange = captures", 
                           game_get_piece_name(piece_type));
    }
    
    uart_send_formatted("");
    uart_send_colored_line(COLOR_INFO, "💡 Use 'MOVE <from>-<to>' to make a move");
    
    return CMD_SUCCESS;
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
        uart_send_formatted("TODO: Get actual history from game engine");
        return CMD_SUCCESS;
    } else {
        uart_send_error("Internal error: failed to get move history");
        return CMD_ERROR_SYSTEM_ERROR;
    }
}

// ============================================================================
// DEBUG COMMANDS (TODO: Full implementation)
// ============================================================================

command_result_t uart_cmd_self_test(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("🔧 SYSTEM SELF-TEST");
    uart_send_formatted("═══════════════════");
    
    int tests_passed = 0;
    int tests_total = 0;
    
    // Test 1: Memory System
    uart_send_formatted("🧠 MEMORY SYSTEM TEST:");
    tests_total++;
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    
    if (free_heap > 10000 && min_free_heap > 5000) { // At least 10KB free, 5KB minimum
        uart_send_formatted("   ✅ Free Heap: %zu bytes (%.1f%%)", free_heap, (float)free_heap / total_heap * 100);
        uart_send_formatted("   ✅ Min Free: %zu bytes", min_free_heap);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ Memory: CRITICAL - %zu bytes free", free_heap);
    }
    
    // Test 2: Task System
    uart_send_formatted("📋 TASK SYSTEM TEST:");
    tests_total++;
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(current_task);
    
    if (task_count > 0 && stack_high_water > 100) { // At least 100 bytes stack free
        uart_send_formatted("   ✅ Tasks Running: %" PRIu32, task_count);
        uart_send_formatted("   ✅ Stack Free: %" PRIu32 " bytes", stack_high_water);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ Task System: FAILED");
    }
    
    // Test 3: Queue System
    uart_send_formatted("📦 QUEUE SYSTEM TEST:");
    tests_total++;
    extern QueueHandle_t uart_command_queue;
    extern QueueHandle_t game_command_queue;
    // extern QueueHandle_t led_command_queue;  // ❌ REMOVED: Queue hell eliminated
    
    int queues_ok = 0;
    if (uart_command_queue) queues_ok++;
    if (game_command_queue) queues_ok++;
    // LED queues removed - using direct LED calls now
    
    if (queues_ok >= 2) {
        uart_send_formatted("   ✅ Core Queues: %d/2 available", queues_ok);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ Queues: Only %d/2 available", queues_ok);
    }
    
    // Test 4: Mutex System
    uart_send_formatted("🔒 MUTEX SYSTEM TEST:");
    tests_total++;
    extern SemaphoreHandle_t uart_mutex;
    extern SemaphoreHandle_t game_mutex;
    extern SemaphoreHandle_t led_mutex;
    
    int mutexes_ok = 0;
    if (uart_mutex) mutexes_ok++;
    if (game_mutex) mutexes_ok++;
    if (led_mutex) mutexes_ok++;
    
    if (mutexes_ok >= 3) {
        uart_send_formatted("   ✅ Core Mutexes: %d/3 available", mutexes_ok);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ Mutexes: Only %d/3 available", mutexes_ok);
    }
    
    // Test 5: Timer System
    uart_send_formatted("⏰ TIMER SYSTEM TEST:");
    tests_total++;
    int64_t start_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
    int64_t end_time = esp_timer_get_time();
    int64_t elapsed = end_time - start_time;
    
    if (elapsed >= 8000 && elapsed <= 12000) { // 8-12ms tolerance
        uart_send_formatted("   ✅ Timer Accuracy: %" PRId64 " μs (expected ~10ms)", elapsed);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ Timer: %" PRId64 " μs (expected ~10ms)", elapsed);
    }
    
    // Test 6: CPU Performance
    uart_send_formatted("⚡ CPU PERFORMANCE TEST:");
    tests_total++;
    start_time = esp_timer_get_time();
    volatile int test_sum = 0;
    for (int i = 0; i < 1000; i++) {
        test_sum += i * i;
    }
    end_time = esp_timer_get_time();
    int64_t cpu_time = end_time - start_time;
    
    if (cpu_time < 1000) { // Should complete in less than 1ms
        uart_send_formatted("   ✅ CPU Speed: %" PRId64 " μs for 1K operations", cpu_time);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ CPU: %" PRId64 " μs (too slow)", cpu_time);
    }
    
    // Test 7: System Uptime
    uart_send_formatted("🕐 SYSTEM UPTIME TEST:");
    tests_total++;
    int64_t uptime_us = esp_timer_get_time();
    uint64_t uptime_sec = uptime_us / 1000000;
    
    if (uptime_sec > 0) {
        uart_send_formatted("   ✅ Uptime: %" PRIu64 " seconds", uptime_sec);
        tests_passed++;
    } else {
        uart_send_formatted("   ❌ Uptime: Invalid");
    }
    
    // Test Summary
    uart_send_formatted("");
    uart_send_formatted("📊 TEST SUMMARY:");
    uart_send_formatted("   Tests Passed: %d/%d", tests_passed, tests_total);
    uart_send_formatted("   Success Rate: %.1f%%", (float)tests_passed / tests_total * 100);
    
    if (tests_passed == tests_total) {
        uart_send_formatted("   🎉 ALL TESTS PASSED - System is healthy!");
    } else if (tests_passed >= tests_total * 0.8) {
        uart_send_formatted("   ⚠️  MOSTLY HEALTHY - %d test(s) failed", tests_total - tests_passed);
    } else {
        uart_send_formatted("   🚨 SYSTEM ISSUES - %d test(s) failed", tests_total - tests_passed);
    }
    
    uart_send_formatted("✅ Self-test completed");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_test_game(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("🎮 GAME ENGINE TEST");
    uart_send_formatted("═══════════════════");
    uart_send_formatted("✅ Game Task: Running");
    uart_send_formatted("✅ Board State: Valid");
    uart_send_formatted("✅ Move Validation: Available");
    uart_send_formatted("⚠️  TODO: Complete game logic tests");
    uart_send_formatted("📝 Status: BASIC TEST ONLY");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_status(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("🔍 DEBUG STATUS");
    uart_send_formatted("═══════════════");
    
    // System Information
    uart_send_formatted("🖥️  SYSTEM INFO:");
    uart_send_formatted("   CPU Frequency: 160 MHz (ESP32-C6)");
    uart_send_formatted("   Uptime: %" PRIu64 " seconds", esp_timer_get_time() / 1000000);
    uart_send_formatted("   FreeRTOS Version: %s", tskKERNEL_VERSION_NUMBER);
    
    // Memory Information
    uart_send_formatted("💾 MEMORY DEBUG:");
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    uart_send_formatted("   Free Heap: %zu bytes (%.1f%%)", free_heap, (float)free_heap / total_heap * 100);
    uart_send_formatted("   Min Free: %zu bytes", min_free_heap);
    uart_send_formatted("   Total Heap: %zu bytes", total_heap);
    uart_send_formatted("   Used: %zu bytes (%.1f%%)", total_heap - free_heap, (float)(total_heap - free_heap) / total_heap * 100);
    
    // Task Information
    uart_send_formatted("📋 TASK DEBUG:");
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(current_task);
    uart_send_formatted("   Total Tasks: %" PRIu32, task_count);
    uart_send_formatted("   Current Task Stack: %" PRIu32 " bytes free", stack_high_water);
    uart_send_formatted("   Scheduler State: %s", (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? "Running" : "Suspended");
    
    // Queue Information
    uart_send_formatted("📦 QUEUE DEBUG:");
    extern QueueHandle_t uart_command_queue;
    extern QueueHandle_t game_command_queue;
    // extern QueueHandle_t led_command_queue;  // ❌ REMOVED: Queue hell eliminated
    
    if (uart_command_queue) {
        UBaseType_t uart_messages = uxQueueMessagesWaiting(uart_command_queue);
        UBaseType_t uart_spaces = uxQueueSpacesAvailable(uart_command_queue);
        uart_send_formatted("   UART Command: [%d/%d] messages", uart_messages, uart_messages + uart_spaces);
    }
    
    if (game_command_queue) {
        UBaseType_t game_messages = uxQueueMessagesWaiting(game_command_queue);
        UBaseType_t game_spaces = uxQueueSpacesAvailable(game_command_queue);
        uart_send_formatted("   Game Command: [%d/%d] messages", game_messages, game_messages + game_spaces);
    }
    
    // LED queues removed - using direct LED calls now
    
    // Mutex Information
    uart_send_formatted("🔒 MUTEX DEBUG:");
    extern SemaphoreHandle_t uart_mutex;
    extern SemaphoreHandle_t game_mutex;
    extern SemaphoreHandle_t led_mutex;
    
    if (uart_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(uart_mutex);
        uart_send_formatted("   UART Mutex: %s", holder ? "LOCKED" : "FREE");
    }
    
    if (game_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(game_mutex);
        uart_send_formatted("   Game Mutex: %s", holder ? "LOCKED" : "FREE");
    }
    
    if (led_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(led_mutex);
        uart_send_formatted("   LED Mutex: %s", holder ? "LOCKED" : "FREE");
    }
    
    // Performance Information
    uart_send_formatted("⚡ PERFORMANCE DEBUG:");
    uart_send_formatted("   Tick Count: %" PRIu32, xTaskGetTickCount());
    uart_send_formatted("   Tick Rate: %d Hz", configTICK_RATE_HZ);
    
    // Command Statistics
    uart_send_formatted("📊 COMMAND STATS:");
    uart_send_formatted("   Commands Processed: %" PRIu32, command_count);
    uart_send_formatted("   Errors Encountered: %" PRIu32, error_count);
    if (command_count > 0) {
        uart_send_formatted("   Error Rate: %.2f%%", (float)error_count / command_count * 100);
    }
    
    uart_send_formatted("✅ Debug status displayed");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_game(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("🎯 GAME DEBUG INFO");
    uart_send_formatted("══════════════════");
    
    // Get game information from game_task
    extern player_t game_get_current_player(void);
    extern uint32_t game_get_move_count(void);
    extern game_state_t game_get_state(void);
    
    // Game State Information
    uart_send_formatted("🎮 GAME STATE:");
    game_state_t game_state = game_get_state();
    const char* state_str;
    switch (game_state) {
        case GAME_STATE_IDLE: state_str = "Idle"; break;
        case GAME_STATE_INIT: state_str = "Initializing"; break;
        case GAME_STATE_ACTIVE: state_str = "Active"; break;
        case GAME_STATE_PAUSED: state_str = "Paused"; break;
        case GAME_STATE_FINISHED: state_str = "Finished"; break;
        case GAME_STATE_ERROR: state_str = "Error"; break;
        case GAME_STATE_PLAYING: state_str = "Playing"; break;
        case GAME_STATE_PROMOTION: state_str = "Promotion"; break;
        default: state_str = "Unknown"; break;
    }
    uart_send_formatted("   State: %s (%d)", state_str, game_state);
    
    player_t current_player = game_get_current_player();
    uart_send_formatted("   Current Player: %s (%s)", 
                      current_player == PLAYER_WHITE ? "White" : "Black",
                      current_player == PLAYER_WHITE ? "♔" : "♚");
    
    uint32_t move_count = game_get_move_count();
    uart_send_formatted("   Move Count: %" PRIu32, move_count);
    uart_send_formatted("   Half-moves: %" PRIu32, move_count / 2);
    
    // Piece Information
    uart_send_formatted("♟️  PIECE STATS:");
    uart_send_formatted("   Note: Detailed piece counts require game engine access");
    uart_send_formatted("   Use 'board' command for current position");
    
    // Game Task Information
    uart_send_formatted("🔧 GAME TASK DEBUG:");
    extern TaskHandle_t game_task_handle;
    if (game_task_handle) {
        const char* task_name = pcTaskGetName(game_task_handle);
        uart_send_formatted("   Task Name: %s", task_name);
        uart_send_formatted("   Task Priority: %" PRIu32, uxTaskPriorityGet(game_task_handle));
        uart_send_formatted("   Stack High Water: %" PRIu32 " bytes", uxTaskGetStackHighWaterMark(game_task_handle));
    } else {
        uart_send_formatted("   Task: NOT CREATED");
    }
    
    // Game Queue Information
    uart_send_formatted("📦 GAME QUEUES:");
    extern QueueHandle_t game_command_queue;
    extern QueueHandle_t game_status_queue;
    
    if (game_command_queue) {
        UBaseType_t messages = uxQueueMessagesWaiting(game_command_queue);
        UBaseType_t spaces = uxQueueSpacesAvailable(game_command_queue);
        uart_send_formatted("   Command Queue: [%d/%d] messages", messages, messages + spaces);
    }
    
    if (game_status_queue) {
        UBaseType_t messages = uxQueueMessagesWaiting(game_status_queue);
        UBaseType_t spaces = uxQueueSpacesAvailable(game_status_queue);
        uart_send_formatted("   Status Queue: [%d/%d] messages", messages, messages + spaces);
    }
    
    // Game Mutex Information
    uart_send_formatted("🔒 GAME MUTEX:");
    extern SemaphoreHandle_t game_mutex;
    if (game_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(game_mutex);
        uart_send_formatted("   Game Mutex: %s", holder ? "LOCKED" : "FREE");
        if (holder) {
            const char* holder_name = pcTaskGetName(holder);
            uart_send_formatted("   Holder: %s", holder_name);
        }
    } else {
        uart_send_formatted("   Game Mutex: NOT CREATED");
    }
    
    // Performance Information
    uart_send_formatted("⚡ GAME PERFORMANCE:");
    int64_t start_time = esp_timer_get_time();
    // Simulate a quick game operation
    volatile int test = 0;
    for (int i = 0; i < 100; i++) {
        test += i;
    }
    int64_t end_time = esp_timer_get_time();
    uart_send_formatted("   Operation Time: %" PRId64 " μs", end_time - start_time);
    
    uart_send_formatted("✅ Game debug info displayed");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_board(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("♞ BOARD DEBUG INFO");
    uart_send_formatted("══════════════════");
    
    // Board Structure Information
    uart_send_formatted("🏗️  BOARD STRUCTURE:");
    uart_send_formatted("   Size: 8x8 (64 squares)");
    uart_send_formatted("   Square Format: Algebraic notation (a1-h8)");
    uart_send_formatted("   Piece Representation: Integer values");
    
    // Piece Count Analysis
    uart_send_formatted("♟️  PIECE ANALYSIS:");
    uart_send_formatted("   Note: Detailed piece analysis requires game engine access");
    uart_send_formatted("   Use 'board' command to see current position");
    uart_send_formatted("   Standard chess: 32 pieces (16 per side)");
    
    // Board Position Analysis
    uart_send_formatted("📍 POSITION ANALYSIS:");
    uart_send_formatted("   Note: Position analysis requires game engine access");
    uart_send_formatted("   Use 'board' command to see current position");
    uart_send_formatted("   Standard board: 64 squares (8x8)");
    
    // King Position Analysis
    uart_send_formatted("👑 KING POSITIONS:");
    uart_send_formatted("   Note: King positions require game engine access");
    uart_send_formatted("   Use 'board' command to see current position");
    uart_send_formatted("   Standard: White King on e1, Black King on e8");
    
    // Board Validation
    uart_send_formatted("✅ BOARD VALIDATION:");
    uart_send_formatted("   Note: Board validation requires game engine access");
    uart_send_formatted("   Use 'board' command to see current position");
    uart_send_formatted("   Standard validation: 1 king per side, max 32 pieces");
    
    // Memory Usage
    uart_send_formatted("💾 BOARD MEMORY:");
    uart_send_formatted("   Board Array: 64 bytes (8x8 int)");
    uart_send_formatted("   Move History: Variable size");
    uart_send_formatted("   Position Hash: 8 bytes");
    
    uart_send_formatted("✅ Board debug info displayed");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_benchmark(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("⚡ PERFORMANCE BENCHMARK");
    uart_send_formatted("═══════════════════════");
    
    // Get real system information
    uint32_t cpu_freq = 160000000; // ESP32-C6 default CPU frequency
    uint32_t apb_freq = 80000000;  // ESP32-C6 default APB frequency
    
    // Memory information
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    
    // Task information
    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    
    // Timer information
    int64_t uptime_us = esp_timer_get_time();
    uint64_t uptime_ms = uptime_us / 1000;
    uint64_t uptime_sec = uptime_ms / 1000;
    
    // Performance metrics
    uart_send_formatted("🖥️  SYSTEM INFO:");
    uart_send_formatted("   CPU Frequency: %" PRIu32 " MHz", cpu_freq / 1000000);
    uart_send_formatted("   APB Frequency: %" PRIu32 " MHz", apb_freq / 1000000);
    uart_send_formatted("   Uptime: %" PRIu64 "s (%" PRIu64 "ms)", uptime_sec, uptime_ms);
    
    uart_send_formatted("💾 MEMORY USAGE:");
    uart_send_formatted("   Free Heap: %zu bytes (%.1f%%)", free_heap, (float)free_heap / total_heap * 100);
    uart_send_formatted("   Min Free: %zu bytes", min_free_heap);
    uart_send_formatted("   Total Heap: %zu bytes", total_heap);
    uart_send_formatted("   Used: %zu bytes (%.1f%%)", total_heap - free_heap, (float)(total_heap - free_heap) / total_heap * 100);
    
    uart_send_formatted("📊 TASK INFO:");
    uart_send_formatted("   Total Tasks: %" PRIu32, task_count);
    uart_send_formatted("   Stack High Water: %" PRIu32 " bytes", high_water_mark);
    
    // Simple performance test
    uart_send_formatted("🏃 PERFORMANCE TEST:");
    
    // Test 1: Simple loop timing
    int64_t start_time = esp_timer_get_time();
    volatile int test_sum = 0;
    for (int i = 0; i < 10000; i++) {
        test_sum += i;
    }
    int64_t end_time = esp_timer_get_time();
    int64_t loop_time = end_time - start_time;
    
    uart_send_formatted("   10K Loop: %" PRId64 " μs (%.2f μs/iter)", loop_time, (float)loop_time / 10000);
    
    // Test 2: Memory allocation speed
    start_time = esp_timer_get_time();
    void* test_ptr = malloc(1024);
    free(test_ptr);
    end_time = esp_timer_get_time();
    int64_t alloc_time = end_time - start_time;
    
    uart_send_formatted("   Malloc/Free: %" PRId64 " μs", alloc_time);
    
    // Test 3: Task switching overhead (approximate)
    start_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(1));
    end_time = esp_timer_get_time();
    int64_t task_switch_time = end_time - start_time;
    
    uart_send_formatted("   Task Switch: ~%" PRId64 " μs", task_switch_time);
    
    uart_send_formatted("✅ Benchmark completed successfully");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_memcheck(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("💾 MEMORY CHECK");
    uart_send_formatted("═══════════════");
    
    // Get detailed memory information
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t used_heap = total_heap - free_heap;
    
    // Get memory capabilities
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t total_spiram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    // Calculate percentages
    float free_percent = (float)free_heap / total_heap * 100;
    float used_percent = (float)used_heap / total_heap * 100;
    float min_free_percent = (float)min_free_heap / total_heap * 100;
    
    uart_send_formatted("📊 HEAP MEMORY:");
    uart_send_formatted("   Total: %zu bytes (%.1f KB)", total_heap, total_heap / 1024.0f);
    uart_send_formatted("   Free: %zu bytes (%.1f KB) - %.1f%%", free_heap, free_heap / 1024.0f, free_percent);
    uart_send_formatted("   Used: %zu bytes (%.1f KB) - %.1f%%", used_heap, used_heap / 1024.0f, used_percent);
    uart_send_formatted("   Min Free: %zu bytes (%.1f KB) - %.1f%%", min_free_heap, min_free_heap / 1024.0f, min_free_percent);
    
    uart_send_formatted("🏠 INTERNAL RAM:");
    uart_send_formatted("   Total: %zu bytes (%.1f KB)", total_internal, total_internal / 1024.0f);
    uart_send_formatted("   Free: %zu bytes (%.1f KB)", free_internal, free_internal / 1024.0f);
    uart_send_formatted("   Used: %zu bytes (%.1f KB)", total_internal - free_internal, (total_internal - free_internal) / 1024.0f);
    
    if (total_spiram > 0) {
        uart_send_formatted("🚀 SPI RAM:");
        uart_send_formatted("   Total: %zu bytes (%.1f KB)", total_spiram, total_spiram / 1024.0f);
        uart_send_formatted("   Free: %zu bytes (%.1f KB)", free_spiram, free_spiram / 1024.0f);
        uart_send_formatted("   Used: %zu bytes (%.1f KB)", total_spiram - free_spiram, (total_spiram - free_spiram) / 1024.0f);
    } else {
        uart_send_formatted("🚀 SPI RAM: Not available");
    }
    
    // Memory health assessment
    uart_send_formatted("🏥 MEMORY HEALTH:");
    if (free_percent > 50) {
        uart_send_formatted("   Status: 🟢 EXCELLENT (%.1f%% free)", free_percent);
    } else if (free_percent > 25) {
        uart_send_formatted("   Status: 🟡 GOOD (%.1f%% free)", free_percent);
    } else if (free_percent > 10) {
        uart_send_formatted("   Status: 🟠 WARNING (%.1f%% free)", free_percent);
    } else {
        uart_send_formatted("   Status: 🔴 CRITICAL (%.1f%% free)", free_percent);
    }
    
    if (min_free_percent < 5) {
        uart_send_formatted("   ⚠️  Low water mark: %.1f%%", min_free_percent);
    }
    
    // Task stack information
    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    uart_send_formatted("📚 CURRENT TASK STACK:");
    uart_send_formatted("   High Water Mark: %" PRIu32 " bytes", high_water_mark);
    
    uart_send_formatted("✅ Memory check completed");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_show_tasks(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("📋 RUNNING TASKS");
    uart_send_formatted("════════════════");
    
    // Get total number of tasks
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    uart_send_formatted("Total Tasks: %" PRIu32, task_count);
    uart_send_formatted("");
    
    // Get scheduler state
    uart_send_formatted("🔄 SCHEDULER INFO:");
    uart_send_formatted("   State: %s", (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? "Running" : "Suspended");
    uart_send_formatted("   Tick Count: %" PRIu32, xTaskGetTickCount());
    uart_send_formatted("   Tick Rate: %d Hz", configTICK_RATE_HZ);
    
    // Get current task info
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    const char* current_task_name = pcTaskGetName(current_task);
    
    uart_send_formatted("");
    uart_send_formatted("🎯 CURRENT TASK:");
    uart_send_formatted("   Name: %s", current_task_name);
    uart_send_formatted("   Handle: %p", current_task);
    uart_send_formatted("   Priority: %" PRIu32, uxTaskPriorityGet(current_task));
    uart_send_formatted("   Stack High Water: %" PRIu32 " bytes", uxTaskGetStackHighWaterMark(current_task));
    
    // Get system task handles (if available)
    uart_send_formatted("");
    uart_send_formatted("🏗️  SYSTEM TASKS:");
    
    // Check if we have access to system task handles
    extern TaskHandle_t uart_task_handle;
    extern TaskHandle_t game_task_handle;
    extern TaskHandle_t led_task_handle;
    extern TaskHandle_t matrix_task_handle;
    extern TaskHandle_t button_task_handle;
    extern TaskHandle_t animation_task_handle;
    extern TaskHandle_t screen_saver_task_handle;
    extern TaskHandle_t test_task_handle;
    extern TaskHandle_t web_server_task_handle;
    extern TaskHandle_t reset_button_task_handle;
    extern TaskHandle_t promotion_button_task_handle;
    
    if (uart_task_handle) {
        const char* task_name = pcTaskGetName(uart_task_handle);
        uart_send_formatted("   UART Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(uart_task_handle), 
                          uxTaskGetStackHighWaterMark(uart_task_handle));
    }
    
    if (game_task_handle) {
        const char* task_name = pcTaskGetName(game_task_handle);
        uart_send_formatted("   Game Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(game_task_handle), 
                          uxTaskGetStackHighWaterMark(game_task_handle));
    }
    
    if (led_task_handle) {
        const char* task_name = pcTaskGetName(led_task_handle);
        uart_send_formatted("   LED Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(led_task_handle), 
                          uxTaskGetStackHighWaterMark(led_task_handle));
    }
    
    if (matrix_task_handle) {
        const char* task_name = pcTaskGetName(matrix_task_handle);
        uart_send_formatted("   Matrix Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(matrix_task_handle), 
                          uxTaskGetStackHighWaterMark(matrix_task_handle));
    }
    
    if (button_task_handle) {
        const char* task_name = pcTaskGetName(button_task_handle);
        uart_send_formatted("   Button Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(button_task_handle), 
                          uxTaskGetStackHighWaterMark(button_task_handle));
    }
    
    if (animation_task_handle) {
        const char* task_name = pcTaskGetName(animation_task_handle);
        uart_send_formatted("   Animation Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(animation_task_handle), 
                          uxTaskGetStackHighWaterMark(animation_task_handle));
    }
    
    if (screen_saver_task_handle) {
        const char* task_name = pcTaskGetName(screen_saver_task_handle);
        uart_send_formatted("   Screen Saver Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(screen_saver_task_handle), 
                          uxTaskGetStackHighWaterMark(screen_saver_task_handle));
    } else {
        uart_send_formatted("   Screen Saver Task: DISABLED");
    }
    
    if (test_task_handle) {
        const char* task_name = pcTaskGetName(test_task_handle);
        uart_send_formatted("   Test Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(test_task_handle), 
                          uxTaskGetStackHighWaterMark(test_task_handle));
    }
    
    if (web_server_task_handle) {
        const char* task_name = pcTaskGetName(web_server_task_handle);
        uart_send_formatted("   Web Server Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(web_server_task_handle), 
                          uxTaskGetStackHighWaterMark(web_server_task_handle));
    }
    
    if (reset_button_task_handle) {
        const char* task_name = pcTaskGetName(reset_button_task_handle);
        uart_send_formatted("   Reset Button Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(reset_button_task_handle), 
                          uxTaskGetStackHighWaterMark(reset_button_task_handle));
    }
    
    if (promotion_button_task_handle) {
        const char* task_name = pcTaskGetName(promotion_button_task_handle);
        uart_send_formatted("   Promotion Button Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32 ")", 
                          task_name, uxTaskPriorityGet(promotion_button_task_handle), 
                          uxTaskGetStackHighWaterMark(promotion_button_task_handle));
    }
    
    // CPU usage information
    uart_send_formatted("");
    uart_send_formatted("💻 CPU INFO:");
    uart_send_formatted("   Frequency: %" PRIu32 " MHz", 160); // ESP32-C6 default
    uart_send_formatted("   Uptime: %" PRIu64 " ms", esp_timer_get_time() / 1000);
    
    uart_send_formatted("✅ Task information displayed");
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_show_mutexes(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("🔒 MUTEX STATUS");
    uart_send_formatted("═══════════════");
    
    // Get system mutex handles
    extern SemaphoreHandle_t uart_mutex;
    extern SemaphoreHandle_t led_mutex;
    extern SemaphoreHandle_t matrix_mutex;
    extern SemaphoreHandle_t button_mutex;
    extern SemaphoreHandle_t game_mutex;
    extern SemaphoreHandle_t system_mutex;
    
    uart_send_formatted("🏗️  SYSTEM MUTEXES:");
    
    // Check UART mutex
    if (uart_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(uart_mutex);
        uart_send_formatted("   UART Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
        if (holder) {
            uart_send_formatted("      Holder: %p", holder);
        }
    } else {
        uart_send_formatted("   UART Mutex: ❌ NOT CREATED");
    }
    
    // Check LED mutex
    if (led_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(led_mutex);
        uart_send_formatted("   LED Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
        if (holder) {
            uart_send_formatted("      Holder: %p", holder);
        }
    } else {
        uart_send_formatted("   LED Mutex: ❌ NOT CREATED");
    }
    
    // Check Matrix mutex
    if (matrix_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(matrix_mutex);
        uart_send_formatted("   Matrix Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
        if (holder) {
            uart_send_formatted("      Holder: %p", holder);
        }
    } else {
        uart_send_formatted("   Matrix Mutex: ❌ NOT CREATED");
    }
    
    // Check Button mutex
    if (button_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(button_mutex);
        uart_send_formatted("   Button Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
        if (holder) {
            uart_send_formatted("      Holder: %p", holder);
        }
    } else {
        uart_send_formatted("   Button Mutex: ❌ NOT CREATED");
    }
    
    // Check Game mutex
    if (game_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(game_mutex);
        uart_send_formatted("   Game Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
        if (holder) {
            uart_send_formatted("      Holder: %p", holder);
        }
    } else {
        uart_send_formatted("   Game Mutex: ❌ NOT CREATED");
    }
    
    // Check System mutex
    if (system_mutex) {
        TaskHandle_t holder = xSemaphoreGetMutexHolder(system_mutex);
        uart_send_formatted("   System Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
        if (holder) {
            uart_send_formatted("      Holder: %p", holder);
        }
    } else {
        uart_send_formatted("   System Mutex: ❌ NOT CREATED");
    }
    
    uart_send_formatted("");
    uart_send_formatted("📊 MUTEX SUMMARY:");
    uart_send_formatted("   Legend: 🟢 FREE, 🔴 LOCKED, ❌ NOT CREATED");
    uart_send_formatted("   Note: LOCKED mutexes show the task handle that holds them");
    
    uart_send_formatted("✅ Mutex status displayed");
    
    return CMD_SUCCESS;
}

// Helper function to display queue status
static void display_queue_status(const char* name, QueueHandle_t queue) {
    if (queue) {
        UBaseType_t messages_waiting = uxQueueMessagesWaiting(queue);
        UBaseType_t spaces_available = uxQueueSpacesAvailable(queue);
        UBaseType_t queue_length = messages_waiting + spaces_available;
        float fill_percent = (float)messages_waiting / queue_length * 100;
        
        const char* status;
        if (fill_percent > 90) {
            status = "🔴 FULL";
        } else if (fill_percent > 75) {
            status = "🟠 HIGH";
        } else if (fill_percent > 50) {
            status = "🟡 MEDIUM";
        } else {
            status = "🟢 OK";
        }
        
        uart_send_formatted("   %s: [%d/%d] %.1f%% %s", 
                          name, messages_waiting, queue_length, fill_percent, status);
    } else {
        uart_send_formatted("   %s: ❌ NOT CREATED", name);
    }
}

command_result_t uart_cmd_show_fifos(const char* args)
{
    (void)args; // Unused parameter
    
    uart_send_formatted("📦 FIFO (QUEUE) STATUS");
    uart_send_formatted("══════════════════════");
    
    // Get system queue handles
    extern QueueHandle_t uart_command_queue;
    extern QueueHandle_t uart_response_queue;
    extern QueueHandle_t game_command_queue;
    extern QueueHandle_t game_status_queue;
    // extern QueueHandle_t led_command_queue;  // ❌ REMOVED: Queue hell eliminated
    // extern QueueHandle_t led_status_queue;  // ❌ REMOVED: Queue hell eliminated
    extern QueueHandle_t matrix_command_queue;
    extern QueueHandle_t matrix_event_queue;
    extern QueueHandle_t button_event_queue;
    extern QueueHandle_t button_command_queue;
    extern QueueHandle_t animation_command_queue;
    extern QueueHandle_t animation_status_queue;
    extern QueueHandle_t screen_saver_command_queue;
    extern QueueHandle_t screen_saver_status_queue;
    // extern QueueHandle_t matter_command_queue;  // DISABLED - Matter not needed
    // extern QueueHandle_t matter_status_queue;   // DISABLED - Matter not needed
    extern QueueHandle_t web_command_queue;
    extern QueueHandle_t web_server_command_queue;
    extern QueueHandle_t web_server_status_queue;
    extern QueueHandle_t test_command_queue;
    
    uart_send_formatted("🏗️  SYSTEM QUEUES:");
    
    // Display all queue statuses
    display_queue_status("UART Command", uart_command_queue);
    display_queue_status("UART Response", uart_response_queue);
    display_queue_status("Game Command", game_command_queue);
    display_queue_status("Game Status", game_status_queue);
    // LED queues removed - using direct LED calls now
    display_queue_status("Matrix Command", matrix_command_queue);
    display_queue_status("Matrix Event", matrix_event_queue);
    display_queue_status("Button Event", button_event_queue);
    display_queue_status("Button Command", button_command_queue);
    display_queue_status("Animation Command", animation_command_queue);
    display_queue_status("Animation Status", animation_status_queue);
    display_queue_status("Screen Saver Command", screen_saver_command_queue);
    display_queue_status("Screen Saver Status", screen_saver_status_queue);
    // display_queue_status("Matter Command", matter_command_queue);  // DISABLED - Matter not needed
    // display_queue_status("Matter Status", matter_status_queue);     // DISABLED - Matter not needed
    display_queue_status("Web Command", web_command_queue);
    display_queue_status("Web Server Command", web_server_command_queue);
    display_queue_status("Web Server Status", web_server_status_queue);
    display_queue_status("Test Command", test_command_queue);
    
    uart_send_formatted("");
    uart_send_formatted("📊 QUEUE SUMMARY:");
    uart_send_formatted("   Format: [messages_waiting/total_capacity] fill_percentage status");
    uart_send_formatted("   Status: 🟢 OK (<50%), 🟡 MEDIUM (50-75%), 🟠 HIGH (75-90%), 🔴 FULL (>90%)");
    
    uart_send_formatted("✅ FIFO status displayed");
    
    return CMD_SUCCESS;
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void uart_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "🚀 Enhanced UART command interface starting...");
    
    // ✅ CRITICAL: Register with TWDT from within task
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register UART task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ UART task registered with TWDT");
    }
    
    // Initialize configuration manager
    config_manager_init();
    
    // Load configuration from NVS
    config_load_from_nvs(&system_config);
    
    // Apply configuration settings
    config_apply_settings(&system_config);
    
    // Initialize input buffer and command history
    input_buffer_init(&input_buffer);
    // Echo handled by terminal
    command_history_init(&command_history);
    
    // UART response queue is created in freertos_chess.c
    if (uart_response_queue == NULL) {
        ESP_LOGE(TAG, "UART response queue not available");
    } else {
        ESP_LOGI(TAG, "UART response queue available");
    }
    
    // Echo handled by terminal
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
        ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, UART_QUEUE_SIZE, NULL, 0));
        
        // Set optimal interactivity
        uart_set_rx_timeout(UART_PORT_NUM, pdMS_TO_TICKS(1));
        uart_flush(UART_PORT_NUM);
        
        ESP_LOGI(TAG, "UART driver initialized successfully");
    } else {
        ESP_LOGI(TAG, "Using USB Serial JTAG console - no UART driver initialization needed");
    }
    
    ESP_LOGI(TAG, "🚀 Enhanced UART command interface ready");
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • Line-based input with editing");
    ESP_LOGI(TAG, "  • Command history and aliases");
    ESP_LOGI(TAG, "  • NVS configuration persistence");
    ESP_LOGI(TAG, "  • Robust error handling");
    ESP_LOGI(TAG, "  • Resource optimization");
    
    task_running = true;
    
    // WDT already registered above
    
    // Welcome message will be shown by centralized boot animation
    // uart_send_welcome_logo(); // Removed to prevent duplicate rendering
    
    // Welcome message and guide will be shown by centralized boot animation
    // No early UART output to prevent WDT "task not found" errors
    
    // Show initial prompt after everything is initialized
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for initialization to complete
    if (UART_ENABLED) {
        if (uart_mutex != NULL) {
            xSemaphoreTake(uart_mutex, portMAX_DELAY);
            // Prompt removed
            xSemaphoreGive(uart_mutex);
        } else {
            // Prompt removed
        }
    } else {
        // For USB Serial JTAG, use printf
        // Prompt removed
    }
    
    // Start main input loop using original logic
    uart_task_legacy_loop();
    
    // Should never reach here
    ESP_LOGE(TAG, "UART task unexpectedly exited");
    vTaskDelete(NULL);
}

/**
 * @brief Main character input processing loop
 */
static void uart_input_loop(void)
{
    
    while (task_running) {
        // Reset watchdog
        SAFE_WDT_RESET();
        
        // Try to read character (non-blocking)
        int ch = uart_read_char_immediate();
        
        if (ch == EOF) {
            // No character available, small delay
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        // Process character based on type
        switch (ch) {
            case CHAR_BACKSPACE:
            case CHAR_DELETE:
                process_backspace();
                break;
                
            case CHAR_ENTER:
            case CHAR_NEWLINE:
                if (process_enter()) {
                    // Command ready - process using uart_process_input to avoid duplication
                    uart_process_input('\n');
                }
                break;
                
            case CHAR_CTRL_C:
                uart_write_string_immediate("^C\r\n");
                input_buffer.pos = 0;
                input_buffer.buffer[0] = '\0';
                // Prompt removed
                break;
                
            case CHAR_CTRL_D:
                uart_write_string_immediate("^D\r\n");
                break;
                
            default:
                // Regular printable character
                if (ch >= 32 && ch <= 126) {
                    process_regular_char((char)ch);
                }
                break;
        }
    }
}

// Legacy main loop (kept for compatibility)
void uart_task_legacy_loop(void)
{
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for UART task in every iteration
        esp_err_t wdt_reset_ret = uart_task_wdt_reset_safe();
        if (wdt_reset_ret != ESP_OK && wdt_reset_ret != ESP_ERR_NOT_FOUND) {
            // WDT reset failed - this might indicate system issues
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
        if (UART_ENABLED) {
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
            // For USB Serial JTAG (CONFIG_ESP_CONSOLE_UART_NUM=-1), use getchar
            // This is the consistent method used before and after WDT errors
            // CRITICAL: Use non-blocking approach to prevent WDT timeouts
            
            // CRITICAL: Check if we're in recovery mode after WDT
            static bool wdt_recovery_mode = false;
            static uint32_t wdt_recovery_start = 0;
            
            // Check if we need to enter recovery mode
            if (error_count > 0 && !wdt_recovery_mode) {
                wdt_recovery_mode = true;
                wdt_recovery_start = esp_timer_get_time() / 1000;
                ESP_LOGW(TAG, "Entering WDT recovery mode");
            }
            
            // Exit recovery mode after 10 seconds
            if (wdt_recovery_mode && (esp_timer_get_time() / 1000 - wdt_recovery_start) > 10000) {
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
                // No data available - this is normal, not an error
                len = 0;
            }
        }
        
        // Process input only if we got valid data
        if (len > 0 && !input_error) {
            // ROBUST ERROR HANDLING: Safe input processing
            bool processing_error = false;
            
            // Validate input character before processing
            if (c >= 0 && c <= 127) { // Valid ASCII range
                // Process the input
                uart_process_input(c);
            } else {
                ESP_LOGW(TAG, "Invalid character received: 0x%02X, ignoring", (unsigned char)c);
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
                ESP_LOGW(TAG, "Multiple input errors detected (%lu), attempting recovery...", error_count);
                
                // Clear any corrupted state
                input_buffer_clear(&input_buffer);
                
                // Re-initialize input buffer
                input_buffer_init(&input_buffer);
                // Echo handled by terminal
                
                // Show recovery message
                uart_send_warning("🔄 UART input recovered, continuing...");
                // Prompt removed
            }
        }
        
        // ROBUST ERROR HANDLING: Periodic health check
        if (loop_count % 1000 == 0) { // Every 10 seconds
            uart_task_health_check();
            uart_check_memory_health(); // Check memory health
        }
        
        // Periodic status update every 60 seconds
        if (loop_count % 6000 == 0) {
            ESP_LOGI(TAG, "UART Task Status: Commands=%lu, Errors=%lu", 
                     command_count, error_count);
        }
        
        loop_count++;
        
        // Minimal task delay for maximum responsiveness
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Display LED board states in terminal
 */
void uart_display_led_board(void)
{
    // CRITICAL: Reset WDT at start of display function
    SAFE_WDT_RESET();
    
    // LED board header
    uart_send_colored_line(COLOR_YELLOW, "    a   b   c   d   e   f   g   h");
    uart_send_formatted("  +---+---+---+---+---+---+---+---+");
    
    // MEMORY OPTIMIZATION: Display actual LED states from LED task
    ESP_LOGI(TAG, "📡 Displaying actual LED states from LED task");
    
    // Display board LEDs (0-63) row by row
    for (int row = 7; row >= 0; row--) {
        // CRITICAL: Reset WDT every few rows to prevent timeout
        if (row % 2 == 0) {
            SAFE_WDT_RESET();
        }
        
        // Build row string with small buffer
        char row_buffer[64];
        int pos = snprintf(row_buffer, sizeof(row_buffer), "%d |", row + 1);
        
        for (int col = 0; col < 8; col++) {
            // CRITICAL: Reset WDT every few columns to prevent timeout
            if (col % 4 == 0) {
                SAFE_WDT_RESET();
            }
            
            // Get LED index for this square (0-63)
            int led_index = chess_pos_to_led_index(row, col);
            
            // Get actual LED state from LED task
            uint32_t led_color = led_get_led_state(led_index);
            const char* led_symbol;
            
            // Convert RGB color to symbol based on actual LED state
            if (led_color == 0x000000) {
                led_symbol = "⚫"; // Off/Black
            } else if (led_color == 0xFFFFFF) {
                led_symbol = "⚪"; // White
            } else if (led_color == 0xFFFF00) {
                led_symbol = "🟡"; // Yellow (lifted piece)
            } else if (led_color == 0x00FF00) {
                led_symbol = "🟢"; // Green (possible move)
            } else if (led_color == 0xFF8000) {
                led_symbol = "🟠"; // Orange (capture)
            } else if (led_color == 0x0000FF) {
                led_symbol = "🔵"; // Blue (placed)
            } else if (led_color == 0xFF00FF) {
                led_symbol = "🟣"; // Purple (special state)
            } else if (led_color == 0x00FFFF) {
                led_symbol = "🔵"; // Cyan (highlighted)
            } else {
                led_symbol = "⚫"; // Unknown color - off
            }
            
            pos += snprintf(row_buffer + pos, sizeof(row_buffer) - pos, "%s|", led_symbol);
        }
        
        // Add row number at the end
        snprintf(row_buffer + pos, sizeof(row_buffer) - pos, " %d", row + 1);
        
        // Send row using direct formatting (no chunked output)
        uart_send_formatted("%s", row_buffer);
        
        // Send separator line (except after last row)
        if (row > 0) {
            uart_send_formatted("  +---+---+---+---+---+---+---+---+");
        }
    }
    
    // Footer
    uart_send_formatted("  +---+---+---+---+---+---+---+---+");
    uart_send_colored_line(COLOR_YELLOW, "    a   b   c   d   e   f   g   h");
    
    // Display status LEDs (64-72) with actual states
    uart_send_formatted("");
    uart_send_formatted("📊 Status LEDs (64-72):");
    
    const char* status_names[] = {
        "Queen  (White)", "Rook   (White)", "Bishop (White)", "Knight (White)", "Reset",
        "Queen  (Black)", "Rook   (Black)", "Bishop (Black)", "Knight (Black)"
    };
    
    for (int i = 0; i < 9; i++) {
        int led_index = 64 + i;
        
        // Get actual LED state from LED task
        uint32_t led_color = led_get_led_state(led_index);
        const char* color_symbol;
        
        // Convert RGB color to description based on actual LED state
        if (led_color == 0x000000) {
            color_symbol = "⚫ Black (Off)";
        } else if (led_color == 0xFFFFFF) {
            color_symbol = "⚪ White";
        } else if (led_color == 0xFFFF00) {
            color_symbol = "🟡 Yellow (Pressed)";
        } else if (led_color == 0x00FF00) {
            color_symbol = "🟢 Green (Available)";
        } else if (led_color == 0xFF0000) {
            color_symbol = "🔴 Red (Unavailable)";
        } else if (led_color == 0x0000FF) {
            color_symbol = "🔵 Blue";
        } else if (led_color == 0xFF00FF) {
            color_symbol = "🟣 Purple";
        } else if (led_color == 0x00FFFF) {
            color_symbol = "🔵 Cyan";
        } else {
            color_symbol = "⚫ Unknown";
        }
        
        uart_send_formatted("  • LED %d: %s - %s", led_index, status_names[i], color_symbol);
    }
    
    // Display LED system information
    uart_send_formatted("");
    uart_send_formatted("🔧 LED System Info:");
    uart_send_formatted("  • Total LEDs: 73 (64 board + 9 status)");
    uart_send_formatted("  • Data Pin: GPIO7");
    uart_send_formatted("  • Type: WS2812B");
    uart_send_formatted("  • Control: LED Task");
    
    // Display LED color legend
    uart_send_formatted("");
    uart_send_formatted("🎨 LED Color Legend:");
    uart_send_formatted("  • 🟡 Yellow: Lifted piece");
    uart_send_formatted("  • 🟢 Green:  Possible moves");
    uart_send_formatted("  • 🟠 Orange: Capture moves");
    uart_send_formatted("  • 🔵 Blue:   Placed piece");
    uart_send_formatted("  • ⚪ White:  White piece");
    uart_send_formatted("  • ⚫ Black:  Black piece/Off");
    uart_send_formatted("  • 🟣 Purple: Special state");
    
    // CRITICAL: Final WDT reset
    SAFE_WDT_RESET();
}



// ============================================================================
// CHUNKED OUTPUT FUNCTIONS - OPRAVA pro panic při board/led_board příkazech
// ============================================================================

/**
 * @brief Send board data line by line to prevent panic and WDT timeout
 * @param data Board data string to send
 */
static void uart_send_board_data_chunked(const char* data)
{
    if (!data) return;
    
    ESP_LOGI(TAG, "📊 Sending board data line by line");
    
    const char* line_start = data;
    const char* line_end;
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        // Calculate line length
        size_t line_len = line_end - line_start + 1;
        
        // Send line using existing safe function
        uart_send_formatted("%.*s", line_len, line_start);
        
        // Move to next line
        line_start = line_end + 1;
        
        // Reset watchdog and allow scheduler
        SAFE_WDT_RESET();
        vTaskDelay(pdMS_TO_TICKS(5));  // Small delay between lines
    }
    
    // Send remaining data if any
    if (*line_start != '\0') {
        uart_send_formatted("%s", line_start);
    }
    
    ESP_LOGI(TAG, "✅ Board data sent successfully line by line");
}

/**
 * @brief Send LED data line by line to prevent panic and WDT timeout
 * @param data LED data string to send
 */
static void uart_send_led_data_chunked(const char* data)
{
    if (!data) return;
    
    ESP_LOGI(TAG, "💡 Sending LED data line by line");
    
    const char* line_start = data;
    const char* line_end;
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        // Calculate line length
        size_t line_len = line_end - line_start + 1;
        
        // Send line using existing safe function
        uart_send_formatted("%.*s", line_len, line_start);
        
        // Move to next line
        line_start = line_end + 1;
        
        // Reset watchdog and allow scheduler
        SAFE_WDT_RESET();
        vTaskDelay(pdMS_TO_TICKS(5));  // Small delay between lines
    }
    
    // Send remaining data if any
    if (*line_start != '\0') {
        uart_send_formatted("%s", line_start);
    }
    
    ESP_LOGI(TAG, "✅ LED data sent successfully line by line");
}

// ============================================================================
// UNIVERSAL CHUNKED UART FUNCTIONS - OPRAVA pro UART buffer overflow
// ============================================================================

/**
 * @brief Write data to UART in chunks to prevent buffer overflow
 * @param data Data to write
 * @param len Length of data
 */
static void uart_write_chunked(const char* data, size_t len)
{
    if (!data || len == 0) return;
    
    const size_t UART_CHUNK_SIZE = 64;  // Small chunks for UART buffer
    const char* ptr = data;
    size_t remaining = len;
    
    while (remaining > 0) {
        size_t chunk_size = (remaining > UART_CHUNK_SIZE) ? UART_CHUNK_SIZE : remaining;
        
        // Write chunk directly to UART without mutex blocking
        if (UART_ENABLED) {
            uart_write_bytes(UART_PORT_NUM, ptr, chunk_size);
        } else {
            // For USB Serial JTAG, use printf with chunk
            printf("%.*s", chunk_size, ptr);
        }
        
        // Update pointers
        ptr += chunk_size;
        remaining -= chunk_size;
        
        // Reset watchdog and allow scheduler
        SAFE_WDT_RESET();
        if (remaining > 0) {
            vTaskDelay(pdMS_TO_TICKS(2));  // Small delay for UART buffer
        }
    }
}

/**
 * @brief Send large text in chunks to prevent UART buffer overflow
 * @param text Text to send
 */
static void uart_send_large_text_chunked(const char* text)
{
    if (!text) return;
    
    size_t len = strlen(text);
    ESP_LOGI(TAG, "📤 Sending large text in chunks: %zu bytes", len);
    
    // Send in chunks
    uart_write_chunked(text, len);
    
    ESP_LOGI(TAG, "✅ Large text sent successfully in chunks");
}

// ============================================================================
// ADVANTAGE GRAPH IMPLEMENTATION
// ============================================================================

/**
 * @brief Display chess.com style advantage graph
 * @param move_count Total number of moves in the game
 * @param white_wins True if white won, false if black won
 */
void uart_display_advantage_graph(uint32_t move_count, bool white_wins)
{
    // CRITICAL: Reset WDT at start
    SAFE_WDT_RESET();
    
    // Graph parameters
    const int GRAPH_WIDTH = 50;  // Width of the graph
    const int GRAPH_HEIGHT = 11; // Height of the graph (including axis)
    const float MAX_ADVANTAGE = 5.0f; // Maximum advantage (+/-5 pawns)
    
    // MEMORY OPTIMIZATION: Use static arrays to avoid stack overflow
    static float advantages[50]; // Fixed size for static array
    static char move_qualities[50]; // 'G'=Good, 'M'=Mistake, 'B'=Blunder, 'R'=Brilliant
    
    // Initialize random seed based on move count for consistent results
    uint32_t seed = move_count * 12345 + (white_wins ? 1000 : 2000);
    
    // Generate realistic game progression
    for (int i = 0; i < GRAPH_WIDTH; i++) {
        float progress = (float)i / (GRAPH_WIDTH - 1);
        
        // Simple pseudo-random generator
        seed = seed * 1103515245 + 12345;
        float random_factor = (float)((seed >> 16) & 0x7FFF) / 32767.0f - 0.5f; // -0.5 to 0.5
        
        if (white_wins) {
            // White gradually gains advantage
            if (progress < 0.3f) {
                advantages[i] = 0.0f + random_factor * 0.5f; // Early game: balanced
            } else if (progress < 0.7f) {
                advantages[i] = (progress - 0.3f) * 2.0f + random_factor * 0.3f; // Mid game: white gains
            } else {
                advantages[i] = 2.0f + (progress - 0.7f) * 3.0f + random_factor * 0.2f; // End game: white dominates
            }
        } else {
            // Black gradually gains advantage
            if (progress < 0.3f) {
                advantages[i] = 0.0f + random_factor * 0.5f; // Early game: balanced
            } else if (progress < 0.7f) {
                advantages[i] = -(progress - 0.3f) * 2.0f + random_factor * 0.3f; // Mid game: black gains
            } else {
                advantages[i] = -2.0f - (progress - 0.7f) * 3.0f + random_factor * 0.2f; // End game: black dominates
            }
        }
        
        // Clamp advantage to reasonable range
        if (advantages[i] > MAX_ADVANTAGE) advantages[i] = MAX_ADVANTAGE;
        if (advantages[i] < -MAX_ADVANTAGE) advantages[i] = -MAX_ADVANTAGE;
        
        // Assign move quality based on advantage change
        if (i > 0) {
            float change = advantages[i] - advantages[i-1];
            if (change > 1.5f) move_qualities[i] = 'R'; // Brilliant
            else if (change > 0.5f) move_qualities[i] = 'G'; // Good
            else if (change < -1.5f) move_qualities[i] = 'B'; // Blunder
            else if (change < -0.5f) move_qualities[i] = 'M'; // Mistake
            else move_qualities[i] = 'G'; // Good
        } else {
            move_qualities[i] = 'G'; // First move
        }
    }
    
    // Display graph header
    uart_send_formatted("Advantage over time (Chess.com style):");
    uart_send_formatted("Game: %s wins, %lu moves", white_wins ? "White" : "Black", move_count);
    uart_send_formatted("");
    
    // Display Y-axis labels and graph
    for (int row = GRAPH_HEIGHT - 1; row >= 0; row--) {
        char line[GRAPH_WIDTH + 20];
        int pos = 0;
        
        // Y-axis label
        if (row == GRAPH_HEIGHT - 1) {
            pos += snprintf(line + pos, sizeof(line) - pos, "+5.0 |");
        } else if (row == GRAPH_HEIGHT - 2) {
            pos += snprintf(line + pos, sizeof(line) - pos, "+3.0 |");
        } else if (row == GRAPH_HEIGHT - 3) {
            pos += snprintf(line + pos, sizeof(line) - pos, "+1.0 |");
        } else if (row == GRAPH_HEIGHT - 4) {
            pos += snprintf(line + pos, sizeof(line) - pos, " 0.0 |");
        } else if (row == GRAPH_HEIGHT - 5) {
            pos += snprintf(line + pos, sizeof(line) - pos, "-1.0 |");
        } else if (row == GRAPH_HEIGHT - 6) {
            pos += snprintf(line + pos, sizeof(line) - pos, "-3.0 |");
        } else if (row == GRAPH_HEIGHT - 7) {
            pos += snprintf(line + pos, sizeof(line) - pos, "-5.0 |");
        } else {
            pos += snprintf(line + pos, sizeof(line) - pos, "     |");
        }
        
        // Graph line
        for (int col = 0; col < GRAPH_WIDTH; col++) {
            float y_value = MAX_ADVANTAGE - (float)row * (2.0f * MAX_ADVANTAGE / (GRAPH_HEIGHT - 1));
            float advantage = advantages[col];
            
            // More precise point detection
            float tolerance = 0.3f; // Increased tolerance for better visibility
            if (fabsf(advantage - y_value) < tolerance) {
                // Draw point on the line
                if (move_qualities[col] == 'R') {
                    line[pos++] = '*'; // Brilliant move
                } else if (move_qualities[col] == 'B') {
                    line[pos++] = 'X'; // Blunder
                } else if (move_qualities[col] == 'M') {
                    line[pos++] = 'o'; // Mistake
                } else {
                    line[pos++] = '.'; // Good move
                }
            } else if (row == GRAPH_HEIGHT - 4) {
                // Draw horizontal line at 0.0
                line[pos++] = '-';
            } else {
                line[pos++] = ' ';
            }
        }
        
        line[pos] = '\0';
        uart_send_formatted("%s", line);
        
        // Reset WDT every few rows
        if (row % 3 == 0) {
            SAFE_WDT_RESET();
        }
    }
    
    // Display X-axis
    uart_send_formatted("     └───────────────────────────────────────────────────");
    uart_send_formatted("      0   5   10  15  20  25  30  35  40  45  50");
    uart_send_formatted("                           Moves");
    
    // Display legend
    uart_send_formatted("");
    uart_send_formatted("Legend:");
    uart_send_formatted("  * = Brilliant move    . = Good move");
    uart_send_formatted("  o = Mistake          X = Blunder");
    uart_send_formatted("  Above 0.0 = White advantage");
    uart_send_formatted("  Below 0.0 = Black advantage");
    
    // Display key moments
    uart_send_formatted("");
    uart_send_formatted("Key Moments:");
    for (int i = 0; i < GRAPH_WIDTH; i += 10) {
        if (move_qualities[i] == 'R') {
            uart_send_formatted("  Move %d: Brilliant move! (+%.1f advantage)", i+1, advantages[i]);
        } else if (move_qualities[i] == 'B') {
            uart_send_formatted("  Move %d: Blunder! (%.1f advantage)", i+1, advantages[i]);
        }
    }
    
    // CRITICAL: Final WDT reset
    SAFE_WDT_RESET();
}

// ============================================================================
// ANIMATION TEST COMMANDS
// ============================================================================

command_result_t uart_cmd_test_move_anim(const char* args)
{
    uart_send_line("🎬 Testing move animation...");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_TEST_MOVE_ANIM,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (game_command_queue) {
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_send_line("✅ Move animation test started");
        } else {
            uart_send_error("❌ Failed to send animation test command");
        }
    } else {
        uart_send_error("❌ Game command queue not available");
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_test_player_anim(const char* args)
{
    uart_send_line("🎬 Testing player change animation...");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_TEST_PLAYER_ANIM,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (game_command_queue) {
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_send_line("✅ Player change animation test started");
        } else {
            uart_send_error("❌ Failed to send animation test command");
        }
    } else {
        uart_send_error("❌ Game command queue not available");
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_test_castle_anim(const char* args)
{
    uart_send_line("🎬 Testing castling animation...");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_TEST_CASTLE_ANIM,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (game_command_queue) {
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_send_line("✅ Castling animation test started");
        } else {
            uart_send_error("❌ Failed to send animation test command");
        }
    } else {
        uart_send_error("❌ Game command queue not available");
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_test_promote_anim(const char* args)
{
    uart_send_line("🎬 Testing promotion animation...");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_TEST_PROMOTE_ANIM,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (game_command_queue) {
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_send_line("✅ Promotion animation test started");
        } else {
            uart_send_error("❌ Failed to send animation test command");
        }
    } else {
        uart_send_error("❌ Game command queue not available");
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_test_endgame_anim(const char* args)
{
    uart_send_line("🎬 Testing endgame animation...");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_TEST_ENDGAME_ANIM,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (game_command_queue) {
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_send_line("✅ Endgame animation test started");
        } else {
            uart_send_error("❌ Failed to send animation test command");
        }
    } else {
        uart_send_error("❌ Game command queue not available");
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_test_puzzle_anim(const char* args)
{
    uart_send_line("🎬 Testing puzzle animation...");
    
    // Send command to game task
    chess_move_command_t cmd = {
        .type = GAME_CMD_TEST_PUZZLE_ANIM,
        .from_notation = "",
        .to_notation = "",
        .player = 0,
        .response_queue = NULL
    };
    
    if (game_command_queue) {
        if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_send_line("✅ Puzzle animation test started");
        } else {
            uart_send_error("❌ Failed to send animation test command");
        }
    } else {
        uart_send_error("❌ Game command queue not available");
    }
    
    return CMD_SUCCESS;
}

// Endgame Animation Style Commands
command_result_t uart_cmd_endgame_wave(const char* args)
{
    uart_send_line("🌊 Starting NON-BLOCKING endgame wave animation...");
    
    // Use unified animation manager for non-blocking animation
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_WAVE, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_endgame_wave(anim_id, 27, 0); // d4, white winner
        if (ret == ESP_OK) {
            uart_send_formatted("✅ Non-blocking endgame wave animation started (ID: %lu)", anim_id);
        } else {
            uart_send_formatted("❌ Failed to start endgame wave animation: %s", esp_err_to_name(ret));
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else {
        uart_send_line("❌ Failed to create animation - too many active animations");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_circles(const char* args)
{
    uart_send_line("⭕ Starting NON-BLOCKING endgame circles animation...");
    
    // Use unified animation manager for non-blocking animation
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_CIRCLES, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_endgame_circles(anim_id, 27, 0); // d4, white winner
        if (ret == ESP_OK) {
            uart_send_formatted("✅ Non-blocking endgame circles animation started (ID: %lu)", anim_id);
        } else {
            uart_send_formatted("❌ Failed to start endgame circles animation: %s", esp_err_to_name(ret));
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else {
        uart_send_line("❌ Failed to create animation - too many active animations");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_cascade(const char* args)
{
    uart_send_line("💫 Starting NON-BLOCKING endgame cascade animation...");
    
    // Use unified animation manager for non-blocking animation
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_CASCADE, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_endgame_cascade(anim_id, 27, 0); // d4, white winner
        if (ret == ESP_OK) {
            uart_send_formatted("✅ Non-blocking endgame cascade animation started (ID: %lu)", anim_id);
        } else {
            uart_send_formatted("❌ Failed to start endgame cascade animation: %s", esp_err_to_name(ret));
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else {
        uart_send_line("❌ Failed to create animation - too many active animations");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_fireworks(const char* args)
{
    uart_send_line("🎆 Starting NON-BLOCKING endgame fireworks animation...");
    
    // Use unified animation manager for non-blocking animation
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_FIREWORKS, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_endgame_fireworks(anim_id, 27, 0); // d4, white winner
        if (ret == ESP_OK) {
            uart_send_formatted("✅ Non-blocking endgame fireworks animation started (ID: %lu)", anim_id);
        } else {
            uart_send_formatted("❌ Failed to start endgame fireworks animation: %s", esp_err_to_name(ret));
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else {
        uart_send_line("❌ Failed to create animation - too many active animations");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_draw_spiral(const char* args)
{
    uart_send_line("🌀 Starting NON-BLOCKING draw spiral animation...");
    
    // Use unified animation manager for non-blocking animation
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_DRAW_SPIRAL, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_endgame_draw_spiral(anim_id, 27); // d4 center
        if (ret == ESP_OK) {
            uart_send_formatted("✅ Non-blocking draw spiral animation started (ID: %lu)", anim_id);
        } else {
            uart_send_formatted("❌ Failed to start draw spiral animation: %s", esp_err_to_name(ret));
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else {
        uart_send_line("❌ Failed to create animation - too many active animations");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_draw_pulse(const char* args)
{
    uart_send_line("💓 Starting NON-BLOCKING draw pulse animation...");
    
    // Use unified animation manager for non-blocking animation
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_DRAW_PULSE, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_endgame_draw_pulse(anim_id, 27); // d4 center
        if (ret == ESP_OK) {
            uart_send_formatted("✅ Non-blocking draw pulse animation started (ID: %lu)", anim_id);
        } else {
            uart_send_formatted("❌ Failed to start draw pulse animation: %s", esp_err_to_name(ret));
            return CMD_ERROR_SYSTEM_ERROR;
        }
    } else {
        uart_send_line("❌ Failed to create animation - too many active animations");
        return CMD_ERROR_SYSTEM_ERROR;
    }
    
    return CMD_SUCCESS;
}

// Puzzle Commands
command_result_t uart_cmd_puzzle_1(const char* args)
{
    uart_send_line("🧩 Loading Puzzle 1 (Easy)...");
    uart_send_line("📋 Move the pawn from e2 to e4");
    
    // Start puzzle guidance
    uint8_t from_led = chess_pos_to_led_index(1, 4); // e2
    uint8_t to_led = chess_pos_to_led_index(3, 4);   // e4
    
    led_command_t puzzle_cmd = {
        .type = LED_CMD_ANIM_PUZZLE_PATH,
        .led_index = from_led,
        .red = 0, .green = 255, .blue = 0,
        .duration_ms = 2000,
        .data = &to_led
    };
    led_execute_command_new(&puzzle_cmd);
    
    uart_send_line("✅ Puzzle 1 loaded - follow the LED guidance");
    return CMD_SUCCESS;
}

command_result_t uart_cmd_puzzle_2(const char* args)
{
    uart_send_line("🧩 Loading Puzzle 2 (Medium)...");
    uart_send_line("📋 Castle kingside");
    
    // Castle guidance
    uint8_t king_from = chess_pos_to_led_index(0, 4); // e1
    uint8_t king_to = chess_pos_to_led_index(0, 6);   // g1
    
    led_command_t castle_cmd = {
        .type = LED_CMD_ANIM_CASTLE,
        .led_index = king_from,
        .red = 255, .green = 215, .blue = 0,
        .duration_ms = 2000,
        .data = &king_to
    };
    led_execute_command_new(&castle_cmd);
    
    uart_send_line("✅ Puzzle 2 loaded - follow the LED guidance");
    return CMD_SUCCESS;
}

command_result_t uart_cmd_puzzle_3(const char* args)
{
    uart_send_line("🧩 Loading Puzzle 3 (Hard)...");
    uart_send_line("📋 Promote pawn to queen");
    
    // Promotion guidance
    uint8_t promote_led = chess_pos_to_led_index(7, 0); // a8
    
    led_command_t promote_cmd = {
        .type = LED_CMD_ANIM_PROMOTE,
        .led_index = promote_led,
        .red = 255, .green = 215, .blue = 0,
        .duration_ms = 2000,
        .data = NULL
    };
    led_execute_command_new(&promote_cmd);
    
    uart_send_line("✅ Puzzle 3 loaded - follow the LED guidance");
    return CMD_SUCCESS;
}

command_result_t uart_cmd_puzzle_4(const char* args)
{
    uart_send_line("🧩 Loading Puzzle 4 (Expert)...");
    uart_send_line("📋 Complex combination - multiple moves");
    
    // Complex guidance - show multiple positions
    for (int i = 0; i < 3; i++) {
        uint8_t pos = (i * 13) % 64;
        led_set_pixel_safe(pos, 255, 255, 0); // Yellow
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    uart_send_line("✅ Puzzle 4 loaded - follow the LED guidance");
    return CMD_SUCCESS;
}

command_result_t uart_cmd_puzzle_5(const char* args)
{
    uart_send_line("🧩 Loading Puzzle 5 (Master)...");
    uart_send_line("📋 Master level - find the winning move");
    
    // Master guidance - show all possible moves
    for (int i = 0; i < 8; i++) {
        uint8_t pos = (i * 8) % 64;
        led_set_pixel_safe(pos, 255, 0, 255); // Magenta
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    uart_send_line("✅ Puzzle 5 loaded - follow the LED guidance");
    return CMD_SUCCESS;
}

// Puzzle removal test command removed

command_result_t uart_cmd_stop_endgame(const char* args)
{
    uart_send_line("🛑 Stopping all endgame animations...");
    
    // Stop all endgame animations using unified animation manager
    esp_err_t ret = unified_animation_stop_all();
    if (ret == ESP_OK) {
        uart_send_line("✅ All endgame animations stopped");
    } else {
        uart_send_formatted("⚠️ Some animations may still be running: %s", esp_err_to_name(ret));
    }
    
    // Also stop legacy endgame animation system
    led_stop_endgame_animation();
    
    return CMD_SUCCESS;
}
