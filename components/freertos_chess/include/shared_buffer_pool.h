/**
 * @file shared_buffer_pool.h
 * @brief ESP32-C6 Chess System - Shared Buffer Pool header
 * 
 * Centralized buffer pool to replace malloc/free calls and elimination
 * heap fragmentation problem in commands like led_board and endgame_white.
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-01-27
 * 
 * @details
 * Shared Buffer Pool provides pre-allocated buffers instead of dynamic allocation.
 * Eliminates heap fragmentation and improves memory allocation performance.
 * Contains 4 buffers of 2KB size each (total 8KB).
 * 
 * Advantages:
 * - Eliminate heap fragmentation
 * - Faster allocation (no malloc overhead)
 * - Memory leak prevention (automatic monitoring)
 * - Buffer leak detection
 * 
 * Example of use:
 * @code
 * char* buffer = get_shared_buffer(1536);
 * if (buffer != NULL) {
 * sprintf(buffer, "Hello World");
 * printf("%s\n", buffer);
 * release_shared_buffer(buffer);
 * }
 * @endcode
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
// MAKRA PRO SNADNE POUZITI
// ============================================================================

/**
 * @brief Get a shared buffer with automatic file/line tracking
 * 
 * @param size Minimum required buffer size in bytes
 * @return Pointer to buffer or NULL on failure
 * 
 * @note Uses __FILE__ and __LINE__ macros for debug tracing
 */
#define get_shared_buffer(size) get_shared_buffer_debug(size, __FILE__, __LINE__)

// ============================================================================
// STRUKTURY
// ============================================================================

/**
 * @brief Buffer pool statistics structure
 * 
 * Contains information about buffer pool usage (size, usage, failures).
 */
typedef struct {
    uint32_t pool_size;          ///< Celkovy pocet bufferu v poolu
    uint32_t buffer_size;        ///< Velikost jednoho bufferu v bajtech
    uint32_t current_usage;      ///< Currently allocated buffers
    uint32_t peak_usage;         ///< Maximalni dosazen e pouziti
    uint32_t total_allocations;  ///< Celkovy pocet alokaci
    uint32_t total_releases;     ///< Celkovy pocet uvolneni
    uint32_t allocation_failures;///< Number of failed allocations
} buffer_pool_stats_t;

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initializes the shared buffer pool
 * 
 * Creates a mutex for thread-safe access and initializes all
 * buffers as free. Resets stats.
 * 
 * @return ESP_OK on success, ESP_ERR_NO_MEM on failure of mutex creation
 */
esp_err_t buffer_pool_init(void);

/**
 * @brief Deinicializuje buffer pool a uvolni prostredky
 * 
 * Checks whether buffers have not been released (leak detection)
 * a uvolni mutex.
 */
void buffer_pool_deinit(void);

// ============================================================================
// BUFFER ALLOCATION/RELEASE FUNCTION
// ============================================================================

/**
 * @brief Get the shared buffer from the pool (internal function)
 * 
 * Searches for a free buffer in the pool and marks it as used.
 * Logs allocation information for debug purposes.
 * 
 * @param min_size Minimum required buffer size in bytes
 * @param file Source file name (for debug)
 * @param line Line number (for debug)
 * @return Pointer to buffer or NULL if allocation failed
 * 
 * @note Don't use directly - use get_shared_buffer() macro
 */
char* get_shared_buffer_debug(size_t min_size, const char* file, int line);

/**
 * @brief Release the shared buffer back into the pool
 * 
 * Marks the buffer as free and can be used by other tasks.
 * 
 * @param buffer Pointer to the buffer to free
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if the buffer is invalid
 */
esp_err_t release_shared_buffer(char* buffer);

// ============================================================================
// UTILITIES AND FUNCTION STATUS
// ============================================================================

/**
 * @brief Vypis detailni status buffer poolu do konzole
 * 
 * Displays all buffers, their status (free/used),
 * vlastnika a statistiky.
 */
void buffer_pool_print_status(void);

/**
 * @brief Get buffer pool statistics
 * 
 * @return Structure with current statistics
 */
buffer_pool_stats_t buffer_pool_get_stats(void);

/**
 * @brief Verify that the buffer pool is in a healthy state
 * 
 * Checks whether there are overflows, leaks or other problems.
 * 
 * @return true if the pool is healthy, false if problems are detected
 */
bool buffer_pool_is_healthy(void);

/**
 * @brief Detekuj potencialni buffer leaky
 * 
 * Logs warnings for buffers held longer than expected.
 * Pomaha identifikovat zapomnute uvolneni bufferu.
 */
void buffer_pool_detect_leaks(void);

// ============================================================================
// POMOCNA MAKRA
// ============================================================================

/**
 * @brief Safe buffer allocation with size control
 * 
 * Use: SAFE_GET_BUFFER(ptr, size, cleanup_label)
 * 
 * @par Example:
 * @code
 * char * buffer;
 * SAFE_GET_BUFFER(buffer, 1536, cleanup);
 * // ... use buffer ...
 * cleanup:
 * release_shared_buffer(buffer);
 * @endcode
 */
#define SAFE_GET_BUFFER(ptr, size, cleanup_label) do { \
    ptr = get_shared_buffer(size); \
    if (ptr == NULL) { \
        ESP_LOGE("BUFFER", "Failed to allocate buffer of size %zu", size); \
        goto cleanup_label; \
    } \
} while(0)

/**
 * @brief Safe release of buffer with NULL check
 * 
 * Use: SAFE_RELEASE_BUFFER(ptr)
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
