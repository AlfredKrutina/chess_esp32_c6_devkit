/**
 * @file test_task.h
 * @brief ESP32-C6 Chess System v2.4 - Test Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani pro test task:
 * - Typy a struktury testu
 * - Prototypy funkci test tasku
 * - Funkce pro ovladani a stav testu
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Test Task poskytuje komplexni testovaci schopnosti pro system:
 * - Hardware testy (LED, matrix, GPIO, WS2812B, reed switche)
 * - Systemove testy (FreeRTOS, fronty, mutexy, timery)
 * - Performance benchmarking
 * - Integracni testy
 * - Detailni reportovani vysledku
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
    TEST_RESULT_FAIL,       ///< Test selhal
    TEST_RESULT_SKIP,       ///< Test preskocen
    TEST_RESULT_ERROR       ///< Chyba pri testu
} test_result_t;

/**
 * @brief Typy test prikazu
 */
typedef enum {
    TEST_CMD_RUN_ALL,     ///< Spust vsechny testy
    TEST_CMD_RUN_SUITE,   ///< Spust specifickou test sadu
    TEST_CMD_RUN_SINGLE,  ///< Spust jednotlivy test
    TEST_CMD_GET_STATUS   ///< Ziskej status testu
} test_command_type_t;

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Spusti test task
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void test_task_start(void *pvParameters);

// ============================================================================
// INICIALIZACNI FUNKCE TEST SAD
// ============================================================================

/**
 * @brief Inicializuj test system
 */
void test_initialize_system(void);

/**
 * @brief Vytvor novou test sadu
 * 
 * @param name Nazev sady
 * @return ID sady nebo 0xFF pri selhani
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
// FUNKCE PRO NAPLNENI TEST SAD
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
// FUNKCE PRO SPUSTENI TESTU
// ============================================================================

/**
 * @brief Spust vsechny test sady
 */
void test_run_all_suites(void);

/**
 * @brief Spust specifickou test sadu
 * 
 * @param suite_id ID sady k spusteni
 */
void test_run_suite(uint8_t suite_id);

/**
 * @brief Spust jednotlivy test
 * 
 * @param suite_id ID sady
 * @param test_id ID testu
 */
void test_run_single_test(uint8_t suite_id, uint8_t test_id);

/**
 * @brief Dokonci vsechny test sady
 */
void test_complete_all_suites(void);

// ============================================================================
// IMPLEMENTACE JEDNOTLIVYCH TESTU
// ============================================================================

/** @brief Proved LED matrix test */
test_result_t test_execute_led_matrix_test(void);
/** @brief Proved button test */
test_result_t test_execute_button_test(void);
/** @brief Proved GPIO test */
test_result_t test_execute_gpio_test(void);
/** @brief Proved WS2812B test */
test_result_t test_execute_ws2812b_test(void);
/** @brief Proved reed switch test */
test_result_t test_execute_reed_switch_test(void);
/** @brief Proved power test */
test_result_t test_execute_power_test(void);
/** @brief Proved clock test */
test_result_t test_execute_clock_test(void);
/** @brief Proved memory test */
test_result_t test_execute_memory_test(void);
/** @brief Proved FreeRTOS test */
test_result_t test_execute_freertos_test(void);
/** @brief Proved queue test */
test_result_t test_execute_queue_test(void);
/** @brief Proved mutex test */
test_result_t test_execute_mutex_test(void);
/** @brief Proved timer test */
test_result_t test_execute_timer_test(void);
/** @brief Proved interrupt test */
test_result_t test_execute_interrupt_test(void);
/** @brief Proved error handling test */
test_result_t test_execute_error_handling_test(void);
/** @brief Proved logging test */
test_result_t test_execute_logging_test(void);
/** @brief Proved configuration test */
test_result_t test_execute_configuration_test(void);
/** @brief Proved performance test */
test_result_t test_execute_performance_test(void);
/** @brief Proved integracni test */
test_result_t test_execute_integration_test(void);

// ============================================================================
// FUNKCE PRO ZPRACOVANI PRIKAZU
// ============================================================================

/**
 * @brief Zpracuj test prikazy z fronty
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
 * @brief Spust performance benchmark
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
