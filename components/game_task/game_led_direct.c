/**
 * @file game_led_direct.c
 * @brief Direct LED function for game task
 * 
 * This file contains functions for direct LED control from the game task.
 * Uses direct LED function calls instead of queues for immediate updates.
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-02
 * 
 * @details
 * The functions in this file provide direct LED control for the display
 * game states, animations and feedback. All functions are used
 * thread-safe LED API for safe calls from any task.
 */

#include "game_task.h"
#include "led_task.h"
#include "freertos_chess.h"
#include "chess_types.h"
#include "game_led_animations.h"
#include "led_mapping.h"
#include "esp_log.h"

static const char *TAG = "GAME_LED_DIRECT";

// ============================================================================
// DIRECT LED FUNCTIONS
// ============================================================================

/**
 * @brief Showing a move using direct LED calls
 * 
 * @param from_row The row of the source array
 * @param from_col The column of the source field
 * @param to_row The row of the target array
 * @param to_col The column of the target field
 * 
 * @details
 * Displays a move animation with progressive illumination of the source and target fields.
 */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col) {
    uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
    uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
    
    // Clear previous highlights
    game_clear_highlights_direct();
    
    // Stroke animation with progressive lighting
    // Step 1: Light source (yellow)
    led_set_pixel_safe(from_led, 255, 255, 0);   // Yellow source
    vTaskDelay(pdMS_TO_TICKS(300)); // 300ms delay
    
    // Step 2: Fade source to orange
    led_set_pixel_safe(from_led, 255, 165, 0);   // Orange source
    vTaskDelay(pdMS_TO_TICKS(200)); // 200ms delay
    
    // Step 3: Light destination (green)
    led_set_pixel_safe(to_led, 0, 255, 0);       // A green destination
    vTaskDelay(pdMS_TO_TICKS(300)); // 300ms delay
    
    // Step 4: Fade source to red (capture indication)
    led_set_pixel_safe(from_led, 255, 0, 0);     // Red source (capture)
    vTaskDelay(pdMS_TO_TICKS(200)); // 200ms delay
    
    // Step 5: Final state - clear source, keep destination
    led_set_pixel_safe(from_led, 0, 0, 0);       // Clear source
    led_set_pixel_safe(to_led, 0, 255, 0);       // Keep destination green
    
    ESP_LOGI(TAG, "Move animation complete: %d,%d -> %d,%d (LEDs %d -> %d)", 
             from_row, from_col, to_row, to_col, from_led, to_led);
}

/**
 * @brief Display figure pick up using direct LED calls
 * 
 * @param row The array row
 * @param col Array column
 * 
 * @details
 * Will display a subtle pulsing effect on the field with a raised figure.
 */
void game_show_piece_lift_direct(uint8_t row, uint8_t col) {
    uint8_t led_index = chess_pos_to_led_index(row, col);
    
    // Subtle vibrant yellow effect
    static uint8_t pulse_phase = 0;
    pulse_phase = (pulse_phase + 1) % 64;
    
    // Fine Pulsation Calculation (220-255)
    uint8_t intensity = 220 + (pulse_phase > 32 ? (64 - pulse_phase) : pulse_phase);
    uint8_t orange_mix = intensity * 0.9; // Soft orange mix
    
    led_set_pixel_safe(led_index, intensity, orange_mix, 0); // Yellow-orange
    ESP_LOGI(TAG, "Piece lift shown with subtle animation: %d,%d (LED %d)", row, col, led_index);
}

/**
 * @brief Show valid moves using direct LED calls
 * 
 * @param valid_positions Array of LED indices of valid moves
 * @param count Number of valid moves
 * 
 * @details
 * Shows a green wave on all fields with valid moves.
 */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count) {
    static uint8_t wave_offset = 0;
    wave_offset = (wave_offset + 1) % 32;
    
    for (int i = 0; i < count; i++) {
        // A gentle green wave for every valid move
        uint8_t wave_intensity = 200 + ((wave_offset + i * 8) % 32);
        uint8_t lime_mix = wave_intensity * 0.8; // Soft lime mix
        
        led_set_pixel_safe(valid_positions[i], lime_mix, wave_intensity, 0); // Greenish yellow
    }
    
    ESP_LOGI(TAG, "Valid moves shown with subtle wave: %d positions", count);
}

