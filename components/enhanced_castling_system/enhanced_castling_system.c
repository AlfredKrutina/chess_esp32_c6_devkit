/**
 * @file enhanced_castling_system.c
 * @brief Implementace Enhanced Castling Systemu
 * 
 * Tento modul implementuje komplexni system pro rochadu s:
 * - Centralizovanou spravou stavu
 * - Pokrocilym LED navodem a indikaci chyb
 * - Inteligentnim error recovery
 * - Timeout handling
 * - Vizualnim navodem pro hrace
 * 
 * @author ESP32 Chess Team
 * @date 2024
 * 
 * @details
 * Enhanced Castling System je pokrocily system pro spravu rochady
 * v sachovem systemu. Poskytuje vizualni navod pro hrace a pomaha
 * s provedenim rochady podle pravidel.
 */

#include "enhanced_castling_system.h"
#include "freertos_chess.h"
#include "game_led_animations.h" // For rgb_color_t
#include "led_task.h"
#include "led_mapping.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "string.h"

static const char *TAG = "ENHANCED_CASTLING";

// Global castling system instance
enhanced_castling_system_t castling_system = {0};

// LED configuration with default values
castling_led_config_t castling_led_config = {
    .colors = {
        .king_highlight = {.r = 255, .g = 215, .b = 0},      // Zlatá
        .king_destination = {.r = 0, .g = 255, .b = 0},      // Zelená
        .rook_highlight = {.r = 192, .g = 192, .b = 192},    // Stříbrná
        .rook_destination = {.r = 0, .g = 0, .b = 255},      // Modrá
        .error_indication = {.r = 255, .g = 0, .b = 0},      // Červená
        .path_guidance = {.r = 255, .g = 255, .b = 0}        // Žlutá
    },
    .timing = {
        .pulsing_speed = 500,                 // 500ms
        .guidance_speed = 300,                // 300ms
        .error_flash_count = 3,               // 3x bliknutí
        .completion_celebration_duration = 2000 // 2s
    }
};

// Error information lookup table
static const castling_error_info_t error_info[] = {
    [CASTLING_ERROR_NONE] = {
        .error_type = CASTLING_ERROR_NONE,
        .description = "No error",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = NULL
    },
    [CASTLING_ERROR_WRONG_KING_POSITION] = {
        .error_type = CASTLING_ERROR_WRONG_KING_POSITION,
        .description = "King is not in correct position for castling",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = castling_recover_king_wrong_position
    },
    [CASTLING_ERROR_WRONG_ROOK_POSITION] = {
        .error_type = CASTLING_ERROR_WRONG_ROOK_POSITION,
        .description = "Rook is not in correct position for castling",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = castling_recover_rook_wrong_position
    },
    [CASTLING_ERROR_TIMEOUT] = {
        .error_type = CASTLING_ERROR_TIMEOUT,
        .description = "Castling timeout - move too slow",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = castling_recover_timeout_error
    },
    [CASTLING_ERROR_INVALID_SEQUENCE] = {
        .error_type = CASTLING_ERROR_INVALID_SEQUENCE,
        .description = "Invalid move sequence for castling",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = castling_show_correct_positions
    },
    [CASTLING_ERROR_HARDWARE_FAILURE] = {
        .error_type = CASTLING_ERROR_HARDWARE_FAILURE,
        .description = "Hardware failure during castling",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = NULL
    },
    [CASTLING_ERROR_GAME_STATE_INVALID] = {
        .error_type = CASTLING_ERROR_GAME_STATE_INVALID,
        .description = "Game state is invalid for castling",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = NULL
    },
    [CASTLING_ERROR_MAX_ERRORS_EXCEEDED] = {
        .error_type = CASTLING_ERROR_MAX_ERRORS_EXCEEDED,
        .description = "Maximum errors exceeded - castling cancelled",
        .error_led_positions = {0},
        .correction_led_positions = {0},
        .recovery_action = castling_show_tutorial
    }
};

/**
 * @brief Initialize enhanced castling system
 */
esp_err_t enhanced_castling_init(void)
{
    ESP_LOGI(TAG, "🏰 Initializing Enhanced Castling System");
    
    // Reset system state
    castling_reset_system();
    
    // Set default configuration
    castling_system.max_errors = 3;
    castling_system.completion_callback = NULL;
    
    ESP_LOGI(TAG, "✅ Enhanced Castling System initialized");
    return ESP_OK;
}

/**
 * @brief Start castling sequence
 */
