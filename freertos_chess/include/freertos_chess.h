/**
 * @file freertos_chess.h
 * @brief ESP32-C6 Chess System v2.4 - Hlavni systemova hlavicka
 * 
 * Tato hlavicka obsahuje hlavni systemove definice, konstanty
 * a globalni promenne pro FreeRTOS sachovy system.
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tato hlavicka je centralni bod definic pro cely sachovy system.
 * Obsahuje vsechny klicove konstanty, GPIO definice, queue handles,
 * mutex handles, timer handles a systemove funkce.
 * 
 * Hlavni funkce:
 * - GPIO pin definice pro ESP32-C6
 * - Systemove konstanty a velikosti
 * - FreeRTOS queue a mutex handles
 * - Inicializacni funkce systemu
 * - Hardware abstraction layer
 * - Utility funkce a makra
 */

#ifndef FREERTOS_CHESS_H
#define FREERTOS_CHESS_H

#include "chess_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SYSTEMOVE INFORMACE O VERZI
// ============================================================================

/** @brief Nazev sachoveho systemu */
#define CHESS_SYSTEM_NAME "ESP32-C6 Chess System"
/** @brief Verze sachoveho systemu */
#define CHESS_SYSTEM_VERSION "2.4"
/** @brief Autor sachoveho systemu */
#define CHESS_SYSTEM_AUTHOR "Alfred Krutina"
/** @brief Kompletni retezec s verzi */
#define CHESS_VERSION_STRING "ESP32-C6 Chess System v2.4"
/** @brief Datum sestaveni */
#define CHESS_BUILD_DATE __DATE__

// ============================================================================
// GPIO PIN DEFINICE (ESP32-C6 Kompatibilni)
// ============================================================================

/** @brief Pin pro WS2812B LED data (GPIO7) */
#define LED_DATA_PIN GPIO_NUM_7        // WS2812B data line
/** @brief Pin pro stavovy LED indikator (GPIO5 - bezpecny pin) */
#define STATUS_LED_PIN GPIO_NUM_5      // Status indicator (safe pin - GPIO8 is boot strapping pin!)

// Piny pro radky matice (vystupy) - potreba 8 pinu
/** @brief Pin pro radek 0 matice (GPIO4 - vystup, zmeneno z GPIO10 pro ESP32-C6 DevKit) */
#define MATRIX_ROW_0 GPIO_NUM_4
/** @brief Pin pro radek 1 matice (GPIO16 - vystup, zmeneno z GPIO11/15 pro ESP32-C6 DevKit, UART TX) */
#define MATRIX_ROW_1 GPIO_NUM_16  
/** @brief Pin pro radek 2 matice (GPIO18 - vystup) */
#define MATRIX_ROW_2 GPIO_NUM_18
/** @brief Pin pro radek 3 matice (GPIO19 - vystup) */
#define MATRIX_ROW_3 GPIO_NUM_19
/** @brief Pin pro radek 4 matice (GPIO20 - vystup) */
#define MATRIX_ROW_4 GPIO_NUM_20
/** @brief Pin pro radek 5 matice (GPIO21 - vystup) */
#define MATRIX_ROW_5 GPIO_NUM_21
/** @brief Pin pro radek 6 matice (GPIO22 - vystup) */
#define MATRIX_ROW_6 GPIO_NUM_22
/** @brief Pin pro radek 7 matice (GPIO23 - vystup) */
#define MATRIX_ROW_7 GPIO_NUM_23

