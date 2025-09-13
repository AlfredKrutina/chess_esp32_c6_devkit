/*
 * ESP32-C6 Chess System v2.4 - LED Task Header
 * 
 * This header defines the LED task interface:
 * - WS2812B LED control (73 LEDs: 64 board + 9 buttons: 8 promotion + 1 reset)
 * - LED animations and patterns
 * - Button LED feedback
 * - Time-multiplexed updates
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include "freertos_chess.h"   // definuje led_command_t, další společné typy
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"



void led_task_start(void *pvParameters);
esp_err_t led_clear_all(void);
void led_process_commands(void);
void led_update_hardware(void);
// void led_execute_command() - REMOVED: Replaced by led_execute_command_new()
void led_execute_command_new(const led_command_t* cmd);
void led_set_pixel_internal(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);
void led_set_all_internal(uint8_t red, uint8_t green, uint8_t blue);
void led_clear_all_internal(void);
void led_show_chess_board(void);
void led_set_button_feedback(uint8_t button_id, bool available);
void led_set_button_press(uint8_t button_id);
void led_set_button_release(uint8_t button_id);
uint32_t led_get_button_color(uint8_t button_id);

// New button LED logic functions
void led_set_button_promotion_available(uint8_t button_id, bool available);
// void led_set_button_pressed_state() - REMOVED: Duplicated led_set_button_pressed()
void led_start_animation(uint32_t duration_ms);
void led_test_pattern(void);

// Smart LED reporting functions
void led_print_compact_status(void);
void led_print_detailed_status(void);
void led_print_changes_only(void);
void led_start_quiet_period(uint32_t duration_ms);

// Safe LED functions for other components
void led_set_pixel_safe(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);
void led_clear_all_safe(void);
void led_set_all_safe(uint8_t red, uint8_t green, uint8_t blue);

// Force immediate LED update for critical operations
void led_force_immediate_update(void);

// LED layer management functions
void led_clear_board_only(void);           // Clear only board LEDs (0-63)
void led_clear_buttons_only(void);         // Clear only button LEDs (64-72)
void led_preserve_buttons(void);           // Preserve button states during operations
void led_update_button_availability_from_game(void); // Update button availability from game state

// Error handling LED functions
void led_error_invalid_move(const led_command_t* cmd);
void led_error_return_piece(const led_command_t* cmd);
void led_error_recovery(const led_command_t* cmd);
void led_show_legal_moves(const led_command_t* cmd);

// Game state integration functions
void led_update_game_state(void);
void led_highlight_pieces_that_can_move(void);
void led_highlight_possible_moves(uint8_t from_square);
void led_clear_all_highlights(void);
void led_player_change_animation(void);
bool led_has_significant_changes(void);

// LED state access functions
uint32_t led_get_led_state(uint8_t led_index);
void led_get_all_states(uint32_t* states, size_t max_count);

// Setup animation after endgame
void led_setup_animation_after_endgame(void);

// Endgame animation control
void led_stop_endgame_animation(void);

// Enhanced brightness control functions
esp_err_t led_set_pixel_enhanced(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue, float brightness);
void led_interpolate_color(const void* from_color, const void* to_color, float progress, void* result_color);
void led_apply_breathing_effect(const void* base_color, float breath_phase, float intensity, void* result_color);
void led_apply_multi_harmonic_pulse(const void* base_color, float pulse_phase, int harmonics, void* result_color);

#endif // LED_TASK_H
