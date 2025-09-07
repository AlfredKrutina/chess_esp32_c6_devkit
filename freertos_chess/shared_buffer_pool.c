/**
 * @file shared_buffer_pool.c
 * @brief ESP32-C6 Chess System - Shared Buffer Pool Implementation
 * 
 * Replaces malloc/free calls with pre-allocated buffer pool to:
 * - Eliminate heap fragmentation
 * - Improve memory allocation performance  
 * - Prevent memory leaks
 * - Control memory usage
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-27
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include "shared_buffer_pool.h"

static const char *TAG = "BUFFER_POOL";

// ============================================================================
// BUFFER POOL CONFIGURATION
// ============================================================================

#define BUFFER_POOL_SIZE 8          // Number of buffers in pool
#define BUFFER_SIZE 2048            // Size of each buffer (was malloc(1536))
#define MAX_BUFFER_WAIT_MS 5000     // Maximum wait time for buffer

// Buffer pool structure
typedef struct {
    char data[BUFFER_SIZE];
    bool in_use;
    TaskHandle_t owner;
    uint32_t allocated_time;
    const char* file;
    int line;
} shared_buffer_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static shared_buffer_t buffer_pool[BUFFER_POOL_SIZE];
static SemaphoreHandle_t buffer_pool_mutex = NULL;
static bool pool_initialized = false;

// Statistics
static uint32_t total_allocations = 0;
static uint32_t total_releases = 0;
static uint32_t peak_usage = 0;
static uint32_t current_usage = 0;
static uint32_t allocation_failures = 0;

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

esp_err_t buffer_pool_init(void)
{
    if (pool_initialized) {
        ESP_LOGW(TAG, "Buffer pool already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing shared buffer pool...");

    // Create mutex for buffer pool protection
    buffer_pool_mutex = xSemaphoreCreateMutex();
    if (buffer_pool_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create buffer pool mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize all buffers as free
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        buffer_pool[i].in_use = false;
        buffer_pool[i].owner = NULL;
        buffer_pool[i].allocated_time = 0;
        buffer_pool[i].file = NULL;
        buffer_pool[i].line = 0;
        memset(buffer_pool[i].data, 0, BUFFER_SIZE);
    }

    // Reset statistics
    total_allocations = 0;
    total_releases = 0;
    peak_usage = 0;
    current_usage = 0;
    allocation_failures = 0;

    pool_initialized = true;
    
    ESP_LOGI(TAG, "✓ Buffer pool initialized: %d buffers × %dB = %dKB total",
             BUFFER_POOL_SIZE, BUFFER_SIZE, (BUFFER_POOL_SIZE * BUFFER_SIZE) / 1024);
    
    return ESP_OK;
}

void buffer_pool_deinit(void)
{
    if (!pool_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing buffer pool...");

    if (buffer_pool_mutex != NULL) {
        xSemaphoreTake(buffer_pool_mutex, portMAX_DELAY);

        // Check for unreleased buffers
        for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
            if (buffer_pool[i].in_use) {
                ESP_LOGW(TAG, "Buffer %d still in use by task %p (allocated at %s:%d)",
                         i, buffer_pool[i].owner, 
                         buffer_pool[i].file ? buffer_pool[i].file : "unknown",
                         buffer_pool[i].line);
            }
        }

        xSemaphoreGive(buffer_pool_mutex);
        vSemaphoreDelete(buffer_pool_mutex);
        buffer_pool_mutex = NULL;
    }

    pool_initialized = false;
    ESP_LOGI(TAG, "Buffer pool deinitialized");
}

// ============================================================================
// BUFFER ALLOCATION/RELEASE FUNCTIONS
// ============================================================================

char* get_shared_buffer_debug(size_t min_size, const char* file, int line)
{
    if (!pool_initialized) {
        ESP_LOGE(TAG, "Buffer pool not initialized");
        return NULL;
    }

    if (min_size > BUFFER_SIZE) {
        ESP_LOGE(TAG, "Requested buffer size %zu exceeds maximum %d", min_size, BUFFER_SIZE);
        allocation_failures++;
        return NULL;
    }

    if (xSemaphoreTake(buffer_pool_mutex, pdMS_TO_TICKS(MAX_BUFFER_WAIT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire buffer pool mutex");
        allocation_failures++;
        return NULL;
    }

    // Find free buffer
    int buffer_index = -1;
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!buffer_pool[i].in_use) {
            buffer_index = i;
            break;
        }
    }

    if (buffer_index == -1) {
        xSemaphoreGive(buffer_pool_mutex);
        ESP_LOGW(TAG, "No free buffers available (requested from %s:%d)", 
                 file ? file : "unknown", line);
        allocation_failures++;
        return NULL;
    }

    // Allocate buffer
    buffer_pool[buffer_index].in_use = true;
    buffer_pool[buffer_index].owner = xTaskGetCurrentTaskHandle();
    buffer_pool[buffer_index].allocated_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    buffer_pool[buffer_index].file = file;
    buffer_pool[buffer_index].line = line;

    // Update statistics
    total_allocations++;
    current_usage++;
    if (current_usage > peak_usage) {
        peak_usage = current_usage;
    }

    xSemaphoreGive(buffer_pool_mutex);

    ESP_LOGD(TAG, "Buffer %d allocated to %p (%s:%d), usage: %d/%d", 
             buffer_index, buffer_pool[buffer_index].owner,
             file ? file : "unknown", line,
             current_usage, BUFFER_POOL_SIZE);

    return buffer_pool[buffer_index].data;
}

esp_err_t release_shared_buffer(char* buffer)
{
    if (!pool_initialized || buffer == NULL) {
        ESP_LOGE(TAG, "Invalid buffer release: pool_init=%d, buffer=%p", 
                 pool_initialized, buffer);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(buffer_pool_mutex, pdMS_TO_TICKS(MAX_BUFFER_WAIT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire buffer pool mutex for release");
        return ESP_ERR_TIMEOUT;
    }

    // Find buffer in pool
    int buffer_index = -1;
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool[i].data == buffer) {
            buffer_index = i;
            break;
        }
    }

    if (buffer_index == -1) {
        xSemaphoreGive(buffer_pool_mutex);
        ESP_LOGE(TAG, "Buffer %p not found in pool", buffer);
        return ESP_ERR_NOT_FOUND;
    }

    if (!buffer_pool[buffer_index].in_use) {
        xSemaphoreGive(buffer_pool_mutex);
        ESP_LOGW(TAG, "Buffer %d already released", buffer_index);
        return ESP_ERR_INVALID_STATE;
    }

    // Release buffer
    buffer_pool[buffer_index].in_use = false;
    buffer_pool[buffer_index].owner = NULL;
    buffer_pool[buffer_index].allocated_time = 0;
    buffer_pool[buffer_index].file = NULL;
    buffer_pool[buffer_index].line = 0;
    
    // Clear buffer contents for security
    memset(buffer_pool[buffer_index].data, 0, BUFFER_SIZE);

    // Update statistics
    total_releases++;
    current_usage--;

    xSemaphoreGive(buffer_pool_mutex);

    ESP_LOGD(TAG, "Buffer %d released, usage: %d/%d", 
             buffer_index, current_usage, BUFFER_POOL_SIZE);

    return ESP_OK;
}

// ============================================================================
// UTILITY AND STATUS FUNCTIONS
// ============================================================================

void buffer_pool_print_status(void)
{
    if (!pool_initialized) {
        ESP_LOGI(TAG, "Buffer pool not initialized");
        return;
    }

    if (xSemaphoreTake(buffer_pool_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for status print");
        return;
    }

    ESP_LOGI(TAG, "=== SHARED BUFFER POOL STATUS ===");
    ESP_LOGI(TAG, "Pool size: %d buffers × %dB = %dKB", 
             BUFFER_POOL_SIZE, BUFFER_SIZE, (BUFFER_POOL_SIZE * BUFFER_SIZE) / 1024);
    ESP_LOGI(TAG, "Current usage: %d/%d buffers (%.1f%%)", 
             current_usage, BUFFER_POOL_SIZE, 
             (float)current_usage / BUFFER_POOL_SIZE * 100.0f);
    ESP_LOGI(TAG, "Peak usage: %d/%d buffers", peak_usage, BUFFER_POOL_SIZE);
    ESP_LOGI(TAG, "Total allocations: %d", total_allocations);
    ESP_LOGI(TAG, "Total releases: %d", total_releases);
    ESP_LOGI(TAG, "Allocation failures: %d", allocation_failures);

    ESP_LOGI(TAG, "Active buffers:");
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool[i].in_use) {
            uint32_t age_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - 
                              buffer_pool[i].allocated_time;
            ESP_LOGI(TAG, "  Buffer %d: Task %p, Age %dms (%s:%d)", 
                     i, buffer_pool[i].owner, age_ms,
                     buffer_pool[i].file ? buffer_pool[i].file : "unknown",
                     buffer_pool[i].line);
        }
    }

    xSemaphoreGive(buffer_pool_mutex);
}

buffer_pool_stats_t buffer_pool_get_stats(void)
{
    buffer_pool_stats_t stats = {0};

    if (!pool_initialized || 
        xSemaphoreTake(buffer_pool_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return stats; // Return zero stats
    }

    stats.pool_size = BUFFER_POOL_SIZE;
    stats.buffer_size = BUFFER_SIZE;
    stats.current_usage = current_usage;
    stats.peak_usage = peak_usage;
    stats.total_allocations = total_allocations;
    stats.total_releases = total_releases;
    stats.allocation_failures = allocation_failures;

    xSemaphoreGive(buffer_pool_mutex);
    return stats;
}

bool buffer_pool_is_healthy(void)
{
    if (!pool_initialized) {
        return false;
    }

    buffer_pool_stats_t stats = buffer_pool_get_stats();
    
    // Health checks
    bool healthy = true;
    
    // Check for excessive usage
    if (stats.current_usage > (BUFFER_POOL_SIZE * 0.8)) {
        ESP_LOGW(TAG, "High buffer usage: %d/%d", stats.current_usage, BUFFER_POOL_SIZE);
        healthy = false;
    }
    
    // Check for allocation failures
    if (stats.allocation_failures > (stats.total_allocations * 0.1)) {
        ESP_LOGW(TAG, "High allocation failure rate: %d/%d", 
                 stats.allocation_failures, stats.total_allocations);
        healthy = false;
    }
    
    // Check for memory leaks
    if (stats.total_allocations != stats.total_releases + stats.current_usage) {
        ESP_LOGW(TAG, "Potential memory leak detected");
        healthy = false;
    }

    return healthy;
}

// ============================================================================
// LEAK DETECTION AND DEBUGGING
// ============================================================================

void buffer_pool_detect_leaks(void)
{
    if (!pool_initialized) {
        return;
    }

    if (xSemaphoreTake(buffer_pool_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for leak detection");
        return;
    }

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool leaks_found = false;

    ESP_LOGI(TAG, "=== BUFFER POOL LEAK DETECTION ===");
    
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool[i].in_use) {
            uint32_t age_ms = current_time - buffer_pool[i].allocated_time;
            
            // Consider buffers older than 30 seconds as potential leaks
            if (age_ms > 30000) {
                ESP_LOGW(TAG, "⚠️ Potential leak - Buffer %d: Task %p, Age %dms (%s:%d)", 
                         i, buffer_pool[i].owner, age_ms,
                         buffer_pool[i].file ? buffer_pool[i].file : "unknown",
                         buffer_pool[i].line);
                leaks_found = true;
            }
        }
    }

    if (!leaks_found) {
        ESP_LOGI(TAG, "✓ No buffer leaks detected");
    }

    xSemaphoreGive(buffer_pool_mutex);
}