/**
 * @file enhanced_puzzle_system.c
 * @brief Enhanced Puzzle System Implementation
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#include "enhanced_puzzle_system.h"
#include "led_state_manager.h"
#include "unified_animation_manager.h"
#include "visual_error_system.h"
#include "../led_task/include/led_task.h"  // For led_set_pixel_safe
#include "esp_log.h"
#include "string.h"
#include "esp_timer.h"

static const char* TAG = "PUZZLE_SYS";

// Puzzle system state
static bool system_initialized = false;
static puzzle_system_config_t current_config;
static enhanced_puzzle_t current_puzzle;
static uint8_t current_puzzle_id = 0;

// Forward declarations
static esp_err_t puzzle_load_puzzle_data(uint8_t puzzle_id);
static esp_err_t puzzle_validate_move(const char* move_notation);
static esp_err_t puzzle_execute_move(const char* move_notation);
static esp_err_t puzzle_check_completion(void);
static void puzzle_clear_removal_guidance(void);
static void puzzle_show_removal_leds(void);
static void puzzle_clear_removal_leds(void);

// Predefined puzzle database
static const enhanced_puzzle_t puzzle_database[] = {
    {
        .name = "Knight Fork",
        .description = "Find the knight fork to win material",
        .difficulty = PUZZLE_DIFFICULTY_BEGINNER,
        .target_board = {
            // Initialize with empty board - will be set in puzzle_load_puzzle_data
        },
        .removal_guidance = {
            .pieces_to_remove = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
            .piece_count = 32,
            .removed_count = 0,
            .pieces_removed = {false},
            .removal_start_time = 0,
            .removal_timeout_ms = 30000
        },
        .steps = {
            {
                .from_row = 1, .from_col = 1,
                .to_row = 3, .to_col = 2,
                .description = "Move knight to fork position",
                .is_forced = false
            }
        },
        .step_count = 1,
        .current_step = 0,
        .state = PUZZLE_STATE_INACTIVE,
        .start_time = 0,
        .hint_count = 0,
        .wrong_moves = 0,
        .max_wrong_moves = 3
    },
    {
        .name = "Pin Tactics",
        .description = "Use pin to win material",
        .difficulty = PUZZLE_DIFFICULTY_INTERMEDIATE,
        .target_board = {
            // Initialize with empty board
        },
        .removal_guidance = {
            .pieces_to_remove = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
            .piece_count = 32,
            .removed_count = 0,
            .pieces_removed = {false},
            .removal_start_time = 0,
            .removal_timeout_ms = 30000
        },
        .steps = {
            {
                .from_row = 2, .from_col = 2,
                .to_row = 5, .to_col = 5,
                .description = "Pin the queen with bishop",
                .is_forced = false
            }
        },
        .step_count = 1,
        .current_step = 0,
        .state = PUZZLE_STATE_INACTIVE,
        .start_time = 0,
        .hint_count = 0,
        .wrong_moves = 0,
        .max_wrong_moves = 3
    },
    {
        .name = "Back Rank Mate",
        .description = "Deliver checkmate on the back rank",
        .difficulty = PUZZLE_DIFFICULTY_ADVANCED,
        .target_board = {
            // Initialize with empty board
        },
        .removal_guidance = {
            .pieces_to_remove = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
            .piece_count = 32,
            .removed_count = 0,
            .pieces_removed = {false},
            .removal_start_time = 0,
            .removal_timeout_ms = 30000
        },
        .steps = {
            {
                .from_row = 6, .from_col = 0,
                .to_row = 7, .to_col = 0,
                .description = "Move rook to back rank",
                .is_forced = true
            }
        },
        .step_count = 1,
        .current_step = 0,
        .state = PUZZLE_STATE_INACTIVE,
        .start_time = 0,
        .hint_count = 0,
        .wrong_moves = 0,
        .max_wrong_moves = 3
    }
};

#define PUZZLE_DATABASE_SIZE (sizeof(puzzle_database) / sizeof(puzzle_database[0]))

esp_err_t puzzle_system_init(const puzzle_system_config_t* config) {
    if (system_initialized) {
        ESP_LOGW(TAG, "Puzzle system already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize configuration
    memcpy(&current_config, config, sizeof(puzzle_system_config_t));
    
    // Initialize current puzzle
    memset(&current_puzzle, 0, sizeof(enhanced_puzzle_t));
    current_puzzle.state = PUZZLE_STATE_INACTIVE;
    current_puzzle_id = 0;
    
    system_initialized = true;
    
    ESP_LOGI(TAG, "Enhanced Puzzle System initialized");
    ESP_LOGI(TAG, "  Max puzzles: %d", current_config.max_puzzles);
    ESP_LOGI(TAG, "  Removal timeout: %dms", current_config.removal_timeout_ms);
    ESP_LOGI(TAG, "  Max wrong moves: %d", current_config.max_wrong_moves);
    ESP_LOGI(TAG, "  Visual guidance: %s", current_config.enable_visual_guidance ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Available puzzles: %d", PUZZLE_DATABASE_SIZE);
    
    return ESP_OK;
}

esp_err_t puzzle_system_deinit(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear current puzzle
    puzzle_reset();
    
    system_initialized = false;
    ESP_LOGI(TAG, "Puzzle system deinitialized");
    
    return ESP_OK;
}

esp_err_t puzzle_load(uint8_t puzzle_id) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (puzzle_id >= PUZZLE_DATABASE_SIZE) {
        ESP_LOGE(TAG, "Invalid puzzle ID: %d", puzzle_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Load puzzle data
    esp_err_t result = puzzle_load_puzzle_data(puzzle_id);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load puzzle data for ID %d", puzzle_id);
        return result;
    }
    
    current_puzzle_id = puzzle_id;
    current_puzzle.state = PUZZLE_STATE_LOADING;
    
    ESP_LOGI(TAG, "Loaded puzzle %d: %s", puzzle_id, current_puzzle.name);
    ESP_LOGI(TAG, "Description: %s", current_puzzle.description);
    ESP_LOGI(TAG, "Difficulty: %s", puzzle_get_difficulty_name(current_puzzle.difficulty));
    
    return ESP_OK;
}

esp_err_t puzzle_start_piece_removal(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_LOADING) {
        ESP_LOGW(TAG, "Puzzle not in loading state");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Initialize removal guidance
    current_puzzle.removal_guidance.removed_count = 0;
    current_puzzle.removal_guidance.removal_start_time = esp_timer_get_time() / 1000;
    memset(current_puzzle.removal_guidance.pieces_removed, false, sizeof(current_puzzle.removal_guidance.pieces_removed));
    
    current_puzzle.state = PUZZLE_STATE_PIECE_REMOVAL;
    
    // Show visual guidance
    if (current_config.enable_visual_guidance) {
        puzzle_show_removal_guidance();
    }
    
    ESP_LOGI(TAG, "Started piece removal phase for puzzle %d", current_puzzle_id);
    ESP_LOGI(TAG, "Pieces to remove: %d", current_puzzle.removal_guidance.piece_count);
    
    return ESP_OK;
}

esp_err_t puzzle_check_piece_removed(uint8_t row, uint8_t col) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_PIECE_REMOVAL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (row >= 8 || col >= 8) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t led_index = row * 8 + col;
    
    // Check if this piece should be removed
    bool should_remove = false;
    for (int i = 0; i < current_puzzle.removal_guidance.piece_count; i++) {
        if (current_puzzle.removal_guidance.pieces_to_remove[i] == led_index) {
            should_remove = true;
            break;
        }
    }
    
    if (!should_remove) {
        ESP_LOGW(TAG, "Piece at (%d,%d) should not be removed", row, col);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Mark piece as removed
    current_puzzle.removal_guidance.pieces_removed[led_index] = true;
    current_puzzle.removal_guidance.removed_count++;
    
    // Clear LED for this piece
    if (current_config.enable_visual_guidance) {
        led_set_pixel_layer(LED_LAYER_GUIDANCE, led_index, 0, 0, 0);
        led_force_full_update();
    }
    
    ESP_LOGI(TAG, "Piece removed at (%d,%d) - %d/%d pieces removed", 
             row, col, current_puzzle.removal_guidance.removed_count, 
             current_puzzle.removal_guidance.piece_count);
    
    // Check if all pieces removed
    if (current_puzzle.removal_guidance.removed_count >= current_puzzle.removal_guidance.piece_count) {
        puzzle_begin_solving();
    }
    
    return ESP_OK;
}

esp_err_t puzzle_begin_solving(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_PIECE_REMOVAL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear removal guidance
    puzzle_clear_removal_guidance();
    
    // Initialize solving state
    current_puzzle.state = PUZZLE_STATE_READY;
    current_puzzle.current_step = 0;
    current_puzzle.start_time = esp_timer_get_time() / 1000;
    current_puzzle.hint_count = 0;
    current_puzzle.wrong_moves = 0;
    
    ESP_LOGI(TAG, "Puzzle solving phase started");
    ESP_LOGI(TAG, "Total steps: %d", current_puzzle.step_count);
    
    return ESP_OK;
}

esp_err_t puzzle_submit_move(const char* move_notation) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_READY && current_puzzle.state != PUZZLE_STATE_IN_PROGRESS) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!move_notation) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate move
    esp_err_t validation_result = puzzle_validate_move(move_notation);
    if (validation_result != ESP_OK) {
        current_puzzle.wrong_moves++;
        
        if (current_puzzle.wrong_moves >= current_puzzle.max_wrong_moves) {
            current_puzzle.state = PUZZLE_STATE_FAILED;
            puzzle_show_failure();
            ESP_LOGI(TAG, "Puzzle failed - too many wrong moves");
            return ESP_ERR_INVALID_ARG;
        }
        
        ESP_LOGW(TAG, "Wrong move: %s (wrong moves: %d/%d)", 
                 move_notation, current_puzzle.wrong_moves, current_puzzle.max_wrong_moves);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Execute move
    esp_err_t execution_result = puzzle_execute_move(move_notation);
    if (execution_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute move: %s", move_notation);
        return execution_result;
    }
    
    // Move to next step
    current_puzzle.current_step++;
    current_puzzle.state = PUZZLE_STATE_IN_PROGRESS;
    
    ESP_LOGI(TAG, "Move executed: %s (step %d/%d)", 
             move_notation, current_puzzle.current_step, current_puzzle.step_count);
    
    // Check if puzzle is complete
    esp_err_t completion_result = puzzle_check_completion();
    if (completion_result == ESP_OK) {
        current_puzzle.state = PUZZLE_STATE_COMPLETED;
        puzzle_celebrate_completion();
        ESP_LOGI(TAG, "Puzzle completed successfully!");
    }
    
    return ESP_OK;
}

esp_err_t puzzle_request_hint(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_READY && current_puzzle.state != PUZZLE_STATE_IN_PROGRESS) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.current_step >= current_puzzle.step_count) {
        ESP_LOGW(TAG, "No more hints available");
        return ESP_ERR_INVALID_STATE;
    }
    
    current_puzzle.state = PUZZLE_STATE_HINT_SHOWN;
    current_puzzle.hint_count++;
    
    // Show visual hint
    if (current_config.enable_visual_guidance) {
        puzzle_show_next_step_hint();
    }
    
    ESP_LOGI(TAG, "Hint shown for step %d: %s", 
             current_puzzle.current_step + 1, 
             current_puzzle.steps[current_puzzle.current_step].description);
    
    return ESP_OK;
}

esp_err_t puzzle_reset(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear visual guidance
    puzzle_clear_removal_guidance();
    
    // Reset puzzle state
    current_puzzle.state = PUZZLE_STATE_INACTIVE;
    current_puzzle.current_step = 0;
    current_puzzle.start_time = 0;
    current_puzzle.hint_count = 0;
    current_puzzle.wrong_moves = 0;
    
    current_puzzle_id = 0;
    
    ESP_LOGI(TAG, "Puzzle system reset");
    
    return ESP_OK;
}

esp_err_t puzzle_next(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle_id + 1 >= PUZZLE_DATABASE_SIZE) {
        ESP_LOGW(TAG, "No more puzzles available");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Load next puzzle
    return puzzle_load(current_puzzle_id + 1);
}

// Visual guidance functions
esp_err_t puzzle_show_removal_guidance(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_PIECE_REMOVAL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    puzzle_show_removal_leds();
    
    ESP_LOGI(TAG, "Showing removal guidance for %d pieces", 
             current_puzzle.removal_guidance.piece_count);
    
    return ESP_OK;
}

esp_err_t puzzle_show_next_step_hint(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_puzzle.current_step >= current_puzzle.step_count) {
        return ESP_ERR_INVALID_STATE;
    }
    
    puzzle_step_t* step = &current_puzzle.steps[current_puzzle.current_step];
    
    // Highlight source and destination squares
    uint8_t from_led = step->from_row * 8 + step->from_col;
    uint8_t to_led = step->to_row * 8 + step->to_col;
    
    // Source square in blue
    led_set_pixel_layer(LED_LAYER_GUIDANCE, from_led, 0, 0, 255);
    // Destination square in green
    led_set_pixel_layer(LED_LAYER_GUIDANCE, to_led, 0, 255, 0);
    
    led_force_full_update();
    
    ESP_LOGI(TAG, "Showing hint: %s", step->description);
    
    return ESP_OK;
}

esp_err_t puzzle_show_solution_path(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Show all steps in sequence
    for (int i = 0; i < current_puzzle.step_count; i++) {
        puzzle_step_t* step = &current_puzzle.steps[i];
        
        uint8_t from_led = step->from_row * 8 + step->from_col;
        uint8_t to_led = step->to_row * 8 + step->to_col;
        
        // Highlight step
        led_set_pixel_layer(LED_LAYER_GUIDANCE, from_led, 255, 255, 0); // Yellow
        led_set_pixel_layer(LED_LAYER_GUIDANCE, to_led, 255, 255, 0);   // Yellow
        
        led_force_full_update();
        
        // Brief delay between steps
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Showed complete solution path");
    
    return ESP_OK;
}

esp_err_t puzzle_celebrate_completion(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Rainbow celebration effect
    for (int i = 0; i < 64; i++) {
        led_rainbow_pixel(i, 2000); // 2 second rainbow effect
    }
    
    led_force_full_update();
    
    ESP_LOGI(TAG, "Puzzle completion celebration");
    
    return ESP_OK;
}

esp_err_t puzzle_show_failure(void) {
    if (!system_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Red flash for failure
    for (int i = 0; i < 64; i++) {
        led_set_pixel_layer(LED_LAYER_ERROR, i, 255, 0, 0); // Red
    }
    
    led_force_full_update();
    
    ESP_LOGI(TAG, "Puzzle failure indication");
    
    return ESP_OK;
}

// State query functions
puzzle_state_t puzzle_get_state(void) {
    if (!system_initialized) {
        return PUZZLE_STATE_INACTIVE;
    }
    
    return current_puzzle.state;
}

uint8_t puzzle_get_removal_progress(void) {
    if (!system_initialized) {
        return 0;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_PIECE_REMOVAL) {
        return 0;
    }
    
    return current_puzzle.removal_guidance.removed_count;
}

bool puzzle_is_piece_removal_complete(void) {
    if (!system_initialized) {
        return false;
    }
    
    if (current_puzzle.state != PUZZLE_STATE_PIECE_REMOVAL) {
        return false;
    }
    
    return current_puzzle.removal_guidance.removed_count >= current_puzzle.removal_guidance.piece_count;
}

const char* puzzle_get_current_hint(void) {
    if (!system_initialized) {
        return "Puzzle system not initialized";
    }
    
    if (current_puzzle.current_step >= current_puzzle.step_count) {
        return "No more hints available";
    }
    
    return current_puzzle.steps[current_puzzle.current_step].description;
}

const char* puzzle_get_name(void) {
    if (!system_initialized) {
        return "Puzzle system not initialized";
    }
    
    return current_puzzle.name;
}

const char* puzzle_get_description(void) {
    if (!system_initialized) {
        return "Puzzle system not initialized";
    }
    
    return current_puzzle.description;
}

puzzle_difficulty_t puzzle_get_difficulty(void) {
    if (!system_initialized) {
        return PUZZLE_DIFFICULTY_BEGINNER;
    }
    
    return current_puzzle.difficulty;
}

// Progress tracking
uint8_t puzzle_get_current_step(void) {
    if (!system_initialized) {
        return 0;
    }
    
    return current_puzzle.current_step;
}

uint8_t puzzle_get_total_steps(void) {
    if (!system_initialized) {
        return 0;
    }
    
    return current_puzzle.step_count;
}

uint32_t puzzle_get_solve_time_ms(void) {
    if (!system_initialized) {
        return 0;
    }
    
    if (current_puzzle.start_time == 0) {
        return 0;
    }
    
    return (esp_timer_get_time() / 1000) - current_puzzle.start_time;
}

uint32_t puzzle_get_wrong_moves_count(void) {
    if (!system_initialized) {
        return 0;
    }
    
    return current_puzzle.wrong_moves;
}

float puzzle_get_progress_percentage(void) {
    if (!system_initialized) {
        return 0.0f;
    }
    
    if (current_puzzle.step_count == 0) {
        return 0.0f;
    }
    
    return (float)current_puzzle.current_step / (float)current_puzzle.step_count * 100.0f;
}

// Internal helper functions
static esp_err_t puzzle_load_puzzle_data(uint8_t puzzle_id) {
    if (puzzle_id >= PUZZLE_DATABASE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy puzzle data from database
    memcpy(&current_puzzle, &puzzle_database[puzzle_id], sizeof(enhanced_puzzle_t));
    
    // Initialize dynamic fields
    current_puzzle.state = PUZZLE_STATE_LOADING;
    current_puzzle.current_step = 0;
    current_puzzle.start_time = 0;
    current_puzzle.hint_count = 0;
    current_puzzle.wrong_moves = 0;
    
    return ESP_OK;
}

static esp_err_t puzzle_validate_move(const char* move_notation) {
    if (!move_notation) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (current_puzzle.current_step >= current_puzzle.step_count) {
        return ESP_ERR_INVALID_STATE;
    }
    
    puzzle_step_t* expected_step = &current_puzzle.steps[current_puzzle.current_step];
    
    // Simple validation - in full implementation, use proper move parsing
    // For now, just check if the move is valid (not empty)
    if (strlen(move_notation) > 0) {
        return ESP_OK;
    }
    
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t puzzle_execute_move(const char* move_notation) {
    // In full implementation, this would execute the actual move on the board
    // For now, just log the move
    ESP_LOGI(TAG, "Executing move: %s", move_notation);
    
    return ESP_OK;
}

static esp_err_t puzzle_check_completion(void) {
    if (current_puzzle.current_step >= current_puzzle.step_count) {
        return ESP_OK; // Puzzle completed
    }
    
    return ESP_ERR_INVALID_STATE; // Not yet completed
}

static void puzzle_clear_removal_guidance(void) {
    // Clear all guidance LEDs
    led_clear_layer(LED_LAYER_GUIDANCE);
    led_force_full_update();
}

static void puzzle_show_removal_leds(void) {
    ESP_LOGI(TAG, "🔴 Showing %d pieces to remove in RED", current_puzzle.removal_guidance.piece_count);
    
    // Clear board first
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
    
    // Show red LEDs for pieces that need to be removed
    for (int i = 0; i < current_puzzle.removal_guidance.piece_count; i++) {
        uint8_t led_index = current_puzzle.removal_guidance.pieces_to_remove[i];
        if (led_index < 64) {
            led_set_pixel_safe(led_index, 255, 0, 0); // Bright red
            ESP_LOGD(TAG, "🔴 LED %d set to RED for piece removal", led_index);
        }
    }
    
    ESP_LOGI(TAG, "🔴 Removal guidance LEDs activated - remove pieces and use matrix scanning");
}

const char* puzzle_get_difficulty_name(puzzle_difficulty_t difficulty) {
    switch (difficulty) {
        case PUZZLE_DIFFICULTY_BEGINNER: return "Beginner";
        case PUZZLE_DIFFICULTY_INTERMEDIATE: return "Intermediate";
        case PUZZLE_DIFFICULTY_ADVANCED: return "Advanced";
        case PUZZLE_DIFFICULTY_MASTER: return "Master";
        default: return "Unknown";
    }
}

static void puzzle_clear_removal_leds(void) {
    // Clear red LEDs for removed pieces
    for (int i = 0; i < current_puzzle.removal_guidance.piece_count; i++) {
        uint8_t led_index = current_puzzle.removal_guidance.pieces_to_remove[i];
        led_set_pixel_layer(LED_LAYER_GUIDANCE, led_index, 0, 0, 0);
    }
    
    led_force_full_update();
}