esp_err_t enhanced_castling_start(player_t player, bool is_kingside)
{
    if (castling_system.active) {
        ESP_LOGW(TAG, "⚠️ Castling already active - cancelling previous");
        enhanced_castling_cancel();
    }
    
    ESP_LOGI(TAG, "🏰 Starting castling: %s %s", 
             player == PLAYER_WHITE ? "White" : "Black",
             is_kingside ? "kingside" : "queenside");
    
    // Initialize system
    castling_system.active = true;
    castling_system.player = player;
    castling_system.is_kingside = is_kingside;
    castling_system.error_count = 0;
    
    // Calculate positions
    castling_calculate_positions(player, is_kingside);
    
    // Start with king lift phase
    enhanced_castling_update_phase(CASTLING_STATE_KING_LIFTED);
    
    // Show initial guidance
    castling_show_king_guidance();
    
    castling_log_state_change("Castling started");
    return ESP_OK;
}

/**
 * @brief Handle king lift event
 */
esp_err_t enhanced_castling_handle_king_lift(uint8_t row, uint8_t col)
{
    if (!castling_system.active) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (castling_system.phase != CASTLING_STATE_KING_LIFTED) {
        ESP_LOGW(TAG, "⚠️ King lift in wrong phase: %d", castling_system.phase);
        enhanced_castling_handle_error(CASTLING_ERROR_INVALID_SEQUENCE, row, col);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate king position
    if (row != castling_system.positions.king_from_row || 
        col != castling_system.positions.king_from_col) {
        ESP_LOGE(TAG, "❌ Wrong king position: %d,%d (expected %d,%d)", 
                 row, col, castling_system.positions.king_from_row, castling_system.positions.king_from_col);
        enhanced_castling_handle_error(CASTLING_ERROR_WRONG_KING_POSITION, row, col);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "✅ King lifted from correct position: %c%d", 
             'a' + col, row + 1);
    
    castling_log_state_change("King lifted");
    return ESP_OK;
}

/**
 * @brief Handle king drop event
 */
esp_err_t enhanced_castling_handle_king_drop(uint8_t row, uint8_t col)
{
    if (!castling_system.active) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (castling_system.phase != CASTLING_STATE_KING_LIFTED) {
        ESP_LOGW(TAG, "⚠️ King drop in wrong phase: %d", castling_system.phase);
        enhanced_castling_handle_error(CASTLING_ERROR_INVALID_SEQUENCE, row, col);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate king move
    if (!castling_validate_king_move(castling_system.positions.king_from_row, 
                                   castling_system.positions.king_from_col,
                                   row, col)) {
        ESP_LOGE(TAG, "❌ Invalid king move: %c%d -> %c%d", 
                 'a' + castling_system.positions.king_from_col, castling_system.positions.king_from_row + 1,
                 'a' + col, row + 1);
        enhanced_castling_handle_error(CASTLING_ERROR_WRONG_KING_POSITION, row, col);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "✅ King moved to correct position: %c%d", 'a' + col, row + 1);
    
    // Update phase to waiting for rook
    enhanced_castling_update_phase(CASTLING_STATE_KING_MOVED_WAITING_ROOK);
    
    // Show rook guidance
    castling_show_rook_guidance();
    
    castling_log_state_change("King moved, waiting for rook");
    return ESP_OK;
}

/**
 * @brief Handle rook lift event
 */
esp_err_t enhanced_castling_handle_rook_lift(uint8_t row, uint8_t col)
{
    if (!castling_system.active) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (castling_system.phase != CASTLING_STATE_KING_MOVED_WAITING_ROOK) {
        ESP_LOGW(TAG, "⚠️ Rook lift in wrong phase: %d", castling_system.phase);
        enhanced_castling_handle_error(CASTLING_ERROR_INVALID_SEQUENCE, row, col);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate rook position
    if (row != castling_system.positions.rook_from_row || 
        col != castling_system.positions.rook_from_col) {
        ESP_LOGE(TAG, "❌ Wrong rook position: %d,%d (expected %d,%d)", 
                 row, col, castling_system.positions.rook_from_row, castling_system.positions.rook_from_col);
        enhanced_castling_handle_error(CASTLING_ERROR_WRONG_ROOK_POSITION, row, col);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "✅ Rook lifted from correct position: %c%d", 
             'a' + col, row + 1);
    
    // Update phase to rook moved
    enhanced_castling_update_phase(CASTLING_STATE_ROOK_LIFTED);
    
    castling_log_state_change("Rook lifted");
    return ESP_OK;
}

/**
 * @brief Handle rook drop event
 */
esp_err_t enhanced_castling_handle_rook_drop(uint8_t row, uint8_t col)
{
    if (!castling_system.active) {
        return ESP_OK; // Not an error if castling not active
    }
    
    if (castling_system.phase != CASTLING_STATE_ROOK_LIFTED) {
        ESP_LOGW(TAG, "⚠️ Rook drop in wrong phase: %d", castling_system.phase);
        enhanced_castling_handle_error(CASTLING_ERROR_INVALID_SEQUENCE, row, col);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate rook move
    if (!castling_validate_rook_move(castling_system.positions.rook_from_row, 
                                   castling_system.positions.rook_from_col,
                                   row, col)) {
        ESP_LOGE(TAG, "❌ Invalid rook move: %c%d -> %c%d", 
                 'a' + castling_system.positions.rook_from_col, castling_system.positions.rook_from_row + 1,
                 'a' + col, row + 1);
        enhanced_castling_handle_error(CASTLING_ERROR_WRONG_ROOK_POSITION, row, col);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "✅ Rook moved to correct position: %c%d", 'a' + col, row + 1);
    
    // Complete castling
    enhanced_castling_update_phase(CASTLING_STATE_COMPLETING);
    
    // Show completion celebration
    castling_show_completion_celebration();
    
    // Complete castling
    enhanced_castling_update_phase(CASTLING_STATE_COMPLETED);
    
    // Call completion callback if set
    if (castling_system.completion_callback) {
        castling_system.completion_callback(true);
    }
    
    // Reset system
    castling_reset_system();
    
    castling_log_state_change("Castling completed successfully");
    return ESP_OK;
}

/**
 * @brief Cancel current castling sequence
 */
esp_err_t enhanced_castling_cancel(void)
{
    if (!castling_system.active) {
        return ESP_OK; // Already inactive
    }
    
    ESP_LOGI(TAG, "❌ Cancelling castling sequence");
    
    // Clear all LED indications
    castling_clear_all_indications();
    
    // Call completion callback with failure
    if (castling_system.completion_callback) {
        castling_system.completion_callback(false);
    }
    
    // Reset system
    castling_reset_system();
    
    castling_log_state_change("Castling cancelled");
    return ESP_OK;
}

/**
 * @brief Check if castling is currently active
 */
bool enhanced_castling_is_active(void)
{
    return castling_system.active;
}

/**
 * @brief Get current castling phase
 */
castling_phase_t enhanced_castling_get_phase(void)
{
    return castling_system.phase;
}

/**
 * @brief Update castling phase with timeout handling
 */
void enhanced_castling_update_phase(castling_phase_t new_phase)
{
    castling_system.phase = new_phase;
    castling_system.phase_start_time = esp_timer_get_time() / 1000;
    
    // Set timeout according to phase
    switch (new_phase) {
        case CASTLING_STATE_KING_LIFTED:
            castling_system.phase_timeout_ms = 30000; // 30s na přemístění krále
            break;
        case CASTLING_STATE_KING_MOVED_WAITING_ROOK:
            castling_system.phase_timeout_ms = 60000; // 60s na věž
            break;
        case CASTLING_STATE_ROOK_LIFTED:
            castling_system.phase_timeout_ms = 30000; // 30s na přemístění věže
            break;
        default:
            castling_system.phase_timeout_ms = 10000; // Výchozí timeout
            break;
    }
    
    ESP_LOGI(TAG, "Phase changed to: %d, timeout: %lums", 
             new_phase, castling_system.phase_timeout_ms);
}

/**
 * @brief Handle castling error
 */
void enhanced_castling_handle_error(castling_error_t error, uint8_t row, uint8_t col)
{
    castling_system.error_count++;
    
    ESP_LOGE(TAG, "❌ Castling error %d at position %d,%d (count: %d/%d)", 
             error, row, col, castling_system.error_count, castling_system.max_errors);
    
    // Show error indication
    castling_show_error_indication(error);
    
    // Execute recovery action if available
    if (error < sizeof(error_info) / sizeof(error_info[0]) && 
        error_info[error].recovery_action) {
        error_info[error].recovery_action();
    }
    
    // Check if max errors exceeded
    if (castling_system.error_count >= castling_system.max_errors) {
        ESP_LOGE(TAG, "❌ Maximum errors exceeded - cancelling castling");
        enhanced_castling_handle_error(CASTLING_ERROR_MAX_ERRORS_EXCEEDED, row, col);
        enhanced_castling_cancel();
    }
}

/**
 * @brief Calculate castling positions
 */
void castling_calculate_positions(player_t player, bool is_kingside)
{
    if (player == PLAYER_WHITE) {
        castling_system.positions.king_from_row = 0;
        castling_system.positions.king_from_col = 4;
        castling_system.positions.king_to_row = 0;
        castling_system.positions.king_to_col = is_kingside ? 6 : 2;
        
        castling_system.positions.rook_from_row = 0;
        castling_system.positions.rook_from_col = is_kingside ? 7 : 0;
        castling_system.positions.rook_to_row = 0;
        castling_system.positions.rook_to_col = is_kingside ? 5 : 3;
    } else {
        castling_system.positions.king_from_row = 7;
        castling_system.positions.king_from_col = 4;
        castling_system.positions.king_to_row = 7;
        castling_system.positions.king_to_col = is_kingside ? 6 : 2;
        
        castling_system.positions.rook_from_row = 7;
        castling_system.positions.rook_from_col = is_kingside ? 7 : 0;
        castling_system.positions.rook_to_row = 7;
        castling_system.positions.rook_to_col = is_kingside ? 5 : 3;
    }
    
    ESP_LOGI(TAG, "Calculated positions: King %c%d->%c%d, Rook %c%d->%c%d",
             'a' + castling_system.positions.king_from_col, castling_system.positions.king_from_row + 1,
             'a' + castling_system.positions.king_to_col, castling_system.positions.king_to_row + 1,
             'a' + castling_system.positions.rook_from_col, castling_system.positions.rook_from_row + 1,
             'a' + castling_system.positions.rook_to_col, castling_system.positions.rook_to_row + 1);
}

/**
 * @brief Validate king move for castling
 */
bool castling_validate_king_move(uint8_t from_row, uint8_t from_col, 
                                uint8_t to_row, uint8_t to_col)
{
    return (from_row == castling_system.positions.king_from_row &&
            from_col == castling_system.positions.king_from_col &&
            to_row == castling_system.positions.king_to_row &&
            to_col == castling_system.positions.king_to_col);
}

/**
 * @brief Validate rook move for castling
 */
bool castling_validate_rook_move(uint8_t from_row, uint8_t from_col,
                                uint8_t to_row, uint8_t to_col)
{
    return (from_row == castling_system.positions.rook_from_row &&
            from_col == castling_system.positions.rook_from_col &&
            to_row == castling_system.positions.rook_to_row &&
            to_col == castling_system.positions.rook_to_col);
}

/**
 * @brief Validate castling sequence
 */
bool castling_validate_sequence(void)
{
    return castling_system.active && 
           castling_system.phase != CASTLING_STATE_IDLE &&
           castling_system.phase != CASTLING_STATE_ERROR_RECOVERY;
}

/**
 * @brief Check if timeout has expired
 */
bool castling_is_timeout_expired(void)
{
    if (!castling_system.active) {
        return false;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t elapsed = current_time - castling_system.phase_start_time;
    
    return elapsed > castling_system.phase_timeout_ms;
}

/**
 * @brief Reset castling system
 */
void castling_reset_system(void)
{
    memset(&castling_system, 0, sizeof(castling_system));
    castling_system.phase = CASTLING_STATE_IDLE;
    castling_system.active = false;
}

/**
 * @brief Log state change
 */
void castling_log_state_change(const char* action)
{
    ESP_LOGI(TAG, "🏰 %s - Phase: %d, Active: %s, Errors: %d/%d", 
             action, castling_system.phase, 
             castling_system.active ? "Yes" : "No",
             castling_system.error_count, castling_system.max_errors);
}

/**
 * @brief Show king guidance for castling
 */
void castling_show_king_guidance(void)
{
    if (!castling_system.active) {
        return;
    }
    
    ESP_LOGI(TAG, "👑 Showing king guidance for castling");
    
    // Clear previous indications
    castling_clear_all_indications();
    
    // Highlight king position in gold
    uint8_t king_led = chess_pos_to_led_index(castling_system.positions.king_from_row, 
                                            castling_system.positions.king_from_col);
    led_set_pixel_safe(king_led,
                      castling_led_config.colors.king_highlight.r,
                      castling_led_config.colors.king_highlight.g,
                      castling_led_config.colors.king_highlight.b);
    
    // Highlight king destination in green
    uint8_t king_dest_led = chess_pos_to_led_index(castling_system.positions.king_to_row, 
                                                 castling_system.positions.king_to_col);
    led_set_pixel_safe(king_dest_led,
                      castling_led_config.colors.king_destination.r,
                      castling_led_config.colors.king_destination.g,
                      castling_led_config.colors.king_destination.b);
    
    castling_system.led_state.showing_guidance = true;
}
/**
 * @brief Show rook guidance for castling
 */
void castling_show_rook_guidance(void)
{
    if (!castling_system.active) {
        return;
    }
    
    ESP_LOGI(TAG, "🏰 Showing rook guidance for castling");
    
    // Clear previous indications
    castling_clear_all_indications();
    
    // Highlight rook position in silver
    uint8_t rook_led = chess_pos_to_led_index(castling_system.positions.rook_from_row, 
                                            castling_system.positions.rook_from_col);
    led_set_pixel_safe(rook_led,
                      castling_led_config.colors.rook_highlight.r,
                      castling_led_config.colors.rook_highlight.g,
                      castling_led_config.colors.rook_highlight.b);
    
    // Highlight rook destination in blue
    uint8_t rook_dest_led = chess_pos_to_led_index(castling_system.positions.rook_to_row, 
                                                 castling_system.positions.rook_to_col);
    led_set_pixel_safe(rook_dest_led,
                      castling_led_config.colors.rook_destination.r,
                      castling_led_config.colors.rook_destination.g,
                      castling_led_config.colors.rook_destination.b);
    
    castling_system.led_state.showing_guidance = true;
}
/**
 * @brief Show error indication
 */
void castling_show_error_indication(castling_error_t error)
{
    if (!castling_system.active) {
        return;
    }
    
    ESP_LOGE(TAG, "❌ Showing error indication for error: %d", error);
    
    // Clear previous indications
    castling_clear_all_indications();
    
    // Flash error color on all relevant positions
    for (int i = 0; i < castling_led_config.timing.error_flash_count; i++) {
        // Flash king position
        uint8_t king_led = chess_pos_to_led_index(castling_system.positions.king_from_row, 
                                                castling_system.positions.king_from_col);
        led_set_pixel_safe(king_led,
                          castling_led_config.colors.error_indication.r,
                          castling_led_config.colors.error_indication.g,
                          castling_led_config.colors.error_indication.b);
        
        // Flash rook position
        uint8_t rook_led = chess_pos_to_led_index(castling_system.positions.rook_from_row, 
                                                castling_system.positions.rook_from_col);
        led_set_pixel_safe(rook_led,
                          castling_led_config.colors.error_indication.r,
                          castling_led_config.colors.error_indication.g,
                          castling_led_config.colors.error_indication.b);
        
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Clear
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    castling_system.led_state.showing_error = true;
}
/**
 * @brief Show completion celebration
 */
void castling_show_completion_celebration(void)
{
    if (!castling_system.active) {
        return;
    }
    
    ESP_LOGI(TAG, "🎉 Showing castling completion celebration");
    
    // Clear previous indications
    castling_clear_all_indications();
    
    // Show celebration animation
    for (int cycle = 0; cycle < 3; cycle++) {
        // Rainbow effect on king and rook positions
        uint8_t king_led = chess_pos_to_led_index(castling_system.positions.king_to_row, 
                                                castling_system.positions.king_to_col);
        uint8_t rook_led = chess_pos_to_led_index(castling_system.positions.rook_to_row, 
                                                castling_system.positions.rook_to_col);
        
        // Cycle through colors
        uint8_t colors[][3] = {
            {255, 0, 0},    // Red
            {0, 255, 0},    // Green
            {0, 0, 255},    // Blue
            {255, 255, 0},  // Yellow
            {255, 0, 255},  // Magenta
            {0, 255, 255}   // Cyan
        };
        
        for (int i = 0; i < 6; i++) {
            led_set_pixel_safe(king_led, colors[i][0], colors[i][1], colors[i][2]);
            led_set_pixel_safe(rook_led, colors[i][0], colors[i][1], colors[i][2]);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // Final state - show completed positions
    uint8_t king_led = chess_pos_to_led_index(castling_system.positions.king_to_row, 
                                            castling_system.positions.king_to_col);
    uint8_t rook_led = chess_pos_to_led_index(castling_system.positions.rook_to_row, 
                                            castling_system.positions.rook_to_col);
    
    led_set_pixel_safe(king_led, 0, 255, 0);  // Green for success
    led_set_pixel_safe(rook_led, 0, 255, 0);  // Green for success
}
/**
 * @brief Clear all castling indications
 */
void castling_clear_all_indications(void)
{
    ESP_LOGI(TAG, "🧹 Clearing all castling indications");
    
    // Clear board LEDs
    led_clear_board_only();
    
    // Reset LED state
    castling_system.led_state.showing_error = false;
    castling_system.led_state.showing_guidance = false;
    
    // Stop any running animations
    if (castling_system.led_state.king_animation_id != 0) {
        // TODO: Stop animation if animation manager is available
        castling_system.led_state.king_animation_id = 0;
    }
    
    if (castling_system.led_state.rook_animation_id != 0) {
        // TODO: Stop animation if animation manager is available
        castling_system.led_state.rook_animation_id = 0;
    }
    
    if (castling_system.led_state.guidance_animation_id != 0) {
        // TODO: Stop animation if animation manager is available
        castling_system.led_state.guidance_animation_id = 0;
    }
}

/**
 * @brief Recover from king wrong position error
 */
void castling_recover_king_wrong_position(void)
{
    ESP_LOGI(TAG, "🔄 Recovering from king wrong position error");
    
    // Show correct king position
    castling_show_king_guidance();
    
    // Show error message
    ESP_LOGE(TAG, "❌ King must be at %c%d for castling", 
             'a' + castling_system.positions.king_from_col, 
             castling_system.positions.king_from_row + 1);
}

/**
 * @brief Recover from rook wrong position error
 */
void castling_recover_rook_wrong_position(void)
{
    ESP_LOGI(TAG, "🔄 Recovering from rook wrong position error");
    
    // Show correct rook position
    castling_show_rook_guidance();
    
    // Show error message
    ESP_LOGE(TAG, "❌ Rook must be at %c%d for castling", 
             'a' + castling_system.positions.rook_from_col, 
             castling_system.positions.rook_from_row + 1);
}

/**
 * @brief Recover from timeout error
 */
void castling_recover_timeout_error(void)
{
    ESP_LOGI(TAG, "🔄 Recovering from timeout error");
    
    // Show timeout indication
    castling_show_error_indication(CASTLING_ERROR_TIMEOUT);
    
    // Show tutorial after delay
    vTaskDelay(pdMS_TO_TICKS(2000));
    castling_show_tutorial();
}

/**
 * @brief Show correct positions for castling
 */
void castling_show_correct_positions(void)
{
    ESP_LOGI(TAG, "📚 Showing correct positions for castling");
    
    // Clear board
    led_clear_board_only();
    
    // Show both king and rook guidance
    castling_show_king_guidance();
    vTaskDelay(pdMS_TO_TICKS(1000));
    castling_show_rook_guidance();
}

/**
 * @brief Show castling tutorial
 */
void castling_show_tutorial(void)
{
    ESP_LOGI(TAG, "📖 Showing castling tutorial");
    
    // Clear board
    led_clear_board_only();
    
    // Show step-by-step tutorial
    ESP_LOGI(TAG, "📖 Castling Tutorial:");
    ESP_LOGI(TAG, "   1. Move king from %c%d to %c%d", 
             'a' + castling_system.positions.king_from_col, 
             castling_system.positions.king_from_row + 1,
             'a' + castling_system.positions.king_to_col, 
             castling_system.positions.king_to_row + 1);
    ESP_LOGI(TAG, "   2. Move rook from %c%d to %c%d", 
             'a' + castling_system.positions.rook_from_col, 
             castling_system.positions.rook_from_row + 1,
             'a' + castling_system.positions.rook_to_col, 
             castling_system.positions.rook_to_row + 1);
    
    // Visual tutorial - show positions one by one
    for (int step = 0; step < 3; step++) {
        led_clear_board_only();
        
        if (step == 0) {
            // Show king position
            uint8_t king_led = chess_pos_to_led_index(castling_system.positions.king_from_row, 
                                                    castling_system.positions.king_from_col);
            led_set_pixel_safe(king_led, 255, 215, 0); // Gold
        } else if (step == 1) {
            // Show rook position
            uint8_t rook_led = chess_pos_to_led_index(castling_system.positions.rook_from_row, 
                                                    castling_system.positions.rook_from_col);
            led_set_pixel_safe(rook_led, 192, 192, 192); // Silver
        } else {
            // Show both positions
            castling_show_correct_positions();
        }
        
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}
