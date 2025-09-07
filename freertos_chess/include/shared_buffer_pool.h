/**
 * @file shared_buffer_pool.h
 * @brief ESP32-C6 Chess System - Shared Buffer Pool Header
 * 
 * Centralized buffer pool to replace malloc/free calls and eliminate
 * heap fragmentation issues in commands like led_board and endgame_white.
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-27
 */

#ifndef SHARED_BUFFER_POOL_H
#define SHARED_BUFFER_POOL_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MACROS FOR EASY USAGE
// ============================================================================

/**
 * @brief Get shared buffer with automatic file/line tracking
 * @param size Minimum required buffer size
 */
#define get_shared_buffer(size) get_shared_buffer_debug(size, __FILE__, __LINE__)

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Buffer pool statistics structure
 */
typedef struct {
    uint32_t pool_size;             // Total number of buffers in pool
    uint32_t buffer_size;           // Size of each buffer in bytes
    uint32_t current_usage;         // Currently allocated buffers
    uint32_t peak_usage;           // Maximum usage reached
    uint32_t total_allocations;     // Total allocations made
    uint32_t total_releases;       // Total releases made
    uint32_t allocation_failures;   // Failed allocation attempts
} buffer_pool_stats_t;

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the shared buffer pool
 * @return ESP_OK on success, error code on failure
 */
esp_err_t buffer_pool_init(void);

/**
 * @brief Deinitialize the buffer pool and free resources
 */
void buffer_pool_deinit(void);

// ============================================================================
// BUFFER ALLOCATION/RELEASE FUNCTIONS
// ============================================================================

/**
 * @brief Get a shared buffer from the pool (internal function)
 * @param min_size Minimum required buffer size in bytes
 * @param file Source file name (for debugging)
 * @param line Source line number (for debugging)
 * @return Pointer to buffer or NULL if allocation failed
 */
char* get_shared_buffer_debug(size_t min_size, const char* file, int line);

/**
 * @brief Release a shared buffer back to the pool
 * @param buffer Buffer pointer to release
 * @return ESP_OK on success, error code on failure
 */
esp_err_t release_shared_buffer(char* buffer);

// ============================================================================
// UTILITY AND STATUS FUNCTIONS
// ============================================================================

/**
 * @brief Print detailed buffer pool status to console
 */
void buffer_pool_print_status(void);

/**
 * @brief Get buffer pool statistics
 * @return Structure with current statistics
 */
buffer_pool_stats_t buffer_pool_get_stats(void);

/**
 * @brief Check if buffer pool is in healthy state
 * @return true if healthy, false if issues detected
 */
bool buffer_pool_is_healthy(void);

/**
 * @brief Detect potential buffer leaks
 * Logs warnings for buffers held longer than expected
 */
void buffer_pool_detect_leaks(void);

// ============================================================================
// HELPER MACROS
// ============================================================================

/**
 * @brief Safe buffer allocation with size check
 * Usage: SAFE_GET_BUFFER(ptr, size, cleanup_label)
 */
#define SAFE_GET_BUFFER(ptr, size, cleanup_label) do { \
    ptr = get_shared_buffer(size); \
    if (ptr == NULL) { \
        ESP_LOGE("BUFFER", "Failed to allocate buffer of size %zu", size); \
        goto cleanup_label; \
    } \
} while(0)

/**
 * @brief Safe buffer release with null check
 * Usage: SAFE_RELEASE_BUFFER(ptr)
 */
#define SAFE_RELEASE_BUFFER(ptr) do { \
    if (ptr != NULL) { \
        release_shared_buffer(ptr); \
        ptr = NULL; \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // SHARED_BUFFER_POOL_H