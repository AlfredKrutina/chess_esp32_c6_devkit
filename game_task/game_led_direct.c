/**
 * @file game_led_direct.c
 * @brief DIRECT LED FUNCTIONS FOR GAME TASK - No Queue Hell
 * 
 * ŘEŠENÍ: Místo xQueueSend(led_command_queue) používat direct LED calls
 * - Žádný queue hell
 * - Žádné timing problémy
 * - Okamžité LED updates
 * 
 * Author: Alfred Krutina
 * Version: 2.5 - DIRECT SYSTEM
 * Date: 2025-09-02
 */

#include "game_task.h"
#include "led_task.h"  // ✅ FIX: Include led_task.h for led_clear_board_only()
#include "freertos_chess.h"
#include "chess_types.h"
#include "game_led_animations.h"
#include "led_mapping.h"  // ✅ FIX: Include LED mapping functions
#include "esp_log.h"

static const char *TAG = "GAME_LED_DIRECT";

// ============================================================================
// DIRECT LED FUNCTIONS - NO QUEUE HELL
// ============================================================================

/**
 * @brief Show move with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col) {
    uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
    uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
    
    // ✅ CRITICAL FIX: Clear previous highlights first
    game_clear_highlights_direct();
    
    // ✅ MOVE ANIMATION: Progressive lighting with smooth transitions
    // Step 1: Light source (yellow)
    led_set_pixel_safe(from_led, 255, 255, 0);   // Žlutá source
    vTaskDelay(pdMS_TO_TICKS(300)); // 300ms delay
    
    // Step 2: Fade source to orange
    led_set_pixel_safe(from_led, 255, 165, 0);   // Orange source
    vTaskDelay(pdMS_TO_TICKS(200)); // 200ms delay
    
    // Step 3: Light destination (green)
    led_set_pixel_safe(to_led, 0, 255, 0);       // Zelená destination
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
 * @brief Show piece lift with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_piece_lift_direct(uint8_t row, uint8_t col) {
    uint8_t led_index = chess_pos_to_led_index(row, col);
    
    // Jemný pulzující žlutý efekt
    static uint8_t pulse_phase = 0;
    pulse_phase = (pulse_phase + 1) % 64;
    
    // Výpočet jemné pulsace (220-255)
    uint8_t intensity = 220 + (pulse_phase > 32 ? (64 - pulse_phase) : pulse_phase);
    uint8_t orange_mix = intensity * 0.9; // Jemný orange mix
    
    led_set_pixel_safe(led_index, intensity, orange_mix, 0); // Žlutooranžová
    ESP_LOGI(TAG, "Piece lift shown with subtle animation: %d,%d (LED %d)", row, col, led_index);
}

/**
 * @brief Show valid moves with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count) {
    static uint8_t wave_offset = 0;
    wave_offset = (wave_offset + 1) % 32;
    
    for (int i = 0; i < count; i++) {
        // Jemná zelená vlna pro každý valid move
        uint8_t wave_intensity = 200 + ((wave_offset + i * 8) % 32);
        uint8_t lime_mix = wave_intensity * 0.8; // Jemný lime mix
        
        led_set_pixel_safe(valid_positions[i], lime_mix, wave_intensity, 0); // Zelenožlutá
    }
    
    ESP_LOGI(TAG, "Valid moves shown with subtle wave: %d positions", count);
}

/**
 * @brief Show error with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_error_direct(uint8_t row, uint8_t col) {
    uint8_t led_index = chess_pos_to_led_index(row, col);
    
    // ✅ PŘÍMÉ VOLÁNÍ - žádný queue
    led_set_pixel_safe(led_index, 255, 0, 0);  // Červená pro error
    
    ESP_LOGI(TAG, "Error shown: %d,%d (LED %d)", row, col, led_index);
}

/**
 * @brief Clear highlights with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_clear_highlights_direct(void) {
    // ✅ CRITICAL FIX: Use new layer management function
    led_clear_board_only();
    
    ESP_LOGI(TAG, "Board highlights cleared (preserving button LEDs)");
}

/**
 * @brief Show game state with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_state_direct(piece_t *board) {
    // ✅ PŘÍMÉ VOLÁNÍ - žádný queue
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t led_index = chess_pos_to_led_index(row, col);
            piece_t piece = board[row * 8 + col];
            
            if (piece != PIECE_EMPTY) {
                if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
                    led_set_pixel_safe(led_index, 255, 255, 255);  // Bílá pro white pieces
                } else {
                    led_set_pixel_safe(led_index, 0, 0, 255);      // Modrá pro black pieces
                }
            } else {
                led_set_pixel_safe(led_index, 0, 0, 0);            // Černá pro empty
            }
        }
    }
    
    ESP_LOGI(TAG, "Game state shown");
}

/**
 * @brief Show puzzle with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_puzzle_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col) {
    uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
    uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
    
    // ✅ PŘÍMÉ VOLÁNÍ - žádný queue
    led_set_pixel_safe(from_led, 255, 165, 0);  // Orange pro source
    led_set_pixel_safe(to_led, 0, 255, 0);      // Zelená pro destination
    
    ESP_LOGI(TAG, "Puzzle shown: %d,%d -> %d,%d (LEDs %d -> %d)", 
             from_row, from_col, to_row, to_col, from_led, to_led);
}

/**
 * @brief Show check with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_check_direct(uint8_t king_row, uint8_t king_col) {
    uint8_t led_index = chess_pos_to_led_index(king_row, king_col);
    
    // ✅ CRITICAL FIX: Clear previous highlights first
    game_clear_highlights_direct();
    
    // ✅ CHECK INDICATION: Purple king LED
    led_set_pixel_safe(led_index, 255, 0, 255);  // Magenta pro check
    
    ESP_LOGI(TAG, "Check indication: King at %d,%d (LED %d) - PURPLE", king_row, king_col, led_index);
}

/**
 * @brief Show player change with wave animation
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_player_change_direct(player_t current_player) {
    // ✅ CRITICAL FIX: Clear previous highlights first
    game_clear_highlights_direct();
    
    // ✅ PLAYER CHANGE ANIMATION: Wave from center
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
// ERROR HANDLING LED FUNCTIONS - NOVÉ PRO KOMPLETNÍ ERROR HANDLING
// ============================================================================

/**
 * @brief Show invalid move error with LED feedback
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_invalid_move_error(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col) {
    uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
    uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
    
    // ✅ CRITICAL FIX: Clear previous highlights first
    game_clear_highlights_direct();
    
    // ✅ ERROR ANIMATION: Red flash pattern
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
 * @brief Show button error with LED feedback
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_button_error(uint8_t button_id) {
    uint8_t button_led = CHESS_LED_COUNT_BOARD + button_id;
    
    // ✅ BUTTON ERROR ANIMATION: Orange flash
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
 * @brief Show castling guidance with step-by-step LED feedback
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_castling_guidance(uint8_t king_row, uint8_t king_col, uint8_t rook_row, uint8_t rook_col, bool is_kingside) {
    uint8_t king_led = chess_pos_to_led_index(king_row, king_col);
    uint8_t rook_led = chess_pos_to_led_index(rook_row, rook_col);
    
    // ✅ CRITICAL FIX: Clear previous highlights first
    game_clear_highlights_direct();
    
    // ✅ CASTLING GUIDANCE: Step-by-step animation
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
 * @brief Show checkmate with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_checkmate_direct(uint8_t king_row, uint8_t king_col) {
    uint8_t king_pos = chess_pos_to_led_index(king_row, king_col);
    
    ESP_LOGI(TAG, "🏆 CHECKMATE! Starting victory animation at %c%c", 
             'a' + king_col, '1' + king_row);
    
    // Spustit Victory Wave animaci
    esp_err_t result = start_endgame_animation(ENDGAME_ANIM_VICTORY_WAVE, king_pos);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start endgame animation, using fallback");
        // Fallback na původní animaci
        led_clear_board_only();
        led_set_pixel_safe(king_pos, 255, 0, 255); // Magenta pro checkmate
    }
}

/**
 * @brief Show promotion with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_promotion_direct(uint8_t row, uint8_t col) {
    uint8_t led_index = chess_pos_to_led_index(row, col);
    
    // ✅ PŘÍMÉ VOLÁNÍ - žádný queue
    led_set_pixel_safe(led_index, 255, 255, 0);  // Žlutá pro promotion
    
    ESP_LOGI(TAG, "Promotion shown: %d,%d (LED %d)", row, col, led_index);
}

/**
 * @brief Show castling with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_castling_direct(uint8_t king_from_row, uint8_t king_from_col, 
                               uint8_t king_to_row, uint8_t king_to_col,
                               uint8_t rook_from_row, uint8_t rook_from_col,
                               uint8_t rook_to_row, uint8_t rook_to_col) {
    uint8_t king_from_led = chess_pos_to_led_index(king_from_row, king_from_col);
    uint8_t king_to_led = chess_pos_to_led_index(king_to_row, king_to_col);
    uint8_t rook_from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
    uint8_t rook_to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);
    
    // ✅ PŘÍMÉ VOLÁNÍ - žádný queue
    led_set_pixel_safe(king_from_led, 255, 165, 0);  // Orange pro king from
    led_set_pixel_safe(king_to_led, 0, 255, 0);      // Zelená pro king to
    led_set_pixel_safe(rook_from_led, 255, 165, 0);  // Orange pro rook from
    led_set_pixel_safe(rook_to_led, 0, 255, 0);      // Zelená pro rook to
    
    ESP_LOGI(TAG, "Castling shown: K%d,%d->%d,%d R%d,%d->%d,%d", 
             king_from_row, king_from_col, king_to_row, king_to_col,
             rook_from_row, rook_from_col, rook_to_row, rook_to_col);
}
