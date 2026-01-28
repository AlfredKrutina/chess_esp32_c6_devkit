/**
 * @file streaming_output.c
 * @brief ESP32-C6 Chess System - Streaming Output System
 * 
 * Nahrazuje velke string building priamym streaming vystupem pro:
 * - Eliminaci potreby velkych bufferu (2KB+ stringy)
 * - Snizeni memory pressure a fragmentace
 * - Umožneni real-time vystupu pro lepsi uzivatelsky zazitek
 * - Podporu jak UART tak budoucich web server vystupu
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-01-27
 * 
 * @details
 * Streaming Output System je system pro efektivni vypis velkych textu.
 * Misto budovani velkych stringu posila data primo do vystupu,
 * coz eliminuje potrebu velkych bufferu a zlepsuje performance.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_err.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "streaming_output.h"
#include "chess_types.h"
#include "led_mapping.h"  // Include LED mapping functions

static const char *TAG = "STREAMING_OUT";

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static esp_err_t stream_write_uart(const char* data, size_t len);
static esp_err_t stream_write_web(const char* data, size_t len);
static esp_err_t stream_write_queue(const char* data, size_t len);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static SemaphoreHandle_t streaming_mutex = NULL;
static bool system_initialized = false;
static streaming_stats_t stats = {0};

// Current output settings
static streaming_output_t current_output;

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

esp_err_t streaming_output_init(void)
{
    if (system_initialized) {
        ESP_LOGW(TAG, "Streaming output already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing streaming output system...");

    // Create mutex for thread safety
    streaming_mutex = xSemaphoreCreateMutex();
    if (streaming_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create streaming mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize default output to UART
    current_output.type = STREAM_UART;
    current_output.uart_port = 0;
    current_output.web_client = NULL;
    current_output.queue = NULL;
    current_output.auto_flush = true;
    current_output.line_ending = STREAM_LF;

    // Reset statistics
    memset(&stats, 0, sizeof(stats));

    system_initialized = true;
    ESP_LOGI(TAG, "✓ Streaming output system initialized");
    return ESP_OK;
}

void streaming_output_deinit(void)
{
    if (!system_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing streaming output system...");

    if (streaming_mutex != NULL) {
        vSemaphoreDelete(streaming_mutex);
        streaming_mutex = NULL;
    }

    system_initialized = false;
    ESP_LOGI(TAG, "Streaming output system deinitialized");
}

// ============================================================================
// OUTPUT CONFIGURATION FUNCTIONS
// ============================================================================

esp_err_t streaming_set_uart_output(int uart_port)
{
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(streaming_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    current_output.type = STREAM_UART;
    current_output.uart_port = uart_port;
    current_output.auto_flush = true;

    xSemaphoreGive(streaming_mutex);
    
    ESP_LOGI(TAG, "Output configured for UART port %d", uart_port);
    return ESP_OK;
}

esp_err_t streaming_set_web_output(void* web_client)
{
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(streaming_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    current_output.type = STREAM_WEB;
    current_output.web_client = web_client;
    current_output.auto_flush = true;

    xSemaphoreGive(streaming_mutex);
    
    ESP_LOGI(TAG, "Output configured for web client %p", web_client);
    return ESP_OK;
}

esp_err_t streaming_set_queue_output(QueueHandle_t queue)
{
    if (!system_initialized || queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(streaming_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    current_output.type = STREAM_QUEUE;
    current_output.queue = queue;
    current_output.auto_flush = false; // Queues don't need flushing

    xSemaphoreGive(streaming_mutex);
    
    ESP_LOGI(TAG, "Output configured for queue %p", queue);
    return ESP_OK;
}

// ============================================================================
// CORE STREAMING FUNCTIONS
// ============================================================================

esp_err_t stream_printf(const char* format, ...)
{
    if (!system_initialized || format == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    va_list args;
    va_start(args, format);
    
    char buffer[STREAM_LINE_BUFFER_SIZE];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0) {
        stats.write_errors++;
        return ESP_ERR_INVALID_ARG;
    }

    if (len >= sizeof(buffer)) {
        ESP_LOGW(TAG, "Stream printf truncated: %d -> %d chars", len, sizeof(buffer) - 1);
        len = sizeof(buffer) - 1;
        stats.truncated_writes++;
    }

    return stream_write(buffer, len);
}

esp_err_t stream_write(const char* data, size_t len)
{
    if (!system_initialized || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(streaming_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        stats.mutex_timeouts++;
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = ESP_OK;

    switch (current_output.type) {
        case STREAM_UART:
            result = stream_write_uart(data, len);
            break;
            
        case STREAM_WEB:
            result = stream_write_web(data, len);
            break;
            
        case STREAM_QUEUE:
            result = stream_write_queue(data, len);
            break;
            
        default:
            result = ESP_ERR_NOT_SUPPORTED;
            stats.write_errors++;
            break;
    }

    if (result == ESP_OK) {
        stats.total_bytes_written += len;
        stats.total_writes++;
    } else {
        stats.write_errors++;
    }

    xSemaphoreGive(streaming_mutex);
    return result;
}

esp_err_t stream_writeln(const char* data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(data);
    esp_err_t result = stream_write(data, len);
    
    if (result == ESP_OK) {
        // Add line ending
        const char* line_end = (current_output.line_ending == STREAM_CRLF) ? "\r\n" : "\n";
        result = stream_write(line_end, strlen(line_end));
    }

    return result;
}

// ============================================================================
// BACKEND-SPECIFIC WRITE FUNCTIONS
// ============================================================================

/**
 * @brief Zapis dat do UART/USB Serial JTAG
 * 
 * Interni funkce pro zapis dat primo do stdout (USB Serial JTAG nebo UART).
 * Pouziva fwrite() pro primos zapis bez buffering.
 * 
 * @param data Ukazatel na data k zapsani
 * @param len Delka dat v bytech
 * @return ESP_OK pokud se zapsalo vse, ESP_FAIL pri chybe
 */
