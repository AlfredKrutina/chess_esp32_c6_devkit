/**
 * @file led_task_simple.h
 * @brief SIMPLE LED SYSTEM - Thread-Safe, No Batch, No Queue Hell
 * 
 * ŘEŠENÍ PRO ESP32-C6 RMT LIMITATIONS:
 * - Žádný batch system (konfliktuje s RMT timing)
 * - Žádný queue hell (8 tasků současně)
 * - Žádné triple buffer (memory chaos)
 * - Žádné critical sections (blokují RMT interrupts)
 * - Pouze direct LED calls s thread-safe mutex
 * 
 * Author: Alfred Krutina
 * Version: 2.5 - SIMPLE SYSTEM
 * Date: 2025-09-02
 */

#ifndef LED_TASK_SIMPLE_H
#define LED_TASK_SIMPLE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SIMPLE LED SYSTEM API
// ============================================================================

/**
 * @brief Thread-safe LED pixel setting
 * ✅ OKAMŽITÝ UPDATE - žádný batch system
 * ✅ ATOMICKÝ PŘÍSTUP s mutex
 * ✅ ESP32-C6 RMT optimalizace
 * 
 * @param index LED index (0-72)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_set_pixel_safe(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Thread-safe LED clear all
 * @return ESP_OK on success
 */
esp_err_t led_clear_all_safe(void);

/**
 * @brief Thread-safe LED set all
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_set_all_safe(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Initialize LED system
 * @return ESP_OK on success
 */
esp_err_t led_system_init(void);

/**
 * @brief Check if LED system is initialized
 * @return true if initialized
 */
bool led_system_is_initialized(void);

/**
 * @brief Get LED color
 * @param index LED index (0-72)
 * @return LED color as uint32_t
 */
uint32_t led_get_color(uint8_t index);

/**
 * @brief Start simple LED task
 * @param pvParameters Task parameters
 */
void led_task_start(void *pvParameters);

// ============================================================================
// COMPATIBILITY API (for existing code)
// ============================================================================

/**
 * @brief Compatibility function for existing code
 */
esp_err_t led_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Compatibility function for existing code
 */
esp_err_t led_clear_all(void);

/**
 * @brief Compatibility function for existing code
 */
esp_err_t led_set_all(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif // LED_TASK_SIMPLE_H
