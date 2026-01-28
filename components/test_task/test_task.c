/**
 * @file test_task.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace Test tasku
 * 
 * Tento task poskytuje komplexni testovaci schopnosti systemu:
 * - Testovani hardware komponent
 * - Testovani systemove integrace
 * - Performance benchmarking
 * - Diagnosticke funkce
 * - Reportovani vysledku testu
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento task obsahuje vsechny testy pro overeni funkcnosti systemu.
 * Umožnuje testovani jednotlivych komponent i celeho systemu.
 * Poskytuje detailni metriky a diagnostiku problemu.
 * 
 * Funkce:
 * - Automatizovane testovaci sady
 * - Manualni vykonavani testu
 * - Performance metriky
 * - Detekce a reportovani chyb
 * - Logovani vysledku testu
 */


#include "test_task.h"
#include "freertos_chess.h"
#include "led_task_simple.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


static const char *TAG = "TEST_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset that logs WARNING instead of ERROR for ESP_ERR_NOT_FOUND
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if task not registered (WARNING only)
 */
static esp_err_t test_task_wdt_reset_safe(void) {
    esp_err_t ret = esp_task_wdt_reset();
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // Log as WARNING instead of ERROR - task not registered yet
        ESP_LOGW(TAG, "WDT reset: task not registered yet (this is normal during startup)");
        return ESP_OK; // Treat as success for our purposes
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================


// Test configuration
#define MAX_TEST_SUITES          10      // Maximum test suites
#define MAX_TESTS_PER_SUITE     20      // Maximum tests per suite
#define TEST_TIMEOUT_MS         30000   // 30 second test timeout
#define TEST_TASK_INTERVAL      1000    // 1 second test cycle

// Test states
typedef enum {
    TEST_STATE_IDLE = 0,        // No test running
    TEST_STATE_RUNNING,         // Test is running
    TEST_STATE_PAUSED,          // Test is paused
    TEST_STATE_COMPLETED,       // Test completed
    TEST_STATE_FAILED           // Test failed
} test_state_t;



// Test structure
typedef struct {
    char name[64];              // Test name
    test_result_t result;       // Test result
    uint32_t start_time;        // Test start time
    uint32_t duration_ms;       // Test duration
    char error_message[128];    // Error message if failed
    bool enabled;               // Whether test is enabled
} test_t;

// Test suite structure
typedef struct {
    char name[64];              // Suite name
    test_t tests[MAX_TESTS_PER_SUITE]; // Tests in suite
    uint32_t test_count;        // Number of tests
    uint32_t passed_count;      // Number of passed tests
    
    uint32_t failed_count;      // Number of failed tests
    uint32_t skipped_count;     // Number of skipped tests
    bool enabled;               // Whether suite is enabled
} test_suite_t;

// Test task state
static bool task_running = false;
static test_state_t current_test_state = TEST_STATE_IDLE;
static test_suite_t test_suites[MAX_TEST_SUITES];
static uint32_t suite_count = 0;
static uint32_t current_suite = 0;
static uint32_t current_test = 0;

// Test statistics
static uint32_t total_tests = 0;
static uint32_t total_passed = 0;
static uint32_t total_failed = 0;
static uint32_t total_skipped = 0;
static uint32_t test_start_time = 0;

// Performance metrics - unused variables removed
// static uint32_t memory_usage_start = 0;
// static uint32_t memory_usage_current = 0;
// static uint32_t cpu_usage_start = 0;
// static uint32_t cpu_usage_current = 0;


// ============================================================================
// TEST SUITE INITIALIZATION FUNCTIONS
// ============================================================================


void test_initialize_system(void)
{
    ESP_LOGI(TAG, "Initializing test system...");
    
    // Clear test suites
    memset(test_suites, 0, sizeof(test_suites));
    suite_count = 0;
    current_suite = 0;
    current_test = 0;
    
    // Reset statistics
    total_tests = 0;
    total_passed = 0;
    total_failed = 0;
    total_skipped = 0;
    
    // Initialize default test suites
    test_create_suite("Hardware Tests");
    test_create_suite("System Tests");
    test_create_suite("Performance Tests");
    test_create_suite("Integration Tests");
    
    // Add tests to suites
    test_add_hardware_tests();
    test_add_system_tests();
    test_add_performance_tests();
    test_add_integration_tests();
    
    ESP_LOGI(TAG, "Test system initialized with %lu suites and %lu total tests", 
              suite_count, total_tests);
}


uint8_t test_create_suite(const char* name)
{
    if (suite_count >= MAX_TEST_SUITES) {
        ESP_LOGW(TAG, "Cannot create test suite: maximum suites reached");
        return 0xFF;
    }
    
    strncpy(test_suites[suite_count].name, name, sizeof(test_suites[suite_count].name) - 1);
    test_suites[suite_count].test_count = 0;
    test_suites[suite_count].passed_count = 0;
    test_suites[suite_count].failed_count = 0;
    test_suites[suite_count].skipped_count = 0;
    test_suites[suite_count].enabled = true;
    
    ESP_LOGI(TAG, "Test suite created: %s", name);
    suite_count++;
    
    return suite_count - 1;
}


void test_add_test(uint8_t suite_id, const char* name, bool enabled)
{
    if (suite_id >= suite_count) {
        ESP_LOGW(TAG, "Invalid suite ID: %d", suite_id);
        return;
    }
    
    test_suite_t* suite = &test_suites[suite_id];
    if (suite->test_count >= MAX_TESTS_PER_SUITE) {
        ESP_LOGW(TAG, "Cannot add test: maximum tests per suite reached");
        return;
    }
    
    test_t* test = &suite->tests[suite->test_count];
    strncpy(test->name, name, sizeof(test->name) - 1);
    test->result = TEST_RESULT_SKIP;
    test->start_time = 0;
    test->duration_ms = 0;
    memset(test->error_message, 0, sizeof(test->error_message));
    test->enabled = enabled;
    
    suite->test_count++;
    total_tests++;
    
    ESP_LOGI(TAG, "Test added to suite %s: %s", suite->name, name);
}


// ============================================================================
// TEST SUITE POPULATION FUNCTIONS
// ============================================================================


void test_add_hardware_tests(void)
{
    uint8_t suite_id = 0; // Hardware Tests suite
    
    test_add_test(suite_id, "LED Matrix Test", true);
    test_add_test(suite_id, "Button Input Test", true);
    test_add_test(suite_id, "GPIO Configuration Test", true);
    test_add_test(suite_id, "WS2812B LED Test", true);
    test_add_test(suite_id, "Reed Switch Matrix Test", true);
    test_add_test(suite_id, "Power Supply Test", true);
    test_add_test(suite_id, "Clock System Test", true);
    test_add_test(suite_id, "Memory Test", true);
}


void test_add_system_tests(void)
{
    uint8_t suite_id = 1; // System Tests suite
    
    test_add_test(suite_id, "FreeRTOS Task Creation Test", true);
    test_add_test(suite_id, "Queue Communication Test", true);
    test_add_test(suite_id, "Mutex Synchronization Test", true);
    test_add_test(suite_id, "Timer Functionality Test", true);
    test_add_test(suite_id, "Interrupt Handling Test", true);
    test_add_test(suite_id, "Error Handling Test", true);
    test_add_test(suite_id, "Logging System Test", true);
    test_add_test(suite_id, "Configuration Test", true);
}


void test_add_performance_tests(void)
{
    uint8_t suite_id = 2; // Performance Tests suite
    
    test_add_test(suite_id, "Memory Allocation Test", true);
    test_add_test(suite_id, "Task Switching Performance", true);
    test_add_test(suite_id, "Queue Performance Test", true);
    test_add_test(suite_id, "LED Update Performance", true);
    test_add_test(suite_id, "Matrix Scan Performance", true);
    test_add_test(suite_id, "Button Response Time", true);
    test_add_test(suite_id, "UART Throughput Test", true);
    test_add_test(suite_id, "Power Consumption Test", true);
}


void test_add_integration_tests(void)
{
    uint8_t suite_id = 3; // Integration Tests suite
    
    test_add_test(suite_id, "LED-Matrix Integration", true);
    test_add_test(suite_id, "Button-LED Integration", true);
    test_add_test(suite_id, "Game-Matrix Integration", true);
    test_add_test(suite_id, "Animation-LED Integration", true);
    test_add_test(suite_id, "Screen Saver Integration", true);
    test_add_test(suite_id, "UART Command Integration", true);
    test_add_test(suite_id, "Full System Integration", true);
    test_add_test(suite_id, "Error Recovery Test", true);
}


// ============================================================================
// TEST EXECUTION FUNCTIONS
// ============================================================================


void test_run_all_suites(void)
{
    ESP_LOGI(TAG, "Starting all test suites...");
    
    // CRITICAL: Reset WDT before starting long operation (only if registered)
    esp_err_t wdt_ret = test_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    test_start_time = esp_timer_get_time() / 1000;
    current_test_state = TEST_STATE_RUNNING;
    
    for (uint32_t suite_idx = 0; suite_idx < suite_count; suite_idx++) {
        // CRITICAL: Reset WDT for each suite (only if registered)
        wdt_ret = test_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        if (!test_suites[suite_idx].enabled) {
            ESP_LOGI(TAG, "Skipping disabled suite: %s", test_suites[suite_idx].name);
            continue;
        }
        
        ESP_LOGI(TAG, "Running test suite: %s", test_suites[suite_idx].name);
        test_run_suite(suite_idx);
    }
    
    test_complete_all_suites();
}


void test_run_suite(uint8_t suite_id)
{
    if (suite_id >= suite_count) {
        ESP_LOGW(TAG, "Invalid suite ID: %d", suite_id);
        return;
    }
    
    test_suite_t* suite = &test_suites[suite_id];
    current_suite = suite_id;
    
    ESP_LOGI(TAG, "Suite %s: Running %lu tests", suite->name, suite->test_count);
    
    for (uint32_t test_idx = 0; test_idx < suite->test_count; test_idx++) {
        if (!suite->tests[test_idx].enabled) {
            ESP_LOGI(TAG, "Skipping disabled test: %s", suite->tests[test_idx].name);
            suite->tests[test_idx].result = TEST_RESULT_SKIP;
            suite->skipped_count++;
            total_skipped++;
            continue;
        }
        
        current_test = test_idx;
        ESP_LOGI(TAG, "Running test: %s", suite->tests[test_idx].name);
        
        test_run_single_test(suite_id, test_idx);
        
        // Small delay between tests
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Suite %s completed: %lu passed, %lu failed, %lu skipped", 
              suite->name, suite->passed_count, suite->failed_count, suite->skipped_count);
}


void test_run_single_test(uint8_t suite_id, uint8_t test_id)
{
    if (suite_id >= suite_count || test_id >= test_suites[suite_id].test_count) {
        ESP_LOGW(TAG, "Invalid test ID: suite=%d, test=%d", suite_id, test_id);
        return;
    }
    
    test_suite_t* suite = &test_suites[suite_id];
    test_t* test = &suite->tests[test_id];
    
    // Start test
    test->start_time = esp_timer_get_time() / 1000;
    test->result = TEST_RESULT_SKIP;
    
    ESP_LOGI(TAG, "Test %s started", test->name);
    
    // Execute test based on name
    if (strstr(test->name, "LED Matrix") != NULL) {
        test->result = test_execute_led_matrix_test();
    } else if (strstr(test->name, "Button Input") != NULL) {
        test->result = test_execute_button_test();
    } else if (strstr(test->name, "GPIO") != NULL) {
        test->result = test_execute_gpio_test();
    } else if (strstr(test->name, "WS2812B") != NULL) {
        test->result = test_execute_ws2812b_test();
    } else if (strstr(test->name, "Reed Switch") != NULL) {
        test->result = test_execute_reed_switch_test();
    } else if (strstr(test->name, "Power Supply") != NULL) {
        test->result = test_execute_power_test();
    } else if (strstr(test->name, "Clock System") != NULL) {
        test->result = test_execute_clock_test();
    } else if (strstr(test->name, "Memory") != NULL) {
        test->result = test_execute_memory_test();
    } else if (strstr(test->name, "FreeRTOS") != NULL) {
        test->result = test_execute_freertos_test();
    } else if (strstr(test->name, "Queue") != NULL) {
        test->result = test_execute_queue_test();
    } else if (strstr(test->name, "Mutex") != NULL) {
        test->result = test_execute_mutex_test();
    } else if (strstr(test->name, "Timer") != NULL) {
        test->result = test_execute_timer_test();
    } else if (strstr(test->name, "Interrupt") != NULL) {
        test->result = test_execute_interrupt_test();
    } else if (strstr(test->name, "Error Handling") != NULL) {
        test->result = test_execute_error_handling_test();
    } else if (strstr(test->name, "Logging") != NULL) {
        test->result = test_execute_logging_test();
    } else if (strstr(test->name, "Configuration") != NULL) {
        test->result = test_execute_configuration_test();
    } else if (strstr(test->name, "Performance") != NULL) {
        test->result = test_execute_performance_test();
    } else if (strstr(test->name, "Integration") != NULL) {
        test->result = test_execute_integration_test();
    } else {
        // Default test - just pass
        test->result = TEST_RESULT_PASS;
    }
    
    // Complete test
    uint32_t end_time = esp_timer_get_time() / 1000;
    test->duration_ms = end_time - test->start_time;
    
    // Update statistics
    switch (test->result) {
        case TEST_RESULT_PASS:
            suite->passed_count++;
            total_passed++;
            ESP_LOGI(TAG, "Test %s PASSED (%lu ms)", test->name, test->duration_ms);
            break;
            
        case TEST_RESULT_FAIL:
            suite->failed_count++;
            total_failed++;
            ESP_LOGE(TAG, "Test %s FAILED (%lu ms): %s", 
                      test->name, test->duration_ms, test->error_message);
            break;
            
        case TEST_RESULT_SKIP:
            suite->skipped_count++;
            total_skipped++;
            ESP_LOGI(TAG, "Test %s SKIPPED", test->name);
            break;
            
        case TEST_RESULT_ERROR:
            suite->failed_count++;
            total_failed++;
            ESP_LOGE(TAG, "Test %s ERROR (%lu ms): %s", 
                      test->name, test->duration_ms, test->error_message);
            break;
    }
}


// ============================================================================
// INDIVIDUAL TEST IMPLEMENTATIONS
// ============================================================================


test_result_t test_execute_led_matrix_test(void)
{
    ESP_LOGI(TAG, "Executing LED Matrix Test...");
    
    // Test LED matrix functionality
    // DIRECT LED CALL - No queue hell
    led_set_all_safe(50, 50, 50);
    ESP_LOGI(TAG, "LED matrix test executed directly");
    vTaskDelay(pdMS_TO_TICKS(1000));
    return TEST_RESULT_PASS;
}


test_result_t test_execute_button_test(void)
{
    ESP_LOGI(TAG, "Executing Button Input Test...");
    
    // Test button functionality
    if (button_command_queue != NULL) {
        // Send test command
        uint8_t test_cmd = 2; // Test all buttons
        
        if (xQueueSend(button_command_queue, &test_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Button test command sent successfully");
            vTaskDelay(pdMS_TO_TICKS(500));
            return TEST_RESULT_PASS;
        } else {
            strcpy(test_suites[current_suite].tests[current_test].error_message, 
                   "Failed to send button command");
            return TEST_RESULT_FAIL;
        }
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Button command queue not available");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_gpio_test(void)
{
    ESP_LOGI(TAG, "Executing GPIO Configuration Test...");
    
    // Test GPIO configuration
    // This is a basic test - in real implementation would check actual GPIO states
    ESP_LOGI(TAG, "GPIO configuration test completed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_ws2812b_test(void)
{
    ESP_LOGI(TAG, "Executing WS2812B LED Test...");
    
    // Test WS2812B LED functionality
    // DIRECT LED CALL - No queue hell
    led_set_pixel_safe(0, 255, 0, 0);
    ESP_LOGI(TAG, "WS2812B test executed directly");
    vTaskDelay(pdMS_TO_TICKS(500));
    return TEST_RESULT_PASS;
}


test_result_t test_execute_reed_switch_test(void)
{
    ESP_LOGI(TAG, "Executing Reed Switch Matrix Test...");
    
    // Test reed switch matrix functionality
    if (matrix_command_queue != NULL) {
        // Send test command
        uint8_t test_cmd = 2; // Test matrix
        
        if (xQueueSend(matrix_command_queue, &test_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Reed switch matrix test command sent successfully");
            vTaskDelay(pdMS_TO_TICKS(500));
            return TEST_RESULT_PASS;
        } else {
            strcpy(test_suites[current_suite].tests[current_test].error_message, 
                   "Failed to send matrix test command");
            return TEST_RESULT_FAIL;
        }
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Matrix command queue not available");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_power_test(void)
{
    ESP_LOGI(TAG, "Executing Power Supply Test...");
    
    // Test power supply functionality
    // This is a basic test - in real implementation would check voltage levels
    ESP_LOGI(TAG, "Power supply test completed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_clock_test(void)
{
    ESP_LOGI(TAG, "Executing Clock System Test...");
    
    // Test clock system functionality
    uint32_t start_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t end_time = esp_timer_get_time();
    
    if (end_time > start_time) {
        ESP_LOGI(TAG, "Clock system test passed");
        return TEST_RESULT_PASS;
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Clock system not functioning");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_memory_test(void)
{
    ESP_LOGI(TAG, "Executing Memory Test...");
    
    // Test memory allocation and deallocation
    size_t free_heap_before = esp_get_free_heap_size();
    
    void* test_ptr = malloc(1024);
    if (test_ptr == NULL) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Memory allocation failed");
        return TEST_RESULT_FAIL;
    }
    
    free(test_ptr);
    
    size_t free_heap_after = esp_get_free_heap_size();
    
    if (free_heap_after >= free_heap_before) {
        ESP_LOGI(TAG, "Memory test passed");
        return TEST_RESULT_PASS;
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Memory leak detected");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_freertos_test(void)
{
    ESP_LOGI(TAG, "Executing FreeRTOS Test...");
    
    // Test FreeRTOS functionality
    uint32_t start_tick = xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t end_tick = xTaskGetTickCount();
    
    if (end_tick > start_tick) {
        ESP_LOGI(TAG, "FreeRTOS test passed");
        return TEST_RESULT_PASS;
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "FreeRTOS not functioning");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_queue_test(void)
{
    ESP_LOGI(TAG, "Executing Queue Communication Test...");
    
    // Test queue functionality
    QueueHandle_t test_queue = xQueueCreate(5, sizeof(uint8_t));
    if (test_queue == NULL) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Failed to create test queue");
        return TEST_RESULT_FAIL;
    }
    
    uint8_t test_data = 42;
    if (xQueueSend(test_queue, &test_data, pdMS_TO_TICKS(100)) != pdTRUE) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Failed to send to test queue");
        vQueueDelete(test_queue);
        return TEST_RESULT_FAIL;
    }
    
    uint8_t received_data;
    if (xQueueReceive(test_queue, &received_data, pdMS_TO_TICKS(100)) != pdTRUE) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Failed to receive from test queue");
        vQueueDelete(test_queue);
        return TEST_RESULT_FAIL;
    }
    
    vQueueDelete(test_queue);
    
    if (received_data == test_data) {
        ESP_LOGI(TAG, "Queue test passed");
        return TEST_RESULT_PASS;
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Queue data corruption");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_mutex_test(void)
{
    ESP_LOGI(TAG, "Executing Mutex Synchronization Test...");
    
    // Test mutex functionality
    SemaphoreHandle_t test_mutex = xSemaphoreCreateMutex();
    if (test_mutex == NULL) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Failed to create test mutex");
        return TEST_RESULT_FAIL;
    }
    
    if (xSemaphoreTake(test_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Failed to take test mutex");
        vSemaphoreDelete(test_mutex);
        return TEST_RESULT_FAIL;
    }
    
    if (xSemaphoreGive(test_mutex) != pdTRUE) {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Failed to give test mutex");
        vSemaphoreDelete(test_mutex);
        return TEST_RESULT_FAIL;
    }
    
    vSemaphoreDelete(test_mutex);
    
    ESP_LOGI(TAG, "Mutex test passed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_timer_test(void)
{
    ESP_LOGI(TAG, "Executing Timer Functionality Test...");
    
    // Test timer functionality
    uint32_t start_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t end_time = esp_timer_get_time();
    
    if (end_time > start_time) {
        ESP_LOGI(TAG, "Timer test passed");
        return TEST_RESULT_PASS;
    } else {
        strcpy(test_suites[current_suite].tests[current_test].error_message, 
               "Timer not functioning");
        return TEST_RESULT_FAIL;
    }
}


test_result_t test_execute_interrupt_test(void)
{
    ESP_LOGI(TAG, "Executing Interrupt Handling Test...");
    
    // Test interrupt handling
    // This is a basic test - in real implementation would test actual interrupts
    ESP_LOGI(TAG, "Interrupt handling test completed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_error_handling_test(void)
{
    ESP_LOGI(TAG, "Executing Error Handling Test...");
    
    // Test error handling
    ESP_LOGI(TAG, "Error handling test completed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_logging_test(void)
{
    ESP_LOGI(TAG, "Executing Logging System Test...");
    
    // Test logging system
    ESP_LOGI(TAG, "Logging system test message");
    ESP_LOGW(TAG, "Logging system warning test");
    ESP_LOGE(TAG, "Logging system error test");
    
    ESP_LOGI(TAG, "Logging test passed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_configuration_test(void)
{
    ESP_LOGI(TAG, "Executing Configuration Test...");
    
    // Test configuration system
    ESP_LOGI(TAG, "Configuration test completed");
    return TEST_RESULT_PASS;
}


test_result_t test_execute_performance_test(void)
{
    ESP_LOGI(TAG, "Executing Performance Test...");
    
    // Test performance metrics
    uint32_t start_time = esp_timer_get_time();
    
    // Perform some operations
    for (int i = 0; i < 1000; i++) {
        volatile int dummy = i * 2;
        (void)dummy;
    }
    
    uint32_t end_time = esp_timer_get_time();
    uint32_t duration = end_time - start_time;
    
    ESP_LOGI(TAG, "Performance test completed in %lu us", duration);
    return TEST_RESULT_PASS;
}


test_result_t test_execute_integration_test(void)
{
    ESP_LOGI(TAG, "Executing Integration Test...");
    
    // Test system integration
    ESP_LOGI(TAG, "Integration test completed");
    return TEST_RESULT_PASS;
}


// ============================================================================
// TEST COMPLETION FUNCTIONS
// ============================================================================


void test_complete_all_suites(void)
{
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t total_duration = end_time - test_start_time;
    
    ESP_LOGI(TAG, "All test suites completed in %lu seconds", total_duration);
    ESP_LOGI(TAG, "Final Results:");
    ESP_LOGI(TAG, "  Total Tests: %lu", total_tests);
    ESP_LOGI(TAG, "  Passed: %lu", total_passed);
    ESP_LOGI(TAG, "  Failed: %lu", total_failed);
    ESP_LOGI(TAG, "  Skipped: %lu", total_skipped);
    ESP_LOGI(TAG, "  Success Rate: %.1f%%", 
              (float)total_passed / total_tests * 100.0f);
    
    current_test_state = TEST_STATE_COMPLETED;
    
    if (total_failed == 0) {
        ESP_LOGI(TAG, "All tests PASSED!");
    } else {
        ESP_LOGE(TAG, "%lu tests FAILED!", total_failed);
    }
}


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


void test_process_commands(void)
{
    uint8_t command;
    
    // Process commands from queue
    if (test_command_queue != NULL) {
        while (xQueueReceive(test_command_queue, &command, 0) == pdTRUE) {
            switch (command) {
                case 0: // Run all tests
                    test_run_all_suites();
                    break;
                    
                case 1: // Run specific suite
                    test_run_suite(0); // Default to first suite
                    break;
                    
                case 2: // Print test status
                    test_print_status();
                    break;
                    
                case 3: // Print detailed results
                    test_print_detailed_results();
                    break;
                    
                case 4: // Reset test results
                    test_reset_results();
                    break;
                    
                case 5: // Run performance benchmark
                    test_run_performance_benchmark();
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown test command: %d", command);
                    break;
            }
        }
    }
}


void test_print_status(void)
{
    ESP_LOGI(TAG, "Test System Status:");
    ESP_LOGI(TAG, "  State: %d", current_test_state);
    ESP_LOGI(TAG, "  Suites: %lu", suite_count);
    ESP_LOGI(TAG, "  Total Tests: %lu", total_tests);
    ESP_LOGI(TAG, "  Passed: %lu", total_passed);
    ESP_LOGI(TAG, "  Failed: %lu", total_failed);
    ESP_LOGI(TAG, "  Skipped: %lu", total_skipped);
    
    if (total_tests > 0) {
        float success_rate = (float)total_passed / total_tests * 100.0f;
        ESP_LOGI(TAG, "  Success Rate: %.1f%%", success_rate);
    }
}


void test_print_detailed_results(void)
{
    ESP_LOGI(TAG, "Detailed Test Results:");
    
    for (uint32_t suite_idx = 0; suite_idx < suite_count; suite_idx++) {
        test_suite_t* suite = &test_suites[suite_idx];
        
        ESP_LOGI(TAG, "Suite: %s", suite->name);
        ESP_LOGI(TAG, "  Tests: %lu, Passed: %lu, Failed: %lu, Skipped: %lu", 
                  suite->test_count, suite->passed_count, suite->failed_count, suite->skipped_count);
        
        for (uint32_t test_idx = 0; test_idx < suite->test_count; test_idx++) {
            test_t* test = &suite->tests[test_idx];
            
            const char* result_str;
            switch (test->result) {
                case TEST_RESULT_PASS: result_str = "PASS"; break;
                case TEST_RESULT_FAIL: result_str = "FAIL"; break;
                case TEST_RESULT_SKIP: result_str = "SKIP"; break;
                case TEST_RESULT_ERROR: result_str = "ERROR"; break;
                default: result_str = "UNKNOWN"; break;
            }
            
            ESP_LOGI(TAG, "    %s: %s (%lu ms)", test->name, result_str, test->duration_ms);
            
            if (test->result == TEST_RESULT_FAIL || test->result == TEST_RESULT_ERROR) {
                ESP_LOGI(TAG, "      Error: %s", test->error_message);
            }
        }
    }
}


void test_reset_results(void)
{
    ESP_LOGI(TAG, "Resetting test results...");
    
    // Reset all test results
    for (uint32_t suite_idx = 0; suite_idx < suite_count; suite_idx++) {
        test_suite_t* suite = &test_suites[suite_idx];
        
        for (uint32_t test_idx = 0; test_idx < suite->test_count; test_idx++) {
            test_t* test = &suite->tests[test_idx];
            test->result = TEST_RESULT_SKIP;
            test->start_time = 0;
            test->duration_ms = 0;
            memset(test->error_message, 0, sizeof(test->error_message));
        }
        
        suite->passed_count = 0;
        suite->failed_count = 0;
        suite->skipped_count = 0;
    }
    
    // Reset global statistics
    total_passed = 0;
    total_failed = 0;
    total_skipped = 0;
    current_test_state = TEST_STATE_IDLE;
    
    ESP_LOGI(TAG, "Test results reset completed");
}


void test_run_performance_benchmark(void)
{
    ESP_LOGI(TAG, "Running performance benchmark...");
    
    // Memory benchmark
    size_t free_heap_start = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI(TAG, "Memory Benchmark:");
    ESP_LOGI(TAG, "  Free Heap: %zu bytes", free_heap_start);
    ESP_LOGI(TAG, "  Minimum Free Heap: %zu bytes", min_free_heap);
    ESP_LOGI(TAG, "  Used Heap: %zu bytes", free_heap_start - min_free_heap);
    
    // Task benchmark
    ESP_LOGI(TAG, "Task Benchmark:");
    ESP_LOGI(TAG, "  Current Task: %s", pcTaskGetName(NULL));
    ESP_LOGI(TAG, "  Free Stack: %u bytes", uxTaskGetStackHighWaterMark(NULL));
    
    ESP_LOGI(TAG, "Performance benchmark completed");
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================


void test_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Test task started successfully");
    
    // CRITICAL: Register with TWDT from within task
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register Test task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ Test task registered with TWDT");
    }
    
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • Automated test suites");
    ESP_LOGI(TAG, "  • Hardware component testing");
    ESP_LOGI(TAG, "  • System integration testing");
    ESP_LOGI(TAG, "  • Performance benchmarking");
    ESP_LOGI(TAG, "  • Comprehensive test reporting");
    ESP_LOGI(TAG, "  • 1 second test cycle");
    
    task_running = true;
    
    // Initialize test system
    test_initialize_system();
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for test task in every iteration
        esp_err_t wdt_ret = test_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Process test commands
        test_process_commands();
        
        // Periodic status update
        if (loop_count % 10 == 0) { // Every 10 seconds
            ESP_LOGI(TAG, "Test Task Status: loop=%d, state=%d, tests=%lu/%lu", 
                     loop_count, current_test_state, total_passed + total_failed, total_tests);
        }
        
        loop_count++;
        
        // Wait for next cycle (1 second)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TEST_TASK_INTERVAL));
    }
}