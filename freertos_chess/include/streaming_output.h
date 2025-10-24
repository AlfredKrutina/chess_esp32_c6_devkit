/**
 * @file streaming_output.h
 * @brief ESP32-C6 Chess System - Streaming Output System Header
 * 
 * Replaces memory-intensive string building with direct streaming output:
 * - Eliminates need for large buffers (saves 2KB+ per large output)
 * - Reduces memory fragmentation
 * - Enables real-time progressive output
 * - Supports multiple output targets (UART, Web, Queue)
 * 
 * Usage Example:
 *   streaming_output_init();
 *   stream_board_header();
 *   for (int row = 7; row >= 0; row--) {
 *       stream_board_row(row, piece_chars);
 *   }
 *   stream_board_footer();
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-27
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
// CONFIGURATION CONSTANTS
// ============================================================================

#define STREAM_LINE_BUFFER_SIZE     256  // Small line buffer instead of large buffers
#define STREAM_MAX_OUTPUT_TARGETS   4    // Maximum concurrent output targets

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

/**
 * @brief Output stream types
 */
typedef enum {
    STREAM_UART = 0,    ///< UART/USB Serial JTAG output
    STREAM_WEB,         ///< Web server HTTP response
    STREAM_QUEUE        ///< FreeRTOS queue output
} stream_type_t;

/**
 * @brief Line ending types
 */
typedef enum {
    STREAM_LF = 0,      ///< Unix line ending (\n)
    STREAM_CRLF         ///< Windows line ending (\r\n)
} stream_line_ending_t;

/**
 * @brief Streaming output configuration
 */
typedef struct {
    stream_type_t type;         ///< Output stream type
    int uart_port;              ///< UART port number (for UART streams)
    void* web_client;           ///< Web client handle (for web streams)
    QueueHandle_t queue;        ///< Queue handle (for queue streams)
    bool auto_flush;            ///< Automatically flush after writes
    stream_line_ending_t line_ending; ///< Line ending type to use
} streaming_output_t;

/**
 * @brief Streaming statistics
 */
typedef struct {
    uint32_t total_writes;          ///< Total number of write operations
    uint32_t total_bytes_written;   ///< Total bytes written
    uint32_t write_errors;          ///< Number of write errors
    uint32_t truncated_writes;      ///< Number of truncated writes
    uint32_t mutex_timeouts;        ///< Number of mutex timeout errors
} streaming_stats_t;

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the streaming output system
 * @return ESP_OK on success, error code on failure
 */
esp_err_t streaming_output_init(void);

/**
 * @brief Deinitialize the streaming output system
 */
void streaming_output_deinit(void);

// ============================================================================
// OUTPUT CONFIGURATION FUNCTIONS
// ============================================================================

/**
 * @brief Set UART as output target
 * @param uart_port UART port number (usually 0 for USB Serial JTAG)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t streaming_set_uart_output(int uart_port);

/**
 * @brief Set web client as output target
 * @param web_client Web client handle (implementation specific)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t streaming_set_web_output(void* web_client);

/**
 * @brief Set FreeRTOS queue as output target
 * @param queue Queue handle to send output messages to
 * @return ESP_OK on success, error code on failure
 */
esp_err_t streaming_set_queue_output(QueueHandle_t queue);

// ============================================================================
// CORE STREAMING FUNCTIONS
// ============================================================================

/**
 * @brief Write formatted string to current output stream
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Write raw data to current output stream
 * @param data Data to write
 * @param len Length of data in bytes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_write(const char* data, size_t len);

/**
 * @brief Write string with line ending to current output stream
 * @param data String to write (without line ending)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_writeln(const char* data);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Flush the output stream
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_flush(void);

/**
 * @brief Enable or disable automatic flushing after each write
 * @param enabled true to enable auto-flush, false to disable
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_set_auto_flush(bool enabled);

/**
 * @brief Set line ending type for writeln operations
 * @param ending Line ending type to use
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_set_line_ending(stream_line_ending_t ending);

// ============================================================================
// HIGH-LEVEL CHESS-SPECIFIC STREAMING FUNCTIONS
// ============================================================================

/**
 * @brief Stream chess board header (column labels and top border)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_board_header(void);

/**
 * @brief Stream one row of the chess board
 * @param row Row number (0-7, where 0=rank 1, 7=rank 8)
 * @param pieces String of 8 characters representing pieces in this row
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_board_row(int row, const char* pieces);

/**
 * @brief Stream chess board footer (bottom border and column labels)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_board_footer(void);

/**
 * @brief Stream LED board header with emoji indicators
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_led_board_header(void);

/**
 * @brief Stream one row of the LED board with colored emoji indicators
 * @param row Row number (0-7)
 * @param led_colors Array of 64 LED colors (RGB values)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t stream_led_board_row(int row, const uint32_t* led_colors);

// ============================================================================
// STATUS AND STATISTICS FUNCTIONS
// ============================================================================

/**
 * @brief Print streaming output statistics to log
 */
void streaming_print_stats(void);

/**
 * @brief Get streaming output statistics
 * @return Current statistics structure
 */
streaming_stats_t streaming_get_stats(void);

/**
 * @brief Reset all statistics counters
 */
void streaming_reset_stats(void);

/**
 * @brief Check if streaming output system is healthy
 * @return true if healthy, false if issues detected
 */
bool streaming_is_healthy(void);

// ============================================================================
// MEMORY OPTIMIZATION MACROS
// ============================================================================

/**
 * @brief Convenience macro to stream a chess board from piece array
 * 
 * Usage:
 *   piece_t board[8][8];
 *   STREAM_CHESS_BOARD(board);
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
 * @brief Convenience macro to stream LED board from LED state array
 * 
 * Usage:
 *   uint32_t led_states[64];
 *   STREAM_LED_BOARD(led_states);
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
 * @brief Convenience macro for streaming large reports in chunks
 * 
 * Usage:
 *   STREAM_CHUNKED_REPORT("Report Title") {
 *       stream_printf("Line 1: %s\n", data1);
 *       stream_printf("Line 2: %d\n", data2);
 *       // ... more lines
 *   }
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
 * @brief End macro for STREAM_CHUNKED_REPORT
 */
#define STREAM_REPORT_END() \
        _stream_result |= stream_writeln("════════════════════════════════════════════════════════════════"); \
        esp_task_wdt_reset(); \
        if (_stream_result != ESP_OK) { \
            ESP_LOGE("STREAMING", "Report streaming failed: %s", esp_err_to_name(_stream_result)); \
        } \
    } while(0)

// ============================================================================
// INTERNAL FUNCTION DECLARATIONS (PRIVATE)
// ============================================================================

// These functions are implemented in streaming_output.c

#ifdef __cplusplus
}
#endif

#endif // STREAMING_OUTPUT_H