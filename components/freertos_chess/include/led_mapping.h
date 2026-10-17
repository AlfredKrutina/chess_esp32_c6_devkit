/**
 * @file led_mapping.h
 * @brief ESP32-C6 Chess System - LED Mapping function
 * 
 * This module provides functions to convert between drawer positions and LED indices.
 * Four identical 4x4 boards. The chain starts at e1. Each next board
 * is the same serpentine rotated 90 degrees counter-clockwise.
 *
 * Chain:
 * - LEDs 0-15:  e1 e2 e3 e4 f4 f3 f2 f1 g1 g2 g3 g4 h4 h3 h2 h1
 * - LEDs 16-31: h5 g5 f5 e5 e6 f6 g6 h6 h7 g7 f7 e7 e8 f8 g8 h8
 * - LEDs 32-47: d8 d7 d6 d5 c5 c6 c7 c8 b8 b7 b6 b5 a5 a6 a7 a8
 * - LEDs 48-63: a4 b4 c4 d4 d3 c3 b3 a3 a2 b2 c2 d2 d1 c1 b1 a1
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
