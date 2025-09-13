/**
 * @file animation_task.h
 * @brief ESP32-C6 Chess System v2.4 - Animation Task Header
 * 
 * This header defines the interface for the animation task:
 * - Animation types and structures
 * - Animation task function prototypes
 * - Animation control and status functions
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef ANIMATION_TASK_H
#define ANIMATION_TASK_H


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




// Animation types
typedef enum {
    ANIM_TYPE_WAVE = 0,         // Wave pattern
    ANIM_TYPE_PULSE,            // Pulse effect
    ANIM_TYPE_FADE,             // Fade transition
    ANIM_TYPE_CHESS_PATTERN,    // Chess board pattern
    ANIM_TYPE_RAINBOW,          // Rainbow colors
    ANIM_TYPE_MOVE_HIGHLIGHT,   // Move path highlight
    ANIM_TYPE_CHECK_HIGHLIGHT,  // Check indicator
    ANIM_TYPE_GAME_OVER,        // Game over pattern
    ANIM_TYPE_CUSTOM            // Custom animation
} animation_type_t;

// Animation state types
typedef enum {
    ANIM_STATE_IDLE = 0,       // Animation is idle
    ANIM_STATE_RUNNING,        // Animation is running
    ANIM_STATE_PAUSED,         // Animation is paused
    ANIM_STATE_FINISHED        // Animation has finished
} animation_state_t;

// Animation structure
typedef struct {
    uint8_t id;                 // Animation ID
    animation_state_t state;    // Current state
    bool active;                // Whether animation is active
    bool paused;                // Whether animation is paused
    bool loop;                  // Whether to loop animation
    uint8_t priority;           // Animation priority (0-255)
    animation_type_t type;      // Animation type
    uint32_t start_time;        // Start timestamp
    uint32_t duration_ms;       // Duration in milliseconds
    uint32_t frame_duration_ms; // Frame duration in ms
    uint32_t current_frame;     // Current frame number
    uint32_t total_frames;      // Total frame count
    uint32_t frame_interval;    // Frame interval in ms
    void* data;                 // Animation-specific data
} animation_t;


// ============================================================================
// TASK FUNCTION PROTOTYPES
// ============================================================================


/**
 * @brief Start the animation task
 * @param pvParameters Task parameters (unused)
 */
void animation_task_start(void *pvParameters);


// ============================================================================
// ANIMATION INITIALIZATION FUNCTIONS
// ============================================================================


/**
 * @brief Initialize the animation system
 */
void animation_initialize_system(void);

/**
 * @brief Create a new animation
 * @param type Animation type
 * @param duration_ms Duration in milliseconds
 * @param priority Animation priority (0-255)
 * @param loop Whether to loop the animation
 * @return Animation ID or 0xFF if failed
 */
uint8_t animation_create(animation_type_t type, uint32_t duration_ms, uint8_t priority, bool loop);

/**
 * @brief Start an animation
 * @param animation_id Animation ID to start
 */
void animation_start(uint8_t animation_id);

/**
 * @brief Stop an animation
 * @param animation_id Animation ID to stop
 */
void animation_stop(uint8_t animation_id);

/**
 * @brief Pause an animation
 * @param animation_id Animation ID to pause
 */
void animation_pause(uint8_t animation_id);

/**
 * @brief Resume an animation
 * @param animation_id Animation ID to resume
 */
void animation_resume(uint8_t animation_id);


// ============================================================================
// ANIMATION PATTERN FUNCTIONS
// ============================================================================


/**
 * @brief Generate wave animation frame
 * @param frame Frame number
 * @param color Base color
 * @param speed Wave speed
 */
void animation_generate_wave_frame(uint32_t frame, uint32_t color, uint8_t speed);

/**
 * @brief Generate pulse animation frame
 * @param frame Frame number
 * @param color Base color
 * @param speed Pulse speed
 */
void animation_generate_pulse_frame(uint32_t frame, uint32_t color, uint8_t speed);

/**
 * @brief Generate fade animation frame
 * @param frame Frame number
 * @param from_color Starting color
 * @param to_color Ending color
 * @param total_frames Total number of frames
 */
void animation_generate_fade_frame(uint32_t frame, uint32_t from_color, uint32_t to_color, uint32_t total_frames);

/**
 * @brief Generate chess pattern frame
 * @param frame Frame number
 * @param color1 First color
 * @param color2 Second color
 */
void animation_generate_chess_pattern(uint32_t frame, uint32_t color1, uint32_t color2);

/**
 * @brief Generate rainbow animation frame
 * @param frame Frame number
 */
void animation_generate_rainbow_frame(uint32_t frame);


// ============================================================================
// ANIMATION EXECUTION FUNCTIONS
// ============================================================================


/**
 * @brief Execute animation frame
 * @param anim Animation to execute
 */
void animation_execute_frame(animation_t* anim);

/**
 * @brief Send frame data to LEDs
 * @param frame Frame data array
 */
void animation_send_frame_to_leds(const uint8_t frame[CHESS_LED_COUNT_TOTAL][3]);

/**
 * @brief Execute move highlight animation
 * @param anim Animation data
 */
void animation_execute_move_highlight(animation_t* anim);

/**
 * @brief Execute check highlight animation
 * @param anim Animation data
 */
void animation_execute_check_highlight(animation_t* anim);

/**
 * @brief Execute game over animation
 * @param anim Animation data
 */
void animation_execute_game_over(animation_t* anim);


// ============================================================================
// ANIMATION CONTROL FUNCTIONS
// ============================================================================


/**
 * @brief Process animation commands from queue
 */
void animation_process_commands(void);

/**
 * @brief Stop all animations
 */
void animation_stop_all(void);

/**
 * @brief Pause all animations
 */
void animation_pause_all(void);

/**
 * @brief Resume all animations
 */
void animation_resume_all(void);

/**
 * @brief Print animation status
 */
void animation_print_status(void);

/**
 * @brief Test all animation types
 */
void animation_test_all(void);

/**
 * @brief Test animation system
 */
void animation_test_system(void);

// Animation text representation functions
const char* animation_get_name(animation_type_t animation_type);
void animation_print_progress(const animation_t* anim);
void animation_print_piece_move(const char* from_square, const char* to_square, 
                               const char* piece_name, float progress);
void animation_print_check_status(bool is_checkmate, float progress);
void animation_print_summary(void);

#endif // ANIMATION_TASK_H