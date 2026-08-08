/**
 * @file streaming_output.h
 * @brief ESP32-C6 Chess System - Streaming Output System header
 * 
 * Replace memory-intensive string building with direct streaming output:
 * - Eliminates the need for large buffers (save 2KB+ for large output)
 * - Reduces memory fragmentation
 * - Enables real-time step-by-step output
 * - Supports multiple output targets (UART, Web, Queue)
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-01-27
 * 
 * @details
 * Streaming Output System enables efficient output of large texts without
 * building large strings in memory. It sends data directly to the output,
 * which eliminates the need for large buffers and improves performance.
 * 
 * @par Example to use:
 * @code
 * streaming_output_init();
 * stream_board_header();
 * for (int row = 7; row >= 0; row--) {
 * stream_board_row(row, piece_chars);
 * }
 * stream_board_footer();
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
/** @brief Maximum number of current output targets */
#define STREAM_MAX_OUTPUT_TARGETS   4

// ============================================================================
// DEFINICE TYPU
// ============================================================================

/**
 * @brief Output current types
 */
typedef enum {
    STREAM_UART = 0,    ///< UART/USB Serial JTAG output
    STREAM_WEB,         ///< Web server HTTP response
    STREAM_QUEUE        ///< FreeRTOS queue exit
} stream_type_t;

/**
 * @brief Typy koncu radku
 */
typedef enum {
    STREAM_LF = 0,      ///< Unix konec radku (\n)
    STREAM_CRLF         ///< Windows konec radku (\r\n)
} stream_line_ending_t;

/**
 * @brief Configuration of streaming output
 */
typedef struct {
    stream_type_t type;              ///< Output current type
    int uart_port;                   ///< Cislo UART portu (pro UART proudy)
    void* web_client;                ///< Handle web klienta (pro web proudy)
    QueueHandle_t queue;             ///< Handle fronty (pro queue proudy)
    bool auto_flush;                 ///< Auto flush after write
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
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the streaming output system
 * 
 * Create a mutex for thread-safe access and set the default output to the UART.
 * 
 * @return ESP_OK on success, ESP_ERR_NO_MEM on failure of mutex creation
 */
esp_err_t streaming_output_init(void);

/**
 * @brief Deinicializuj streaming output system
 * 
 * Release the mutex and clear the internal state.
 */
void streaming_output_deinit(void);

// ============================================================================
// OUTPUT CONFIGURATION FUNCTION
// ============================================================================

/**
 * @brief Set UART as output target
 * 
 * @param uart_port UART port number (usually 0 for USB Serial JTAG)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on mutex acquisition failure
 */
esp_err_t streaming_set_uart_output(int uart_port);

/**
 * @brief Set the client website as the output target
 * 
 * @param web_client Web client handle (implementation specific)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on mutex acquisition failure
 */
esp_err_t streaming_set_web_output(void* web_client);

/**
 * @brief Set the FreeRTOS queue as output target
 * 
 * @param queue Handle of the queue for sending outgoing messages
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on mutex acquisition failure
 */
esp_err_t streaming_set_queue_output(QueueHandle_t queue);

// ============================================================================
// ESTABLISH STREAMING FEATURE
// ============================================================================

/**
 * @brief Write the formatted string to the current output stream
 * 
 * @param format Format string (printf style)
 * @param ... Variable arguments for the format string
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Write raw data to the current output stream
 * 
 * @param data Data to write
 * @param len Data length in bytes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_write(const char* data, size_t len);

/**
 * @brief Writes a line-ending string to the current output stream
 * 
 * @param data String to write (no end of line)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_writeln(const char* data);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Flush flush the stream
 * 
 * Ensure that all data is sent to the output.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_flush(void);

/**
 * @brief Enable/disable automatic flush after each write
 * 
 * @param enabled true to enable auto-flush, false to disable
 * @return ESP_OK on success
 */
esp_err_t stream_set_auto_flush(bool enabled);

/**
 * @brief Set the end-of-line type for writeln operations
 * 
 * @param ending Line ending type (STREAM_LF or STREAM_CRLF)
 * @return ESP_OK on success
 */
esp_err_t stream_set_line_ending(stream_line_ending_t ending);

// ============================================================================
// HIGH-LEVEL SACH SPECIFIC STREAMING FUNCTIONS
// ============================================================================

/**
 * @brief Stream inbox header (column labels and top border)
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_board_header(void);

/**
 * @brief Stream a single row of a box
 * 
 * @param row Row number (0-7, where 0=rank 1, 7=rank 8)
 * @param pieces A string of 8 characters representing the pieces in this row
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_board_row(int row, const char* pieces);

/**
 * @brief Stream the box footer (bottom border and column labels)
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_board_footer(void);

/**
 * @brief Stream LED box header with emoji indicators
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_led_board_header(void);

/**
 * @brief Stream a single line LED box with colorful emoji indicators
 * 
 * @param row Row number (0-7)
 * @param led_colors Array of 64 LED colors (RGB values)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_led_board_row(int row, const uint32_t* led_colors);

// ============================================================================
// STATUS AND STATISTICAL FUNCTIONS
// ============================================================================

/**
 * @brief Output of streaming output statistics to the log
 */
void streaming_print_stats(void);

/**
 * @brief Get streaming performance statistics
 * 
 * @return Structure with current statistics
 */
streaming_stats_t streaming_get_stats(void);

/**
 * @brief Reset all statistical citations
 */
void streaming_reset_stats(void);

/**
 * @brief Verify if the streaming output system is in a healthy state
 * 
 * @return true if healthy, false if problems are detected
 */
bool streaming_is_healthy(void);

// ============================================================================
// MACROS FOR MEMORY OPTIMIZATION
// ============================================================================

/**
 * @brief Helper macro for streaming a box from a figure array
 * 
 * Use:
 * @code
 * piece_t board[8][8];
 * STREAM_CHESS_BOARD(board);
 * @endcode
 * 
 * This macro eliminates the need for temporary string buffers.
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
 * @brief A helper macro for streaming the inbox LED from the status LED field
 * 
 * Use:
 * @code
 * uint32_t led_states[64];
 * STREAM_LED_BOARD(led_states);
 * @endcode
 * 
 * This macro eliminates the need for large string buffers.
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
 * @brief Helpful macro for streaming large reports in parts
 * 
 * Use:
 * @code
 * STREAM_CHUNKED_REPORT("Report Name") {
 * stream_printf("Line 1: %s\n", data1);
 * stream_printf("Line 2: %d\n", data2);
 * // ... more lines
 * }
 * @endcode
 * 
 * This macro automatically handles watchdog resets and error checking.
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