/**
 * @brief Error display using direct LED calls
 * 
 * @param row The row of the array with the error
 * @param col The column of the array with the error
 * 
 * @details
 * Shows red color on error field.
 */
void game_show_error_direct(uint8_t row, uint8_t col) {
    uint8_t led_index = chess_pos_to_led_index(row, col);
    
    // Direct call of the LED function
    led_set_pixel_safe(led_index, 255, 0, 0);  // Red for error
    
    ESP_LOGI(TAG, "Error shown: %d,%d (LED %d)", row, col, led_index);
}

/**
 * @brief Clearing highlights using direct LED calls
 * 
 * @details
 * Clears all the highlights on the chessboard, keeping the LEDs on the buttons.
 */
void game_clear_highlights_direct(void) {
    // Clearing only the checkerboard (keeping the LEDs on the buttons)
    led_clear_board_only();
    
    ESP_LOGI(TAG, "Board highlights cleared (preserving button LEDs)");
}

/**
 * @brief Display game state using direct LED calls
 * 
 * @param board Board with pieces (8x8)
 * 
 * @details
 * Displays all pieces on the board using LEDs.
 */
void game_show_state_direct(piece_t *board) {
    // Direct call of LED functions for status display
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t led_index = chess_pos_to_led_index(row, col);
            piece_t piece = board[row * 8 + col];
            
            if (piece != PIECE_EMPTY) {
                if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
                    led_set_pixel_safe(led_index, 255, 255, 255);  // White for white pieces
                } else {
                    led_set_pixel_safe(led_index, 0, 0, 255);      // Blue for black pieces
                }
            } else {
                led_set_pixel_safe(led_index, 0, 0, 0);            // Black for empty
            }
        }
    }
    
    ESP_LOGI(TAG, "Game state shown");
}

/**
 * @brief Show chess using direct LED calls
 * 
 * @param king_row The row of the king in chess
 * @param king_col The king column in chess
 * 
 * @details
 * Shows purple color on the king in chess.
 */
void game_show_check_direct(uint8_t king_row, uint8_t king_col) {
    uint8_t led_index = chess_pos_to_led_index(king_row, king_col);
    
    // Clear previous highlights
    game_clear_highlights_direct();
    
    // Showing chess in purple
    led_set_pixel_safe(led_index, 255, 0, 255);  // Magenta pro check
    
    ESP_LOGI(TAG, "Check indication: King at %d,%d (LED %d) - PURPLE", king_row, king_col, led_index);
}

/**
 * @brief Show player change with wave animation
 * 
 * @param current_player The current player
 * 
 * @details
 * Shows a wave animation from the center when the player changes.
 */
