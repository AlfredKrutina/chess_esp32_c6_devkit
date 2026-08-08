/**
 * @file led_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - LED Task Header
 *
 * This header defines the LED task interface:
 * - WS2812B LED control (73 LEDs: 64 boxes + 9 buttons)
 * - LED animations and patterns
 * - Button LED feedback
 * - Time-multiplexed updates
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * The LED task is responsible for all LED operations in the system:
 * - Control of 73 WS2812B LEDs (64 boxes + 9 buttons)
 * - Advanced animations and effects
 * - Button LED feedback (availability, press/release states)
 * - Error handling and visualization
 * - Thread-safe operation with mutex protection
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include "esp_err.h"
#include "freertos_chess.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PROTOTYPY HLAVNICH FUNKCI
// ============================================================================

/**
 * @brief Start the LED task
 *
 * @param pvParameters Task parameters (not used)
 */
void led_task_start(void *pvParameters);

/**
 * @brief Clear all LEDs
 *
 * @return ESP_OK on success
 */
esp_err_t led_clear_all(void);

/**
 * @brief Process LED commands from the queue
 */
void led_process_commands(void);

/**
 * @brief Update the LED hardware
 *
 * Send current LED data to WS2812B LED strip.
 */
void led_update_hardware(void);

/**
 * @brief Set global LED brightness
 *
 * @param brightness Brightness in percent (0-100)
 */
void led_set_brightness_global(uint8_t brightness);

/**
 * @brief Execute a new LED command
 *
 * @param cmd Pointer to the LED command
 */
void led_execute_command_new(const led_command_t *cmd);

// ============================================================================
// INTERNAL LED FUNCTIONS
// ============================================================================

/**
 * @brief Set LED pixel (internal functions without mutex)
 *
 * @param led_index LED index (0-72)
 * @param red Red (0-255)
 * @param green Green (0-255)
 * @param blue Blue (0-255)
 */
void led_set_pixel_internal(uint8_t led_index, uint8_t red, uint8_t green,
                            uint8_t blue);

/**
 * @brief Set all LEDs (internal function without mutex)
 *
 * @param red Red (0-255)
 * @param green Green (0-255)
 * @param blue Blue (0-255)
 */
