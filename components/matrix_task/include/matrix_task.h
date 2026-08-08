/**
 * @file matrix_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - Matrix Task Header
 * 
 * This header defines the interface for the matrix task:
 * - Scan of 8x8 reed switch matrix
 * - Stroke detection and validation
 * - Generation of matrix events
 * - Time-multiplexed GPIO control
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * The matrix task is responsible for detecting pieces on the help box
 * 8x8 reed switch matrix. It scans the matrix every 20ms and detects
 * when a player picks up or places a piece. It communicates with the game task via queues.
 * 
 * Hardware:
 * - 8x8 reed switch matrix
 * - Row pins: GPIO10-11,18-23 (outputs)
 * - Column pins: GPIO0-3,6,14,16-17 (inputs with pull-up)
 * - Time-multiplexed with buttons
 */

#ifndef MATRIX_TASK_H
#define MATRIX_TASK_H

#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Start the matrix task
 * 
 * The main functions of the matrix task. It runs in an infinite loop and processes
 * matrix scanning every 1ms. Detects movement of figures and generates events.
 * 
 * @param pvParameters Task parameters (not used)
 */
void matrix_task_start(void *pvParameters);

// ============================================================================
// MATRIX SCAN FUNCTION
// ============================================================================

/**
 * @brief Scan one row of the matrix
 * 
 * Set row pin to HIGH, clear all column pins
 * and updates the matrix state for that row.
 * 
 * @param row Row to scan (0-7)
 */
void matrix_scan_row(uint8_t row);

/**
 * @brief Scan the entire matrix
 * 
 * Scans all 8 rows of the matrix and detects state changes.
 * Uses mutex for thread-safe access to array state.
 */
void matrix_scan_all(void);

// ============================================================================
// DRAFT DETECTION FUNCTION
// ============================================================================

/**
 * @brief Detect moves from a matrix
 * 
 * Search for transitions 1->0 (figure raised) and 0->1 (figure laid down)
 * and generates matrix events.
 */
void matrix_detect_moves(void);

/**
 * @brief Detect complete stroke
 * 
 * Detects a complete move (raised + placed a piece) and a messenger
 * MATRIX_EVENT_MOVE_DETECTED event.
 * 
 * @param from_square Source square (0-63)
 * @param to_square Target square (0-63)
 */
void matrix_detect_complete_move(uint8_t from_square, uint8_t to_square);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Converts an array number to Sach notation
 * 
 * @param square Square number (0-63)
 * @param[out] notation Output buffer for notation (min. 3 characters)
 */
void matrix_square_to_notation(uint8_t square, char* notation);

/**
 * @brief Converts Saxon notation to array number
 * 
 * @param notation Sach's notation (eg "e2")
 * @return Field number (0-63)
 */
uint8_t matrix_notation_to_square(const char* notation);

/**
 * @brief List the current state of the matrix
 * 
 * Prints an 8x8 matrix with the current state of all fields (0 = empty, 1 = figure).
 */
void matrix_print_state(void);

/**
 * @brief Simulate a pull in a matrix
 * 
 * For testing - simulates picking up pieces from one square
 * and placed on another field.
 * 
 * @param from Source's Sach notation (eg "e2")
 * @param to Cila's Sacha notation (eg "e4")
 */
void matrix_simulate_move(const char* from, const char* to);

/**
 * @brief Get the state of the matrix
 * 
 * @param[out] state_buffer Buffer for the 64-element state array (1 = piece, 0 = empty)
 */
void matrix_get_state(uint8_t* state_buffer);

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

/**
 * @brief Process matrix commands from the queue
 * 
 * Reads commands from matrix_command_queue and executes them
 * (scan, reset, test, calibrate, enable/disable).
 */
void matrix_process_commands(void);

/**
 * @brief Reset the matrix state
 * 
 * Clears all internal matrix states and resets pull detection.
 */
void matrix_reset(void);

/**
 * @brief Cancels the local matrix guard and aligns the baseline (previous = HW state).
 *
 * Calls game_task when we ignore the matrix guard on the game side (tutorial / puzzle),
 * or after game_reset — to avoid stuck detection and invalid last_piece_lifted
 * didn't cause false "multiple lifts at once".
 */
void matrix_abort_ambiguous_guard_baseline(void);

/**
 * @brief Set expected physical occupancy for matrix guard (0/1 per field).
 *
 * Matches the recovery target with the logic board (board[]). Calls game_task on activation
 * guard or after NVS restore.
 */
void matrix_guard_apply_expected_occupancy(const uint8_t expected[64]);

/**
 * @brief Is the matrix guard active on the matrix side?
 */
bool matrix_is_guard_mode_active(void);

/**
 * @brief Square waiting for DROP (255 = none), for races with a game pickup queue.
 */
uint8_t matrix_get_pending_lift_square(void);

// ============================================================================
// TIME-MULTIPLEXING FUNCTION
// ============================================================================

/**
 * @brief Release matrix row pins for button scanning
 * 
 * Set all row pins to HIGH (inactive state) to button task
 * could clean column pins without interference. It is called before the scan window button.
 * 
 * @details
 * This function MUST be called before button_scan_all() so it doesn't fail
 * to conflicts on shared column pins (MATRIX_COL_0-7).
 */
void matrix_release_pins(void);

/**
 * @brief Re-enable matrix row pins for matrix scanning
 * 
 * Restore normal matrix scanning mode. It is called button scan window.
 * 
 * @details
 * This function resumes matrix scanning after the button scan window.
 * Matrix can continue normal scanning.
 */
void matrix_acquire_pins(void);

/**
 * @brief Verify if matrix pins are released for button scan
 * 
 * @return true if pins are released (all rows HIGH)
 */
bool matrix_pins_released(void);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_TASK_H
