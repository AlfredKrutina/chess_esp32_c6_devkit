/**
 * @file streaming_output.h
 * @brief ESP32-C6 Chess System - Hlavicka Streaming Output Systemu
 * 
 * Nahradi pameti intenzivni budovani stringu primym streaming vystupem:
 * - Eliminuje potrebu velkych bufferu (usetri 2KB+ na velky vystup)
 * - Snizuje fragmentaci pameti
 * - Umoznuje real-time postupny vystup
 * - Podporuje vice vystupnich cilu (UART, Web, Queue)
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-01-27
 * 
 * @details
 * Streaming Output System umoznuje efektivni vypis velkych textu bez
 * budovani velkych stringu v pameti. Posila data primo do vystupu,
 * coz eliminuje potrebu velkych bufferu a zlepsuje performance.
 * 
 * @par Priklad pouziti:
 * @code
 *   streaming_output_init();
 *   stream_board_header();
 *   for (int row = 7; row >= 0; row--) {
 *       stream_board_row(row, piece_chars);
 *   }
 *   stream_board_footer();
 * @endcode
 */

#ifndef STREAMING_OUTPUT_H
#define STREAMING_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KONFIGURACNI KONSTANTY
// ============================================================================

/** @brief Velikost radkoveho bufferu (misto velkych bufferu) */
#define STREAM_LINE_BUFFER_SIZE     256
/** @brief Maximalni pocet soucastnych vystupnich cilu */
#define STREAM_MAX_OUTPUT_TARGETS   4

// ============================================================================
// DEFINICE TYPU
// ============================================================================

/**
 * @brief Typy vystupnich proudu
 */
typedef enum {
    STREAM_UART = 0,    ///< UART/USB Serial JTAG vystup
    STREAM_WEB,         ///< Web server HTTP odpoved
    STREAM_QUEUE        ///< FreeRTOS fronta vystup
} stream_type_t;

/**
 * @brief Typy koncu radku
 */
typedef enum {
    STREAM_LF = 0,      ///< Unix konec radku (\n)
    STREAM_CRLF         ///< Windows konec radku (\r\n)
} stream_line_ending_t;

/**
 * @brief Konfigurace streaming vystupu
 */
typedef struct {
    stream_type_t type;              ///< Typ vystupniho proudu
    int uart_port;                   ///< Cislo UART portu (pro UART proudy)
    void* web_client;                ///< Handle web klienta (pro web proudy)
    QueueHandle_t queue;             ///< Handle fronty (pro queue proudy)
    bool auto_flush;                 ///< Automaticky flush po zapisu
    stream_line_ending_t line_ending;///< Typ konce radku
} streaming_output_t;

/**
 * @brief Statistiky streamovani
 */
typedef struct {
    uint32_t total_writes;          ///< Celkovy pocet zapisu
    uint32_t total_bytes_written;   ///< Celkovy pocet zapsanych bajtu
    uint32_t write_errors;          ///< Pocet chyb pri zapisu
    uint32_t truncated_writes;      ///< Pocet zkracenych zapisu
    uint32_t mutex_timeouts;        ///< Pocet mutex timeout chyb
} streaming_stats_t;

// ============================================================================
// INICIALIZACNI FUNKCE
// ============================================================================

/**
 * @brief Inicializuj streaming output system
 * 
 * Vytvori mutex pro thread-safe pristup a nastavi vychozi vystup na UART.
 * 
 * @return ESP_OK pri uspechu, ESP_ERR_NO_MEM pri selhani vytvoreni mutexu
 */
esp_err_t streaming_output_init(void);

/**
 * @brief Deinicializuj streaming output system
 * 
 * Uvolni mutex a vymaze interni stav.
 */
void streaming_output_deinit(void);

// ============================================================================
// FUNKCE PRO KONFIGURACI VYSTUPU
// ============================================================================

/**
 * @brief Nastav UART jako vystupni cil
 * 
 * @param uart_port Cislo UART portu (obvykle 0 pro USB Serial JTAG)
 * @return ESP_OK pri uspechu, ESP_ERR_TIMEOUT pri selhani ziskani mutexu
 */
esp_err_t streaming_set_uart_output(int uart_port);

/**
 * @brief Nastav web klienta jako vystupni cil
 * 
 * @param web_client Handle web klienta (implementacne specificky)
 * @return ESP_OK pri uspechu, ESP_ERR_TIMEOUT pri selhani ziskani mutexu
 */
esp_err_t streaming_set_web_output(void* web_client);

/**
 * @brief Nastav FreeRTOS frontu jako vystupni cil
 * 
 * @param queue Handle fronty pro posilani vystupnich zprav
 * @return ESP_OK pri uspechu, ESP_ERR_TIMEOUT pri selhani ziskani mutexu
 */
esp_err_t streaming_set_queue_output(QueueHandle_t queue);

// ============================================================================
// ZAKLADNI STREAMING FUNKCE
// ============================================================================

/**
 * @brief Zapis formatovany retezec do aktualniho vystupniho proudu
 * 
 * @param format Formatovaci retezec (printf styl)
 * @param ... Promenne argumenty pro formatovaci retezec
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Zapis surova data do aktualniho vystupniho proudu
 * 
 * @param data Data k zapsani
 * @param len Delka dat v bajtech
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_write(const char* data, size_t len);

/**
 * @brief Zapis retezec s koncem radku do aktualniho vystupniho proudu
 * 
 * @param data Retezec k zapsani (bez konce radku)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_writeln(const char* data);

// ============================================================================
// UTILITY FUNKCE
// ============================================================================

/**
 * @brief Flush vystupni proud
 * 
 * Zajisti ze vsechna data jsou poslana do vystupu.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_flush(void);

/**
 * @brief Zapni/vypni automaticky flush po kazdem zapisu
 * 
 * @param enabled true pro zapnuti auto-flush, false pro vypnuti
 * @return ESP_OK pri uspechu
 */