// Piny pro sloupce matice (vstupy s pull-up) - potreba 8 pinu
/** @brief Pin pro sloupec 0 matice (GPIO0 - vstup s pull-up, bezpecny pin) */
#define MATRIX_COL_0 GPIO_NUM_0        // Safe pin
/** @brief Pin pro sloupec 1 matice (GPIO1 - vstup s pull-up, bezpecny pin) */
#define MATRIX_COL_1 GPIO_NUM_1        // Safe pin
/** @brief Pin pro sloupec 2 matice (GPIO2 - vstup s pull-up, bezpecny pin) */
#define MATRIX_COL_2 GPIO_NUM_2        // Safe pin
/** @brief Pin pro sloupec 3 matice (GPIO3 - vstup s pull-up, bezpecny pin) */
#define MATRIX_COL_3 GPIO_NUM_3        // Safe pin
/** @brief Pin pro sloupec 4 matice (GPIO6 - vstup s pull-up, bezpecny pin) */
#define MATRIX_COL_4 GPIO_NUM_6        // Safe pin
/** @brief Pin pro sloupec 5 matice (GPIO14 - vstup s pull-up, bezpecny pin, zmeneno z GPIO9) */
#define MATRIX_COL_5 GPIO_NUM_14       // Safe pin (changed from GPIO9 to avoid strapping pin)
/** @brief Pin pro sloupec 6 matice (GPIO17 - vstup s pull-up, zmeneno z GPIO12, UART RX - JTAG safe) */
#define MATRIX_COL_6 GPIO_NUM_17       // Changed from GPIO12 - UART RX pin, safe for JTAG usage
/** @brief Pin pro sloupec 7 matice (GPIO27 - vstup s pull-up, zmeneno z GPIO13, SPI Flash pin - JTAG safe) */
#define MATRIX_COL_7 GPIO_NUM_27       // Changed from GPIO13 - SPI Flash pin but usable for I/O, safe for JTAG

/** @brief Pin pro reset tlacitko (GPIO4 - zmeneno z GPIO27 pro ESP32-C6 DevKit, sdileno s ROW_0 - time-multiplexed) */
#define BUTTON_RESET GPIO_NUM_4       // Changed from GPIO27 for ESP32-C6 DevKit, shared with ROW_0 via time-multiplexing

// Definice tlacitek (time-multiplexed se sloupci matice)
/** @brief Tlacitko pro promoci na damu (sdileno s MATRIX_COL_0) */
#define BUTTON_QUEEN     MATRIX_COL_0  // A1 square + Button Queen
/** @brief Tlacitko pro promoci na vez (sdileno s MATRIX_COL_1) */
#define BUTTON_ROOK      MATRIX_COL_1  // B1 square + Button Rook
/** @brief Tlacitko pro promoci na strelce (sdileno s MATRIX_COL_2) */
#define BUTTON_BISHOP    MATRIX_COL_2  // C1 square + Button Bishop
/** @brief Tlacitko pro promoci na kone (sdileno s MATRIX_COL_3) */
#define BUTTON_KNIGHT    MATRIX_COL_3  // D1 square + Button Knight
/** @brief Tlacitko pro promoci na damu B (sdileno s MATRIX_COL_4) */
#define BUTTON_PROMOTION_QUEEN  MATRIX_COL_4 // E1 square + Promotion Queen
/** @brief Tlacitko pro promoci na vez B (sdileno s MATRIX_COL_5) */
#define BUTTON_PROMOTION_ROOK   MATRIX_COL_5 // F1 square + Promotion Rook
/** @brief Tlacitko pro promoci na strelce B (sdileno s MATRIX_COL_6) */
#define BUTTON_PROMOTION_BISHOP MATRIX_COL_6 // G1 square + Promotion Bishop
/** @brief Tlacitko pro promoci na kone B (sdileno s MATRIX_COL_7) */
#define BUTTON_PROMOTION_KNIGHT MATRIX_COL_7 // H1 square + Promotion Knight

// ============================================================================
// SYSTEMOVE CASOVE KONSTANTY
// ============================================================================
    
// Konfigurace time-multiplexingu (25ms celkovy cyklus - LED update odstranen)
/** @brief Cas skenovani matice v milisekundach (0-20ms) */
#define MATRIX_SCAN_TIME_MS 20      // Matrix scanning time (0-20ms)
/** @brief Cas skenovani tlacitek v milisekundach (20-25ms) */
#define BUTTON_SCAN_TIME_MS 5       // Button scanning time (20-25ms)
// #define LED_UPDATE_TIME_MS 5        // ❌ REMOVED: No longer needed
/** @brief Celkovy cas multiplexing cyklu v milisekundach (snizeno z 30ms) */
#define TOTAL_CYCLE_TIME_MS 25      // Total multiplexing cycle (reduced from 30ms)
/** @brief Interval kontroly zdravi systemu v milisekundach */
#define SYSTEM_HEALTH_TIME_MS 1000  // System health check interval

