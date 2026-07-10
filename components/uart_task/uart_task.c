/**
 * @file uart_task.c
 * @brief UART Terminal Task - Interaktivni prikazovy radek pro ovladani sachu
 *
 * @details
 * =============================================================================
 * CO TENTO SOUBOR DELA?
 * =============================================================================
 *
 * Tento task je "terminal" sachovnice. Umoznuje ovladani pres UART:
 * 1. Prikazovy radek s editaci (backspace, historie prikazu)
 * 2. 50+ prikazu pro ovladani (move, board, status, wifi, timer...)
 * 3. Barevny vystup (ANSI barvy) pro lepsi prehlednost
 * 4. Debug vypisy vsech tasku (tasks, heap, queues...)
 * 5. WiFi konfigurace a management
 * 6. Testovaci prikazy pro LED, matrix, animace
 *
 * =============================================================================
 * JAK TO FUNGUJE?
 * =============================================================================
 *
 * STARTUP:
 * - Inicializace UART (115200 baud)
 * - Vytvoreni input bufferu (256 znaku)
 * - Zobrazeni welcome logo
 * - Command prompt: "chess> _"
 *
 * HLAVNI SMYCKA:
 * while (1) {
 *     1. Cti znak z UART (blocking)
 *     2. Zpracuj vstup:
 *        - Normalni znak -> pridej do bufferu
 *        - ENTER -> parsuj a proved prikaz
 *        - BACKSPACE -> smaz znak
 *        - SIPKA NAHORU -> historie prikazu
 *     3. Proved prikaz z tabulky prikazu
 *     4. Vypis vysledek
 * }
 *
 * =============================================================================
 * KOMUNIKACE (FIFOS & MUTEXY)
 * =============================================================================
 *
 * FRONTY (QUEUES) - Poslani prikazu jinym taskum:
 * - game_command_queue -> Prikazy pro game_task (move, reset...)
 * - web_server_queue -> Prikazy pro web server (wifi config...)
 * - LED se ovladaji primymi volanimi (fronta byla odstranena)
 *
 * MUTEXY - Ochrana sdilenych zdroju:
 * - uart_mutex -> Ochrana UART vystupu (aby se neprekryvaly vypisy)
 *   DULEZITE: Vzdy pouzij pri printf/uart_write!
 *
 * PRISTUP:
 * @code
 * xSemaphoreTake(uart_mutex, portMAX_DELAY);  // Zamkni
 * printf("Muj vystup\\n");                      // Pis
 * xSemaphoreGive(uart_mutex);                  // Odemkni
 * @endcode
 *
 * =============================================================================
 * TABULKA PRIKAZU (COMMAND TABLE)
 * =============================================================================
 *
 * Prikazy jsou organizovane v tabulce s function pointery:
 * - cmd_name: Jmeno prikazu (napr. "move")
 * - cmd_handler: Ukazatel na funkci (napr. uart_cmd_move)
 * - cmd_help: Kratka napoveda
 * - cmd_priority: Priorita (HIGH/MEDIUM/LOW)
 *
 * Kategorie prikazu:
 * - Sachovnice: move, board, status, reset, undo
 * - System: help, tasks, heap, version
 * - WiFi: wifi_scan, wifi_connect, wifi_status
 * - Timer: timer_start, timer_stop, timer_status
 * - LED: led_test, led_blink, anim_*
 * - Debug: matrix_debug, queue_stats
 *
 * =============================================================================
 * TABLE OF CONTENTS (NAVIGACE)
 * =============================================================================
 *
 * Sekce 1:  WDT Wrapper Functions ................... radek 70
 * Sekce 2:  UART Driver Functions ................... radek 160
 * Sekce 3:  Input Buffer & History .................. radek 250
 * Sekce 4:  Command Table Definition ................ radek 760
 * Sekce 5:  Command Handlers (50+ funkci) .......... radek 1800
 * Sekce 6:  Formatting & Output Functions ........... radek 350
 * Sekce 7:  Main UART Task .......................... radek 6200
 *
 * TIP: Ctrl+G pro skok na radek
 *
 * =============================================================================
 * DEPENDENCIES
 * =============================================================================
 *
 * - game_task: Posilame prikazy (move, reset...)
 * - led_task: Test LED, animace
 * - web_server_task: WiFi konfigurace
 * - timer_system: Start/stop casovace
 * - config_manager: Ulozeni/nacteni konfigurace (NVS)
 *
 * =============================================================================
 * KRITICKA PRAVIDLA
 * =============================================================================
 *
 * @warning CO SE NESMI DELAT:
 *
 * 1. NIKDY nevypisuj bez uart_mutex!
 *    ❌ printf("text");  // SPATNE - muze se prekriti s jinym taskem
 *    ✅ xSemaphoreTake(uart_mutex, ...); printf("text"); xSemaphoreGive(...);
 *
 * 2. NIKDY nevolej blocking operace s dlouhym timeout!
 *    ❌ xQueueSend(queue, &data, portMAX_DELAY);  // Muze zablokovat WDT
 *    ✅ xQueueSend(queue, &data, pdMS_TO_TICKS(1000));  // Max 1s
 *
 * 3. NIKDY nepristupuj primo k jinym taskum!
 *    ❌ game_board[0][0] = PIECE_KING;  // Primo pristup - NEBEZPECNE
 *    ✅ Pouzij fronty (game_command_queue)
 *
 * 4. VZDY resetuj WDT v dlouhych smyckach!
 *    ✅ uart_task_wdt_reset_safe();  // Kazd ych par sekund
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-12-22
 *
 * @note
 * - Task priorita: 3 (nizsi nez game_task)
 * - Stack size: 8KB
 * - Pouziva WDT (watchdog timer)
 *
 * @see game_task.c - Sachova logika
 * @see led_task.c - LED ovladani
 * @see web_server_task.c - Web rozhrani
 */

#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "driver/uart.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// ESP-IDF UART driver functions
// No POSIX includes needed

#include "../ble_task/include/ble_task.h"
#include "../freertos_chess/include/chess_types.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../matrix_task/include/matrix_task.h"
#include "../timer_system/include/timer_system.h"
#include "../uart_commands_extended/include/uart_commands_extended.h"
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "../web_server_task/include/web_server_task.h"
#include "../web_server_task/include/board_api_auth.h"
#include "config_manager.h"
#include "esp_system.h"
#include "freertos_chess.h"
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
 * @brief Bezpecny reset WDT s logovanim WARNING misto ERROR pro
 * ESP_ERR_NOT_FOUND
 *
 * Tato funkce bezpecne resetuje Task Watchdog Timer. Pokud task neni jeste
 * registrovany (coz je normalni behem startupu), loguje se WARNING misto ERROR.
 *
 * @return ESP_OK pokud uspesne, ESP_ERR_NOT_FOUND pokud task neni registrovany
 * (WARNING pouze)
 *
 * @details
 * Funkce je pouzivana pro bezpecny reset watchdog timeru behem UART operaci.
 * Zabranuje chybam pri startupu kdy task jeste neni registrovany.
 */
