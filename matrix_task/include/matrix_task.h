/*
 * ESP32-C6 Chess System v2.4 - Matrix Task Header
 * 
 * This header defines the matrix task interface:
 * - 8x8 reed switch matrix scanning
 * - Move detection and validation
 * - Matrix event generation
 * - Time-multiplexed GPIO control
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef MATRIX_TASK_H
#define MATRIX_TASK_H

#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Task function signature
void matrix_task_start(void *pvParameters);

// Matrix scanning functions
void matrix_scan_row(uint8_t row);
void matrix_scan_all(void);

// Move detection functions
void matrix_detect_moves(void);
void matrix_detect_complete_move(uint8_t from_square, uint8_t to_square);

// Utility functions
void matrix_square_to_notation(uint8_t square, char* notation);
uint8_t matrix_notation_to_square(const char* notation);
void matrix_print_state(void);
void matrix_simulate_move(const char* from, const char* to);

// Command processing functions
void matrix_process_commands(void);
void matrix_reset(void);

#endif // MATRIX_TASK_H