// ============================================================================
// LED TIMING OPTIMALIZACNI KONSTANTY - NOVE PRO OPRAVU BLIKANI
// ============================================================================

// WS2812B optimalni casove konstanty
/** @brief Bezpecny timeout pro LED prikazy v milisekundach (500ms misto 10-100ms) */
#define LED_COMMAND_TIMEOUT_MS      500   // ✅ Bezpečný timeout pro příkazy (místo 10-100ms)
/** @brief Timeout pro LED mutex v milisekundach (200ms) */
#define LED_MUTEX_TIMEOUT_MS        200   // ✅ Mutex timeout
/** @brief Bezpecny interval LED aktualizace v milisekundach (300ms = 3.3Hz pro lidske oko) */
#define LED_HARDWARE_UPDATE_MS      300   // ✅ BEZPEČNÝ interval - 300ms (3.3Hz) pro lidské oko
/** @brief Bezpecna mezera mezi LED framy v milisekundach (200ms) */
#define LED_FRAME_SPACING_MS        200   // ✅ BEZPEČNÁ mezera - 200ms mezi frames
/** @brief Bezpecny reset cas pro WS2812B v mikrosekundach (500μs = 10x vice nez minimum) */
#define LED_RESET_TIME_US           500   // ✅ BEZPEČNÝ reset - 500μs (10x více než minimum)

// LED synchronizacni konstanty
/** @brief Bezpecny timeout pro LED operace v FreeRTOS tickach */
#define LED_SAFE_TIMEOUT            pdMS_TO_TICKS(LED_COMMAND_TIMEOUT_MS)
/** @brief Bezpecny mutex timeout v FreeRTOS tickach */
#define LED_MUTEX_SAFE_TIMEOUT      pdMS_TO_TICKS(LED_MUTEX_TIMEOUT_MS)

// ============================================================================
// VELIKOSTI FRONT
// ============================================================================

// PAMETI OPTIMALIZOVANE VELIKOSTI FRONT - Faze 1
// Celkova pamet front: 8KB → 5KB = 3KB uspora
/** @brief Velikost LED fronty (50 prvku, zvetseno z 15 na 50 pro stabilitu) */
#define LED_QUEUE_SIZE 50           // ✅ OPRAVA: Zvětšeno z 15 na 50 pro stabilitu
/** @brief Velikost matrix fronty (8 prvku, snizeno z 15 na 8, dostatecne pro skenovani) */
#define MATRIX_QUEUE_SIZE 8      // Reduced from 15 to 8 (sufficient for scanning)
/** @brief Velikost button fronty (5 prvku, snizeno z 8 na 5, udalosti jsou vzacne) */
#define BUTTON_QUEUE_SIZE 5      // Reduced from 8 to 5 (button events are infrequent)
/** @brief Velikost UART fronty (10 prvku, snizeno z 20 na 10, uspora ~38 KB pameti) */
#define UART_QUEUE_SIZE 10       // Reduced from 20 to 10 to save ~38 KB memory
/** @brief Velikost game fronty (20 prvku, snizeno z 30 na 20, dostatecne pro game prikazy) */
#define GAME_QUEUE_SIZE 20       // Reduced from 30 to 20 (sufficient for game commands)
/** @brief Velikost animation fronty (5 prvku, snizeno z 8 na 5, jednoduche animace) */
#define ANIMATION_QUEUE_SIZE 5   // Reduced from 8 to 5 (simple animations)
/** @brief Velikost screen saver fronty (3 prvky, nezmeneno, jiz minimalni) */
#define SCREEN_SAVER_QUEUE_SIZE 3 // Unchanged (already minimal)
/** @brief Velikost web server fronty (10 prvku, snizeno z 15 na 10, streaming snizuje potreby) */
#define WEB_SERVER_QUEUE_SIZE 10 // Reduced from 15 to 10 (streaming reduces needs)
// #define MATTER_QUEUE_SIZE 10     // DISABLED - Matter not needed

// ============================================================================
// VELIKOSTI STACKU A PRIORITY TASKU
// ============================================================================

