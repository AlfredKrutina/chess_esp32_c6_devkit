/**
 * @file led_task_simple.h
 * @brief Simple LED system - Safe multi-fiber interface
 * 
 * This header defines a simple LED system for ESP32-C6:
 * - Safe multi-fiber interface for LED control
 * - Prime calls WS2812B driver with mutex protection
 * - Optimized for ESP32-C6 RMT
 * - Simple interface without complex fronts
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-02
 * 
 * @details
 * Simple LED system provides clean, safe multi-fiber interfaces for LED control
 * without complex queues and a tax system. It uses the WS2812B driver directly
 * with mutex protection for multi-fiber security.
 * 
 * Features:
 * - Instant LED updates without tax delay
 * - Multi-thread safe with mutex to prevent concurrent access
 * - Simple interface with 3 basic functions
 * - Optimized for ESP32-C6 RMT
 * - Minimum memory requirements
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
// JEDNODUCHE LED SYSTEM API
// ============================================================================

/**
 * @brief Safe LED pixel setup from multiple fibers
 * 
 * @param index LED index (0-72, where 0-63 case, 64-72 button)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ESP_OK on success, error code on failure
 * 
 * @details
 * Set the color of a single LED pixel. The feature will update instantly
 * without tax system. It uses mutex for multi-fiber security and is optimized
 * for ESP32-C6 RMT.
 * 
 * @note This function is thread-safe and can be called from any task
 */
esp_err_t led_set_pixel_safe(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Safely clear all LEDs from multiple fibers
 * 
 * Set all LEDs to black (off).
 * 
 * @return ESP_OK on success, error code on failure
 * 
 * @note This function is thread-safe
 */
esp_err_t led_clear_all_safe(void);

/**
 * @brief Safely set all LEDs to the same color from multiple fibers
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ESP_OK on success, error code on failure
 * 
 * @note This function is thread-safe
 */
esp_err_t led_set_all_safe(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Initialize the LED system
 * 
 * Initializes WS2812B LED driver, create mutex and set
 * all LEDs to off.
 * 
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t led_system_init(void);

/**
 * @brief Verify if the LED system is initialized
 * 
 * @return true if the system is initialized and ready to use
 */
bool led_system_is_initialized(void);

/**
 * @brief Ziskej barvu LED
 * 
 * @param index LED index (0-72)
 * @return LED barva jako uint32_t RGB (0x00RRGGBB)
 */
uint32_t led_get_color(uint8_t index);

/**
 * @brief Start a simple LED task
 * 
 * This is the main function of the LED bag. Do not use it directly.
 * 
 * @param pvParameters Task parameters
 */
void led_task_start(void *pvParameters);

// ============================================================================
// KOMPATIBILNI API (pro existujici kod)
// ============================================================================

/**
 * @brief Compatible functions for existing code
 * 
 * Alias for led_set_pixel_safe().
 * 
 * @param index LED index (0-72)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Compatible functions for existing code
 * 
 * Alias for led_clear_all_safe().
 * 
 * @return ESP_OK on success
 */
esp_err_t led_clear_all(void);

/**
 * @brief Compatible functions for existing code
 * 
 * Alias for led_set_all_safe().
 * 
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_set_all(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif // LED_TASK_SIMPLE_H
