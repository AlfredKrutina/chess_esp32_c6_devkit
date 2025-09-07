/**
 * @file test_task.h
 * @brief ESP32-C6 Chess System v2.4 - Test Task Header
 * 
 * This header defines the interface for the test task:
 * - Test types and structures
 * - Test task function prototypes
 * - Test control and status functions
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef TEST_TASK_H
#define TEST_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>


// ============================================================================
// CONSTANTS AND DEFINITIONS
// ============================================================================


// Test result types
typedef enum {
    TEST_RESULT_PASS = 0,       // Test passed
    TEST_RESULT_FAIL,           // Test failed
    TEST_RESULT_SKIP,           // Test skipped
    TEST_RESULT_ERROR           // Test error
} test_result_t;


// Test command types
typedef enum {
    TEST_CMD_RUN_ALL,           // Run all tests
    TEST_CMD_RUN_SUITE,         // Run specific test suite
    TEST_CMD_RUN_SINGLE,        // Run single test
    TEST_CMD_GET_STATUS         // Get test status
} test_command_type_t;


// ============================================================================
// TASK FUNCTION PROTOTYPES
// ============================================================================


/**
 * @brief Start the test task
 * @param pvParameters Task parameters (unused)
 */
void test_task_start(void *pvParameters);


// ============================================================================
// TEST SUITE INITIALIZATION FUNCTIONS
// ============================================================================


/**
 * @brief Initialize the test system
 */
void test_initialize_system(void);

/**
 * @brief Create a new test suite
 * @param name Suite name
 * @return Suite ID or 0xFF if failed
 */
uint8_t test_create_suite(const char* name);

/**
 * @brief Add a test to a suite
 * @param suite_id Suite ID
 * @param name Test name
 * @param enabled Whether test is enabled
 */
void test_add_test(uint8_t suite_id, const char* name, bool enabled);


// ============================================================================
// TEST SUITE POPULATION FUNCTIONS
// ============================================================================


/**
 * @brief Add hardware tests to suite
 */
void test_add_hardware_tests(void);

/**
 * @brief Add system tests to suite
 */
void test_add_system_tests(void);

/**
 * @brief Add performance tests to suite
 */
void test_add_performance_tests(void);

/**
 * @brief Add integration tests to suite
 */
void test_add_integration_tests(void);


// ============================================================================
// TEST EXECUTION FUNCTIONS
// ============================================================================


/**
 * @brief Run all test suites
 */
void test_run_all_suites(void);

/**
 * @brief Run a specific test suite
 * @param suite_id Suite ID to run
 */
void test_run_suite(uint8_t suite_id);

/**
 * @brief Run a single test
 * @param suite_id Suite ID
 * @param test_id Test ID
 */
void test_run_single_test(uint8_t suite_id, uint8_t test_id);

/**
 * @brief Complete all test suites
 */
void test_complete_all_suites(void);


// ============================================================================
// INDIVIDUAL TEST IMPLEMENTATIONS
// ============================================================================


/**
 * @brief Execute LED matrix test
 * @return Test result
 */
test_result_t test_execute_led_matrix_test(void);

/**
 * @brief Execute button test
 * @return Test result
 */
test_result_t test_execute_button_test(void);

/**
 * @brief Execute GPIO test
 * @return Test result
 */
test_result_t test_execute_gpio_test(void);

/**
 * @brief Execute WS2812B test
 * @return Test result
 */
test_result_t test_execute_ws2812b_test(void);

/**
 * @brief Execute reed switch test
 * @return Test result
 */
test_result_t test_execute_reed_switch_test(void);

/**
 * @brief Execute power test
 * @return Test result
 */
test_result_t test_execute_power_test(void);

/**
 * @brief Execute clock test
 * @return Test result
 */
test_result_t test_execute_clock_test(void);

/**
 * @brief Execute memory test
 * @return Test result
 */
test_result_t test_execute_memory_test(void);

/**
 * @brief Execute FreeRTOS test
 * @return Test result
 */
test_result_t test_execute_freertos_test(void);

/**
 * @brief Execute queue test
 * @return Test result
 */
test_result_t test_execute_queue_test(void);

/**
 * @brief Execute mutex test
 * @return Test result
 */
test_result_t test_execute_mutex_test(void);

/**
 * @brief Execute timer test
 * @return Test result
 */
test_result_t test_execute_timer_test(void);

/**
 * @brief Execute interrupt test
 * @return Test result
 */
test_result_t test_execute_interrupt_test(void);

/**
 * @brief Execute error handling test
 * @return Test result
 */
test_result_t test_execute_error_handling_test(void);

/**
 * @brief Execute logging test
 * @return Test result
 */
test_result_t test_execute_logging_test(void);

/**
 * @brief Execute configuration test
 * @return Test result
 */
test_result_t test_execute_configuration_test(void);

/**
 * @brief Execute performance test
 * @return Test result
 */
test_result_t test_execute_performance_test(void);

/**
 * @brief Execute integration test
 * @return Test result
 */
test_result_t test_execute_integration_test(void);


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


/**
 * @brief Process test commands from queue
 */
void test_process_commands(void);

/**
 * @brief Print test status
 */
void test_print_status(void);

/**
 * @brief Print detailed test results
 */
void test_print_detailed_results(void);

/**
 * @brief Reset test results
 */
void test_reset_results(void);

/**
 * @brief Run performance benchmark
 */
void test_run_performance_benchmark(void);


// ============================================================================
// EXTERNAL VARIABLES
// ============================================================================


/**
 * @brief Test command queue handle
 */
extern QueueHandle_t test_command_queue;


#endif // TEST_TASK_H