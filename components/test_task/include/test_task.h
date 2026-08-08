/**
 * @file test_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - Test Task Head
 * 
 * This header defines the interface for the test task:
 * - Test types and structures
 * - Test task function prototypes
 * - Functions for control and test status
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * Test Task provides comprehensive testing capabilities for the system:
 * - Hardware tests (LED, matrix, GPIO, WS2812B, reed switches)
 * - System tests (FreeRTOS, queues, mutexes, timers)
 * - Performance benchmarking
 * - Integration tests
 * - Detailed reporting of the result
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
// KONSTANTY A DEFINICE
// ============================================================================

/**
 * @brief Typy vysledku testu
 */
typedef enum {
    TEST_RESULT_PASS = 0,   ///< Test prosel
    TEST_RESULT_FAIL,       ///< Test failed
    TEST_RESULT_SKIP,       ///< Test preskocen
    TEST_RESULT_ERROR       ///< Test error
} test_result_t;

/**
 * @brief Test command types
 */
typedef enum {
    TEST_CMD_RUN_ALL,     ///< Run all tests
    TEST_CMD_RUN_SUITE,   ///< Run specific test suite
    TEST_CMD_RUN_SINGLE,  ///< Run single test
    TEST_CMD_GET_STATUS   ///< Ziskej status testu
} test_command_type_t;

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Run the test task
 * 
 * @param pvParameters Task parameters (not used)
 */
void test_task_start(void *pvParameters);

// ============================================================================
// INITIALIZATION FUNCTION TEST SET
// ============================================================================

/**
 * @brief Initialize the test system
 */
void test_initialize_system(void);

/**
 * @brief Create a new test suite
 * 
 * @param name The name of the set
 * @return set ID or 0xFF on failure
 */
uint8_t test_create_suite(const char* name);

/**
 * @brief Pridej test do sady
 * 
 * @param suite_id ID sady
 * @param name Nazev testu
 * @param enabled Je test povolen?
 */
void test_add_test(uint8_t suite_id, const char* name, bool enabled);

// ============================================================================
// FUNCTION FOR FILLING TEST SETS
// ============================================================================

/**
 * @brief Pridej hardware testy do sady
 */
void test_add_hardware_tests(void);

/**
 * @brief Pridej systemove testy do sady
 */
void test_add_system_tests(void);

/**
 * @brief Pridej performance testy do sady
 */
void test_add_performance_tests(void);

/**
 * @brief Pridej integracni testy do sady
 */
void test_add_integration_tests(void);

// ============================================================================
// TEST RUN FUNCTION
// ============================================================================

/**
 * @brief Run all test suites
 */
void test_run_all_suites(void);

/**
 * @brief Run a specific test suite
 * 
 * @param suite_id ID sady k spusteni
 */
void test_run_suite(uint8_t suite_id);

/**
 * @brief Run the test individually
 * 
 * @param suite_id ID sady
 * @param test_id ID testu
 */
void test_run_single_test(uint8_t suite_id, uint8_t test_id);

/**
 * @brief Complete all test suites
 */
void test_complete_all_suites(void);

// ============================================================================
// IMPLEMENTACE JEDNOTLIVYCH TESTU
// ============================================================================

/** @brief Performed LED matrix test */
test_result_t test_execute_led_matrix_test(void);
/** @brief Performed button test */
test_result_t test_execute_button_test(void);
/** @brief Performed GPIO test */
test_result_t test_execute_gpio_test(void);
/** @brief Performed WS2812B test */
test_result_t test_execute_ws2812b_test(void);
/** @brief Performed reed switch test */
test_result_t test_execute_reed_switch_test(void);
/** @brief Performed power test */
test_result_t test_execute_power_test(void);
/** @brief Performed clock test */
test_result_t test_execute_clock_test(void);
/** @brief Performed memory test */
test_result_t test_execute_memory_test(void);
/** @brief Performed FreeRTOS test */
test_result_t test_execute_freertos_test(void);
/** @brief Performed queue test */
test_result_t test_execute_queue_test(void);
/** @brief Performed mutex test */
test_result_t test_execute_mutex_test(void);
/** @brief Performed timer test */
test_result_t test_execute_timer_test(void);
/** @brief Interrupt test performed */
test_result_t test_execute_interrupt_test(void);
/** @brief Performed error handling test */
test_result_t test_execute_error_handling_test(void);
/** @brief Performed logging test */
test_result_t test_execute_logging_test(void);
/** @brief Conducted configuration test */
test_result_t test_execute_configuration_test(void);
/** @brief Performed a performance test */
test_result_t test_execute_performance_test(void);
/** @brief Performed an integration test */
test_result_t test_execute_integration_test(void);

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

/**
 * @brief Process test commands from the queue
 */
void test_process_commands(void);

/**
 * @brief Vypis status testu
 */
void test_print_status(void);

/**
 * @brief Vypis detailni vysledky testu
 */
void test_print_detailed_results(void);

/**
 * @brief Resetuj vysledky testu
 */
void test_reset_results(void);

/**
 * @brief Run performance benchmark
 */
void test_run_performance_benchmark(void);

// ============================================================================
// EXTERNI PROMENNE
// ============================================================================

/**
 * @brief Test command queue handle
 */
extern QueueHandle_t test_command_queue;

#ifdef __cplusplus
}
#endif

#endif // TEST_TASK_H
