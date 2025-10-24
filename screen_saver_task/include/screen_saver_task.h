/**
 * @file screen_saver_task.h
 * @brief ESP32-C6 Chess System v2.4 - Screen Saver Task Header
 * 
 * This header defines the interface for the screen saver task:
 * - Screen saver types and structures
 * - Screen saver task function prototypes
 * - Screen saver control and status functions
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef SCREEN_SAVER_TASK_H
#define SCREEN_SAVER_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>


// ============================================================================
// CONSTANTS AND DEFINITIONS
// ============================================================================


// LED count constants


// Activity source types
typedef enum {
    ACTIVITY_MATRIX = 0,        // Matrix activity
    ACTIVITY_BUTTON,            // Button activity
    ACTIVITY_LED,               // LED activity
    ACTIVITY_UART               // UART activity
} activity_source_t;

// Screen saver state types
typedef enum {
    SCREEN_SAVER_STATE_ACTIVE = 0,    // Screen saver is active
    SCREEN_SAVER_STATE_INACTIVE,      // Screen saver is inactive
    SCREEN_SAVER_STATE_TRANSITIONING  // Transitioning between states
} screen_saver_state_t;

// Screen saver pattern types
typedef enum {
    PATTERN_FIREWORKS = 0,      // Fireworks effect
    PATTERN_STARS,              // Twinkling stars
    PATTERN_OCEAN,              // Ocean waves
    PATTERN_FOREST,             // Forest animation
    PATTERN_CITY,               // City lights
    PATTERN_SPACE,              // Space theme
    PATTERN_GEOMETRIC,          // Geometric patterns
    PATTERN_MINIMAL             // Minimal energy pattern
} screen_saver_pattern_t;

// Screen saver structure
typedef struct {
    screen_saver_state_t state;
    screen_saver_pattern_t current_pattern;
    uint32_t last_activity_time;
    uint32_t timeout_ms;
    uint32_t pattern_start_time;
    uint32_t frame_count;
    bool enabled;
    uint8_t brightness;
    uint8_t pattern_speed;
} screen_saver_t;

// ============================================================================
// TASK FUNCTION PROTOTYPES
// ============================================================================


/**
 * @brief Start the screen saver task
 * @param pvParameters Task parameters (unused)
 */
void screen_saver_task_start(void *pvParameters);


// ============================================================================
// SCREEN SAVER INITIALIZATION FUNCTIONS
// ============================================================================


/**
 * @brief Initialize the screen saver system
 */
void screen_saver_initialize(void);

/**
 * @brief Set screen saver timeout
 * @param timeout_ms Timeout in milliseconds
 */
void screen_saver_set_timeout(uint32_t timeout_ms);

/**
 * @brief Set screen saver brightness
 * @param brightness Brightness percentage (0-100)
 */
void screen_saver_set_brightness(uint8_t brightness);

/**
 * @brief Set screen saver pattern
 * @param pattern Pattern type to use
 */
void screen_saver_set_pattern(screen_saver_pattern_t pattern);


// ============================================================================
// ACTIVITY DETECTION FUNCTIONS
// ============================================================================


/**
 * @brief Update activity timestamp
 * @param source Source of activity
 */
void screen_saver_update_activity(activity_source_t source);

/**
 * @brief Check if timeout has occurred
 * @return true if timeout occurred
 */
bool screen_saver_check_timeout(void);


// ============================================================================
// SCREEN SAVER STATE MANAGEMENT
// ============================================================================


/**
 * @brief Activate screen saver
 */
void screen_saver_activate(void);

/**
 * @brief Deactivate screen saver
 */
void screen_saver_deactivate(void);

/**
 * @brief Fade out display for screen saver
 */
void screen_saver_fade_out(void);

/**
 * @brief Fade in display from screen saver
 */
void screen_saver_fade_in(void);

/**
 * @brief Set global brightness
 * @param brightness Brightness percentage (0-100)
 */
void screen_saver_set_global_brightness(uint8_t brightness);


// ============================================================================
// SCREEN SAVER PATTERN FUNCTIONS
// ============================================================================


/**
 * @brief Generate screen saver pattern
 */
void screen_saver_generate_pattern(void);

/**
 * @brief Generate fireworks pattern
 * @param time Current time
 */
void screen_saver_generate_fireworks(uint32_t time);

/**
 * @brief Generate stars pattern
 * @param time Current time
 */
void screen_saver_generate_stars(uint32_t time);

/**
 * @brief Generate ocean pattern
 * @param time Current time
 */
void screen_saver_generate_ocean(uint32_t time);

/**
 * @brief Generate forest pattern
 * @param time Current time
 */
void screen_saver_generate_forest(uint32_t time);

/**
 * @brief Generate city pattern
 * @param time Current time
 */
void screen_saver_generate_city(uint32_t time);

/**
 * @brief Generate space pattern
 * @param time Current time
 */
void screen_saver_generate_space(uint32_t time);

/**
 * @brief Generate geometric pattern
 * @param time Current time
 */
void screen_saver_generate_geometric(uint32_t time);

/**
 * @brief Generate minimal pattern
 * @param time Current time
 */
void screen_saver_generate_minimal(uint32_t time);

/**
 * @brief Apply brightness reduction to pattern
 */
void screen_saver_apply_brightness(void);

/**
 * @brief Send pattern to LEDs
 */
void screen_saver_send_pattern_to_leds(void);


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


/**
 * @brief Process screen saver commands from queue
 */
void screen_saver_process_commands(void);

/**
 * @brief Print screen saver status
 */
void screen_saver_print_status(void);

/**
 * @brief Test all screen saver patterns
 */
void screen_saver_test_patterns(void);


#endif // SCREEN_SAVER_TASK_H
