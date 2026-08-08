/**
 * @file led_mapping.h
 * @brief ESP32-C6 Chess System - LED Mapping function
 * 
 * This module provides functions to convert between drawer positions and LED indices.
 * Uses a serpentine layout for optimal LED distribution on the box.
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * LED Mapping module converts box positions (row, column) to LED indexes
 * and vice versa. It uses a serpentine layout for the serpentine layout of the LED strip:
 * - Even rows: a->h (left to right)
 * - Odd rows: h->a (right to left)
 * 
 * Example serpentine layout:
 * @code
 * Row 1: LED 0, 1, 2, 3, 4, 5, 6, 7 (a1->h1)
 * Row 2: LED 15, 14, 13, 12, 11, 10, 9, 8 (h2->a2)
 * Row 3: LED 16, 17, 18, 19, 20, 21, 22, 23 (a3->h3)
 * ...
 * @endcode
 */

#ifndef LED_MAPPING_H
#define LED_MAPPING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// POSITION MAPPING FUNCTION
// ============================================================================

/**
 * @brief Converts the box position to an LED index
 * 
 * This function converts the coordinates of the box (row, column) to an LED index
 * in serpentine layout.
 * 
 * @param row Drawer row (0-7, where 0 = rank 1, 7 = rank 8)
 * @param col Inbox column (0-7, where 0 = file a, 7 = file h)
 * @return LED index (0-63) for the position on the box
 * 
 * @note The function automatically handles invalid inputs and logs the error
 */
uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col);

/**
 * @brief Converts the LED index to the box position
 * 
 * This function converts the LED index to the coordinates of the box (row, column)
 * in serpentine layout.
 * 
 * @param led_index LED index (0-63)
 * @param[out] row Pointer to output row (0-7)
 * @param[out] col Pointer to output column (0-7)
 * 
 * @note The function automatically handles invalid inputs and logs the error
 */
void led_index_to_chess_pos(uint8_t led_index, uint8_t* row, uint8_t* col);

/**
 * @brief Converts Sach notation to LED index
 * 
 * This function converts Sachian notation (eg "e2", "a8") to an LED index
 * I will use the serpentine layout.
 * 
 * @param notation Sacha notation in "e2" format (letter a-h, numbers 1-8)
 * @return LED index (0-63) for the given notation, or 0 on error
 * 
 * @note The function automatically handles invalid inputs and logs the error
 * 
 * @par Example to use:
 * @code
 * uint8_t led = chess_notation_to_led_index("e4");  // LED for field e4
 * @endcode
 */
uint8_t chess_notation_to_led_index(const char* notation);

/**
 * @brief Tests the correctness of the LED mapping
 * 
 * This function will automatically test all known positions and verify
 * correctness of conversion between positions and LED indexes.
 * 
 * @note Test results are written to the log
 */
void test_led_mapping(void);

#ifdef __cplusplus
}
#endif

#endif // LED_MAPPING_H