// Velikosti stacku tasku (v bajtech) - PAMETI OPTIMALIZOVANE FAZE 1
// Celkove pouziti stacku: 57KB → 41KB = 16KB uspora (UART task zvysen na 10KB pro game_response_t na stacku)
/** @brief Velikost stacku LED tasku (8KB - KRITICKE: LED task potrebuje vice stacku pro critical sections + velka pole) */
#define LED_TASK_STACK_SIZE (8 * 1024)          // 8KB (CRITICAL: LED task needs more stack for critical sections + large arrays)
/** @brief Velikost stacku Matrix tasku (3KB - nezmeneno, jiz optimalni) */
#define MATRIX_TASK_STACK_SIZE (3 * 1024)       // 3KB (unchanged - already optimal)
/** @brief Velikost stacku Button tasku (3KB - nezmeneno, jiz optimalni) */
#define BUTTON_TASK_STACK_SIZE (3 * 1024)       // 3KB (unchanged - already optimal)
/** @brief Velikost stacku UART tasku (10KB - game_response_t je 3.8KB + overhead) */
#define UART_TASK_STACK_SIZE (10 * 1024)        // ✅ OPRAVA: 10KB (game_response_t je 3.8KB + overhead!)
/** @brief Velikost stacku Game tasku (10KB pro bezpecny error handling, zvetseno z 6KB) */
#define GAME_TASK_STACK_SIZE (10 * 1024)        // ✅ OPRAVA: 10KB pro bezpečný error handling (zvětšeno z 6KB)
/** @brief Velikost stacku Animation tasku (2KB, snizeno z 3KB, jednoduche animace) */
#define ANIMATION_TASK_STACK_SIZE (2 * 1024)    // 2KB (reduced from 3KB - simple animations)
/** @brief Velikost stacku Screen Saver tasku (2KB, snizeno z 3KB, jednoduche vzory) */
#define SCREEN_SAVER_TASK_STACK_SIZE (2 * 1024) // 2KB (reduced from 3KB - simple patterns)
/** @brief Velikost stacku Test tasku (4KB, zvyseno z 2KB pro prevenci stack overflow) */
#define TEST_TASK_STACK_SIZE (4 * 1024)         // 4KB (increased from 2KB to prevent stack overflow)
// #define MATTER_TASK_STACK_SIZE (8 * 1024)       // DISABLED - Matter not needed
/** @brief Velikost stacku Web Server tasku (20KB, zvyseno pro WiFi/HTTP server stabilitu + HTML handling) */
#define WEB_SERVER_TASK_STACK_SIZE (20 * 1024)  // 20KB (increased for WiFi/HTTP server stability + HTML handling)
/** @brief Velikost stacku Reset Button tasku (2KB - nezmeneno) */
#define RESET_BUTTON_TASK_STACK_SIZE (2 * 1024) // 2KB (unchanged)
/** @brief Velikost stacku Promotion Button tasku (2KB - nezmeneno) */
#define PROMOTION_BUTTON_TASK_STACK_SIZE (2 * 1024) // 2KB (unchanged)

// Priority tasku
/** @brief Priorita LED tasku (7 - nejvyssi priorita pro LED timing) */
#define LED_TASK_PRIORITY 7          // ✅ OPRAVA: Nejvyšší priorita pro LED timing
/** @brief Priorita Matrix tasku (6 - hardware vstup) */
#define MATRIX_TASK_PRIORITY 6       // ✅ Hardware input
/** @brief Priorita Button tasku (5 - uzivatelsky vstup) */
#define BUTTON_TASK_PRIORITY 5       // ✅ User input
/** @brief Priorita UART tasku (3 - komunikace) */
#define UART_TASK_PRIORITY 3         // ✅ Communication
/** @brief Priorita Game tasku (4 - snizeno z 5 na 4) */
#define GAME_TASK_PRIORITY 4         // ✅ OPRAVA: Sníženo z 5 na 4
/** @brief Priorita Animation tasku (3 - vizualni efekty) */
#define ANIMATION_TASK_PRIORITY 3    // ✅ Visual effects
/** @brief Priorita Screen Saver tasku (2 - pozadi) */
#define SCREEN_SAVER_TASK_PRIORITY 2 // ✅ Background
/** @brief Priorita Test tasku (1 - pouze pro debug) */
#define TEST_TASK_PRIORITY 1         // ✅ Debug only
// #define MATTER_TASK_PRIORITY 4       // DISABLED - Matter not needed
/** @brief Priorita Web Server tasku (3 - komunikace) */
#define WEB_SERVER_TASK_PRIORITY 3   // ✅ Communication
/** @brief Priorita Reset Button tasku (3 - uzivatelsky vstup) */
#define RESET_BUTTON_TASK_PRIORITY 3 // ✅ User input
/** @brief Priorita Promotion Button tasku (3 - uzivatelsky vstup) */
#define PROMOTION_BUTTON_TASK_PRIORITY 3 // ✅ User input