void game_show_player_change_direct(player_t current_player) {
    // Clear previous highlights
    game_clear_highlights_direct();
    
    // Wave animation from center
    uint8_t r = (current_player == PLAYER_WHITE) ? 255 : 0;
    uint8_t g = (current_player == PLAYER_WHITE) ? 255 : 255;
    uint8_t b = (current_player == PLAYER_WHITE) ? 255 : 0;
    
    // Wave animation - progressive lighting from center
    for (int wave = 0; wave < 4; wave++) {
        for (int i = 0; i < 64; i++) {
            int row = i / 8;
            int col = i % 8;
            int center_dist = abs(row - 3) + abs(col - 3); // Distance from center
            
            if (center_dist == wave) {
                led_set_pixel_safe(i, r, g, b);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay between waves
    }
    
    ESP_LOGI(TAG, "Player change animation: %s - WAVE", (current_player == PLAYER_WHITE) ? "WHITE" : "BLACK");
}

// ============================================================================
// ERROR HANDLING LED FUNCTIONS
// ============================================================================

/**
 * @brief Invalid move error display with LED feedback
 * 
 * @param from_row The row of the source array
 * @param from_col The column of the source field
 * @param to_row The row of the target array
 * @param to_col The column of the target field
 * 
 * @details
 * Shows a red flash on an invalid move and then instructions to return the piece.
 */
void game_show_invalid_move_error(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col) {
    uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
    uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
    
    // Clear previous highlights
    game_clear_highlights_direct();
    
    // Error animation - red flashing
    for (int flash = 0; flash < 3; flash++) {
        led_set_pixel_safe(from_led, 255, 0, 0); // Red source
        led_set_pixel_safe(to_led, 255, 0, 0);   // Red destination
        vTaskDelay(pdMS_TO_TICKS(200));
        
        led_set_pixel_safe(from_led, 0, 0, 0);   // Clear
        led_set_pixel_safe(to_led, 0, 0, 0);     // Clear
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Show return guidance
    led_set_pixel_safe(from_led, 255, 255, 0);   // Yellow - return piece
    led_set_pixel_safe(to_led, 255, 165, 0);     // Orange - invalid destination
    
    ESP_LOGI(TAG, "Invalid move error shown: %d,%d -> %d,%d (LEDs %d -> %d)", 
             from_row, from_col, to_row, to_col, from_led, to_led);
}

/**
 * @brief Button error display with LED feedback
 * 
 * @param button_id The ID of the button
 * 
 * @details
 * Shows orange flashing on the error button.
 */
void game_show_button_error(uint8_t button_id) {
    uint8_t button_led = CHESS_LED_COUNT_BOARD + button_id;
    
    // Button error animation - orange flashing
    for (int flash = 0; flash < 5; flash++) {
        led_set_pixel_safe(button_led, 255, 165, 0); // Orange
        vTaskDelay(pdMS_TO_TICKS(150));
        
        led_set_pixel_safe(button_led, 0, 0, 0);     // Clear
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    
    // Return to normal button state (green - available)
    led_set_pixel_safe(button_led, 0, 255, 0); // Green
    
    ESP_LOGI(TAG, "Button error shown: Button %d (LED %d)", button_id, button_led);
}

/**
 * @brief Showing casting instructions with step-by-step LED feedback
 * 
 * @param king_row The king row
 * @param king_col The king column
 * @param rook_row The rook row
 * @param rook_col The tower column
 * @param is_kingside True for kingside, false for ladyside
 * 
 * @details
 * Shows a step-by-step guide to casting with an animation of the king and rook movement.
 */
void game_show_castling_guidance(uint8_t king_row, uint8_t king_col, uint8_t rook_row, uint8_t rook_col, bool is_kingside) {
    uint8_t king_led = chess_pos_to_led_index(king_row, king_col);
    uint8_t rook_led = chess_pos_to_led_index(rook_row, rook_col);
    
    // Clear previous highlights
    game_clear_highlights_direct();
    
    // Step-by-step cast animation
    // Step 1: Show king and rook
    led_set_pixel_safe(king_led, 255, 255, 0);   // Yellow king
    led_set_pixel_safe(rook_led, 255, 255, 0);   // Yellow rook
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Step 2: Show king move path
    uint8_t king_to_col = is_kingside ? king_col + 2 : king_col - 2;
    uint8_t king_path_led = chess_pos_to_led_index(king_row, king_col + (is_kingside ? 1 : -1));
    uint8_t king_final_led = chess_pos_to_led_index(king_row, king_to_col);
    led_set_pixel_safe(king_path_led, 0, 255, 0); // Green path
    led_set_pixel_safe(king_final_led, 0, 255, 0); // Green final
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Step 3: Show rook move path
    uint8_t rook_to_col = is_kingside ? rook_col - 2 : rook_col + 2;
    uint8_t rook_path_led = chess_pos_to_led_index(rook_row, rook_col + (is_kingside ? -1 : 1));
    uint8_t rook_final_led = chess_pos_to_led_index(rook_row, rook_to_col);
    led_set_pixel_safe(rook_path_led, 0, 255, 0); // Green path
    led_set_pixel_safe(rook_final_led, 0, 255, 0); // Green final
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Step 4: Show final positions
    led_clear_board_only();
    led_set_pixel_safe(king_final_led, 255, 0, 255); // Magenta final king
    led_set_pixel_safe(rook_final_led, 255, 0, 255); // Magenta final rook
    
    ESP_LOGI(TAG, "Castling guidance shown: %s side (King %d,%d, Rook %d,%d)", 
             is_kingside ? "KINGSIDE" : "QUEENSIDE", king_row, king_col, rook_row, rook_col);
}

/**
 * @brief Show mat using direct LED calls
 * 
 * @param king_row King row in checkmate
 * @param king_col King column in checkmate
 * 
 * @details
 * Starts a winning animation in the king position in checkmate.
 */
void game_show_checkmate_direct(uint8_t king_row, uint8_t king_col) {
    uint8_t king_pos = chess_pos_to_led_index(king_row, king_col);
    
    ESP_LOGI(TAG, "🏆 CHECKMATE! Starting victory animation at %c%c", 
             'a' + king_col, '1' + king_row);
    
    // Start the Victory Wave animation
    esp_err_t result = start_endgame_animation(ENDGAME_ANIM_VICTORY_WAVE, king_pos);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start endgame animation, using fallback");
        // Fallback to the original animation
        led_clear_board_only();
        led_set_pixel_safe(king_pos, 255, 0, 255); // Magenta pro checkmate
    }
}

/**
 * @brief Graduation display using direct LED calls
 * 
 * @param row The row of the graduation field
 * @param col Array column with graduation
 * 
 * @details
 * Shows yellow color on graduation field.
 */
void game_show_promotion_direct(uint8_t row, uint8_t col) {
    uint8_t led_index = chess_pos_to_led_index(row, col);
    
    // Direct call of the LED function
    led_set_pixel_safe(led_index, 255, 255, 0);  // Yellow for promotion
    
    ESP_LOGI(TAG, "Promotion shown: %d,%d (LED %d)", row, col, led_index);
}

/**
 * @brief Casting display using direct LED calls
 * 
 * @param king_from_row The row of the king's source array
 * @param king_from_col The column of the king's source field
 * @param king_to_row The row of the king's target field
 * @param king_to_col The column of the king's target field
 * @param rook_from_row The rook's source field row
 * @param rook_from_col The column of the rook's source field
 * @param rook_to_row The rook's target field row
 * @param rook_to_col The rook's target array column
 * 
 * @details
 * Displays orange color on source fields and green on target fields.
 */
void game_show_castling_direct(uint8_t king_from_row, uint8_t king_from_col, 
                               uint8_t king_to_row, uint8_t king_to_col,
                               uint8_t rook_from_row, uint8_t rook_from_col,
                               uint8_t rook_to_row, uint8_t rook_to_col) {
    uint8_t king_from_led = chess_pos_to_led_index(king_from_row, king_from_col);
    uint8_t king_to_led = chess_pos_to_led_index(king_to_row, king_to_col);
    uint8_t rook_from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
    uint8_t rook_to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);
    
    // Direct call of the LED functions to display the cast
    led_set_pixel_safe(king_from_led, 255, 165, 0);  // Orange pro king from
    led_set_pixel_safe(king_to_led, 0, 255, 0);      // Green for king it
    led_set_pixel_safe(rook_from_led, 255, 165, 0);  // Orange pro rook from
    led_set_pixel_safe(rook_to_led, 0, 255, 0);      // Green for rook it
    
    ESP_LOGI(TAG, "Castling shown: K%d,%d->%d,%d R%d,%d->%d,%d", 
             king_from_row, king_from_col, king_to_row, king_to_col,
             rook_from_row, rook_from_col, rook_to_row, rook_to_col);
}