void led_set_all_internal(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Clear all LEDs (internal function without mutex)
 */
void led_clear_all_internal(void);

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

/**
 * @brief Show the box on the LED
 */
void led_show_chess_board(void);

/**
 * @brief Set button feedback LED
 *
 * @param button_id Button ID (0-8)
 * @param available Is the button available?
 */
void led_set_button_feedback(uint8_t button_id, bool available);

/**
 * @brief Set the button as pressed
 *
 * @param button_id Button ID (0-8)
 */
void led_set_button_press(uint8_t button_id);

/**
 * @brief Set the button as released
 *
 * @param button_id Button ID (0-8)
 */
void led_set_button_release(uint8_t button_id);

/**
 * @brief Get the color of the button
 *
 * @param button_id Button ID (0-8)
 * @return Color as uint32_t RGB
 */
uint32_t led_get_button_color(uint8_t button_id);

// ============================================================================
// NOVA LOGIKA BUTTON LED
// ============================================================================

/**
 * @brief Set the availability of the power button
 *
 * @param button_id Button ID (0-8)
 * @param available Is the button available for graduation?
 */
void led_set_button_promotion_available(uint8_t button_id, bool available);

/**
 * @brief Start the animation
 *
 * @param duration_ms Animation duration in milliseconds
 */
void led_start_animation(uint32_t duration_ms);

/**
 * @brief Show the test pattern
 */
void led_test_pattern(void);

// ============================================================================
// SMART LED REPORTING FUNCTION
// ============================================================================

/**
 * @brief Vypis kompaktni status LED
 */
void led_print_compact_status(void);

/**
 * @brief Vypis detailni status LED
 */
void led_print_detailed_status(void);

/**
 * @brief List only LED changes
 */
void led_print_changes_only(void);

/**
 * @brief Start a quiet exit period
 *
 * @param duration_ms The duration in milliseconds
 */
void led_start_quiet_period(uint32_t duration_ms);

// ============================================================================
// SAFE LED FUNCTIONS FOR OTHER COMPONENTS
// ============================================================================

/**
 * @brief Set LED pixel (thread-safe with mutex)
 *
 * @param led_index LED index (0-72)
 * @param red Red (0-255)
 * @param green Green (0-255)
 * @param blue Blue (0-255)
 */
void led_set_pixel_safe(uint8_t led_index, uint8_t red, uint8_t green,
                        uint8_t blue);

/**
 * @brief Clear all LEDs (thread-safe with mutex)
 */
void led_clear_all_safe(void);

/**
 * @brief Set all LEDs (thread-safe with mutex)
 *
 * @param red Red (0-255)
 * @param green Green (0-255)
 * @param blue Blue (0-255)
 */
void led_set_all_safe(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Vynuceni okamzita LED aktualizace pro kriticke operace
 */
void led_force_immediate_update(void);

// ============================================================================
// LED LAYER MANAGEMENT FUNCTION
// ============================================================================

/**
 * @brief Clear only the drawer LEDs (0-63), keep the buttons
 */
void led_clear_board_only(void);

/**
 * @brief Clear only the button LEDs (64-72), keep the case
 */
void led_clear_buttons_only(void);

/**
 * @brief Preserve button states during operation
 */
void led_preserve_buttons(void);

/**
 * @brief Update button availability based on game state
 */
void led_update_button_availability_from_game(void);

/**
 * @brief Set the color of all 64 LED boards for HA mod
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param brightness Brightness (0-255) - applied to RGB
 */
void led_set_ha_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

/**
 * @brief Restore inbox after HA mod
 *
 * This function restores the normal display of the inbox (black/white field).
 */
void led_restore_chess_board(void);

/**
 * @brief Reset the LED display of all buttons according to the current state (availability/press)
 * Call after returning from HA mode.
 */
void led_refresh_all_button_leds(void);

// ============================================================================
// ERROR HANDLING LED FUNCTION
// ============================================================================

/**
 * @brief LED error - invalid move
 *
 * @param cmd LED display with error information
 */
void led_error_invalid_move(const led_command_t *cmd);

/**
 * @brief LED error - return figure
 *
 * @param cmd LED display with error information
 */
void led_error_return_piece(const led_command_t *cmd);

/**
 * @brief LED endgame animation
 *
 * @param cmd LED display with animation parameters
 */
void led_anim_endgame(const led_command_t *cmd);

/**
 * @brief LED error recovery
 *
 * @param cmd LED display with recovery information
 */
void led_error_recovery(const led_command_t *cmd);

/**
 * @brief Display legal moves
 *
 * @param cmd LED display with positions of legal moves
 */
void led_show_legal_moves(const led_command_t *cmd);

// ============================================================================
// FEATURES FOR INTEGRATION WITH GAME STATE
// ============================================================================

/**
 * @brief Update LED based on game state
 */
void led_update_game_state(void);

/**
 * @brief Highlight figures that can move
 */
void led_highlight_pieces_that_can_move(void);

/**
 * @brief Highlight possible moves for an array
 *
 * @param from_square The square of the source figure (0-63)
 */
void led_highlight_possible_moves(uint8_t from_square);

/**
 * @brief Clear all highlights
 */
void led_clear_all_highlights(void);

/**
 * @brief Game change animation
 */
void led_player_change_animation(void);

/**
 * @brief Check if the LED has changed
 *
 * @return true if there are significant changes
 */
bool led_has_significant_changes(void);

// ============================================================================
// FUNCTION TO ACCESS LED STATUS
// ============================================================================

/**
 * @brief Ziskej stav LED
 *
 * @param led_index Index LED (0-72)
 * @return Stav LED jako uint32_t RGB
 */
uint32_t led_get_led_state(uint8_t led_index);

/**
 * @brief Get all LED states
 *
 * @param[out] states Field for storing the state (min. 73 elements)
 * @param max_count Maximum number of state to get
 */
void led_get_all_states(uint32_t *states, size_t max_count);

/**
 * @brief Set animation after endgame
 */
void led_setup_animation_after_endgame(void);

/**
 * @brief Zastav endgame animaci
 */
void led_stop_endgame_animation(void);

// BOOT ANIMATION LED FUNCTION
// ============================================================================

/**
 * @brief Start boot animation (progressive light up)
 */
void led_booting_animation(void);

/**
 * @brief Find out if the boot animation is running
 * @return true if the boot animation is in progress
 */
bool led_is_booting(void);

/**
 * @brief LED boot animation step - light up the LED according to progress
 *
 * @param progress_percent Progress in percent (0-100)
 * @details
 * Light up the LED according to the progress of the boot process. Used for display
 * system initialization procedure. Turn on the LED light green.
 */
void led_boot_animation_step(uint8_t progress_percent);

/**
 * @brief LED boot animation fade out - gradually fade all LEDs to 0
 * @details
 * Gradually dim all board LEDs from brightness 128 to 0.
 * Used at the end of the boot process for smooth muting.
 */
void led_boot_animation_fade_out(void);

/**
 * @brief Display of OTA progress on LED (board + buttons).
 *
 * Call from any task; does not block. Recommended frequency limit (~200ms)
 * calls ota_update (throttling).
 *
 * @param progress_percent 0-100; at 0 with no known progress it will show "indeterminate" mode.
 */
void led_ota_progress_ui(uint8_t progress_percent);

/** Restore checkerboard after OTA abort / error (eg BLE abort). */
void led_ota_restore_board_after_update_abort(void);

#ifdef __cplusplus
}
#endif

#endif // LED_TASK_H