// ============================================================================
// GLOBALNI QUEUE HANDLES
// ============================================================================

// LED control queues - ODSTRANENO: Pouzivaji se prime LED volani misto toho
// extern QueueHandle_t led_command_queue;  // ❌ REMOVED: Queue hell eliminated
// extern QueueHandle_t led_status_queue;   // ❌ REMOVED: Queue hell eliminated
/** @brief Fronta pro matrix udalosti (piece lifted/placed) */
extern QueueHandle_t matrix_event_queue;
/** @brief Fronta pro matrix prikazy (scan, reset, test) */
extern QueueHandle_t matrix_command_queue;
/** @brief Fronta pro button udalosti (press, release, long press) */
extern QueueHandle_t button_event_queue;
/** @brief Fronta pro button prikazy (reset, status, test) */
extern QueueHandle_t button_command_queue;
/** @brief Fronta pro UART prikazy (komunikace se systemem) */
extern QueueHandle_t uart_command_queue;
/** @brief Fronta pro UART odpovedi (odpovedi ze systemu) */
extern QueueHandle_t uart_response_queue;
/** @brief Fronta pro game prikazy (new game, move, status) */
extern QueueHandle_t game_command_queue;
/** @brief Fronta pro game status (stav hry) */
extern QueueHandle_t game_status_queue;
/** @brief Fronta pro animation prikazy (start, stop, pause) */
extern QueueHandle_t animation_command_queue;
/** @brief Fronta pro animation status (stav animaci) */
extern QueueHandle_t animation_status_queue;
/** @brief Fronta pro screen saver prikazy (activate, deactivate) */
extern QueueHandle_t screen_saver_command_queue;
/** @brief Fronta pro screen saver status (stav screen saveru) */
extern QueueHandle_t screen_saver_status_queue;
/** @brief Fronta pro matter prikazy (DISABLED - Matter neni potreba) */
extern QueueHandle_t matter_command_queue;
/** @brief Fronta pro matter status (DISABLED - Matter neni potreba) */
extern QueueHandle_t matter_status_queue;
/** @brief Fronta pro web prikazy (start, stop, config) */
extern QueueHandle_t web_command_queue;
/** @brief Fronta pro web server prikazy (HTTP requests) */
extern QueueHandle_t web_server_command_queue;
/** @brief Fronta pro web server status (server state) */
extern QueueHandle_t web_server_status_queue;
/** @brief Fronta pro test prikazy (run, status, reset) */
extern QueueHandle_t test_command_queue;

// ============================================================================
// GLOBALNI MUTEX HANDLES
// ============================================================================

/** @brief Mutex pro LED operace (ochrana LED stavu) */
extern SemaphoreHandle_t led_mutex;
/** @brief Mutex pro matrix operace (ochrana matrix stavu) */
extern SemaphoreHandle_t matrix_mutex;
/** @brief Mutex pro button operace (ochrana button stavu) */
extern SemaphoreHandle_t button_mutex;
/** @brief Mutex pro game operace (ochrana game stavu) */
extern SemaphoreHandle_t game_mutex;
/** @brief Mutex pro systemove operace (globalni systemova ochrana) */
extern SemaphoreHandle_t system_mutex;
/** @brief Mutex pro UART operace (ochrana UART vystupu) */
extern SemaphoreHandle_t uart_mutex;

// ============================================================================
// GLOBALNI TIMER HANDLES
// ============================================================================

/** @brief Timer pro periodicke skenovani matice */
extern TimerHandle_t matrix_scan_timer;
/** @brief Timer pro periodicke skenovani tlacitek */
extern TimerHandle_t button_scan_timer;
// extern TimerHandle_t led_update_timer;  // ❌ REMOVED: No longer needed
/** @brief Timer pro periodicke kontroly zdravi systemu */
extern TimerHandle_t system_health_timer;