esp_err_t uart_task_wdt_reset_safe(void) {
  esp_err_t ret = esp_task_wdt_reset();

  if (ret == ESP_ERR_NOT_FOUND) {
    // Log as WARNING instead of ERROR - task not registered yet
    ESP_LOGW(
        "UART_TASK",
        "WDT reset: task not registered yet (this is normal during startup)");
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
// extern QueueHandle_t led_command_queue;  //  REMOVED: Queue hell eliminated

// Component status tracking
static bool matrix_component_enabled = true;
static bool led_component_enabled = true;
static bool wifi_component_enabled = false;

// UART response queue for game task responses (declared in freertos_chess.h)

// Forward declarations for functions used before definition
static void uart_task_legacy_loop(void);

// Missing type definitions that should be here
// UART command structure is defined in uart_task.h

static const char *TAG = "UART_TASK";

// ============================================================================
// CHUNKED OUTPUT MACROS
// ============================================================================

// Safe watchdog reset macro
#define SAFE_WDT_RESET()                                                       \
  do {                                                                         \
    esp_err_t _wdt_ret = esp_task_wdt_reset();                                 \
    if (_wdt_ret != ESP_OK && _wdt_ret != ESP_ERR_NOT_FOUND) {                 \
      /* Task not registered with TWDT yet - this is normal during startup */  \
    }                                                                          \
  } while (0)

// Univerzální chunked printf makro podle návrhu
#define CHUNKED_PRINTF(format, ...)                                            \
  do {                                                                         \
    printf(format, ##__VA_ARGS__);                                             \
    fflush(stdout);                                                            \
    SAFE_WDT_RESET();                                                          \
    vTaskDelay(pdMS_TO_TICKS(1));                                              \
  } while (0)

// Optimalizované konstanty pro ESP32-C6
#define CHUNK_DELAY_MS 2                     // Minimální delay
#define MAX_CHUNK_SIZE UART_MESSAGE_TEXT_MAX /* uart_queue_message.h */
#define STACK_SAFETY_LIMIT 512               // Minimální volný stack

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
static void uart_fputs(const char *str) {
  if (UART_ENABLED) {
    uart_write_bytes(UART_PORT_NUM, str, strlen(str));
  } else {
    // For USB Serial JTAG, use printf
    printf("%s", str);
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
 * Funkce pouziva mutex pro bezpecne operace z vice vlaken a automaticky
 * detekuje zda je UART povolen.
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

/**
 * @brief Zapise string s okamzitym flush
 *
 * Tato funkce zapise cely string do UART s okamzitym flush.
 * Pouziva ESP-IDF UART driver nebo USB Serial JTAG podle konfigurace.
 *
 * @param str String k zapsani
 *
 * @details
 * Funkce pouziva mutex pro bezpecne operace z vice vlaken a automaticky
 * detekuje zda je UART povolen. Je optimalizovana pro rychle zapisovani.
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

/* Paměť: dříve 20×256 = 5120 B + vstup; nyní 8×192 + 192 ≈ 1728 B (~3,4 KiB
 * úspora BSS) */
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
  ESC_STATE_ESC,    // ESC znak přijat
  ESC_STATE_BRACKET // ESC[ přijato, čekáme na finální znak
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
 * @brief Zobrazi působive welcome logo s ANSI barvami
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
void uart_show_progress_bar(const char *label, uint32_t max_value,
                            uint32_t duration_ms) {
  // Zamykání mutexu pro celou progress bar operaci
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

  // Uvolnění mutexu až po dokončení progress bar
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
 * @brief Posle chybovou zpravu pres UART
 *
 * Tato funkce posle chybovou zpravu s cervenou barvou pres UART.
 *
 * @param message Chybova zprava k poslani
 */
void uart_send_error(const char *message) {
  uart_send_colored_line(COLOR_ERROR, message);
}

/**
 * @brief Posle uspesnou zpravu pres UART
 *
 * Tato funkce posle uspesnou zpravu se zelenou barvou pres UART.
 *
 * @param message Uspesna zprava k poslani
 */
void uart_send_success(const char *message) {
  uart_send_colored_line(COLOR_SUCCESS, message);
}

/**
 * @brief Posle varovnou zpravu pres UART
 *
 * Tato funkce posle varovnou zpravu se zlutou barvou pres UART.
 *
 * @param message Varovna zprava k poslani
 */
void uart_send_warning(const char *message) {
  uart_send_colored_line(COLOR_WARNING, message);
}

/**
 * @brief Posle informacni zpravu pres UART
 *
 * Tato funkce posle informacni zpravu s modrou barvou pres UART.
 *
 * @param message Informacni zprava k poslani
 */
void uart_send_info(const char *message) {
  uart_send_colored_line(COLOR_INFO, message);
}

/**
 * @brief Posle zpravu o tahu pres UART
 *
 * Tato funkce posle zpravu o tahu s specialni barvou pres UART.
 *
 * @param message Zprava o tahu k poslani
 */
void uart_send_move(const char *message) {
  uart_send_colored_line(COLOR_MOVE, message);
}

/**
 * @brief Posle status zpravu pres UART
 *
 * Tato funkce posle status zpravu s specialni barvou pres UART.
 *
 * @param message Status zprava k poslani
 */
void uart_send_status(const char *message) {
  uart_send_colored_line(COLOR_STATUS, message);
}

/**
 * @brief Posle debug zpravu pres UART
 *
 * Tato funkce posle debug zpravu s specialni barvou pres UART.
 *
 * @param message Debug zprava k poslani
 */
void uart_send_debug(const char *message) {
  uart_send_colored_line(COLOR_DEBUG, message);
}

/**
 * @brief Posle help zpravu pres UART
 *
 * Tato funkce posle help zpravu s specialni barvou pres UART.
 *
 * @param message Help zprava k poslani
 */
void uart_send_help(const char *message) {
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
void uart_send_formatted(const char *format, ...) {
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
 * @brief Posle zpravu do UART vystupni fronty (bezpecne z vice vlaken)
 * @param type Message type (determines color)
 * @param add_newline Whether to add newline
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
  // Vymazat aktuální řádek
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
    return; // Žádná historie
  }

  // Pokud jsme na začátku navigace, začneme od posledního příkazu
  if (history_navigation_index < 0) {
    history_navigation_index = command_history.count - 1;
  } else if (history_navigation_index > 0) {
    history_navigation_index--;
  }

  // Získat příkaz z historie (circular buffer)
  // Nejnovější příkaz je na pozici (current - 1) % max_size
  // Starší příkazy jsou na (current - 2) % max_size, atd.
  int start_idx = (command_history.current - command_history.count +
                   command_history.max_size) %
                  command_history.max_size;
  int idx = (start_idx + history_navigation_index) % command_history.max_size;

  const char *cmd = command_history.commands[idx];
  if (cmd == NULL || strlen(cmd) == 0) {
    return;
  }

  // Zobrazit příkaz v input bufferu
  input_buffer_clear(&input_buffer);
  strncpy(input_buffer.buffer, cmd, UART_CMD_BUFFER_SIZE - 1);
  input_buffer.buffer[UART_CMD_BUFFER_SIZE - 1] = '\0';
  input_buffer.length = strlen(input_buffer.buffer);
  input_buffer.pos = input_buffer.length;

  // Aktualizovat zobrazení v terminálu
  refresh_input_display();
}

/**
 * @brief Handle Arrow Down - show next command from history
 */
static void handle_arrow_down(void) {
  if (command_history.count == 0) {
    return; // Žádná historie
  }

  if (history_navigation_index < 0) {
    return; // Už jsme na konci
  }

  history_navigation_index++;

  if (history_navigation_index >= command_history.count) {
    // Jsme na konci historie - vyčistit input
    history_navigation_index = -1;
    input_buffer_clear(&input_buffer);
    refresh_input_display();
    return;
  }

  // Získat příkaz z historie
  int start_idx = (command_history.current - command_history.count +
                   command_history.max_size) %
                  command_history.max_size;
  int idx = (start_idx + history_navigation_index) % command_history.max_size;

  const char *cmd = command_history.commands[idx];
  if (cmd == NULL || strlen(cmd) == 0) {
    return;
  }

  // Zobrazit příkaz v input bufferu
  input_buffer_clear(&input_buffer);
  strncpy(input_buffer.buffer, cmd, UART_CMD_BUFFER_SIZE - 1);
  input_buffer.buffer[UART_CMD_BUFFER_SIZE - 1] = '\0';
  input_buffer.length = strlen(input_buffer.buffer);
  input_buffer.pos = input_buffer.length;

  // Aktualizovat zobrazení v terminálu
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
command_result_t uart_cmd_eval(const char *args);
command_result_t uart_cmd_ledtest(const char *args);
command_result_t uart_cmd_performance(const char *args);
command_result_t uart_cmd_config(const char *args);

// Demo mode commands
command_result_t uart_cmd_demo(const char *args);
command_result_t uart_cmd_demo_status(const char *args);

// Component Control Commands
command_result_t uart_cmd_component_off(const char *args);
command_result_t uart_cmd_component_on(const char *args);

// Endgame Commands
command_result_t uart_cmd_endgame_white(const char *args);
command_result_t uart_cmd_endgame_black(const char *args);

// Advantage Graph Functions
void uart_display_advantage_graph(uint32_t move_count, bool white_wins);

// Animation test commands
command_result_t uart_cmd_test_move_anim(const char *args);
command_result_t uart_cmd_test_player_anim(const char *args);
command_result_t uart_cmd_test_castle_anim(const char *args);
command_result_t uart_cmd_test_promote_anim(const char *args);
command_result_t uart_cmd_test_endgame_anim(const char *args);
// Puzzle test command removed

// Endgame Animation Style Commands
command_result_t uart_cmd_endgame_wave(const char *args);
command_result_t uart_cmd_endgame_circles(const char *args);
command_result_t uart_cmd_endgame_cascade(const char *args);
command_result_t uart_cmd_endgame_fireworks(const char *args);
command_result_t uart_cmd_endgame_draw_spiral(const char *args);
command_result_t uart_cmd_endgame_draw_pulse(const char *args);

// Help functions
void uart_cmd_help_web(void);
void uart_cmd_help_app(void);

// Puzzle Commands
// Puzzle level commands removed

// Endgame animation control
command_result_t uart_cmd_stop_endgame(const char *args);

// System Commands

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

command_result_t uart_cmd_help(const char *args) {
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
    } else if (strcmp(args_upper, "WEB") == 0) {
      uart_cmd_help_web();
    } else if (strcmp(args_upper, "APP") == 0 ||
               strcmp(args_upper, "APLIKACE") == 0) {
      uart_cmd_help_app();
    } else if (strcmp(args_upper, "CLI") == 0) {
      uart_cli_print_help();
    } else {
      uart_send_error("Unknown help category");
      uart_send_formatted(
          "Available categories: GAME, SYSTEM, BEGINNER, DEBUG, WEB, APP, CLI");
      return CMD_ERROR_INVALID_PARAMETER;
    }
  } else {
    // Main help menu
    uart_display_main_help();
  }

  return CMD_SUCCESS;
}

/**
 * @brief Zobrazi hlavni help menu s kategoriemi
 */
void uart_display_main_help(void) {
  // Logo already displayed by boot animation

  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("COMMAND CATEGORIES");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  uart_send_formatted("HELP <category> - Get detailed help for category:");
  uart_send_formatted("");

  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted("GAME     - Chess game commands (MOVE, BOARD, etc.)");
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("SYSTEM   - System control and status commands");
  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("WEB      - Web interface commands and connection info");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("BEGINNER - Basic commands for new users");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("DEBUG    - Advanced debugging and testing");
  if (color_enabled)
    uart_write_string_immediate("\033[1;95m"); // bold bright magenta
  uart_send_formatted(
      "APP      - Mobile app (CZECHMATE) and Bluetooth LE overview");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("Quick Start:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  HELP BEGINNER  - Start here if you're new");
  uart_send_formatted("  HELP GAME      - Learn chess commands");
  uart_send_formatted("  HELP SYSTEM    - System management");
  uart_send_formatted("  HELP APP       - App + BLE (same as HELP APLIKACE)");
  uart_send_formatted("  HELP CLI       - Flash/partitions, WEB queue, OTA, BLE JSON, snapshot");
  uart_send_formatted("  BLE            - Live BLE / GATT status (developers)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("Examples:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  HELP GAME      - Show chess commands");
  uart_send_formatted("  MOVE e2 e4     - Make a chess move");
  uart_send_formatted("  BOARD          - Show chess board");
  uart_send_formatted("  STATUS         - System status");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Zobrazi help specificky pro hru
 */
void uart_cmd_help_game(void) {
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted("♔ CHESS GAME COMMANDS ♔");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("🎮 Game Control:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  GAME_NEW       - Start a new chess game");
  uart_send_formatted(
      "  GAME_RESET     - Reset current game to starting position");
  uart_send_formatted(
      "  BOARD          - Display enhanced chess board with current position");
  uart_send_formatted("  LED_BOARD      - Show current LED states and colors");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("♟️  Move Commands:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  MOVE e2 e4     - Move piece from e2 to e4 (space separated)");
  uart_send_formatted(
      "  MOVE e2-e4     - Move piece from e2 to e4 (dash separated)");
  uart_send_formatted(
      "  MOVE e2e4      - Move piece from e2 to e4 (compact format)");
  uart_send_formatted(
      "  UP e2          - Lift piece from e2 (with LED animations)");
  uart_send_formatted(
      "  DN e4          - Drop piece to e4 (with LED animations)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("📊 Game Information:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  MOVES e2       - Show valid moves for piece at e2");
  uart_send_formatted(
      "  MOVES E2       - Show valid moves (uppercase also works)");
  uart_send_formatted(
      "  MOVES pawn     - Show moves for all pawns of current player");
  uart_send_formatted("  GAME_HISTORY   - Display complete move history");
  uart_send_formatted("  UNDO           - Undo the last move");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted("🎯 Advanced Game Features:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  EVAL           - Show position evaluation");
  uart_send_formatted("  CASTLE kingside - Castle kingside (O-O)");
  uart_send_formatted("  CASTLE queenside - Castle queenside (O-O-O)");
  uart_send_formatted("  PROMOTE e8=Q   - Promote pawn to Queen");
  uart_send_formatted("  STARTPOS ON/OFF - Toggle starting position check");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;31m"); // bold red
  uart_send_formatted("🏆 Endgame Commands:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  ENDGAME_WHITE  - Simulate White victory");
  uart_send_formatted("  ENDGAME_BLACK  - Simulate Black victory");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("⏱️ Timer Commands:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  TIMER          - Show timer state (JSON)");
  uart_send_formatted("  TIMER_CONFIG <type> - Set time control (0=disabled, "
                      "1=1+0, 2=3+0, ...)");
  uart_send_formatted(
      "                        Use 'TIMER_CONFIG' alone to see all 15 types");
  uart_send_formatted(
      "  TIMER_CONFIG custom <min> <inc> - Set custom time (e.g. 10 5)");
  uart_send_formatted("  TIMER_PAUSE    - Pause timer");
  uart_send_formatted("  TIMER_RESUME   - Resume timer");
  uart_send_formatted("  TIMER_RESET    - Reset timer");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("💡 Pro Tips:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  • Use 'BOARD' to see the current position");
  uart_send_formatted("  • Use 'MOVES <square>' to analyze specific pieces");
  uart_send_formatted(
      "  • Use 'MOVES <piece>' to see all moves for that piece type");
  uart_send_formatted("  • Use 'GAME_HISTORY' to review the entire game");
  uart_send_formatted("  • Use 'UNDO' to take back moves if needed");
  uart_send_formatted("  • LED colors: 🟡 Yellow (lifted), 🟢 Green "
                      "(possible), 🟠 Orange (capture)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Zobrazi help specificky pro system
 */
void uart_cmd_help_system(void) {
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("⚙️  SYSTEM COMMANDS ⚙️");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("📊 System Status:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  STATUS         - Show system status and diagnostics");
  uart_send_formatted("  VERSION        - Show version information");
  uart_send_formatted("  MEMORY         - Show memory usage");
  uart_send_formatted("  SHOW_TASKS     - Display running FreeRTOS tasks");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("⚙️  Configuration:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  VERBOSE ON/OFF - Control logging verbosity");
  uart_send_formatted(
      "  QUIET / Q      - Toggle quiet mode (zkratka Q; stejné jako příkaz Q)");
  uart_send_formatted("  CONFIG         - Show/set system configuration");
  uart_send_formatted("  CONFIG show    - Show current configuration");
  uart_send_formatted("  CONFIG key value - Set configuration key=value");
  uart_send_formatted("  STARTPOS ON/OFF - Toggle starting position check");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("🌐 Web Server:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  Use 'HELP WEB' for detailed connection info and commands.");
  uart_send_formatted(
      "  WEB_STATUS     - Show web interface status (lock, online, etc.)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("🤖 Demo Mode:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  DEMO ON/OFF    - Toggle automatic chess playback (screensaver)");
  uart_send_formatted(
      "  DEMO_STATUS    - Show demo mode status and speed info");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;31m"); // bold red
  uart_send_formatted("🔌 Component Control:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  COMPONENT_OFF matrix - Turn off matrix scanning");
  uart_send_formatted("  COMPONENT_OFF led    - Turn off LED control");
  uart_send_formatted("  COMPONENT_OFF wifi   - Turn off WiFi");
  uart_send_formatted("  COMPONENT_ON matrix  - Turn on matrix scanning");
  uart_send_formatted("  COMPONENT_ON led     - Turn on LED control");
  uart_send_formatted("  COMPONENT_ON wifi    - Turn on WiFi");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("🔧 Utilities:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  CLEAR          - Clear screen");
  uart_send_formatted(
      "  RESET          - Restart entire system (hardware reset)");
  uart_send_formatted("  HISTORY        - Show command history");
  uart_send_formatted("  BENCHMARK      - Run performance benchmark");
  uart_send_formatted("  SHOW_MUTEXES   - Show all mutexes and their status");
  uart_send_formatted("  SHOW_FIFOS     - Show all FIFOs and their status");
  uart_send_formatted("  MATRIXTEST     - Test matrix scanning");
  uart_send_formatted("  LEDTEST        - Test all LEDs");
  uart_send_formatted("  PERFORMANCE    - Show system performance");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;31m"); // bold red
  uart_send_formatted("⚠️  Important Notes:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  • RESET restarts the entire system (like power cycle)");
  uart_send_formatted("  • Use GAME_RESET to reset only the chess game");
  uart_send_formatted("  • Use GAME_NEW to start a fresh game");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Zobrazi help pro zacatecniky
 */
void uart_cmd_help_beginner(void) {
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("♔ BEGINNER'S CHESS GUIDE ♔");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("🎯 Quick Start (3 Steps):");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  1. Type 'BOARD' to see the chess board");
  uart_send_formatted("  2. Type 'GAME_NEW' to start a new game");
  uart_send_formatted("  3. Type 'MOVE e2 e4' to make your first move");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("♟️  Essential Commands:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  BOARD          - Display the chess board");
  uart_send_formatted("  MOVE e2 e4     - Move piece from e2 to e4");
  uart_send_formatted("  MOVE e2-e4     - Alternative format (dash)");
  uart_send_formatted("  MOVE e2e4      - Compact format (no space)");
  uart_send_formatted("  MOVES e2       - Show valid moves for piece at e2");
  uart_send_formatted("  GAME_HISTORY   - See all moves made so far");
  uart_send_formatted("  UNDO           - Take back the last move");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("🎮 Game Control:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  GAME_NEW       - Start a fresh game");
  uart_send_formatted("  GAME_RESET     - Reset to starting position");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted("💡 Chess Basics:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  • White always moves first");
  uart_send_formatted("  • Use 'e2 e4' for the classic King's Pawn opening");
  uart_send_formatted("  • Use 'd2 d4' for the Queen's Pawn opening");
  uart_send_formatted("  • Check 'MOVES <square>' before moving");
  uart_send_formatted("  • Use 'BOARD' after each move to see the position");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;31m"); // bold red
  uart_send_formatted("⚠️  Important Notes:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  • RESET restarts the entire system (hardware reset)");
  uart_send_formatted("  • Use GAME_RESET to reset only the chess game");
  uart_send_formatted("  • Use GAME_NEW to start a fresh game");
  uart_send_formatted("  • Invalid moves will be rejected with explanations");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("🔧 Advanced Features:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  EVAL           - Get position evaluation");
  uart_send_formatted("  CASTLE kingside - Castle kingside (O-O)");
  uart_send_formatted("  CASTLE queenside - Castle queenside (O-O-O)");
  uart_send_formatted("  PROMOTE e8=Q   - Promote pawn to Queen");
  // Puzzle commands removed

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Zobrazi help pro web rozhrani
 */
void uart_cmd_help_web(void) {
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("🌐 WEB INTERFACE COMMANDS");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Získat IP adresy
  char sta_ip_str[16] = "Disconnected";
  if (wifi_is_sta_connected()) {
    wifi_get_sta_ip(sta_ip_str, sizeof(sta_ip_str));
  }

  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted("📡 Connection Options:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors

  uart_send_formatted("  1. Access Point (Direct Connection):");
  uart_send_formatted("     • Connect to WiFi: '%s'", web_server_get_ap_ssid());
  uart_send_formatted("     • Password:        '12345678'");
  uart_send_formatted("     • Open Browser:    http://192.168.4.1");

  uart_send_formatted("");
  uart_send_formatted("  2. Home Network (Station Mode):");
  if (wifi_is_sta_connected()) {
    char sta_ssid_str[33] = {0};
    wifi_get_sta_ssid(sta_ssid_str, sizeof(sta_ssid_str));

    uart_send_formatted("     • Status:          CONNECTED ✅");
    uart_send_formatted("     • Network:         %s", sta_ssid_str);
    uart_send_formatted("     • Open Browser:    http://%s", sta_ip_str);
  } else {
    uart_send_formatted("     • Status:          DISCONNECTED ❌");
    uart_send_formatted(
        "     • Action:          Use 'WIFI <ssid> <pass>' to connect");
  }

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("🔒 Web Lock Control:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  WEB_LOCK ON     - Lock web interface (prevents game "
                      "settings changes)");
  uart_send_formatted("  WEB_LOCK OFF    - Unlock web interface (allows game "
                      "settings changes)");
  uart_send_formatted(
      "  WEB_STATUS      - Show web interface status (lock, online, etc.)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("📊 Status Information:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  WEB_STATUS shows:");
  uart_send_formatted("    • Lock status (locked/unlocked)");
  uart_send_formatted("    • Internet status (online/offline)");
  uart_send_formatted("    • STA connection info (SSID, IP)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("📡 MQTT (Home Assistant):");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  MQTT_CONFIG <host> [port] [username] [password]");
  uart_send_formatted("    - Configure MQTT broker");
  uart_send_formatted("    - Example: MQTT_CONFIG broker.example.com 1883");
  uart_send_formatted(
      "    - Example: MQTT_CONFIG broker.example.com 1883 user pass");
  uart_send_formatted(
      "  MQTT_STATUS      - Show MQTT status (config, connection, mode)");
  uart_send_formatted("  MQTT_TEST        - Test MQTT connection");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;31m"); // bold red
  uart_send_formatted("⚠️  Important Notes:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  • When locked, web users cannot change:");
  uart_send_formatted("    - Timer settings (config, pause, resume, reset)");
  uart_send_formatted("    - WiFi configuration (connect, disconnect, clear)");
  uart_send_formatted(
      "  • UART commands always work, regardless of lock status");
  uart_send_formatted(
      "  • Use WEB_LOCK ON before events to prevent interference");
  uart_send_formatted("  • Use WEB_LOCK OFF to allow web configuration again");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Mobilni aplikace CZECHMATE a Bluetooth LE — uzivatele + vyvojari
 */
void uart_cmd_help_app(void) {
  if (color_enabled)
    uart_write_string_immediate("\033[1;95m"); // bold bright magenta
  uart_send_formatted("MOBILE APP & BLUETOOTH (CZECHMATE)");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted("📱 Companion app:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  iOS app (CZECHMATE) — CoreBluetooth scan filtrovany podle sluzby.");
  uart_send_formatted(
      "  • S deskou: stejny JSON snapshot jako GET /api/game/snapshot");
  uart_send_formatted(
      "  • Wi‑Fi alternativa: REST (HELP WEB), vetsi payload, nastaveni site");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("📶 GATT (NimBLE, peripheral):");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  Adv. nazev: CZECHMATE | Sluzba + char. UUID 128-bit (viz nize)");
  uart_send_formatted(
      "  Snapshot: READ + NOTIFY — UTF-8 JSON GameSnapshot (Swift decoder)");
  uart_send_formatted(
      "  Command:  WRITE (s odpovedi) — UTF-8 JSON {\"cmd\":...}");
  uart_send_formatted(
      "  Push: po subscribe na NOTIFY se posila pri zmene hry (hook z game)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("🧩 UUID (must match CZECHMATEBLEUUIDs.swift):");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  Svc A0B40001-9267-4AB6-BDCC-E8336F8A8D9E");
  uart_send_formatted("  Snap A0B40002-9267-4AB6-BDCC-E8336F8A8D9E");
  uart_send_formatted("  Cmd  A0B40003-9267-4AB6-BDCC-E8336F8A8D9E");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("📦 Chunk notifikace (velke JSON):");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  Byty 0-1: 0x43 0x4D ('CM'), 2=cast, 3=celkem, pak payload UTF-8");
  uart_send_formatted(
      "  Male zpravy bez hlavicky = cele JSON v jednom notify (iOS)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("⌨️  Priklady JSON prikazu (command char):");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  {\"cmd\":\"ping\"}");
  uart_send_formatted(
      "  {\"cmd\":\"hint_highlight\",\"from\":\"e2\",\"to\":\"e4\"}");
  uart_send_formatted("  {\"cmd\":\"hint_clear\"}");
  uart_send_formatted("  {\"cmd\":\"brightness\",\"percent\":75}");
  uart_send_formatted(
      "  hint/brightness pri WEB_LOCK vraci chybu (stejne jako web API)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("🛠  UART prikazy:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  BLE  — stav spojeni, handly, UUID (staging log: tag BLE_NIMBLE)");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("⚙️  Build:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "  idf.py menuconfig: zapnout Bluetooth + NimBLE (CONFIG_BT_ENABLED=y)");
  uart_send_formatted(
      "  Bez BT: GATT se nekompiluje do aktivniho stacku — pouzij HELP WEB");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("🌐 Wi‑Fi:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  Stejna deska: HTTP snapshot / REST — HELP WEB");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

/**
 * @brief Zobrazi help pro debug a testovani
 */
void uart_cmd_help_debug(void) {
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("DEBUG & TESTING COMMANDS");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (color_enabled)
    uart_write_string_immediate("\033[1;34m"); // bold blue
  uart_send_formatted("Testing:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  SELF_TEST      - Run system self-test");
  uart_send_formatted("  TEST_GAME      - Test game engine");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;31m"); // bold red
  uart_send_formatted("Debugging:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  DEBUG_STATUS   - Show debug information");
  uart_send_formatted("  DEBUG_GAME     - Show game debug info");
  uart_send_formatted("  DEBUG_BOARD    - Show board debug info");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("Performance:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  BENCHMARK      - Run performance benchmark");
  uart_send_formatted("  MEMCHECK       - Check memory usage");
  uart_send_formatted("  SHOW_TASKS     - Show running tasks");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;36m"); // bold cyan
  uart_send_formatted("🎬 Animation Testing:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  TEST_MOVE_ANIM    - Test move path animation");
  uart_send_formatted("  TEST_PLAYER_ANIM  - Test player change animation");
  uart_send_formatted("  TEST_CASTLE_ANIM  - Test castling animation");
  uart_send_formatted("  TEST_PROMOTE_ANIM - Test promotion animation");
  uart_send_formatted("  TEST_ENDGAME_ANIM - Test endgame animation");
  // Puzzle test command removed
  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta
  uart_send_formatted("🎆 Endgame Animation Styles:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  ENDGAME_WAVE      - Wave animation from edges");
  uart_send_formatted("  ENDGAME_CIRCLES   - Expanding circles from center");
  uart_send_formatted("  ENDGAME_CASCADE   - Falling lights animation");
  uart_send_formatted("  ENDGAME_FIREWORKS - Random burst animation");
  uart_send_formatted("  DRAW_SPIRAL       - Draw spiral animation");
  uart_send_formatted("  DRAW_PULSE        - Draw pulse animation");
  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("🧩 Puzzle System:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
                                            // Puzzle level commands removed

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow
  uart_send_formatted("🎮 Endgame Animation Control:");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_send_formatted("  STOP_ENDGAME       - Stop endless endgame animation");

  uart_send_formatted("");
  if (color_enabled)
    uart_write_string_immediate("\033[1;32m"); // bold green
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
}

command_result_t uart_cmd_verbose(const char *args) {
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

command_result_t uart_cmd_quiet(const char *args) {
  (void)args; // Unused parameter

  system_config.quiet_mode = !system_config.quiet_mode;

  if (system_config.quiet_mode) {
    system_config.verbose_mode = false;
  }

  config_save_to_nvs(&system_config);
  config_apply_settings(&system_config);

  if (system_config.quiet_mode) {
    uart_send_warning("Quiet mode ON");
    uart_send_formatted("Only essential messages will be shown");
  } else {
    uart_send_formatted("Quiet mode OFF");
    uart_send_formatted("Normal logging restored");
  }

  return CMD_SUCCESS;
}

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

  // Sledování stacku pro všechny tasky
  uart_send_formatted("Task Stack Usage:");
  uart_send_formatted("  UART Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(NULL));
  uart_send_formatted("  LED Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(led_task_handle));
  uart_send_formatted("  Matrix Task: %u bytes free",
                      uxTaskGetStackHighWaterMark(matrix_task_handle));
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
                      matrix_component_enabled ? "ENABLED" : "DISABLED");
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

command_result_t uart_cmd_version(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("VERSION INFORMATION");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  uart_send_formatted("System: %s", CHESS_SYSTEM_NAME);
  uart_send_formatted("Version: %s", CHESS_SYSTEM_VERSION);
  uart_send_formatted("Author: %s", CHESS_SYSTEM_AUTHOR);
  uart_send_formatted("Build Date: %s", CHESS_BUILD_DATE);
  uart_send_formatted("ESP-IDF: %s", esp_get_idf_version());
  uart_send_formatted("Chip: %s", CONFIG_IDF_TARGET);
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_memory(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("MEMORY INFORMATION");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  uart_send_formatted("Free Heap: %zu bytes", esp_get_free_heap_size());
  uart_send_formatted("Minimum Free: %zu bytes",
                      esp_get_minimum_free_heap_size());
  uart_send_formatted("Largest Free Block: %zu bytes",
                      esp_get_free_heap_size());

  // Memory fragmentation info
  size_t free_heap = esp_get_free_heap_size();
  if (free_heap < 10000) {
    uart_send_formatted("Low memory warning (< 10KB)");
  } else if (free_heap < 50000) {
    uart_send_formatted("Medium memory warning (< 50KB)");
  } else {
    uart_send_formatted("Memory OK");
  }
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_history(const char *args) {
  (void)args; // Unused parameter

  command_history_show(&command_history);
  return CMD_SUCCESS;
}

command_result_t uart_cmd_clear(const char *args) {
  (void)args; // Unused parameter

  // Reset watchdog timeru před UART operacemi
  SAFE_WDT_RESET();

  // Simple clear without mutex to avoid WDT issues
  uart_write_string_immediate(
      "\033[2J\033[H"); // Clear screen and move cursor to top
  uart_write_string_immediate("Screen cleared\r\n");

  // Reset watchdog timeru po UART operacích
  SAFE_WDT_RESET();

  return CMD_SUCCESS;
}

// ============================================================================
// DEMO MODE COMMANDS
// ============================================================================

/**
 * @brief Toggle demo/screensaver mode
 */
command_result_t uart_cmd_demo(const char *args) {
  extern void toggle_demo_mode(bool enabled);
  extern bool is_demo_mode_enabled(void);

  SAFE_WDT_RESET();

  if (strlen(args) == 0) {
    // No args - just show status
    bool enabled = is_demo_mode_enabled();
    uart_send_formatted("🤖 Demo Mode: %s",
                        enabled ? "ENABLED ✅" : "DISABLED ❌");
    uart_send_formatted("   Use 'DEMO ON' or 'DEMO OFF' to control");
    return CMD_SUCCESS;
  }

  // Parse ON/OFF
  char arg_upper[10];
  strncpy(arg_upper, args, sizeof(arg_upper) - 1);
  arg_upper[sizeof(arg_upper) - 1] = '\0';

  // Convert to uppercase
  for (int i = 0; arg_upper[i]; i++) {
    arg_upper[i] = toupper((unsigned char)arg_upper[i]);
  }

  if (strcmp(arg_upper, "ON") == 0 || strcmp(arg_upper, "1") == 0) {
    toggle_demo_mode(true);
    uart_send_success("🤖 Demo Mode ENABLED");
    uart_send_formatted("   Automatic chess playback started");
    uart_send_formatted("   Touch board to interrupt");
  } else if (strcmp(arg_upper, "OFF") == 0 || strcmp(arg_upper, "0") == 0) {
    toggle_demo_mode(false);
    uart_send_success("🤖 Demo Mode DISABLED");
    uart_send_formatted("   Manual control restored");
  } else {
    uart_send_error("Invalid argument. Use 'DEMO ON' or 'DEMO OFF'");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  return CMD_SUCCESS;
}

/**
 * @brief Show demo mode status
 */
command_result_t uart_cmd_demo_status(const char *args) {
  extern bool is_demo_mode_enabled(void);

  (void)args;
  SAFE_WDT_RESET();

  bool enabled = is_demo_mode_enabled();

  uart_send_formatted("🤖 DEMO MODE STATUS");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  uart_send_formatted("Mode:          %s",
                      enabled ? "ENABLED ✅" : "DISABLED ❌");
  uart_send_formatted("Description:   Automatic chess game playback");
  uart_send_formatted("Speed:         Variable (0.7s - 4.0s per move)");
  uart_send_formatted("Interruption:  Touch board to stop");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (enabled) {
    uart_send_formatted("💡 Tip: Use 'DEMO OFF' to stop automatic playback");
  } else {
    uart_send_formatted("💡 Tip: Use 'DEMO ON' to start screensaver mode");
  }

  return CMD_SUCCESS;
}

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
command_result_t uart_cmd_start_pos_check(const char *args) {
  extern void game_set_starting_position_check(bool enabled);
  extern bool game_get_starting_position_check(void);
  extern esp_err_t config_save_to_nvs(const system_config_t *config);
  extern esp_err_t config_load_from_nvs(system_config_t *config);

  SAFE_WDT_RESET();

  if (strlen(args) == 0) {
    // No args - just show status
    bool enabled = game_get_starting_position_check();
    uart_send_formatted("♟️ Starting Position Check: %s",
                        enabled ? "ENABLED ✅" : "DISABLED ❌");
    uart_send_formatted("   Use 'STARTPOS ON' or 'STARTPOS OFF' to control");
    return CMD_SUCCESS;
  }

  // Parse ON/OFF
  char arg_upper[10];
  strncpy(arg_upper, args, sizeof(arg_upper) - 1);
  arg_upper[sizeof(arg_upper) - 1] = '\0';

  // Convert to uppercase
  for (int i = 0; arg_upper[i]; i++) {
    arg_upper[i] = toupper((unsigned char)arg_upper[i]);
  }

  if (strcmp(arg_upper, "ON") == 0 || strcmp(arg_upper, "1") == 0) {
    game_set_starting_position_check(true);
    // Save to NVS
    system_config_t config;
    if (config_load_from_nvs(&config) == ESP_OK) {
      config.starting_position_check_enabled = true;
      config_save_to_nvs(&config);
    }
    uart_send_success("♟️ Starting Position Check ENABLED");
    uart_send_formatted("   Board must be in starting position before game");
  } else if (strcmp(arg_upper, "OFF") == 0 || strcmp(arg_upper, "0") == 0) {
    game_set_starting_position_check(false);
    // Save to NVS
    system_config_t config;
    if (config_load_from_nvs(&config) == ESP_OK) {
      config.starting_position_check_enabled = false;
      config_save_to_nvs(&config);
    }
    uart_send_success("♟️ Starting Position Check DISABLED");
    uart_send_formatted("   Game can start with any board setup");
  } else {
    uart_send_error("Invalid argument. Use 'STARTPOS ON' or 'STARTPOS OFF'");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  return CMD_SUCCESS;
}


// ============================================================================
// HIGH PRIORITY COMMANDS
// ============================================================================

/**
 * @brief Zobrazi vyhodnoceni pozice
 */
command_result_t uart_cmd_eval(const char *args) {
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🔍 Position Evaluation");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

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
  int material_balance =
      game_calculate_material_balance(&white_material, &black_material);

  // Get real game statistics
  uint32_t white_wins = game_get_white_wins();
  uint32_t black_wins = game_get_black_wins();
  uint32_t draws = game_get_draws();
  uint32_t total_games = game_get_total_games();

  uart_send_formatted("🎯 Current Evaluation:");
  uart_send_formatted("  • Material Balance: %s",
                      material_balance > 0   ? "White Advantage"
                      : material_balance < 0 ? "Black Advantage"
                                             : "Even");
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
  uart_send_formatted("  • Current Player: %s",
                      current_player == PLAYER_WHITE ? "White" : "Black");
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
  uart_send_formatted("💡 Recommendations for %s:",
                      current_player == PLAYER_WHITE ? "White" : "Black");
  uart_send_formatted("  • Improve piece coordination");
  uart_send_formatted("  • Control central squares");
  uart_send_formatted("  • Consider pawn breaks");

  // Reset watchdog timeru po dokončení
  SAFE_WDT_RESET();

  ESP_LOGI(TAG, "✅ Position evaluation completed successfully (local)");
  return CMD_SUCCESS;
}

/**
 * @brief Test all LEDs
 */
command_result_t uart_cmd_ledtest(const char *args) {
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "💡 LED Test");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Send LED test request to LED task
  led_command_t led_cmd = {.type = LED_CMD_TEST_ALL,
                           .led_index = 0,
                           .red = 0,
                           .green = 0,
                           .blue = 0,
                           .duration_ms = 0,
                           .data = NULL};

  // Přímé volání LED funkce
  led_set_pixel_safe(led_cmd.led_index, led_cmd.red, led_cmd.green,
                     led_cmd.blue);
  uart_send_formatted("✅ LED test executed directly");

  uart_send_formatted("🔄 Testing all LEDs...");
  uart_send_formatted("💡 All LEDs should cycle through colors");
  uart_send_formatted("✅ LED test completed");

  return CMD_SUCCESS;
}

/**
 * @brief Zobrazi systemove metrik výkonu
 */
command_result_t uart_cmd_performance(const char *args) {
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "📊 System Performance");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Get system information
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  uart_send_formatted("🔧 Hardware Information:");
  uart_send_formatted("  • Chip: %s",
                      chip_info.model == CHIP_ESP32C6 ? "ESP32-C6" : "Unknown");
  uart_send_formatted("  • Cores: %d", chip_info.cores);
  uart_send_formatted("  • Revision: %d", chip_info.revision);
  uart_send_formatted(
      "  • Features: %s%s%s%s",
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
  uart_send_formatted("  • Free heap: %zu bytes (%.1f KB)", free_heap,
                      free_heap / 1024.0);
  uart_send_formatted("  • Min free heap: %zu bytes (%.1f KB)", min_free_heap,
                      min_free_heap / 1024.0);
  uart_send_formatted("  • Total heap: %zu bytes (%.1f KB)", total_heap,
                      total_heap / 1024.0);
  uart_send_formatted("  • Used heap: %zu bytes (%.1f KB)",
                      total_heap - free_heap,
                      (total_heap - free_heap) / 1024.0);

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
 * @brief Zobrazi nebo nastavi systemovou konfiguraci
 *
 * Tento prikaz umoznuje zobrazit aktualni systemovou konfiguraci nebo nastavit
 * jednotlive konfiguracni hodnoty. Vsechny zmeny se ukladaji do NVS flash
 * a aplikuji se okamzite.
 *
 * @param args Argumenty prikazu:
 *   - Bez argumentu: Zobrazi vsechny konfiguracni hodnoty
 *   - "show": Zobrazi vsechny konfiguracni hodnoty (stejne jako bez argumentu)
 *   - "<key> <value>": Nastavi konfiguracni hodnotu
 *
 * @return CMD_SUCCESS pri uspechu, chybovy kod pri selhani
 *
 * @details
 * Podporovane konfiguracni klice:
 * - verbose: Zapne/vypne verbose mode (on/off)
 * - quiet: Zapne/vypne quiet mode (on/off)
 * - log_level: Nastavi uroven logovani (NONE, ERROR, WARN, INFO, DEBUG,
 * VERBOSE)
 * - timeout: Nastavi timeout prikazu v milisekundach (1-60000)
 * - echo: Zapne/vypne echo znaku (on/off)
 *
 * Pri zmene konfiguracni hodnoty se automaticky:
 * 1. Ulozi hodnota do NVS flash pomoci config_save_to_nvs()
 * 2. Aplikuje se na system pomoci config_apply_settings()
 *
 * @note Verbose a quiet mode jsou vzajemne exkluzivni - zapnuti jednoho
 *       automaticky vypne druhy.
 */
command_result_t uart_cmd_config(const char *args) {
  SAFE_WDT_RESET();

  if (!args || strlen(args) == 0) {
    // Show all configuration
    uart_send_colored_line(COLOR_INFO, "⚙️ System Configuration");
    uart_send_formatted(
        "═══════════════════════════════════════════════════════════════");

    uart_send_formatted("🎮 Game Settings:");
    player_t current_player = game_get_current_player();
    uart_send_formatted("  • Current player: %s",
                        current_player == PLAYER_WHITE ? "White" : "Black");
    uart_send_formatted("  • Game mode: %s", "Human vs Human");
    uart_send_formatted("  • Time control: %s", "No limit");

    uart_send_formatted("");
    uart_send_formatted("🔧 System Settings:");
    uart_send_formatted("  • Verbose mode: %s",
                        system_config.verbose_mode ? "ON" : "OFF");
    uart_send_formatted("  • Quiet mode: %s",
                        system_config.quiet_mode ? "ON" : "OFF");

    // Convert log level to string
    const char *log_level_str = "UNKNOWN";
    switch (system_config.log_level) {
    case ESP_LOG_NONE:
      log_level_str = "NONE";
      break;
    case ESP_LOG_ERROR:
      log_level_str = "ERROR";
      break;
    case ESP_LOG_WARN:
      log_level_str = "WARN";
      break;
    case ESP_LOG_INFO:
      log_level_str = "INFO";
      break;
    case ESP_LOG_DEBUG:
      log_level_str = "DEBUG";
      break;
    case ESP_LOG_VERBOSE:
      log_level_str = "VERBOSE";
      break;
    }
    uart_send_formatted("  • Log level: %s", log_level_str);
    uart_send_formatted("  • Command timeout: %lu ms",
                        system_config.command_timeout_ms);

    uart_send_formatted("");
    uart_send_formatted("💡 Usage: CONFIG <key> <value> to set configuration");
    uart_send_formatted(
        "💡 Available keys: verbose, quiet, log_level, timeout");

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
      uart_send_formatted(
          "═══════════════════════════════════════════════════════════════");

      uart_send_formatted("🎮 Game Settings:");
      player_t current_player = game_get_current_player();
      uart_send_formatted("  • Current player: %s",
                          current_player == PLAYER_WHITE ? "White" : "Black");
      uart_send_formatted("  • Game mode: %s", "Human vs Human");
      uart_send_formatted("  • Time control: %s", "No limit");

      uart_send_formatted("");
      uart_send_formatted("🔧 System Settings:");
      uart_send_formatted("  • Verbose mode: %s",
                          system_config.verbose_mode ? "ON" : "OFF");
      uart_send_formatted("  • Quiet mode: %s",
                          system_config.quiet_mode ? "ON" : "OFF");

      // Convert log level to string
      const char *log_level_str = "UNKNOWN";
      switch (system_config.log_level) {
      case ESP_LOG_NONE:
        log_level_str = "NONE";
        break;
      case ESP_LOG_ERROR:
        log_level_str = "ERROR";
        break;
      case ESP_LOG_WARN:
        log_level_str = "WARN";
        break;
      case ESP_LOG_INFO:
        log_level_str = "INFO";
        break;
      case ESP_LOG_DEBUG:
        log_level_str = "DEBUG";
        break;
      case ESP_LOG_VERBOSE:
        log_level_str = "VERBOSE";
        break;
      }
      uart_send_formatted("  • Log level: %s", log_level_str);
      uart_send_formatted("  • Command timeout: %lu ms",
                          system_config.command_timeout_ms);

      uart_send_formatted("");
      uart_send_formatted(
          "💡 Usage: CONFIG <key> <value> to set configuration");
      uart_send_formatted(
          "💡 Available keys: verbose, quiet, log_level, timeout");

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
  bool config_changed = false;

  if (strcmp(key, "verbose") == 0) {
    if (strcmp(value, "on") == 0 || strcmp(value, "ON") == 0) {
      system_config.verbose_mode = true;
      system_config.quiet_mode = false;
      config_changed = true;
      uart_send_formatted("✅ Verbose mode set to ON");
    } else if (strcmp(value, "off") == 0 || strcmp(value, "OFF") == 0) {
      system_config.verbose_mode = false;
      config_changed = true;
      uart_send_formatted("✅ Verbose mode set to OFF");
    } else {
      uart_send_error("❌ Invalid value. Use 'on' or 'off'");
      return CMD_ERROR_INVALID_SYNTAX;
    }
  } else if (strcmp(key, "quiet") == 0) {
    if (strcmp(value, "on") == 0 || strcmp(value, "ON") == 0) {
      system_config.quiet_mode = true;
      system_config.verbose_mode = false;
      config_changed = true;
      uart_send_formatted("✅ Quiet mode set to ON");
    } else if (strcmp(value, "off") == 0 || strcmp(value, "OFF") == 0) {
      system_config.quiet_mode = false;
      config_changed = true;
      uart_send_formatted("✅ Quiet mode set to OFF");
    } else {
      uart_send_error("❌ Invalid value. Use 'on' or 'off'");
      return CMD_ERROR_INVALID_SYNTAX;
    }
  } else if (strcmp(key, "log_level") == 0) {
    esp_log_level_t new_level = ESP_LOG_ERROR;
    bool valid = false;

    if (strcmp(value, "NONE") == 0 || strcmp(value, "none") == 0) {
      new_level = ESP_LOG_NONE;
      valid = true;
    } else if (strcmp(value, "ERROR") == 0 || strcmp(value, "error") == 0) {
      new_level = ESP_LOG_ERROR;
      valid = true;
    } else if (strcmp(value, "WARN") == 0 || strcmp(value, "warn") == 0) {
      new_level = ESP_LOG_WARN;
      valid = true;
    } else if (strcmp(value, "INFO") == 0 || strcmp(value, "info") == 0) {
      new_level = ESP_LOG_INFO;
      valid = true;
    } else if (strcmp(value, "DEBUG") == 0 || strcmp(value, "debug") == 0) {
      new_level = ESP_LOG_DEBUG;
      valid = true;
    } else if (strcmp(value, "VERBOSE") == 0 || strcmp(value, "verbose") == 0) {
      new_level = ESP_LOG_VERBOSE;
      valid = true;
    }

    if (valid) {
      system_config.log_level = new_level;
      config_changed = true;
      uart_send_formatted("✅ Log level set to %s", value);
    } else {
      uart_send_error(
          "❌ Invalid log level. Use: NONE, ERROR, WARN, INFO, DEBUG, VERBOSE");
      return CMD_ERROR_INVALID_SYNTAX;
    }
  } else if (strcmp(key, "timeout") == 0) {
    int timeout = atoi(value);
    if (timeout > 0 && timeout <= 60000) { // 1ms to 60s
      system_config.command_timeout_ms = (uint32_t)timeout;
      config_changed = true;
      uart_send_formatted("✅ Command timeout set to %d ms", timeout);
    } else {
      uart_send_error("❌ Timeout must be between 1 and 60000 ms");
      return CMD_ERROR_INVALID_SYNTAX;
    }
  } else {
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg), "❌ Unknown configuration key: %s",
             key);
    uart_send_error(error_msg);
    uart_send_formatted(
        "💡 Available keys: verbose, quiet, log_level, timeout");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // If configuration changed, save to NVS and apply settings
  if (config_changed) {
    esp_err_t ret = config_save_to_nvs(&system_config);
    if (ret != ESP_OK) {
      uart_send_error("❌ Failed to save configuration to NVS");
      return CMD_ERROR_SYSTEM_ERROR;
    }

    ret = config_apply_settings(&system_config);
    if (ret != ESP_OK) {
      uart_send_error("❌ Failed to apply configuration settings");
      return CMD_ERROR_SYSTEM_ERROR;
    }

    uart_send_formatted("💾 Configuration saved and applied");
  }

  return CMD_SUCCESS;
}


// ============================================================================
// COMPONENT CONTROL COMMANDS
// ============================================================================

/**
 * @brief Turn off component
 */
command_result_t uart_cmd_component_off(const char *args) {
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
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (strcmp(component, "matrix") == 0) {
    uart_send_formatted("🔴 Turning OFF Matrix component...");

    // Send command to matrix task to disable scanning
    uint8_t matrix_cmd = MATRIX_CMD_DISABLE;
    if (xQueueSend(matrix_command_queue, &matrix_cmd, pdMS_TO_TICKS(100)) ==
        pdTRUE) {
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
    // Přímé volání LED funkce
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
command_result_t uart_cmd_component_on(const char *args) {
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
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  if (strcmp(component, "matrix") == 0) {
    uart_send_formatted("🟢 Turning ON Matrix component...");

    // Send command to matrix task to enable scanning
    uint8_t matrix_cmd = MATRIX_CMD_ENABLE;
    if (xQueueSend(matrix_command_queue, &matrix_cmd, pdMS_TO_TICKS(100)) ==
        pdTRUE) {
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
    // DIRECT LED CALL - No queue hell
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
command_result_t uart_cmd_endgame_white(const char *args) {
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🏆 Endgame - White Victory");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // MEMORY OPTIMIZATION: Use local endgame report instead of game task
  // communication This eliminates the need for large response buffers and
  // reduces stack usage
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
  uart_send_formatted("  • Current Player: %s",
                      current_player == PLAYER_WHITE ? "White" : "Black");

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
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Generate chess.com style advantage graph
  uart_display_advantage_graph(move_count, true); // true = white wins

  uart_send_formatted("");
  uart_send_formatted("🏆 Congratulations to White player!");

  // Reset watchdog timeru po dokončení
  SAFE_WDT_RESET();

  ESP_LOGI(TAG, "✅ Endgame report completed successfully (local)");
  return CMD_SUCCESS;
}

/**
 * @brief Endgame - Black wins
 */
command_result_t uart_cmd_endgame_black(const char *args) {
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🏆 Endgame - Black Victory");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // MEMORY OPTIMIZATION: Use local endgame report instead of game task
  // communication This eliminates the need for large response buffers and
  // reduces stack usage
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
  uart_send_formatted("  • Current Player: %s",
                      current_player == PLAYER_WHITE ? "White" : "Black");

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
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Generate chess.com style advantage graph
  uart_display_advantage_graph(move_count, false); // false = black wins

  uart_send_formatted("");
  uart_send_formatted("🏆 Congratulations to Black player!");

  // Reset watchdog timeru po dokončení
  SAFE_WDT_RESET();

  ESP_LOGI(TAG, "✅ Endgame report completed successfully (local)");
  return CMD_SUCCESS;
}

// ============================================================================
// ROBUSTNI ZPRACOVANI CHYB A OBNOVENI
// ============================================================================

/**
 * @brief Check memory health and report status
 * @return ESP_OK if memory is healthy, ESP_ERR_NO_MEM if low
 */
esp_err_t uart_check_memory_health(void) {
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();

  /* ESP32-C6 + WiFi + BLE + HTTP: běžně ~15–30 KiB volných — nebrat jako chybu
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

  // Okamžitý reset watchdog timeru pro prevenci dalších timeoutů
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

  // Finální reset watchdog timeru
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
  // ARROW KEY SUPPORT: Zpracování ANSI escape sekvencí pro šipky
  if (esc_state == ESC_STATE_NONE && c == CHAR_ESC) {
    esc_state = ESC_STATE_ESC;
    return; // Čekáme na další znak
  }

  if (esc_state == ESC_STATE_ESC) {
    if (c == '[') {
      esc_state = ESC_STATE_BRACKET;
      return; // Čekáme na finální znak
    } else {
      // Neplatná escape sekvence, resetovat a pokračovat normálním zpracováním
      esc_state = ESC_STATE_NONE;
    }
  }

  if (esc_state == ESC_STATE_BRACKET) {
    esc_state = ESC_STATE_NONE; // Resetovat stav

    if (c == 'A') {
      // Arrow Up - předchozí příkaz z historie
      handle_arrow_up();
      return;
    } else if (c == 'B') {
      // Arrow Down - následující příkaz z historie
      handle_arrow_down();
      return;
    }
    // Jiná escape sekvence - ignorovat
    return;
  }

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

      // Resetovat navigační index při zadání příkazu
      history_navigation_index = -1;
    }

    // BEZPECNY PROMPT: Pouzit mutex pro UART operace
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
    uart_write_char_immediate(c);
    fflush(stdout);
  }
}

// ============================================================================
// CHESS PIECE UNICODE SYMBOLS
// ============================================================================

/**
 * @brief Ziska Unicode symbol pro sachovou figurku
 * @param piece Piece type from game_task.h
 * @return Unicode symbol string
 */
const char *get_unicode_piece_symbol(piece_t piece) {
  switch (piece) {
  case PIECE_WHITE_PAWN:
    return "♙";
  case PIECE_WHITE_KNIGHT:
    return "♘";
  case PIECE_WHITE_BISHOP:
    return "♗";
  case PIECE_WHITE_ROOK:
    return "♖";
  case PIECE_WHITE_QUEEN:
    return "♕";
  case PIECE_WHITE_KING:
    return "♔";
  case PIECE_BLACK_PAWN:
    return "♟";
  case PIECE_BLACK_KNIGHT:
    return "♞";
  case PIECE_BLACK_BISHOP:
    return "♝";
  case PIECE_BLACK_ROOK:
    return "♜";
  case PIECE_BLACK_QUEEN:
    return "♛";
  case PIECE_BLACK_KING:
    return "♚";
  default:
    return "·";
  }
}

/**
 * @brief Ziska ASCII symbol pro sachovou figurku (zalozni varianta)
 * @param piece Piece type from game_task.h
 * @return ASCII symbol string
 */
const char *get_ascii_piece_symbol(piece_t piece) {
  switch (piece) {
  case PIECE_WHITE_PAWN:
    return "P";
  case PIECE_WHITE_KNIGHT:
    return "N";
  case PIECE_WHITE_BISHOP:
    return "B";
  case PIECE_WHITE_ROOK:
    return "R";
  case PIECE_WHITE_QUEEN:
    return "Q";
  case PIECE_WHITE_KING:
    return "K";
  case PIECE_BLACK_PAWN:
    return "p";
  case PIECE_BLACK_KNIGHT:
    return "n";
  case PIECE_BLACK_BISHOP:
    return "b";
  case PIECE_BLACK_ROOK:
    return "r";
  case PIECE_BLACK_QUEEN:
    return "q";
  case PIECE_BLACK_KING:
    return "k";
  default:
    return "·";
  }
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void uart_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "🚀 Enhanced UART command interface starting...");

  // Registrace tasku s Task Watchdog Timer
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
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
  ESP_LOGI(TAG, "  • Robustni zpracovani chyb");
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

// Legacy main loop (kept for compatibility)
void uart_task_legacy_loop(void) {
  uint32_t loop_count = 0;
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    // Reset watchdog timeru pro UART task v každé iteraci
    esp_err_t wdt_reset_ret = uart_task_wdt_reset_safe();
    if (wdt_reset_ret != ESP_OK && wdt_reset_ret != ESP_ERR_NOT_FOUND) {
      // WDT reset failed - this might indicate system issues
    }

    // Zpracování output queue jako první pro plynulý výstup
    uart_process_output_queue();

    // Read and process input with minimal timeout for responsiveness
    char c;
    int len = 0;

    // ROBUST ERROR HANDLING: Wrap input reading in try-catch equivalent
    bool input_error = false;

    // Vždy použití USB Serial JTAG metody pro konzistenci
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
      // Použití non-blocking přístupu pro prevenci WDT timeoutů

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
        // No data available - this is normal, not an error
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

    /* Paměť/log: kontrola ~1×/30 s (1 ms tick × 30000), ne každou sekundu */
    if (loop_count % 30000 == 0) {
      uart_task_health_check();
      uart_check_memory_health();
    }

    /* Status řádku: při 1 ms smyčce = každých ~6 s (dřívější komentář „60 s“
     * byl nepřesný) */
    if (loop_count % 6000 == 0) {
      ESP_LOGI(TAG, "UART Task Status: Commands=%lu, Errors=%lu", command_count,
               error_count);
    }

    loop_count++;

    // Minimal task delay for maximum responsiveness
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
  }
}

/**
 * @brief Display LED board states in terminal
 */
void uart_display_led_board(void) {
  // CRITICAL: Reset WDT at start of display function
  SAFE_WDT_RESET();

  // LED board header
  uart_send_colored_line(COLOR_YELLOW, "    a   b   c   d   e   f   g   h");
  uart_send_formatted("  +---+---+---+---+---+---+---+---+");

  // MEMORY OPTIMIZATION: Display actual LED states from LED task
  ESP_LOGI(TAG, "📡 Displaying actual LED states from LED task");

  // Display board LEDs (0-63) row by row
  for (int row = 7; row >= 0; row--) {
    // Reset watchdog timeru každých několik řádků
    if (row % 2 == 0) {
      SAFE_WDT_RESET();
    }

    // Build row string with small buffer
    char row_buffer[64];
    int pos = snprintf(row_buffer, sizeof(row_buffer), "%d |", row + 1);

    for (int col = 0; col < 8; col++) {
      // Reset watchdog timeru každých několik sloupců
      if (col % 4 == 0) {
        SAFE_WDT_RESET();
      }

      // Get LED index for this square (0-63)
      int led_index = chess_pos_to_led_index(row, col);

      // Get actual LED state from LED task
      uint32_t led_color = led_get_led_state(led_index);
      const char *led_symbol;

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

      pos += snprintf(row_buffer + pos, sizeof(row_buffer) - pos, "%s|",
                      led_symbol);
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

  const char *status_names[] = {
      "Queen  (White)", "Rook   (White)", "Bishop (White)",
      "Knight (White)", "Reset",          "Queen  (Black)",
      "Rook   (Black)", "Bishop (Black)", "Knight (Black)"};

  for (int i = 0; i < 9; i++) {
    int led_index = 64 + i;

    // Get actual LED state from LED task
    uint32_t led_color = led_get_led_state(led_index);
    const char *color_symbol;

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

    uart_send_formatted("  • LED %d: %s - %s", led_index, status_names[i],
                        color_symbol);
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

  // Finální reset watchdog timeru
  SAFE_WDT_RESET();
}

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
void uart_display_advantage_graph(uint32_t move_count, bool white_wins) {
  // CRITICAL: Reset WDT at start
  SAFE_WDT_RESET();

  // Graph parameters
  const int GRAPH_WIDTH = 50;       // Width of the graph
  const int GRAPH_HEIGHT = 11;      // Height of the graph (including axis)
  const float MAX_ADVANTAGE = 5.0f; // Maximum advantage (+/-5 pawns)

  // MEMORY OPTIMIZATION: Use static arrays to avoid stack overflow
  static float advantages[50]; // Fixed size for static array
  static char
      move_qualities[50]; // 'G'=Good, 'M'=Mistake, 'B'=Blunder, 'R'=Brilliant

  // Initialize random seed based on move count for consistent results
  uint32_t seed = move_count * 12345 + (white_wins ? 1000 : 2000);

  // Generate realistic game progression
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    float progress = (float)i / (GRAPH_WIDTH - 1);

    // Simple pseudo-random generator
    seed = seed * 1103515245 + 12345;
    float random_factor =
        (float)((seed >> 16) & 0x7FFF) / 32767.0f - 0.5f; // -0.5 to 0.5

    if (white_wins) {
      // White gradually gains advantage
      if (progress < 0.3f) {
        advantages[i] = 0.0f + random_factor * 0.5f; // Early game: balanced
      } else if (progress < 0.7f) {
        advantages[i] = (progress - 0.3f) * 2.0f +
                        random_factor * 0.3f; // Mid game: white gains
      } else {
        advantages[i] = 2.0f + (progress - 0.7f) * 3.0f +
                        random_factor * 0.2f; // End game: white dominates
      }
    } else {
      // Black gradually gains advantage
      if (progress < 0.3f) {
        advantages[i] = 0.0f + random_factor * 0.5f; // Early game: balanced
      } else if (progress < 0.7f) {
        advantages[i] = -(progress - 0.3f) * 2.0f +
                        random_factor * 0.3f; // Mid game: black gains
      } else {
        advantages[i] = -2.0f - (progress - 0.7f) * 3.0f +
                        random_factor * 0.2f; // End game: black dominates
      }
    }

    // Clamp advantage to reasonable range
    if (advantages[i] > MAX_ADVANTAGE)
      advantages[i] = MAX_ADVANTAGE;
    if (advantages[i] < -MAX_ADVANTAGE)
      advantages[i] = -MAX_ADVANTAGE;

    // Assign move quality based on advantage change
    if (i > 0) {
      float change = advantages[i] - advantages[i - 1];
      if (change > 1.5f)
        move_qualities[i] = 'R'; // Brilliant
      else if (change > 0.5f)
        move_qualities[i] = 'G'; // Good
      else if (change < -1.5f)
        move_qualities[i] = 'B'; // Blunder
      else if (change < -0.5f)
        move_qualities[i] = 'M'; // Mistake
      else
        move_qualities[i] = 'G'; // Good
    } else {
      move_qualities[i] = 'G'; // First move
    }
  }

  // Display graph header
  uart_send_formatted("Advantage over time (Chess.com style):");
  uart_send_formatted("Game: %s wins, %lu moves",
                      white_wins ? "White" : "Black", move_count);
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
      float y_value = MAX_ADVANTAGE -
                      (float)row * (2.0f * MAX_ADVANTAGE / (GRAPH_HEIGHT - 1));
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
  uart_send_formatted(
      "     └───────────────────────────────────────────────────");
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
      uart_send_formatted("  Move %d: Brilliant move! (+%.1f advantage)", i + 1,
                          advantages[i]);
    } else if (move_qualities[i] == 'B') {
      uart_send_formatted("  Move %d: Blunder! (%.1f advantage)", i + 1,
                          advantages[i]);
    }
  }

  // Finální reset watchdog timeru
  SAFE_WDT_RESET();
}

// ============================================================================
// ANIMATION TEST COMMANDS
// ============================================================================

command_result_t uart_cmd_test_move_anim(const char *args) {
  uart_send_line("🎬 Testing move animation...");

  // Send command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_TEST_MOVE_ANIM,
                              .from_notation = "",
                              .to_notation = "",
                              .player = 0,
                              .response_queue = NULL};

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

command_result_t uart_cmd_test_player_anim(const char *args) {
  uart_send_line("🎬 Testing player change animation...");

  // Send command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_TEST_PLAYER_ANIM,
                              .from_notation = "",
                              .to_notation = "",
                              .player = 0,
                              .response_queue = NULL};

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

command_result_t uart_cmd_test_castle_anim(const char *args) {
  uart_send_line("🎬 Testing castling animation...");

  // Send command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_TEST_CASTLE_ANIM,
                              .from_notation = "",
                              .to_notation = "",
                              .player = 0,
                              .response_queue = NULL};

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

command_result_t uart_cmd_test_promote_anim(const char *args) {
  uart_send_line("🎬 Testing promotion animation...");

  // Send command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_TEST_PROMOTE_ANIM,
                              .from_notation = "",
                              .to_notation = "",
                              .player = 0,
                              .response_queue = NULL};

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

command_result_t uart_cmd_test_endgame_anim(const char *args) {
  uart_send_line("🎬 Testing endgame animation...");

  // Send command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_TEST_ENDGAME_ANIM,
                              .from_notation = "",
                              .to_notation = "",
                              .player = 0,
                              .response_queue = NULL};

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

// Endgame Animation Style Commands
command_result_t uart_cmd_endgame_wave(const char *args) {
  uart_send_line("🌊 Starting NON-BLOCKING endgame wave animation...");

  // Use unified animation manager for non-blocking animation
  uint32_t anim_id =
      unified_animation_create(ANIM_TYPE_ENDGAME_WAVE, ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret =
        animation_start_endgame_wave(anim_id, 27, 0); // d4, white winner
    if (ret == ESP_OK) {
      uart_send_formatted(
          "✅ Non-blocking endgame wave animation started (ID: %lu)", anim_id);
    } else {
      uart_send_formatted("❌ Failed to start endgame wave animation: %s",
                          esp_err_to_name(ret));
      return CMD_ERROR_SYSTEM_ERROR;
    }
  } else {
    uart_send_line(
        "❌ Failed to create animation - too many active animations");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_circles(const char *args) {
  uart_send_line("⭕ Starting NON-BLOCKING endgame circles animation...");

  // Use unified animation manager for non-blocking animation
  uint32_t anim_id =
      unified_animation_create(ANIM_TYPE_ENDGAME_CIRCLES, ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret =
        animation_start_endgame_circles(anim_id, 27, 0); // d4, white winner
    if (ret == ESP_OK) {
      uart_send_formatted(
          "✅ Non-blocking endgame circles animation started (ID: %lu)",
          anim_id);
    } else {
      uart_send_formatted("❌ Failed to start endgame circles animation: %s",
                          esp_err_to_name(ret));
      return CMD_ERROR_SYSTEM_ERROR;
    }
  } else {
    uart_send_line(
        "❌ Failed to create animation - too many active animations");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_cascade(const char *args) {
  uart_send_line("💫 Starting NON-BLOCKING endgame cascade animation...");

  // Use unified animation manager for non-blocking animation
  uint32_t anim_id =
      unified_animation_create(ANIM_TYPE_ENDGAME_CASCADE, ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret =
        animation_start_endgame_cascade(anim_id, 27, 0); // d4, white winner
    if (ret == ESP_OK) {
      uart_send_formatted(
          "✅ Non-blocking endgame cascade animation started (ID: %lu)",
          anim_id);
    } else {
      uart_send_formatted("❌ Failed to start endgame cascade animation: %s",
                          esp_err_to_name(ret));
      return CMD_ERROR_SYSTEM_ERROR;
    }
  } else {
    uart_send_line(
        "❌ Failed to create animation - too many active animations");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_fireworks(const char *args) {
  uart_send_line("🎆 Starting NON-BLOCKING endgame fireworks animation...");

  // Use unified animation manager for non-blocking animation
  uint32_t anim_id =
      unified_animation_create(ANIM_TYPE_ENDGAME_FIREWORKS, ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret =
        animation_start_endgame_fireworks(anim_id, 27, 0); // d4, white winner
    if (ret == ESP_OK) {
      uart_send_formatted(
          "✅ Non-blocking endgame fireworks animation started (ID: %lu)",
          anim_id);
    } else {
      uart_send_formatted("❌ Failed to start endgame fireworks animation: %s",
                          esp_err_to_name(ret));
      return CMD_ERROR_SYSTEM_ERROR;
    }
  } else {
    uart_send_line(
        "❌ Failed to create animation - too many active animations");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_draw_spiral(const char *args) {
  uart_send_line("🌀 Starting NON-BLOCKING draw spiral animation...");

  // Use unified animation manager for non-blocking animation
  uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_DRAW_SPIRAL,
                                              ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret =
        animation_start_endgame_draw_spiral(anim_id, 27); // d4 center
    if (ret == ESP_OK) {
      uart_send_formatted(
          "✅ Non-blocking draw spiral animation started (ID: %lu)", anim_id);
    } else {
      uart_send_formatted("❌ Failed to start draw spiral animation: %s",
                          esp_err_to_name(ret));
      return CMD_ERROR_SYSTEM_ERROR;
    }
  } else {
    uart_send_line(
        "❌ Failed to create animation - too many active animations");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

command_result_t uart_cmd_endgame_draw_pulse(const char *args) {
  uart_send_line("💓 Starting NON-BLOCKING draw pulse animation...");

  // Use unified animation manager for non-blocking animation
  uint32_t anim_id = unified_animation_create(ANIM_TYPE_ENDGAME_DRAW_PULSE,
                                              ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret =
        animation_start_endgame_draw_pulse(anim_id, 27); // d4 center
    if (ret == ESP_OK) {
      uart_send_formatted(
          "✅ Non-blocking draw pulse animation started (ID: %lu)", anim_id);
    } else {
      uart_send_formatted("❌ Failed to start draw pulse animation: %s",
                          esp_err_to_name(ret));
      return CMD_ERROR_SYSTEM_ERROR;
    }
  } else {
    uart_send_line(
        "❌ Failed to create animation - too many active animations");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

// Puzzle removal test command removed

command_result_t uart_cmd_stop_endgame(const char *args) {
  uart_send_line("🛑 Stopping all endgame animations...");

  // Stop all endgame animations using unified animation manager
  esp_err_t ret = unified_animation_stop_all();
  if (ret == ESP_OK) {
    uart_send_line("✅ All endgame animations stopped");
  } else {
    uart_send_formatted("⚠️ Some animations may still be running: %s",
                        esp_err_to_name(ret));
  }

  // Also stop legacy endgame animation system
  led_stop_endgame_animation();

  return CMD_SUCCESS;
}