esp_err_t stream_set_auto_flush(bool enabled);

/**
 * @brief Nastav typ konce radku pro writeln operace
 * 
 * @param ending Typ konce radku (STREAM_LF nebo STREAM_CRLF)
 * @return ESP_OK pri uspechu
 */
esp_err_t stream_set_line_ending(stream_line_ending_t ending);

// ============================================================================
// VYSOKOUROVNOVE SACHOVE SPECIFICKE STREAMING FUNKCE
// ============================================================================

/**
 * @brief Streamuj hlavicku sachovnice (popisky sloupcu a horni okraj)
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_board_header(void);

/**
 * @brief Streamuj jeden radek sachovnice
 * 
 * @param row Cislo radku (0-7, kde 0=rank 1, 7=rank 8)
 * @param pieces Retezec 8 znaku reprezentujicich figurky v tomto radku
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_board_row(int row, const char* pieces);

/**
 * @brief Streamuj paticku sachovnice (spodni okraj a popisky sloupcu)
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_board_footer(void);

/**
 * @brief Streamuj hlavicku LED sachovnice s emoji indikatory
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_led_board_header(void);

/**
 * @brief Streamuj jeden radek LED sachovnice s barevnymi emoji indikatory
 * 
 * @param row Cislo radku (0-7)
 * @param led_colors Pole 64 LED barev (RGB hodnoty)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t stream_led_board_row(int row, const uint32_t* led_colors);

// ============================================================================
// STATUS A STATISTICKE FUNKCE
// ============================================================================

/**
 * @brief Vypis statistiky streaming vystupu do logu
 */
void streaming_print_stats(void);

/**
 * @brief Ziskej statistiky streaming vystupu
 * 
 * @return Struktura s aktualnimi statistikami
 */
streaming_stats_t streaming_get_stats(void);

/**
 * @brief Resetuj vsechny statisticke citace
 */
void streaming_reset_stats(void);

/**
 * @brief Overi zda je streaming output system ve zdravem stavu
 * 
 * @return true pokud je zdravy, false pokud jsou detekovany problemy
 */
bool streaming_is_healthy(void);

// ============================================================================
// MAKRA PRO OPTIMALIZACI PAMETI
// ============================================================================

/**
 * @brief Pomocne makro pro streamovani sachovnice z pole figurek
 * 
 * Pouziti:
 * @code
 *   piece_t board[8][8];
 *   STREAM_CHESS_BOARD(board);
 * @endcode
 * 
 * Toto makro eliminuje potrebu docasnych string bufferu.
 */
#define STREAM_CHESS_BOARD(board_array) do { \
    stream_board_header(); \
    for (int row = 7; row >= 0; row--) { \
        char row_pieces[9] = {0}; \
        for (int col = 0; col < 8; col++) { \
            piece_t piece = board_array[row][col]; \
            row_pieces[col] = get_piece_char(piece); \
        } \
        stream_board_row(row, row_pieces); \
        esp_task_wdt_reset(); \
    } \
    stream_board_footer(); \
} while(0)

/**
 * @brief Pomocne makro pro streamovani LED sachovnice z pole LED stavu
 * 
 * Pouziti:
 * @code
 *   uint32_t led_states[64];
 *   STREAM_LED_BOARD(led_states);
 * @endcode
 * 
 * Toto makro eliminuje potrebu velkych string bufferu.
 */
#define STREAM_LED_BOARD(led_array) do { \
    stream_led_board_header(); \
    for (int row = 7; row >= 0; row--) { \
        stream_led_board_row(row, led_array); \
        esp_task_wdt_reset(); \
    } \
    stream_writeln(" +---+---+---+---+---+---+---+---+"); \
    stream_writeln("     a   b   c   d   e   f   g   h"); \
} while(0)

/**
 * @brief Pomocne makro pro streamovani velkych reportu po castech
 * 
 * Pouziti:
 * @code
 *   STREAM_CHUNKED_REPORT("Nazev Reportu") {
 *       stream_printf("Radek 1: %s\n", data1);
 *       stream_printf("Radek 2: %d\n", data2);
 *       // ... dalsi radky
 *   }
 * @endcode
 * 
 * Toto makro automaticky zpracovava watchdog resety a kontrolu chyb.
 */
#define STREAM_CHUNKED_REPORT(title) \
    do { \
        esp_err_t _stream_result = ESP_OK; \
        _stream_result |= stream_writeln(""); \
        _stream_result |= stream_writeln("════════════════════════════════════════════════════════════════"); \
        _stream_result |= stream_printf("📊 %s\n", title); \
        _stream_result |= stream_writeln("════════════════════════════════════════════════════════════════"); \
        esp_task_wdt_reset(); \
        if (_stream_result == ESP_OK)

/**
 * @brief Ukoncovaci makro pro STREAM_CHUNKED_REPORT
 */
#define STREAM_REPORT_END() \
        _stream_result |= stream_writeln("════════════════════════════════════════════════════════════════"); \
        esp_task_wdt_reset(); \
        if (_stream_result != ESP_OK) { \
            ESP_LOGE("STREAMING", "Report streaming failed: %s", esp_err_to_name(_stream_result)); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // STREAMING_OUTPUT_H