// ============================================================================
// GPIO PIN POLE
// ============================================================================

/** @brief Pole GPIO pinu pro radky matice (8 vystupu) */
extern const gpio_num_t matrix_row_pins[8];
/** @brief Pole GPIO pinu pro sloupce matice (8 vstupu s pull-up) */
extern const gpio_num_t matrix_col_pins[8];
/** @brief Pole GPIO pinu pro promotion tlacitka A (4 tlacitka) */
extern const gpio_num_t promotion_button_pins_a[4];
/** @brief Pole GPIO pinu pro promotion tlacitka B (4 tlacitka) */
extern const gpio_num_t promotion_button_pins_b[4];

// ============================================================================
// SYSTEMOVE INICIALIZACNI FUNKCE
// ============================================================================

/**
 * @brief Inicializuje sachovy system
 * 
 * Tato funkce inicializuje cely sachovy system vcetne hardware, front,
 * mutexu a timeru. Vola vsechny potrebne inicializacni funkce.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_system_init(void);

/**
 * @brief Inicializuje pameti optimalizacni systemy
 * 
 * Inicializuje buffer pool a streaming output pro efektivni vyuziti pameti.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_memory_systems_init(void);

/**
 * @brief Inicializuje hardwarove komponenty
 * 
 * Inicializuje GPIO piny, LED pasek a ostatni hardware komponenty.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_hardware_init(void);

/**
 * @brief Vytvori vsechny FreeRTOS fronty
 * 
 * Vytvori vsechny potrebne fronty pro komunikaci mezi tasky.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_create_queues(void);

/**
 * @brief Vytvori vsechny FreeRTOS mutexy
 * 
 * Vytvori vsechny potrebne mutexy pro thread-safe pristup k datum.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_create_mutexes(void);

/**
 * @brief Vytvori vsechny FreeRTOS timery
 * 
 * Vytvori periodicke timery pro skenovani matice, tlacitek a systemu.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_create_timers(void);

/**
 * @brief Spusti vsechny FreeRTOS timery
 * 
 * Spusti periodicke timery vytvorene funkci chess_create_timers().
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_start_timers(void);

/**
 * @brief Callback funkce pro button scan timer
 * 
 * Tato funkce je volana periodicky timerem pro skenovani tlacitek.
 * 
 * @param xTimer Handle timeru
 */
void button_scan_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Callback funkce pro matrix scan timer
 * 
 * Tato funkce je volana periodicky timerem pro skenovani matice.
 * 
 * @param xTimer Handle timeru
 */
void matrix_scan_timer_callback(TimerHandle_t xTimer);

/**
 * @brief LED update timer callback - ODSTRANENO: Pouzivaji se prime LED volani
 * @param xTimer Handle timeru
 */
// void led_update_timer_callback(TimerHandle_t xTimer);  // ❌ REMOVED: No longer needed

/**
 * @brief Inicializuje GPIO piny
 * 
 * Inicializuje vsechny GPIO piny pro matrix, tlacitka a LED.
 * Validuje bezpecnost pinu a nastavuje pull-up/pull-down rezistory.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_gpio_init(void);

// ============================================================================
// HARDWARE ABSTRACTION FUNKCE
// ============================================================================

/**
 * @brief Posle retezec pres UART
 * 
 * @param str Retezec k poslani pres UART
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_uart_send_string(const char* str);

/**
 * @brief Posle formatovany retezec pres UART
 * 
 * Funguje jako printf() ale posila vystup pres UART.
 * 
 * @param format Formatovaci retezec (printf styl)
 * @param ... Argumenty pro formatovaci retezec
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_uart_printf(const char* format, ...);

/**
 * @brief Nastavi barvu LED pixelu
 * 
 * @param led_index Index LED (0-72, kde 0-63 sachovnice, 64-72 tlacitka)
 * @param red Cervena komponenta (0-255)
 * @param green Zelena komponenta (0-255)
 * @param blue Modra komponenta (0-255)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_led_set_pixel(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Nastavi vsechny LED na stejnou barvu
 * 
 * @param red Cervena komponenta (0-255)
 * @param green Zelena komponenta (0-255)
 * @param blue Modra komponenta (0-255)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_led_set_all(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Ziska stav matice
 * 
 * @param[out] status Vystupni buffer pro 64-prvkove pole stavu (1 = piece present, 0 = empty)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t chess_matrix_get_status(uint8_t* status);

// ============================================================================
// SYSTEMOVE UTILITY FUNKCE
// ============================================================================

/**
 * @brief Vypise systemove informace
 * 
 * Vypise informace o verzi systemu, konfiguraci GPIO pinu,
 * velikostech front a priority tasku.
 */
