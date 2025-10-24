/**
 * @file visual_error_system.c
 * @brief Visual Error System Implementation
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#include "visual_error_system.h"
#include "led_state_manager.h"
#include "unified_animation_manager.h"
#include "esp_log.h"
#include "string.h"

static const char* TAG = "ERROR_SYS";

// Error system state
static bool system_initialized = false;
static error_system_config_t current_config;
static error_guidance_t last_error;
static bool displaying_error = false;
static uint8_t active_error_count = 0;

// Forward declarations
static void error_clear_leds(uint8_t* led_positions, uint8_t count);
static void error_flash_leds(uint8_t* led_positions, uint8_t count, uint8_t r, uint8_t g, uint8_t b);
static void error_pulse_leds(uint8_t* led_positions, uint8_t count, uint8_t r, uint8_t g, uint8_t b);
static const char* error_get_message_for_type(visual_error_type_t type);
static const char* error_get_hint_for_type(visual_error_type_t type);
static esp_err_t error_get_color_for_type(visual_error_type_t type, uint8_t* r, uint8_t* g, uint8_t* b);

esp_err_t error_system_init(const error_system_config_t* config) {
    if (system_initialized) {
        ESP_LOGW(TAG, "Error system already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize configuration
    memcpy(&current_config, config, sizeof(error_system_config_t));
    
    // Initialize last error
    memset(&last_error, 0, sizeof(error_guidance_t));
    
    displaying_error = false;
    active_error_count = 0;
    system_initialized = true;
    
    ESP_LOGI(TAG, "Visual Error System initialized");
    ESP_LOGI(TAG, "  Flash count: %d", current_config.flash_count);
    ESP_LOGI(TAG, "  Flash duration: %dms", current_config.flash_duration_ms);
    ESP_LOGI(TAG, "  Guidance duration: %dms", current_config.guidance_duration_ms);
    ESP_LOGI(TAG, "  Recovery hints: %s", current_config.enable_recovery_hints ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Max concurrent errors: %d", current_config.max_concurrent_errors);
    
    return ESP_OK;
}

esp_err_t error_system_deinit(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear all error indicators
    error_clear_all_indicators();
    
    system_initialized = false;
    ESP_LOGI(TAG, "Error system deinitialized");
    
    return ESP_OK;
}

esp_err_t error_show_visual(visual_error_type_t error_type, const chess_move_t* failed_move) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (error_type >= ERROR_VISUAL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if we can display more errors
    if (active_error_count >= current_config.max_concurrent_errors) {
        ESP_LOGW(TAG, "Maximum concurrent errors reached, ignoring new error");
        return ESP_ERR_NO_MEM;
    }
    
    // Clear previous error indicators
    error_clear_all_indicators();
    
    // Get error color
    uint8_t r, g, b;
    esp_err_t color_result = error_get_color_for_type(error_type, &r, &g, &b);
    if (color_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get color for error type %d", error_type);
        return color_result;
    }
    
    // Get LED positions for error
    uint8_t led_positions[8];
    uint8_t led_count = 0;
    
    if (failed_move) {
        esp_err_t pos_result = error_get_led_positions_for_move(failed_move, led_positions, &led_count);
        if (pos_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get LED positions for move");
            return pos_result;
        }
    } else {
        // Generic error - flash all board LEDs
        for (int i = 0; i < 64; i++) {
            led_positions[i] = i;
        }
        led_count = 64;
    }
    
    // Store error information
    last_error.error_type = error_type;
    last_error.error_led_count = led_count;
    memcpy(last_error.error_led_positions, led_positions, led_count);
    last_error.display_duration_ms = current_config.flash_duration_ms * current_config.flash_count;
    
    // Set error messages
    strncpy(last_error.user_message, error_get_message_for_type(error_type), sizeof(last_error.user_message) - 1);
    last_error.user_message[sizeof(last_error.user_message) - 1] = '\0';
    
    if (current_config.enable_recovery_hints) {
        strncpy(last_error.recovery_hint, error_get_hint_for_type(error_type), sizeof(last_error.recovery_hint) - 1);
        last_error.recovery_hint[sizeof(last_error.recovery_hint) - 1] = '\0';
    } else {
        last_error.recovery_hint[0] = '\0';
    }
    
    // Display error visually
    error_flash_leds(led_positions, led_count, r, g, b);
    
    displaying_error = true;
    active_error_count++;
    
    ESP_LOGI(TAG, "Displaying error: %s", last_error.user_message);
    if (strlen(last_error.recovery_hint) > 0) {
        ESP_LOGI(TAG, "Recovery hint: %s", last_error.recovery_hint);
    }
    
    return ESP_OK;
}

esp_err_t error_show_blocking_path(uint8_t from_row, uint8_t from_col,
                                  uint8_t to_row, uint8_t to_col) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get LED positions for source and destination
    uint8_t from_led_positions[8], to_led_positions[8];
    uint8_t from_count = 0, to_count = 0;
    
    esp_err_t from_result = error_get_led_positions_for_square(from_row, from_col, from_led_positions, &from_count);
    esp_err_t to_result = error_get_led_positions_for_square(to_row, to_col, to_led_positions, &to_count);
    
    if (from_result != ESP_OK || to_result != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Flash source in red, destination in orange
    error_flash_leds(from_led_positions, from_count, 255, 0, 0); // Red for source
    error_flash_leds(to_led_positions, to_count, 255, 165, 0);   // Orange for destination
    
    // Store error information
    last_error.error_type = ERROR_VISUAL_PIECE_BLOCKING;
    last_error.error_led_count = from_count + to_count;
    memcpy(last_error.error_led_positions, from_led_positions, from_count);
    memcpy(last_error.error_led_positions + from_count, to_led_positions, to_count);
    
    strncpy(last_error.user_message, "Path is blocked by another piece", sizeof(last_error.user_message) - 1);
    strncpy(last_error.recovery_hint, "Remove the blocking piece or choose a different path", sizeof(last_error.recovery_hint) - 1);
    
    displaying_error = true;
    active_error_count++;
    
    ESP_LOGI(TAG, "Showing blocking path error: %s", last_error.user_message);
    
    return ESP_OK;
}

esp_err_t error_show_check_escape_options(uint8_t king_row, uint8_t king_col) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get LED position for king
    uint8_t king_led_positions[8];
    uint8_t king_count = 0;
    
    esp_err_t king_result = error_get_led_positions_for_square(king_row, king_col, king_led_positions, &king_count);
    if (king_result != ESP_OK) {
        return king_result;
    }
    
    // Flash king in purple
    error_flash_leds(king_led_positions, king_count, 128, 0, 128); // Purple for check
    
    // Store error information
    last_error.error_type = ERROR_VISUAL_CHECK_VIOLATION;
    last_error.error_led_count = king_count;
    memcpy(last_error.error_led_positions, king_led_positions, king_count);
    
    strncpy(last_error.user_message, "King is in check - this move is not allowed", sizeof(last_error.user_message) - 1);
    strncpy(last_error.recovery_hint, "Move the king out of check or block the attacking piece", sizeof(last_error.recovery_hint) - 1);
    
    displaying_error = true;
    active_error_count++;
    
    ESP_LOGI(TAG, "Showing check violation error: %s", last_error.user_message);
    
    return ESP_OK;
}

esp_err_t error_show_puzzle_guidance(const char* puzzle_hint) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!puzzle_hint) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Store puzzle guidance
    last_error.error_type = ERROR_VISUAL_PUZZLE_WRONG_MOVE;
    last_error.error_led_count = 0; // No specific LEDs for puzzle guidance
    strncpy(last_error.user_message, "Puzzle guidance", sizeof(last_error.user_message) - 1);
    strncpy(last_error.recovery_hint, puzzle_hint, sizeof(last_error.recovery_hint) - 1);
    
    displaying_error = true;
    active_error_count++;
    
    ESP_LOGI(TAG, "Showing puzzle guidance: %s", puzzle_hint);
    
    return ESP_OK;
}

esp_err_t error_clear_all_indicators(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear error layer
    led_clear_layer(LED_LAYER_ERROR);
    
    // Force update
    led_force_full_update();
    
    displaying_error = false;
    active_error_count = 0;
    
    ESP_LOGD(TAG, "Cleared all error indicators");
    
    return ESP_OK;
}

esp_err_t error_guide_correct_move(const chess_move_t* suggested_move) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!suggested_move) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get LED positions for suggested move
    uint8_t led_positions[8];
    uint8_t led_count = 0;
    
    esp_err_t pos_result = error_get_led_positions_for_move(suggested_move, led_positions, &led_count);
    if (pos_result != ESP_OK) {
        return pos_result;
    }
    
    // Pulse LEDs in green to show correct move
    error_pulse_leds(led_positions, led_count, 0, 255, 0); // Green for correct move
    
    // Store guidance information
    last_error.guidance_led_count = led_count;
    memcpy(last_error.guidance_led_positions, led_positions, led_count);
    
    strncpy(last_error.recovery_hint, "Try this move instead", sizeof(last_error.recovery_hint) - 1);
    
    ESP_LOGI(TAG, "Showing correct move guidance");
    
    return ESP_OK;
}

esp_err_t error_guide_piece_selection(uint8_t* movable_pieces, uint8_t count) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!movable_pieces || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Pulse movable pieces in blue
    error_pulse_leds(movable_pieces, count, 0, 0, 255); // Blue for movable pieces
    
    // Store guidance information
    last_error.guidance_led_count = count;
    memcpy(last_error.guidance_led_positions, movable_pieces, count);
    
    strncpy(last_error.recovery_hint, "Select one of these pieces to move", sizeof(last_error.recovery_hint) - 1);
    
    ESP_LOGI(TAG, "Showing piece selection guidance for %d pieces", count);
    
    return ESP_OK;
}

esp_err_t error_guide_valid_destinations(uint8_t from_row, uint8_t from_col) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // This would need to be implemented with actual move generation
    // For now, just show the source square
    uint8_t source_led_positions[8];
    uint8_t source_count = 0;
    
    esp_err_t source_result = error_get_led_positions_for_square(from_row, from_col, source_led_positions, &source_count);
    if (source_result != ESP_OK) {
        return source_result;
    }
    
    // Pulse source square in yellow
    error_pulse_leds(source_led_positions, source_count, 255, 255, 0); // Yellow for source
    
    strncpy(last_error.recovery_hint, "Valid destinations will be highlighted", sizeof(last_error.recovery_hint) - 1);
    
    ESP_LOGI(TAG, "Showing valid destinations guidance from (%d,%d)", from_row, from_col);
    
    return ESP_OK;
}

esp_err_t error_guide_puzzle_removal(uint8_t* pieces_to_remove, uint8_t count) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!pieces_to_remove || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Pulse pieces to remove in red
    error_pulse_leds(pieces_to_remove, count, 255, 0, 0); // Red for pieces to remove
    
    // Store guidance information
    last_error.guidance_led_count = count;
    memcpy(last_error.guidance_led_positions, pieces_to_remove, count);
    
    strncpy(last_error.recovery_hint, "Remove these pieces from the board", sizeof(last_error.recovery_hint) - 1);
    
    ESP_LOGI(TAG, "Showing puzzle removal guidance for %d pieces", count);
    
    return ESP_OK;
}

const char* error_get_last_message(void) {
    if (!system_initialized) {
        return "Error system not initialized";
    }
    
    return last_error.user_message;
}

const char* error_get_recovery_hint(void) {
    if (!system_initialized) {
        return "Error system not initialized";
    }
    
    return last_error.recovery_hint;
}

const char* error_get_error_type_name(visual_error_type_t type) {
    switch (type) {
        case ERROR_VISUAL_INVALID_MOVE: return "Invalid Move";
        case ERROR_VISUAL_PIECE_BLOCKING: return "Piece Blocking";
        case ERROR_VISUAL_CHECK_VIOLATION: return "Check Violation";
        case ERROR_VISUAL_NO_PIECE: return "No Piece";
        case ERROR_VISUAL_WRONG_TURN: return "Wrong Turn";
        case ERROR_VISUAL_PUZZLE_WRONG_MOVE: return "Puzzle Wrong Move";
        case ERROR_VISUAL_PUZZLE_WRONG_REMOVAL: return "Puzzle Wrong Removal";
        case ERROR_VISUAL_SYSTEM_ERROR: return "System Error";
        case ERROR_VISUAL_INVALID_SYNTAX: return "Invalid Syntax";
        default: return "Unknown Error";
    }
}

esp_err_t error_get_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int written = snprintf(buffer, buffer_size,
        "Visual Error System Status:\n"
        "  Initialized: %s\n"
        "  Displaying error: %s\n"
        "  Active errors: %d/%d\n"
        "  Flash count: %d\n"
        "  Flash duration: %lums\n"
        "  Recovery hints: %s\n",
        system_initialized ? "Yes" : "No",
        displaying_error ? "Yes" : "No",
        active_error_count,
        current_config.max_concurrent_errors,
        current_config.flash_count,
        current_config.flash_duration_ms,
        current_config.enable_recovery_hints ? "Yes" : "No");
    
    if (written >= buffer_size) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

bool error_is_displaying(void) {
    return displaying_error;
}

uint8_t error_get_active_count(void) {
    return active_error_count;
}

esp_err_t error_set_config(const error_system_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&current_config, config, sizeof(error_system_config_t));
    ESP_LOGI(TAG, "Error system configuration updated");
    
    return ESP_OK;
}

esp_err_t error_set_flash_count(uint8_t count) {
    current_config.flash_count = count;
    ESP_LOGI(TAG, "Flash count set to %d", count);
    
    return ESP_OK;
}

esp_err_t error_set_flash_duration(uint32_t duration_ms) {
    current_config.flash_duration_ms = duration_ms;
    ESP_LOGI(TAG, "Flash duration set to %dms", duration_ms);
    
    return ESP_OK;
}

esp_err_t error_convert_move_error_to_visual(move_error_t move_error, visual_error_type_t* visual_type) {
    if (!visual_type) {
        return ESP_ERR_INVALID_ARG;
    }
    
    switch (move_error) {
        case MOVE_ERROR_NONE:
            *visual_type = ERROR_VISUAL_COUNT; // No visual error
            break;
        case MOVE_ERROR_INVALID_MOVE:
            *visual_type = ERROR_VISUAL_INVALID_MOVE;
            break;
        case MOVE_ERROR_PIECE_NOT_FOUND:
            *visual_type = ERROR_VISUAL_PIECE_BLOCKING;
            break;
        case MOVE_ERROR_CHECK_VIOLATION:
            *visual_type = ERROR_VISUAL_CHECK_VIOLATION;
            break;
        case MOVE_ERROR_NO_PIECE:
            *visual_type = ERROR_VISUAL_NO_PIECE;
            break;
        case MOVE_ERROR_WRONG_COLOR:
            *visual_type = ERROR_VISUAL_WRONG_TURN;
            break;
        default:
            *visual_type = ERROR_VISUAL_SYSTEM_ERROR;
            break;
    }
    
    return ESP_OK;
}

esp_err_t error_get_led_positions_for_move(const chess_move_t* move, uint8_t* led_positions, uint8_t* count) {
    if (!move || !led_positions || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get LED positions for source and destination squares
    uint8_t from_positions[8], to_positions[8];
    uint8_t from_count = 0, to_count = 0;
    
    esp_err_t from_result = error_get_led_positions_for_square(move->from_row, move->from_col, from_positions, &from_count);
    esp_err_t to_result = error_get_led_positions_for_square(move->to_row, move->to_col, to_positions, &to_count);
    
    if (from_result != ESP_OK || to_result != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Combine positions
    *count = from_count + to_count;
    memcpy(led_positions, from_positions, from_count);
    memcpy(led_positions + from_count, to_positions, to_count);
    
    return ESP_OK;
}

esp_err_t error_get_led_positions_for_square(uint8_t row, uint8_t col, uint8_t* led_positions, uint8_t* count) {
    if (!led_positions || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (row >= 8 || col >= 8) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert chess position to LED index
    uint8_t led_index = row * 8 + col;
    
    led_positions[0] = led_index;
    *count = 1;
    
    return ESP_OK;
}

// Internal helper functions
static void error_clear_leds(uint8_t* led_positions, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        led_set_pixel_layer(LED_LAYER_ERROR, led_positions[i], 0, 0, 0);
    }
}

static void error_flash_leds(uint8_t* led_positions, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t flash = 0; flash < current_config.flash_count; flash++) {
        // Flash on
        for (uint8_t i = 0; i < count; i++) {
            led_set_pixel_layer(LED_LAYER_ERROR, led_positions[i], r, g, b);
        }
        led_force_full_update();
        
        // Flash off
        for (uint8_t i = 0; i < count; i++) {
            led_set_pixel_layer(LED_LAYER_ERROR, led_positions[i], 0, 0, 0);
        }
        led_force_full_update();
    }
}

static void error_pulse_leds(uint8_t* led_positions, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    // Simple pulsing - in full implementation, use animation manager
    for (uint8_t i = 0; i < count; i++) {
        led_set_pixel_layer(LED_LAYER_ERROR, led_positions[i], r, g, b);
    }
    led_force_full_update();
}

static const char* error_get_message_for_type(visual_error_type_t type) {
    switch (type) {
        case ERROR_VISUAL_INVALID_MOVE: return "Invalid move - this piece cannot move there";
        case ERROR_VISUAL_PIECE_BLOCKING: return "Path is blocked by another piece";
        case ERROR_VISUAL_CHECK_VIOLATION: return "King is in check - this move is not allowed";
        case ERROR_VISUAL_NO_PIECE: return "No piece on this square";
        case ERROR_VISUAL_WRONG_TURN: return "It's not your turn to move";
        case ERROR_VISUAL_PUZZLE_WRONG_MOVE: return "Wrong move for this puzzle";
        case ERROR_VISUAL_PUZZLE_WRONG_REMOVAL: return "This piece should not be removed";
        case ERROR_VISUAL_SYSTEM_ERROR: return "System error occurred";
        case ERROR_VISUAL_INVALID_SYNTAX: return "Invalid command syntax";
        default: return "Unknown error occurred";
    }
}

static const char* error_get_hint_for_type(visual_error_type_t type) {
    switch (type) {
        case ERROR_VISUAL_INVALID_MOVE: return "Try moving to a different square";
        case ERROR_VISUAL_PIECE_BLOCKING: return "Remove the blocking piece or choose a different path";
        case ERROR_VISUAL_CHECK_VIOLATION: return "Move the king out of check or block the attacking piece";
        case ERROR_VISUAL_NO_PIECE: return "Select a square that contains a piece";
        case ERROR_VISUAL_WRONG_TURN: return "Wait for your opponent to move";
        case ERROR_VISUAL_PUZZLE_WRONG_MOVE: return "Follow the puzzle instructions carefully";
        case ERROR_VISUAL_PUZZLE_WRONG_REMOVAL: return "Only remove pieces that are highlighted in red";
        case ERROR_VISUAL_SYSTEM_ERROR: return "Try again or restart the system";
        case ERROR_VISUAL_INVALID_SYNTAX: return "Check the command format and try again";
        default: return "Try a different approach";
    }
}

static esp_err_t error_get_color_for_type(visual_error_type_t type, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) {
        return ESP_ERR_INVALID_ARG;
    }
    
    switch (type) {
        case ERROR_VISUAL_INVALID_MOVE:
            *r = 255; *g = 0; *b = 0; // Red
            break;
        case ERROR_VISUAL_PIECE_BLOCKING:
            *r = 255; *g = 165; *b = 0; // Orange
            break;
        case ERROR_VISUAL_CHECK_VIOLATION:
            *r = 128; *g = 0; *b = 128; // Purple
            break;
        case ERROR_VISUAL_NO_PIECE:
            *r = 255; *g = 255; *b = 0; // Yellow
            break;
        case ERROR_VISUAL_WRONG_TURN:
            *r = 0; *g = 0; *b = 255; // Blue
            break;
        case ERROR_VISUAL_PUZZLE_WRONG_MOVE:
            *r = 255; *g = 192; *b = 203; // Pink
            break;
        case ERROR_VISUAL_PUZZLE_WRONG_REMOVAL:
            *r = 255; *g = 0; *b = 0; // Red
            break;
        case ERROR_VISUAL_SYSTEM_ERROR:
            *r = 255; *g = 255; *b = 255; // White
            break;
        case ERROR_VISUAL_INVALID_SYNTAX:
            *r = 0; *g = 255; *b = 255; // Cyan
            break;
        default:
            *r = 128; *g = 128; *b = 128; // Gray
            break;
    }
    
    return ESP_OK;
}