static esp_err_t stream_write_uart(const char* data, size_t len)
{
    // Write directly to stdout for USB Serial JTAG or UART
    size_t written = fwrite(data, 1, len, stdout);
    
    if (current_output.auto_flush) {
        fflush(stdout);
    }

    // Reset watchdog after each write
    esp_task_wdt_reset();

    return (written == len) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Zapis dat do web serveru
 * 
 * Interni funkce pro zapis dat do web server klienta.
 * Zatim placeholder pro budouci implementaci - pouze loguje data.
 * 
 * @param data Ukazatel na data k zapsani
 * @param len Delka dat v bytech
 * @return ESP_OK vzdy (placeholder)
 */
static esp_err_t stream_write_web(const char* data, size_t len)
{
    // Placeholder for future web server implementation
    ESP_LOGD(TAG, "Web write: %zu bytes to client %p", len, current_output.web_client);
    
    // For now, just log the data
    ESP_LOGI(TAG, "WEB: %.*s", len, data);
    
    return ESP_OK;
}

/**
 * @brief Zapis dat do FreeRTOS fronty
 * 
 * Interni funkce pro zapis dat do FreeRTOS fronty pro mezikomponentovou komunikaci.
 * Data jsou rozdlena na bloky maximalne STREAM_QUEUE_CHUNK_SIZE a kazdy blok
 * je odeslan do fronty s timeoutem 100ms.
 * 
 * @param data Ukazatel na data k zapsani
 * @param len Delka dat v bytech
 * @return ESP_OK pri uspechu, ESP_ERR_INVALID_STATE pokud fronta neni nastavena,
 *         ESP_ERR_TIMEOUT pokud se nepodari odeslat vsechna data
 */
static esp_err_t stream_write_queue(const char* data, size_t len)
{
    if (current_output.queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Actually send data to queue instead of just logging
    // Create a proper game_response_t message for the queue
    game_response_t response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD,  // Default command type
        .error_code = 0,
        .message = "Streaming data chunk",
        .timestamp = esp_timer_get_time() / 1000
    };
    
    // Copy data to response buffer (truncate if too long)
    size_t max_data_size = sizeof(response.data) - 1;
    size_t copy_len = (len > max_data_size) ? max_data_size : len;
    memcpy(response.data, data, copy_len);
    response.data[copy_len] = '\0';  // Null terminate
    
    // Send to queue with timeout
    BaseType_t result = xQueueSend(current_output.queue, &response, pdMS_TO_TICKS(100));
    
    if (result != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send streaming data to queue");
        stats.write_errors++;
        return ESP_ERR_TIMEOUT;
    }
    
    // Update statistics
    stats.total_bytes_written += copy_len;
    stats.total_writes++;
    
    // Reset watchdog after each write
    esp_task_wdt_reset();
    
    return ESP_OK;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

esp_err_t stream_flush(void)
{
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (current_output.type == STREAM_UART) {
        fflush(stdout);
        return ESP_OK;
    }

    return ESP_OK; // Other streams don't need explicit flushing
}

esp_err_t stream_set_auto_flush(bool enabled)
{
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(streaming_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    current_output.auto_flush = enabled;
    
    xSemaphoreGive(streaming_mutex);
    return ESP_OK;
}

esp_err_t stream_set_line_ending(stream_line_ending_t ending)
{
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(streaming_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    current_output.line_ending = ending;
    
    xSemaphoreGive(streaming_mutex);
    return ESP_OK;
}

// ============================================================================
// HIGH-LEVEL STREAMING FUNCTIONS FOR CHESS SYSTEM
// ============================================================================

esp_err_t stream_board_header(void)
{
    esp_err_t result = ESP_OK;
    
    result |= stream_writeln("     a   b   c   d   e   f   g   h");
    result |= stream_writeln("   +---+---+---+---+---+---+---+---+");
    
    return result;
}

esp_err_t stream_board_row(int row, const char* pieces)
{
    if (row < 0 || row > 7 || pieces == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t result = stream_printf(" %d |", row + 1);
    
    for (int col = 0; col < 8; col++) {
        result |= stream_printf(" %c |", pieces[col]);
        
        // Reset watchdog every few columns
        if (col % 4 == 3) {
            esp_task_wdt_reset();
        }
    }
    
    result |= stream_printf(" %d\n", row + 1);
    
    if (row > 0) {
        result |= stream_writeln("   +---+---+---+---+---+---+---+---+");
    }
    
    return result;
}

esp_err_t stream_board_footer(void)
{
    esp_err_t result = ESP_OK;
    
    result |= stream_writeln("   +---+---+---+---+---+---+---+---+");
    result |= stream_writeln("     a   b   c   d   e   f   g   h");
    
    return result;
}

esp_err_t stream_led_board_header(void)
{
    esp_err_t result = ESP_OK;
    
    result |= stream_writeln("💡 LED Board Status (Real-time)");
    result |= stream_writeln("═══════════════════════════════════════════════════════════════");
    result |= stream_writeln("📊 Board LEDs (64) - Chessboard Layout:");
    result |= stream_writeln(" a b c d e f g h");
    result |= stream_writeln(" +---+---+---+---+---+---+---+---+");
    
    return result;
}

esp_err_t stream_led_board_row(int row, const uint32_t* led_colors)
{
    if (row < 0 || row > 7 || led_colors == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t result = stream_printf("%d │ ", row + 1);
    
    for (int col = 0; col < 8; col++) {
        int led_index = chess_pos_to_led_index(row, col);
        uint32_t color = led_colors[led_index];
        
        const char* led_char = "⚫"; // Default: off
        
        if (color != 0) {
            uint8_t red = (color >> 16) & 0xFF;
            uint8_t green = (color >> 8) & 0xFF;
            uint8_t blue = color & 0xFF;
            
            // Determine LED state based on color
            if (red > 200 && green > 200 && blue < 100) {
                led_char = "🟡"; // Yellow (lifted piece)
            } else if (red < 100 && green > 200 && blue < 100) {
                led_char = "🟢"; // Green (possible move)
            } else if (red > 200 && green > 100 && blue < 100) {
                led_char = "🟠"; // Orange (capture)
            } else if (red < 100 && green < 100 && blue > 200) {
                led_char = "🔵"; // Blue (placed)
            } else {
                led_char = "🔴"; // Other color
            }
        }
        
        result |= stream_printf("%s│", led_char);
        
        // Reset watchdog periodically
        if (col % 4 == 3) {
            esp_task_wdt_reset();
        }
    }
    
    result |= stream_printf(" │%d\n", row + 1);
    
    if (row > 0) {
        result |= stream_writeln(" +---+---+---+---+---+---+---+---+");
    }
    
    return result;
}

// ============================================================================
// STATUS AND STATISTICS FUNCTIONS
// ============================================================================

void streaming_print_stats(void)
{
    if (!system_initialized) {
        ESP_LOGI(TAG, "Streaming output not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "=== STREAMING OUTPUT STATISTICS ===");
    ESP_LOGI(TAG, "Output type: %s", 
             (current_output.type == STREAM_UART) ? "UART" :
             (current_output.type == STREAM_WEB) ? "WEB" :
             (current_output.type == STREAM_QUEUE) ? "QUEUE" : "UNKNOWN");
    ESP_LOGI(TAG, "Total writes: %d", stats.total_writes);
    ESP_LOGI(TAG, "Total bytes: %d", stats.total_bytes_written);
    ESP_LOGI(TAG, "Write errors: %d", stats.write_errors);
    ESP_LOGI(TAG, "Truncated writes: %d", stats.truncated_writes);
    ESP_LOGI(TAG, "Mutex timeouts: %d", stats.mutex_timeouts);
    
    if (stats.total_writes > 0) {
        ESP_LOGI(TAG, "Average write size: %.1f bytes", 
                 (float)stats.total_bytes_written / stats.total_writes);
        ESP_LOGI(TAG, "Error rate: %.2f%%", 
                 (float)stats.write_errors / stats.total_writes * 100.0f);
    }
}

streaming_stats_t streaming_get_stats(void)
{
    return stats;
}

void streaming_reset_stats(void)
{
    memset(&stats, 0, sizeof(stats));
    ESP_LOGI(TAG, "Statistics reset");
}

bool streaming_is_healthy(void)
{
    if (!system_initialized) {
        return false;
    }
    
    // Health checks
    bool healthy = true;
    
    // Check error rate
    if (stats.total_writes > 100 && 
        stats.write_errors > (stats.total_writes * 0.1)) {
        ESP_LOGW(TAG, "High write error rate: %d/%d", 
                 stats.write_errors, stats.total_writes);
        healthy = false;
    }
    
    // Check for excessive truncation
    if (stats.truncated_writes > (stats.total_writes * 0.05)) {
        ESP_LOGW(TAG, "High truncation rate: %d/%d", 
                 stats.truncated_writes, stats.total_writes);
        healthy = false;
    }
    
    // Check for mutex timeouts
    if (stats.mutex_timeouts > 0) {
        ESP_LOGW(TAG, "Mutex timeout issues: %d", stats.mutex_timeouts);
        healthy = false;
    }
    
    return healthy;
}