void chess_print_system_info(void);

/**
 * @brief Monitoruje systemove tasky
 * 
 * Kontroluje zda vsechny tasky bezi spravne a zda nejsou zablokovane.
 * 
 * @return ESP_OK pokud vsechny tasky bezi spravne, chybovy kod pri problemu
 */
esp_err_t chess_monitor_tasks(void);

// ============================================================================
// UTILITY MAKRA
// ============================================================================

/**
 * @brief Bezpecne vytvoreni fronty s kontrolou chyb
 * 
 * Toto makro vytvori FreeRTOS frontu a automaticky kontroluje uspesnost.
 * Pokud fronta neni vytvorena, loguje chybu a vrati ESP_ERR_NO_MEM.
 * 
 * @param handle Promenna pro ulozeni handle fronty
 * @param size Pocet prvku ve fronte
 * @param item_size Velikost jednoho prvku ve fronte (v bajtech)
 * @param name Nazev fronty pro logovani
 */
#define SAFE_CREATE_QUEUE(handle, size, item_size, name) \
    do { \
        handle = xQueueCreate(size, item_size); \
        if (handle == NULL) { \
            ESP_LOGE(TAG, "Failed to create queue: %s", name); \
            return ESP_ERR_NO_MEM; \
        } \
        ESP_LOGI(TAG, "✓ Queue created: %s", name); \
    } while (0)

/**
 * @brief Bezpecne vytvoreni mutexu s kontrolou chyb
 * 
 * Toto makro vytvori FreeRTOS mutex a automaticky kontroluje uspesnost.
 * Pokud mutex neni vytvoren, loguje chybu a vrati ESP_ERR_NO_MEM.
 * 
 * @param handle Promenna pro ulozeni handle mutexu
 * @param name Nazev mutexu pro logovani
 */
#define SAFE_CREATE_MUTEX(handle, name) \
    do { \
        handle = xSemaphoreCreateMutex(); \
        if (handle == NULL) { \
            ESP_LOGE(TAG, "Failed to create mutex: %s", name); \
            return ESP_ERR_NO_MEM; \
        } \
        ESP_LOGI(TAG, "✓ Mutex created: %s", name); \
    } while (0)

// ============================================================================
// EXTERNI TASK HANDLES (pro pristup z uart_task.c a dalsich modulu)
// ============================================================================

/** @brief Handle pro LED task */
extern TaskHandle_t led_task_handle;
/** @brief Handle pro Matrix task */
extern TaskHandle_t matrix_task_handle;
/** @brief Handle pro Button task */
extern TaskHandle_t button_task_handle;
/** @brief Handle pro UART task */
extern TaskHandle_t uart_task_handle;
/** @brief Handle pro Game task */
extern TaskHandle_t game_task_handle;
/** @brief Handle pro Animation task */
extern TaskHandle_t animation_task_handle;
/** @brief Handle pro Screen Saver task */
extern TaskHandle_t screen_saver_task_handle;
/** @brief Handle pro Test task */
extern TaskHandle_t test_task_handle;
/** @brief Handle pro Matter task (DISABLED - Matter neni potreba) */
extern TaskHandle_t matter_task_handle;
/** @brief Handle pro Web Server task */
extern TaskHandle_t web_server_task_handle;
/** @brief Handle pro Reset Button task */
extern TaskHandle_t reset_button_task_handle;
/** @brief Handle pro Promotion Button task */
extern TaskHandle_t promotion_button_task_handle;

#ifdef __cplusplus
}
#endif

#endif // FREERTOS_CHESS_H