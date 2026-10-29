/**
 * @file led_task.c
 * @brief LED Task - WS2812B LED control and animation
 *
 * @details
 * =============================================================================
 * WHAT DID THIS FILE DO?
 * =============================================================================
 *
 * This task controls ALL LEDs in the system:
 * 1. 64 LEDs for the box (8x8 grid)
 * 2. 8 LEDs for promotion buttons
 * 3. 1 LED for reset button
 * 4. LED animation (turn, rose, graduation, end of game)
 * 5. LED feedback for buttons (available/unavailable/pressed)
 * 6. Batch update system for optimal performance
 *
 * =============================================================================
 * HOW DOES IT WORK?
 * =============================================================================
 *
 * HARDWARE:
 * - WS2812B LED strip on GPIO7
 * - 73 LEDs in total (64 boxes + 9 buttons)
 * - Timing-critical protocol (must disable interrupt)
 *
 * START UP:
 * - Initialize the ESP-IDF led_strip driver
 * - Box setting (black/white field)
 * - Initialization button LED (green/blue)
 * - Registration with WDT
 *
 * MAIN LOOP (10ms cycle):
 * while (1) {
 * 1. Reset WDT
 * 2. Process duration timers (LED with time limit)
 * 3. Process the button blink animation
 * 4. Update endgame wave animation
 * 5. Commit pending LED changes (batch update)
 * 6. Wait 10ms
 * }
 *
 * BATCH UPDATE SYSTEM:
 * - LED changes are NOT sent immediately
 * - Changes are COLLECTED in the buffer (led_pending_changes)
 * - Commit all changes AT ONCE (atomically)
 * - WHY? -> WS2812B requires timing-critical refresh
 *
 * =============================================================================
 * COMMUNICATION (FIFOS & MUTEXY)
 * =============================================================================
 *
 * DIRECT CALLS (NO QUEUE):
 * - led_set_pixel_internal() -> Set 1 LED
 * - led_highlight_square() -> Highlight the shelf
 * - led_show_chess_board() -> Show chess board
 *
 * MUTEXY - Protection of shared resources:
 * - led_unified_mutex -> LED state protection (led_states[])
 * IMPORTANT: Always use when changing LEDs!
 *
 * ACCESS:
 * @code
 * xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS);
 * led_states[index] = color;
 * led_pending_changes[index] = color;
 * led_changed_flags[index] = true;
 * xSemaphoreGive(led_unified_mutex);
 * @endcode
 *
 * =============================================================================
 * LED ANIMATION
 * =============================================================================
 *
 * BASE ANIMATION:
 * - Player change - Color the edges of the box according to the game
 * - Move path - Animation of the move path
 * - Valid moves - Display of valid moves (green)
 * - Capture - Red highlighting of collected pieces
 *
 * SPECIAL ANIMATIONS:
 * - Castling - Castling animation (king + carriage)
 * - Promotion - Graduation animation (variegated effect)
 * - Check - Blinking king in suit
 * - Checkmate/Stalemate - Wave effect across the board
 *
 * ENDGAME WAVE:
 * - Non-blocking animation (runs in the background)
 * - Wave effect from the center of the board
 * - Automatically stops after X seconds
 *
 * =============================================================================
 * BUTTON LED LOGIC
 * =============================================================================
 *
 * BUTTON LED STATUS:
 * - GREEN: Button available (can be used)
 * - BLUE: Button unavailable (cannot be used)
 * - RED: Button pressed
 * - FLASHING: Feedback after releasing the button (2s)
 *
 * EXAMPLE:
 * 1. Promoc: 8 promotion buttons BLUE (not available)
 * 2. Pesec reaches the last row -> GREEN (available)
 * 3. The player presses the Queen button -> RED
 * 4. Player release -> FLASH 2s -> BLUE
 *
 * =============================================================================
 * TABLE OF CONTENTS (NAVIGATION)
 * =============================================================================
 *
 * Section 1: Hardware Init & WDT ..................... line 70
 * Section 2: Global Variables ........................ radek 183
 * Sekce 3:  LED Control Functions ................... radek 292
 * Sekce 4:  Batch Update System ..................... radek 600
 * Sekce 5:  Button LED Logic ........................ radek 1200
 * Sekce 6:  Animation Functions ..................... radek 1500
 * Sekce 7:  Main LED Task Loop ...................... radek 2800
 *
 * TIP: Ctrl+G pro skok na radek
 *
 * =============================================================================
 * DEPENDENCIES
 * =============================================================================
 *
 * - ESP-IDF led_strip driver: WS2812B control
 * - game_task: Informace o stavu hry
 * - button_task: Button press/release eventy
 * - unified_animation_manager: Koordinace animaci
 *
 * =============================================================================
 * CRITICAL RULES
 * =============================================================================
 *
 * @warning DO NOT:
 *
 * 1. NEVER call led_strip_refresh() directly!
 *    ❌ led_strip_refresh(led_strip);  // WRONG - breaks the batch system
 *    ✅ led_commit_pending_changes();  // CORRECT - commit all changes
 *
 * 2. NEVER hold the mutex too long!
 *    ❌ xSemaphoreTake(...); vTaskDelay(100); xSemaphoreGive(...);
 *    ✅ Perform only necessary operations under the mutex
 *
 * 3. NEVER call blocking animations in the main loop!
 *    ❌ led_anim_endgame_blocking();  // Blocks the system for 10s
 *    ✅ led_update_endgame_wave();  // Non-blocking update
 *
 * 4. ALWAYS check LED index bounds!
 *    ❌ led_states[100] = color;  // Buffer overrun!
 *    ✅ if (index < CHESS_LED_COUNT_TOTAL) { led_states[index] = color; }
 *
 * 5. ALWAYS use the batch system for multiple LEDs!
 *    ❌ for (i=0; i<64; i++) led_force_immediate_update();  // 64x refresh!
 *    ✅ for (i=0; i<64; i++) led_set_pixel();  // 64x change
 *       led_commit_pending_changes();  // 1x refresh
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-12-23
 *
 * @note
 * - Task priority: 3 (medium priority)
 * - Stack size: 4KB
 * - Uses WDT (watchdog timer)
 * - Cycle: 10ms
 * - LED count: 73 (64 board + 9 buttons)
 *
 * @see game_task.c - Game state for LEDs
 * @see button_task.c - Button events
 * @see unified_animation_manager.c - Animation coordinator
 */

#include "led_task.h"
#include "power_guard.h"
#include "../freertos_chess/include/chess_types.h"
#include "../freertos_chess/include/streaming_output.h"
#include "../game_task/include/game_task.h" // game_session_snap_piece()
#include "../unified_animation_manager/include/unified_animation_manager.h" // Non-blocking endgame animations
#include "led_board_restore_guide.h"
#include "led_required_step_guide.h"
#include "led_gesture_anim.h"
#include "led_gesture_anim.h"
#include "led_mapping.h" // Include LED mapping functions

// Define min macro if not available
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos_chess.h"
#include "task_wdt.h"

void led_game_events_process(void);
void led_game_events_report_overlay(void);
#include "led_strip.h"
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "LED_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset that logs WARNING instead of ERROR for
 * ESP_ERR_NOT_FOUND
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if task not registered
 * (WARNING only)
 */
static esp_err_t led_task_wdt_reset_safe(void) {
  return chess_task_wdt_reset_if_owner(TAG, led_task_handle);
}

// ============================================================================
// LED SYSTEM OPTIMIZATION CONSTANTS - NEW FOR COMPLETE REPAIR
// ============================================================================

// WS2812B optimal timing constants
#define LED_CRITICAL_SECTION_TIMEOUT_MS 50 // Critical section timeout
#define LED_TASK_MUTEX_TIMEOUT_MS 100      // Mutex timeout in milliseconds
#define LED_TASK_MUTEX_TIMEOUT_TICKS                                           \
  (LED_TASK_MUTEX_TIMEOUT_MS / portTICK_PERIOD_MS) // Convert to ticks
#define LED_FRAME_BUFFER_SIZE 2                    // Double buffering
#define LED_MAX_RETRY_COUNT 3                      // Max retry attempts
#define LED_ERROR_RECOVERY_THRESHOLD 10            // Error recovery threshold
#define LED_HEALTH_CHECK_INTERVAL_MS 5000          // Health check interval
#define LED_BATCH_COMMIT_INTERVAL_MS                                           \
  50 // Batch commit interval for optimal performance
#define LED_WATCHDOG_RESET_INTERVAL                                            \
  10 // Reset watchdog every N LEDs during batch update

// ============================================================================
// LED DURATION MANAGEMENT SYSTEM - NEW TO SOLVE THE DURATION PROBLEM
// ============================================================================

// Duration tracking structure
typedef struct {
  uint8_t led_index;
  uint32_t original_color; // Color before duration
  uint32_t duration_color; // Color during duration
  uint32_t start_time;     // Beginning in ms
  uint32_t duration_ms;    // Duration
  bool is_active;          // Active duration?
  bool restore_original;   // Restore original color?
} led_duration_state_t;

// ============================================================================
// LED FRAME SYNCHRONIZATION SYSTEM - NEW FOR STABILITY
// ============================================================================

// Frame buffer structure for double buffering
typedef struct {
  uint32_t frame_id;
  uint32_t timestamp;
  bool is_complete_frame;
  uint8_t led_count;
  uint32_t pixels[CHESS_LED_COUNT_TOTAL];
} led_frame_buffer_t;

// ============================================================================
// LED HEALTH MONITORING SYSTEM - NEW FOR DIAGNOSIS
// ============================================================================

typedef struct {
  uint32_t commands_processed;
  uint32_t commands_failed;
  uint32_t mutex_timeouts;
  uint32_t frame_drops;
  uint32_t hardware_errors;
  uint32_t last_update_time;
  float success_rate;
  uint32_t average_frame_time_ms;
  uint32_t min_frame_time_ms;
  uint32_t max_frame_time_ms;
} led_health_stats_t;

// Hardware configuration
#define LED_DATA_PIN GPIO_NUM_7

// Forward declarations
static void led_send_status_to_uart_immediate(QueueHandle_t response_queue);
void led_execute_command_new(const led_command_t *cmd);
bool led_is_booting(void);
static esp_err_t led_hardware_init(void);
// static void led_hardware_update(void);  // REMOVED: Obsolete with immediate
// refresh
static void led_hardware_cleanup(void);

// BATCH UPDATE FUNCTIONS
static void
led_commit_pending_changes(void); // Commit all pending changes atomically
static void led_frame_begin(void);
static void led_frame_end(void);
static void led_release_held_picture(bool force);
void led_force_immediate_update(
    void); // Force immediate update for critical operations
static void led_privileged_batch_commit(
    void); // Optimized batch commit with privileged mutex sync
static void led_render_targets(void);
static void led_target_kick(void);
static bool led_visual_busy(void);

// LED layer management functions
void led_clear_board_only(void);   // Clear only board LEDs (0-63)
void led_clear_buttons_only(void); // Clear only button LEDs (64-72)
void led_preserve_buttons(void);   // Preserve button states during operations

// NEW: Duration management functions
static void led_set_pixel_with_duration(uint8_t led_index, uint8_t r, uint8_t g,
                                        uint8_t b, uint32_t duration_ms);
static void led_process_duration_expirations(void);
static void led_init_duration_system(void);

// Helper function to get ANSI color code from RGB
static const char *get_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

// Button LED logic functions
static void led_update_button_led_state(uint8_t button_id);
static void led_set_button_pressed(uint8_t button_id, bool pressed);
static void led_process_button_blink_timers(void);

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

// LED state storage (73 LEDs total)
static uint32_t led_states[CHESS_LED_COUNT_TOTAL] = {
    0}; // RGB values for each LED - initialized to 0

// Button LED state tracking
static bool button_pressed[CHESS_BUTTON_COUNT] = {false}; // Button press state
static bool button_available[CHESS_BUTTON_COUNT] = {
    false, false, false, false, false,
    false, false, false, true}; // Only reset button (index 8) available
                                // by default
static uint32_t button_release_time[CHESS_BUTTON_COUNT] = {
    0}; // Release time for blink
static bool button_blinking[CHESS_BUTTON_COUNT] = {false}; // Blink state

// Animation state
static bool animation_active = false;
static uint32_t animation_start_time = 0;
static uint32_t animation_duration = 0;
static uint8_t animation_pattern = 0;

// Task state
static bool task_running = false;
static bool matrix_scanning_enabled = true; // Component control
static bool led_component_enabled = true;   // LED component enabled by default
static bool simulation_mode =
    false; // Changed to false for real hardware operation

// LED STRIP HARDWARE STATE - Using official driver
static led_strip_handle_t led_strip = NULL;
static bool led_initialized = false;

// Hardware update throttling - OBSOLETE (keeping for compatibility)
// static uint32_t last_hardware_update = 0;  // REMOVED: No longer needed with
// immediate refresh static const uint32_t HARDWARE_UPDATE_INTERVAL_MS =
// LED_HARDWARE_UPDATE_MS;  // REMOVED: No longer needed

// LED synchronization - BATCH UPDATE SYSTEM
static SemaphoreHandle_t led_unified_mutex = NULL; // Queue synchronization only

// BATCH UPDATE SYSTEM - Collect changes, then commit atomically
static bool led_changes_pending = false; // Flag indicating pending changes
static uint32_t
    led_pending_changes[CHESS_LED_COUNT_TOTAL];       // Pending color changes
static bool led_changed_flags[CHESS_LED_COUNT_TOTAL]; // Track which LEDs
                                                      // actually changed
/* >0 while a multi-pixel picture is still being written. The strip stays dark
 * of that picture until the matching led_frame_end(). */
static portMUX_TYPE s_led_compose_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_led_compose;
/* Naked clear from another task stays open until its following pixel writes
 * land, so the periodic commit cannot publish the black frame in between. */
#define LED_PICTURE_SETTLE_MS 20
static volatile bool s_held_picture;
static volatile TickType_t s_picture_touch;

/* Crossfade and one-shot error pulse. Breathe is one shared clock. */
#define LED_FADE_MS 160U
#define LED_PULSE_MS 240U
#define LED_BREATHE_STEP_MS 88U
#define LED_BREATHE_PERIOD_MS 2200U
#define LED_BREATHE_FLOOR 102U /* 40% — the trough never goes to black */
#define MOVE_COLLAPSE_MS 180U
#define MOVE_COLLAPSE_FADE_MS 80U
#define LED_BOARD_N 64

static const uint8_t k_breathe_gain[16] = {
    128, 152, 176, 198, 218, 234, 246, 255,
    246, 234, 218, 198, 176, 152, 128, 140,
};

typedef struct {
  uint8_t tr, tg, tb;
  uint8_t mode;
  uint8_t sr, sg, sb;
  uint8_t pulse;
  uint8_t pr, pg, pb;
  uint16_t t0;
  uint8_t out_r, out_g, out_b;
  uint8_t touched;
  /* HOME guide only. period 0 keeps the shared breathe. */
  uint8_t br_floor;
  uint16_t br_period;
  uint16_t br_phase;
  uint16_t br_fade_ms;
  uint16_t br_delay;
  uint8_t br_fading;
} led_tgt_t;

static led_tgt_t s_tgt[LED_BOARD_N];
static uint16_t s_frame_fade;
static bool s_hold_direct;
static volatile uint8_t s_picture_open;
/* Direct pixel writes (boot bloom, wave) bypass out_*. The next target frame
 * must be pushed even when the target colour matches the cached out_*. */
static volatile uint8_t s_hw_resync;
static uint8_t s_target_write;
static uint32_t s_pic_gen;
static uint8_t s_force_hw;

/* Triangle → smoothstep → gamma lift. Floor stays 40%; midtones are not dark. */
static uint8_t led_breathe_gain_now(void) {
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
  uint32_t half = LED_BREATHE_PERIOD_MS / 2U;
  uint32_t t = now % LED_BREATHE_PERIOD_MS;
  uint32_t tri;
  uint32_t s;
  uint32_t down;
  uint32_t lifted;
  if (t < half) {
    tri = (t * 255U) / half;
  } else {
    tri = ((LED_BREATHE_PERIOD_MS - t) * 255U) / half;
  }
  s = (tri * tri * (765U - 2U * tri)) / 65025U;
  down = 255U - s;
  lifted = 255U - (down * down) / 255U;
  return (uint8_t)(LED_BREATHE_FLOOR +
                   ((255U - LED_BREATHE_FLOOR) * lifted) / 255U);
}

/* Same curve as the shared breathe, with a fixed phase and its own floor. */
static uint8_t led_breathe_sq_gain(uint16_t period, uint16_t phase, uint8_t floor) {
  uint32_t now;
  uint32_t half;
  uint32_t t;
  uint32_t tri;
  uint32_t s;
  uint32_t down;
  uint32_t lifted;
  if (period < 2U || floor >= 255U) {
    return 255U;
  }
  now = (uint32_t)(esp_timer_get_time() / 1000);
  half = (uint32_t)period / 2U;
  t = (now + (uint32_t)phase) % (uint32_t)period;
  if (t < half) {
    tri = (t * 255U) / half;
  } else {
    tri = (((uint32_t)period - t) * 255U) / half;
  }
  s = (tri * tri * (765U - 2U * tri)) / 65025U;
  down = 255U - s;
  lifted = 255U - (down * down) / 255U;
  return (uint8_t)((uint32_t)floor + ((255U - floor) * lifted) / 255U);
}

static uint8_t led_smoothstep(uint16_t elapsed, uint16_t dur) {
  uint32_t t;
  if (dur == 0U || elapsed >= dur) {
    return 255U;
  }
  t = ((uint32_t)elapsed * 255U) / dur;
  return (uint8_t)((t * t * (765U - 2U * t)) / 65025U);
}

static uint8_t led_lerp_u8(uint8_t a, uint8_t b, uint8_t t) {
  return (uint8_t)((int)a + (((int)b - (int)a) * (int)t) / 255);
}

static uint8_t led_scale_u8(uint8_t c, uint8_t gain) {
  return (uint8_t)(((uint16_t)c * gain) / 255U);
}

// NEW: Duration management system
static led_duration_state_t led_durations[CHESS_LED_COUNT_TOTAL] = {0};
static bool led_duration_system_enabled = true;

// GLOBAL BRIGHTNESS CONTROL
static uint8_t global_brightness = 100; // Full scale; not dimmed by a stored value

/* No rail scale and no stored dim. Every mode shifts out the requested level. */

// LED patterns
static const uint32_t chess_board_pattern[64] = {
    // White squares (even indices)
    0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0x000000, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0x000000, 0xFFFFFF, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0x000000, 0xFFFFFF, 0x000000, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0x000000, 0xFFFFFF,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0xFFFFFF,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000,
    0xFFFFFF};

// Button LED colors (kept for reference, not used in new logic)
// static const uint32_t button_colors[9] = {
//     0xFF00FF, // Queen A - Magenta
//     0xFF0000, // Rook A - Red
//     0x00FFFF, // Bishop A - Cyan
//     0xFF8000, // Knight A - Orange
//     0xFF00FF, // Queen B - Magenta
//     0xFF0000, // Rook B - Red
//     0x00FFFF, // Bishop B - Cyan
//     0xFF8000, // Knight B - Orange
//     0xFF0000  // Reset - Red
// };

// Test pattern colors
static const uint32_t test_pattern_colors[] = {
    0xFF0000, // Red
    0x00FF00, // Green
    0x0000FF, // Blue
    0xFFFF00, // Yellow
    0xFF00FF, // Magenta
    0x00FFFF, // Cyan
    0xFFFFFF, // White
    0xFF8000  // Orange
};

// ============================================================================
// SMART LED REPORTING SYSTEM
// ============================================================================

// LED state tracking for smart reporting
static uint32_t
    previous_led_states[CHESS_LED_COUNT_TOTAL]; // Previous LED states
static bool led_states_initialized = false;
static uint32_t last_status_report_time = 0;
static uint32_t led_changes_count = 0;
static bool quiet_period_active = false;
static uint32_t quiet_period_start = 0;
static uint32_t quiet_period_duration =
    5000; // 5 seconds quiet after board operations

// LED grouping for compact display
static const char *color_names[] = {"Black", "Blue",    "Green",  "Cyan",
                                    "Red",   "Magenta", "Yellow", "White"};

// Button names for LED display
static const char *button_names[] = {
    "Q", "R", "B", "N", "Q",
    "R", "B", "N", "R" // Queen, Rook, Bishop, Knight (4 for each player) +
                       // Reset
};

// ============================================================================
// LED CONTROL FUNCTIONS
// ============================================================================

void led_set_brightness_global(uint8_t brightness) {
  if (brightness > 100)
    brightness = 100;

  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) ==
        pdTRUE) {
      global_brightness = brightness;

      led_changes_pending = true;
      for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        led_pending_changes[i] = led_states[i];
        led_changed_flags[i] = true;
      }

      /* Leave the commit on the LED task loop — calling led_commit_pending_changes()
       * from the HTTP/BLE thread resulted in visible "fading" when changing brightness. */
      xSemaphoreGive(led_unified_mutex);
      ESP_LOGI(TAG, "Global brightness set to %d%% (deferred to LED task)",
               brightness);
    } else {
      ESP_LOGW(TAG, "Failed to set brightness: Mutex timeout");
    }
  } else {
    // No mutex (startup?), just set
    global_brightness = brightness;
  }
}

/**
 * @brief Map button ID to correct LED index
 * @param button_id Button ID (0-8)
 * @return LED index (64-72)
 */
static uint8_t led_get_button_led_index(uint8_t button_id) {
  if (button_id <= 7) {
    return 64 + button_id; // Promotion buttons: 64-71 (8 buttons total)
  } else if (button_id == 8) {
    return 72; // Reset button: 72
  } else {
    return 64 + button_id; // Fallback (should not happen)
  }
}

static void led_set_pixel_channels(uint8_t led_index, uint8_t red, uint8_t green,
                                   uint8_t blue, uint8_t white) {
  if (led_index >= CHESS_LED_COUNT_TOTAL) {
    ESP_LOGE(TAG, "Invalid LED index: %d (max: %d)", led_index,
             CHESS_LED_COUNT_TOTAL - 1);
    return;
  }

  // Check if LED component is enabled
  if (!led_component_enabled) {
    ESP_LOGD(TAG, "LED component disabled - ignoring LED command for LED %d",
             led_index);
    return;
  }

  // If matrix scanning is disabled and this is a board LED (0-63), limit
  // animations
  if (!matrix_scanning_enabled && led_index < 64) {
    // Only allow static colors, no animations
    if (red > 0 || green > 0 || blue > 0 || white > 0) {
      ESP_LOGD(
          TAG,
          "Matrix scanning disabled - limiting LED animation for board LED %d",
          led_index);
    }
  }

  // Unified mutex protection
  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) !=
        pdTRUE) {
      ESP_LOGW(TAG,
               "Failed to take LED unified mutex - skipping LED operation");
      return;
    }
  }

  // BATCH UPDATE SYSTEM - Just collect change, don't commit immediately.
  // White lives in the top byte. RGB-only callers leave it at 0 and the
  // budgeted writer extracts a neutral white onto the white die.
  uint32_t color = ((uint32_t)white << 24) | ((uint32_t)red << 16) |
                   ((uint32_t)green << 8) | blue;

  /* Same color is already on the strip. A refresh here is a full frame and
   * the guide used to request one 30 times a second, which blinks under WiFi.
   * A direct board write still leaves out_* stale, so the next target frame
   * is sent in full. The target renderer sets s_target_write / s_force_hw. */
  if (led_index < LED_BOARD_N && !s_target_write) {
    s_hw_resync = 1U;
    s_pic_gen++;
  }
  if (!s_force_hw && led_states[led_index] == color) {
    if (led_unified_mutex != NULL) {
      xSemaphoreGive(led_unified_mutex);
    }
    return;
  }

  // Update internal state
  led_states[led_index] = color;

  // Mark change as pending for next batch commit
  if (led_initialized && led_strip != NULL && !simulation_mode) {
    led_pending_changes[led_index] = color;
    led_changed_flags[led_index] = true; // Mark this LED as changed
    led_changes_pending = true;
  }

  if (simulation_mode) {
    ESP_LOGI(TAG, "LED[%d] = RGBW(%d,%d,%d,%d) = 0x%08" PRIX32, led_index, red,
             green, blue, white, led_states[led_index]);
  }

  // Release unified mutex
  if (led_unified_mutex != NULL) {
    xSemaphoreGive(led_unified_mutex);
  }
}

void led_set_pixel_internal(uint8_t led_index, uint8_t red, uint8_t green,
                            uint8_t blue) {
  led_set_pixel_channels(led_index, red, green, blue, 0);
}

void led_set_pixel_rgbw(uint8_t led_index, uint8_t red, uint8_t green,
                        uint8_t blue, uint8_t white) {
  led_set_pixel_channels(led_index, red, green, blue, white);
}

void led_set_all_internal(uint8_t red, uint8_t green, uint8_t blue) {
  uint32_t color = (red << 16) | (green << 8) | blue;

  led_frame_begin();
  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) !=
        pdTRUE) {
      led_frame_end();
      ESP_LOGW(TAG, "Failed to take LED mutex for set-all");
      return;
    }
  }
  bool any = false;
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    if (led_states[i] == color) {
      continue;
    }
    led_states[i] = color;
    led_pending_changes[i] = color;
    led_changed_flags[i] = true;
    any = true;
  }
  if (any) {
    led_changes_pending = true;
  }
  if (led_unified_mutex != NULL) {
    xSemaphoreGive(led_unified_mutex);
  }
  led_frame_end();
  led_commit_pending_changes();

  if (simulation_mode) {
    ESP_LOGI(TAG, "All LEDs = RGB(%d,%d,%d) = 0x%06" PRIX32, red, green, blue,
             color);
  }
}

void led_clear_all_internal(void) {
  led_frame_begin();
  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) !=
        pdTRUE) {
      led_frame_end();
      ESP_LOGW(TAG, "Failed to take LED mutex for clear");
      return;
    }
  }
  bool any = false;
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    if (led_states[i] == 0 && !led_changed_flags[i]) {
      continue;
    }
    led_states[i] = 0;
    led_pending_changes[i] = 0;
    led_changed_flags[i] = true;
    any = true;
  }
  if (any) {
    led_changes_pending = true;
  }
  if (led_unified_mutex != NULL) {
    xSemaphoreGive(led_unified_mutex);
  }
  led_frame_end();
  /* One black frame after the logical buffer is finished. Nested inside
   * another frame, this returns and the outer end sends it. */
  led_commit_pending_changes();
}

void led_show_chess_board(void) {
  if (led_is_booting()) {
    return;
  }
  ESP_LOGI(TAG, "Setting chess board pattern...");

  led_frame_begin();
  /* Pattern is stored in square order (a1 = index 0). The strip index is
   * the quadrant map, so a checker follows the board on CzechMate v2. */
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      uint32_t color = chess_board_pattern[row * 8 + col];
      led_set_pixel_internal(chess_pos_to_led_index(row, col),
                             (uint8_t)((color >> 16) & 0xFF),
                             (uint8_t)((color >> 8) & 0xFF),
                             (uint8_t)(color & 0xFF));
    }
  }

  // Initialize button LED states and set to default colors
  // 8 promotion buttons (0-7) + 1 reset button (8) = 9 total
  for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
    button_pressed[i] = false;
    button_available[i] =
        (i == 8); // Only reset button (8) available by default
    button_release_time[i] = 0;
    button_blinking[i] = false;

    // Set button LED color based on availability
    uint8_t led_index = led_get_button_led_index(i);
    if (button_available[i]) {
      led_set_pixel_internal(led_index, 0, 255, 0); // Green - available
    } else {
      led_set_pixel_internal(led_index, 0, 0, 255); // Blue - unavailable
    }
  }
  led_frame_end();
  ESP_LOGI(TAG, "✅ Button LEDs initialized - reset button (LED 72) green, "
                "promotion buttons (LED 64-71) blue");

  // Force immediate update to ensure button LEDs are visible
  led_force_immediate_update();

  // The checkerboard is already set above, just confirm
  ESP_LOGI(TAG, "✅ Chess board pattern displayed - button LEDs preserved");

  // CRITICAL: Force another update to ensure button LEDs are visible
  // immediately
  led_force_immediate_update();

  // Debug button LEDs after initialization
  for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
    uint8_t led_index = led_get_button_led_index(i);
    ESP_LOGI(TAG, "Button %d (LED %d) after final init: 0x%06" PRIX32, i, led_index,
             led_states[led_index]);
  }

  if (simulation_mode) {
    ESP_LOGI(TAG, "Chess board pattern displayed");
    ESP_LOGI(TAG, "  - 64 board LEDs: alternating black/white pattern");
    ESP_LOGI(TAG, "  - 9 button LEDs: 8 promotion (64-71) + 1 reset (72)");
  }

  // Start quiet period to reduce verbose LED status reports
  led_start_quiet_period(5000); // 5 seconds quiet after board setup

  ESP_LOGI(TAG, "🎉 Chess board pattern complete!");
}

/* sRGB gamma (IEC 61966-2-1) as 0–255 linear. */
static const uint8_t k_sk6812_srgb_lin[256] = {
    0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,
    3,   3,   4,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,
    7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   10,  10,  10,  11,  11,
    12,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  17,
    18,  18,  19,  19,  20,  20,  21,  22,  22,  23,  23,  24,  24,  25,  25,
    26,  27,  27,  28,  29,  29,  30,  30,  31,  32,  32,  33,  34,  35,  35,
    36,  37,  37,  38,  39,  40,  41,  41,  42,  43,  44,  45,  45,  46,  47,
    48,  49,  50,  51,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,
    62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  76,  77,
    78,  79,  80,  81,  82,  84,  85,  86,  87,  88,  90,  91,  92,  93,  95,
    96,  97,  99,  100, 101, 103, 104, 105, 107, 108, 109, 111, 112, 114, 115,
    116, 118, 119, 121, 122, 124, 125, 127, 128, 130, 131, 133, 134, 136, 138,
    139, 141, 142, 144, 146, 147, 149, 151, 152, 154, 156, 157, 159, 161, 163,
    164, 166, 168, 170, 171, 173, 175, 177, 179, 181, 183, 184, 186, 188, 190,
    192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220,
    222, 224, 226, 229, 231, 233, 235, 237, 239, 242, 244, 246, 248, 250, 253,
    255};

/*
 * OPSCO SK6812MINIRGBW-XX-P6-A-000, typical mid of the datasheet window at
 * Vds = 1 V: R 620 nm / 293 mcd, G 530 nm / 815 mcd, B 470 nm / 210 mcd.
 * Q8 matrix maps linear sRGB onto those dice so equal requested white is not
 * a green-heavy mix. Orange stays red-orange instead of sliding to yellow.
 * W is left at 0: the CCT bin (BW/NW/WS/GW) is not in the BOM, and the old
 * min(RGB) split painted near-whites with that unknown phosphor.
 */
void led_correct_sk6812(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                        uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
  uint8_t lr = k_sk6812_srgb_lin[r];
  uint8_t lg = k_sk6812_srgb_lin[g];
  uint8_t lb = k_sk6812_srgb_lin[b];
  int32_t rr = (154 * (int32_t)lr + 87 * (int32_t)lg + 15 * (int32_t)lb) >> 8;
  int32_t gg = (10 * (int32_t)lr + 187 * (int32_t)lg - 4 * (int32_t)lb) >> 8;
  int32_t bb = (1 * (int32_t)lr + 8 * (int32_t)lg + 80 * (int32_t)lb) >> 8;
  if (brightness != 255) {
    rr = rr * brightness / 255;
    gg = gg * brightness / 255;
    bb = bb * brightness / 255;
  }
  if (rr < 0) {
    rr = 0;
  } else if (rr > 255) {
    rr = 255;
  }
  if (gg < 0) {
    gg = 0;
  } else if (gg > 255) {
    gg = 255;
  }
  if (bb < 0) {
    bb = 0;
  } else if (bb > 255) {
    bb = 255;
  }
  if (out_r) {
    *out_r = (uint8_t)rr;
  }
  if (out_g) {
    *out_g = (uint8_t)gg;
  }
  if (out_b) {
    *out_b = (uint8_t)bb;
  }
}

void led_set_ha_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  /* A gesture animation (shutdown, sweep, wave) owns the squares until it
   * finishes. Applying the lamp here would wipe that picture. */
  if (led_gesture_anim_busy()) {
    ESP_LOGI(TAG, "HA color deferred, gesture animation owns the board");
    return;
  }
  /* Lamp brightness is the caller's 0-255 level. No extra cap. */

  uint8_t r_scaled = 0, g_scaled = 0, b_scaled = 0;
  led_correct_sk6812(r, g, b, brightness, &r_scaled, &g_scaled, &b_scaled);
  uint8_t w_scaled = 0;

  ESP_LOGI(TAG,
           "Setting HA color: RGB(%d,%d,%d) brightness=%d -> RGBW(%d,%d,%d,%d)",
           r, g, b, brightness, r_scaled, g_scaled, b_scaled, w_scaled);

  // Set all 64 board LEDs (indices 0-63)
  led_frame_begin();
  for (int i = 0; i < CHESS_LED_COUNT_BOARD; i++) {
    led_set_pixel_rgbw(i, r_scaled, g_scaled, b_scaled, w_scaled);
  }

  // Apply same HA color to button LEDs (64-72) so they shine with the light
  for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
    uint8_t led_index = led_get_button_led_index((uint8_t)i);
    led_set_pixel_rgbw(led_index, r_scaled, g_scaled, b_scaled, w_scaled);
  }
  led_frame_end();

  // Force immediate update
  led_force_immediate_update();

  ESP_LOGI(TAG, "✅ HA color applied to board + %d button LEDs",
           CHESS_LED_COUNT_BOARD + CHESS_BUTTON_COUNT);
}

/**
 * @brief Restores the board after HA mode
 *
 * This function restores the normal display of the chessboard (black/white squares).
 */
void led_restore_chess_board(void) {
  ESP_LOGI(TAG, "Restoring chess board pattern");
  led_show_chess_board();
}

/**
 * @brief Resets the display of all buttons according to the current state (available/pressed)
 * Called after returning from HA mode so that the buttons display the correct colors.
 */
void led_refresh_all_button_leds(void) {
  for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
    led_update_button_led_state((uint8_t)i);
  }
  led_force_immediate_update();
}

void led_set_button_feedback(uint8_t button_id, bool available) {
  if (button_id >= CHESS_BUTTON_COUNT) {
    ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id,
             CHESS_BUTTON_COUNT - 1);
    return;
  }

  // Update button availability state
  button_available[button_id] = available;

  // Update LED state based on new availability
  led_update_button_led_state(button_id);

  if (simulation_mode) {
    ESP_LOGI(TAG, "Button %d LED: %s", button_id,
             available ? "Available (green)" : "Not available (blue)");
  }
}

void led_set_button_press(uint8_t button_id) {
  if (button_id >= CHESS_BUTTON_COUNT) {
    ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id,
             CHESS_BUTTON_COUNT - 1);
    return;
  }

  // Set button as pressed
  button_pressed[button_id] = true;
  button_blinking[button_id] = false; // Stop any blinking

  // Update LED to red (pressed state)
  led_update_button_led_state(button_id);

  if (simulation_mode) {
    ESP_LOGI(TAG, "Button %d LED: Press feedback (red)", button_id);
  }
}

void led_set_button_release(uint8_t button_id) {
  if (button_id >= CHESS_BUTTON_COUNT) {
    ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id,
             CHESS_BUTTON_COUNT - 1);
    return;
  }

  // Set button as released
  button_pressed[button_id] = false;
  button_release_time[button_id] = esp_timer_get_time() / 1000;
  button_blinking[button_id] = true; // Start blinking

  // Update LED state
  led_update_button_led_state(button_id);

  if (simulation_mode) {
    ESP_LOGI(TAG,
             "Button %d LED: Release feedback (return to availability state)",
             button_id);
  }
}

uint32_t led_get_button_color(uint8_t button_id) {
  if (button_id >= CHESS_BUTTON_COUNT) {
    ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id,
             CHESS_BUTTON_COUNT - 1);
    return 0;
  }

  // Correct mapping of button IDs to LED indices
  uint8_t led_index = led_get_button_led_index(button_id);
  return led_states[led_index];
}

/**
 * @brief Get LED state for a specific LED index
 * @param led_index LED index (0-72)
 * @return RGB color value (0xRRGGBB)
 */
uint32_t led_get_led_state(uint8_t led_index) {
  if (led_index >= CHESS_LED_COUNT_TOTAL) {
    ESP_LOGE(TAG, "Invalid LED index: %d (max: %d)", led_index,
             CHESS_LED_COUNT_TOTAL - 1);
    return 0;
  }

  // Unified mutex protection
  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) ==
        pdTRUE) {
      uint32_t state = led_states[led_index];
      xSemaphoreGive(led_unified_mutex);
      return state;
    } else {
      ESP_LOGW(TAG, "Failed to acquire LED unified mutex for get state");
      return 0;
    }
  } else {
    // Fallback if mutex not available
    return led_states[led_index];
  }
}

/**
 * @brief Get all LED states
 * @param states Array to store LED states
 * @param max_count Maximum number of states to copy
 */
void led_get_all_states(uint32_t *states, size_t max_count) {
  if (states == NULL) {
    ESP_LOGE(TAG, "Invalid states array");
    return;
  }

  size_t count =
      (max_count < CHESS_LED_COUNT_TOTAL) ? max_count : CHESS_LED_COUNT_TOTAL;

  // Unified mutex protection
  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) ==
        pdTRUE) {
      memcpy(states, led_states, count * sizeof(uint32_t));
      xSemaphoreGive(led_unified_mutex);
    } else {
      ESP_LOGW(TAG, "Failed to acquire LED unified mutex for get all states");
      memset(states, 0, count * sizeof(uint32_t));
    }
  } else {
    // Fallback if mutex not available
    memcpy(states, led_states, count * sizeof(uint32_t));
  }
}

void led_start_animation(uint32_t duration_ms) {
  animation_active = true;
  animation_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
  animation_duration = duration_ms;
  animation_pattern = 0;

  if (simulation_mode) {
    ESP_LOGI(TAG, "Animation started: duration=%" PRIu32 "ms", duration_ms);
  }
}

void led_test_pattern(void) {
  ESP_LOGI(TAG, "=== LED Test Pattern ===");

  // Test all LEDs with different colors
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    uint32_t color = test_pattern_colors[i % 8];
    uint8_t red = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;

    led_set_pixel_internal(i, red, green, blue);
    vTaskDelay(
        pdMS_TO_TICKS(50)); // 50ms delay between LEDs for better performance
  }

  ESP_LOGI(TAG, "=== LED Test Pattern Complete ===");
}

void led_test_all_pattern(void) {
  ESP_LOGI(TAG, "=== LED Test All Pattern ===");

  // Test all LEDs with cycling colors
  uint32_t test_colors[] = {
      0xFF0000, // Red
      0x00FF00, // Green
      0x0000FF, // Blue
      0xFFFF00, // Yellow
      0xFF00FF, // Magenta
      0x00FFFF, // Cyan
      0xFFFFFF, // White
      0x000000  // Off
  };

  for (int color_idx = 0; color_idx < 8; color_idx++) {
    uint32_t color = test_colors[color_idx];
    uint8_t red = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;

    ESP_LOGI(TAG, "Testing color: R=%d, G=%d, B=%d", red, green, blue);

    // Set all LEDs to current test color
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      led_set_pixel_internal(i, red, green, blue);
    }

    vTaskDelay(pdMS_TO_TICKS(
        500)); // 500ms delay between color changes for better performance
  }

  // Clear all LEDs
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    led_set_pixel_internal(i, 0, 0, 0);
  }

  ESP_LOGI(TAG, "=== LED Test All Pattern Complete ===");
}

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

// ============================================================================
// OBSOLETE FUNCTION - REMOVED
// ============================================================================

// REMOVED: led_execute_command() - Replaced by led_execute_command_new()
// This function was obsolete and duplicated functionality

void led_process_commands(void) {
  // LED command processing removed - using direct LED calls now
}

// Forward declarations for animation functions
void led_anim_move_path(const led_command_t *cmd);
void led_anim_castle(const led_command_t *cmd);
void led_anim_promote(const led_command_t *cmd);
void led_anim_endgame(const led_command_t *cmd);
void led_update_endgame_wave(
    void); // Update endgame wave animation (non-blocking)
void led_anim_check(const led_command_t *cmd);
void led_anim_checkmate(const led_command_t *cmd);

// Enhanced Castling System functions
void led_enhanced_castling_guidance(const led_command_t *cmd);
void led_enhanced_castling_error(const led_command_t *cmd);
void led_enhanced_castling_celebration(const led_command_t *cmd);
void led_enhanced_castling_tutorial(const led_command_t *cmd);
void led_enhanced_castling_clear(void);

/**
 * @brief Execute LED command with full command structure
 */
static uint8_t s_board_show;

static void led_show_begin(void) {
  if (s_board_show < 255) {
    s_board_show++;
  }
  led_board_restore_guide_yield();
  led_required_step_guide_yield();
}

static void led_show_end(void) {
  if (s_board_show > 0) {
    s_board_show--;
  }
}

void led_execute_command_new(const led_command_t *cmd) {
  if (!cmd) {
    return;
  }
  if (led_is_booting()) {
    ESP_LOGD(TAG, "Skip LED command during boot (type=%d)", (int)cmd->type);
    return;
  }
  ESP_LOGI(TAG, "🔄 led_execute_command_new: type=%d", cmd->type);
  switch (cmd->type) {
  case LED_CMD_SET_PIXEL:
    // Podporovat duration management
    if (cmd->duration_ms > 0) {
      led_set_pixel_with_duration(cmd->led_index, cmd->red, cmd->green,
                                  cmd->blue, cmd->duration_ms);
    } else {
      led_set_pixel_internal(cmd->led_index, cmd->red, cmd->green, cmd->blue);
    }
    break;

  case LED_CMD_SET_ALL:
    led_set_all_internal(cmd->red, cmd->green, cmd->blue);
    break;

  case LED_CMD_SET_BRIGHTNESS:
    // Value represents percentage 0-100
    // Using red channel or explicit value field if available,
    // but led_command_t doesn't have 'value', so use 'red' or 'led_index' as
    // value carrier? Looking at struct, we can use 'red' as value carrier for
    // single byte, or we can use 'led_index' (uint8) Let's use 'red' as the
    // value carrier by convention for single-value commands
    {
      uint8_t new_brightness = cmd->red;
      if (new_brightness > 100)
        new_brightness = 100;

      ESP_LOGI(TAG, "💡 Setting global brightness: %d%%", new_brightness);
      global_brightness = new_brightness;

      led_frame_begin();
      if (led_unified_mutex != NULL &&
          xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) ==
              pdTRUE) {
        led_changes_pending = true;
        for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
          led_pending_changes[i] = led_states[i];
          led_changed_flags[i] = true;
        }
        xSemaphoreGive(led_unified_mutex);
      }
      led_frame_end();
      led_commit_pending_changes();
    }
    break;

  case LED_CMD_CLEAR:
    led_clear_all_internal();
    break;

  case LED_CMD_SHOW_BOARD:
    led_show_chess_board();
    break;

  case LED_CMD_BUTTON_FEEDBACK:
    if (cmd->led_index >= CHESS_LED_COUNT_BOARD) {
      uint8_t button_id = cmd->led_index - CHESS_LED_COUNT_BOARD;
      led_set_button_feedback(
          button_id, (cmd->red > 0 || cmd->green > 0 || cmd->blue > 0));
    }
    break;

  case LED_CMD_BUTTON_PRESS:
    if (cmd->led_index >= CHESS_LED_COUNT_BOARD) {
      uint8_t button_id = cmd->led_index - CHESS_LED_COUNT_BOARD;
      led_set_button_press(button_id);
    }
    break;

  case LED_CMD_BUTTON_RELEASE:
    if (cmd->led_index >= CHESS_LED_COUNT_BOARD) {
      uint8_t button_id = cmd->led_index - CHESS_LED_COUNT_BOARD;
      led_set_button_release(button_id);
    }
    break;

  case LED_CMD_ANIMATION:
    led_start_animation(cmd->duration_ms);
    break;

  case LED_CMD_TEST:
    led_test_pattern();
    break;

  case LED_CMD_STATUS_DETAILED:
    ESP_LOGI(TAG, "🔍 Processing LED_CMD_STATUS_DETAILED command");
    // Send status immediately without delays to avoid blocking main loop
    led_send_status_to_uart_immediate(cmd->response_queue);
    break;

  case LED_CMD_STATUS_COMPACT:
    led_print_compact_status();
    break;

  case LED_CMD_STATUS_ACTIVE:
    led_print_compact_status();
    break;

  // Advanced Chess Animation Cases
  case LED_CMD_ANIM_MOVE_PATH:
    ESP_LOGI(TAG, "➡️ Move path animation");
    led_show_begin();
    led_anim_move_path(cmd);
    led_show_end();
    break;

  case LED_CMD_ANIM_CASTLE:
    ESP_LOGI(TAG, "🏰 Castling animation");
    led_show_begin();
    led_anim_castle(cmd);
    led_show_end();
    break;

  case LED_CMD_ANIM_PROMOTE:
    ESP_LOGI(TAG, "👑 Promotion animation");
    led_anim_promote(cmd);
    break;

  case LED_CMD_ANIM_ENDGAME:
    ESP_LOGI(TAG, "🏆 Endgame animation");
    led_anim_endgame(cmd);
    break;

  case LED_CMD_ANIM_CHECK:
    ESP_LOGI(TAG, "⚠️ Check animation");
    led_show_begin();
    led_anim_check(cmd);
    led_show_end();
    break;

  case LED_CMD_ANIM_CHECKMATE:
    ESP_LOGI(TAG, "☠️ Checkmate animation");
    led_show_begin();
    led_anim_checkmate(cmd);
    led_show_end();
    break;

  case LED_CMD_BUTTON_PROMOTION_AVAILABLE:
    ESP_LOGI(TAG, "🟢 Button %d promotion available",
             cmd->led_index - CHESS_LED_COUNT_BOARD);
    led_set_button_promotion_available(cmd->led_index - CHESS_LED_COUNT_BOARD,
                                       true);
    break;

  case LED_CMD_BUTTON_PROMOTION_UNAVAILABLE:
    ESP_LOGI(TAG, "🔴 Button %d promotion unavailable",
             cmd->led_index - CHESS_LED_COUNT_BOARD);
    led_set_button_promotion_available(cmd->led_index - CHESS_LED_COUNT_BOARD,
                                       false);
    break;

  case LED_CMD_BUTTON_SET_PRESSED:
    ESP_LOGI(TAG, "🔴 Button %d pressed",
             cmd->led_index - CHESS_LED_COUNT_BOARD);
    led_set_button_pressed(cmd->led_index - CHESS_LED_COUNT_BOARD, true);
    break;

  case LED_CMD_BUTTON_SET_RELEASED:
    ESP_LOGI(TAG, "🔵 Button %d released",
             cmd->led_index - CHESS_LED_COUNT_BOARD);
    led_set_button_pressed(cmd->led_index - CHESS_LED_COUNT_BOARD, false);
    break;

  // Game state integration commands
  case LED_CMD_GAME_STATE_UPDATE:
    ESP_LOGI(TAG, "🎮 Game state update requested");
    led_update_game_state();
    break;

  case LED_CMD_HIGHLIGHT_PIECES:
    ESP_LOGI(TAG, "🎯 Highlight pieces that can move");
    led_highlight_pieces_that_can_move();
    break;

  case LED_CMD_HIGHLIGHT_MOVES:
    ESP_LOGI(TAG, "🎯 Highlight possible moves for square %d", cmd->led_index);
    led_highlight_possible_moves(cmd->led_index);
    break;

  case LED_CMD_CLEAR_HIGHLIGHTS:
    ESP_LOGI(TAG, "🧹 Clear all highlights");
    led_clear_all_highlights();
    break;

  case LED_CMD_ERROR_INVALID_MOVE:
    ESP_LOGI(TAG, "🚨 Error: Invalid move at LED %d", cmd->led_index);
    led_error_invalid_move(cmd);
    break;

  case LED_CMD_ERROR_RETURN_PIECE:
    ESP_LOGI(TAG, "🔄 Error: Return piece to LED %d", cmd->led_index);
    led_error_return_piece(cmd);
    break;

  case LED_CMD_ERROR_RECOVERY:
    ESP_LOGI(TAG, "✅ Error recovery completed");
    led_error_recovery(cmd);
    break;

  case LED_CMD_SHOW_LEGAL_MOVES:
    ESP_LOGI(TAG, "💡 Show legal moves for piece at LED %d", cmd->led_index);
    led_show_legal_moves(cmd);
    break;

  // Enhanced Castling System commands
  case LED_CMD_CASTLING_GUIDANCE:
    ESP_LOGI(TAG, "🏰 Enhanced castling guidance");
    led_enhanced_castling_guidance(cmd);
    break;

  case LED_CMD_CASTLING_ERROR:
    ESP_LOGI(TAG, "❌ Enhanced castling error indication");
    led_enhanced_castling_error(cmd);
    break;

  case LED_CMD_CASTLING_CELEBRATION:
    ESP_LOGI(TAG, "🎉 Enhanced castling celebration");
    led_enhanced_castling_celebration(cmd);
    break;

  case LED_CMD_CASTLING_TUTORIAL:
    ESP_LOGI(TAG, "📖 Enhanced castling tutorial");
    led_enhanced_castling_tutorial(cmd);
    break;

  case LED_CMD_CASTLING_CLEAR:
    ESP_LOGI(TAG, "🧹 Clear enhanced castling indications");
    led_enhanced_castling_clear();
    break;

  case LED_CMD_HIGHLIGHT_HINT: {
    uint8_t from_idx = cmd->led_index;
    uint8_t to_idx = cmd->data ? *(uint8_t *)cmd->data : 0;
    ESP_LOGI(TAG, "💡 Hint highlight: from LED %u -> to LED %u", (unsigned)from_idx,
             (unsigned)to_idx);
    if (from_idx < 64) {
      led_set_pixel_internal(from_idx, 0, 255, 255); /* cyan = hint from where */
    }
    if (to_idx < 64) {
      led_set_pixel_internal(to_idx, 255, 140, 0); /* orange = help where */
    }
    break;
  }

  default:
    ESP_LOGW(TAG, "Unknown LED command type: %d", cmd->type);
    break;
  }
}

// ============================================================================
// OBSOLETE PUBLIC FUNCTION - KEPT FOR COMPATIBILITY
// ============================================================================

void led_update_hardware(void) {
  // OBSOLETE: This function is no longer needed
  // All LED updates now happen immediately via led_set_pixel_internal()
  // Kept only for compatibility with existing code that might call it
  ESP_LOGD(
      TAG,
      "led_update_hardware() called but obsolete - all updates now immediate");
}

// ============================================================================
// ANIMATION FUNCTIONS
// ============================================================================

void led_update_animation(void) {
  if (!animation_active) {
    return;
  }

  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t elapsed = current_time - animation_start_time;

  if (elapsed >= animation_duration) {
    // Animation complete
    animation_active = false;
    if (simulation_mode) {
      ESP_LOGI(TAG, "Animation completed after %" PRIu32 "ms", elapsed);
    }
    return;
  }

  // Calculate animation progress (0.0 to 1.0)
  float progress = (float)elapsed / (float)animation_duration;

  // Apply animation pattern
  switch (animation_pattern) {
  case 0: // Rainbow wave
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      float hue = progress + (float)i / CHESS_LED_COUNT_TOTAL;
      hue = fmod(hue, 1.0f);

      // Convert HSV to RGB (simplified)
      uint8_t red, green, blue;
      if (hue < 0.33f) {
        red = 255;
        green = (uint8_t)(hue * 3.0f * 255);
        blue = 0;
      } else if (hue < 0.66f) {
        red = (uint8_t)((0.66f - hue) * 3.0f * 255);
        green = 255;
        blue = 0;
      } else {
        red = 0;
        green = (uint8_t)((1.0f - hue) * 3.0f * 255);
        blue = 255;
      }

      led_set_pixel_internal(i, red, green, blue);
    }
    break;

  case 1: // Breathing effect
  {
    float intensity = (sinf(progress * 2 * M_PI) + 1.0f) / 2.0f;
    uint8_t level = (uint8_t)(intensity * 255);

    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      uint32_t base_color = led_states[i];
      uint8_t red = ((base_color >> 16) & 0xFF) * level / 255;
      uint8_t green = ((base_color >> 8) & 0xFF) * level / 255;
      uint8_t blue = (base_color & 0xFF) * level / 255;

      led_set_pixel_internal(i, red, green, blue);
    }
  } break;

  default:
    // Default animation: fade in/out
    {
      float intensity = (sinf(progress * 2 * M_PI) + 1.0f) / 2.0f;
      uint8_t level = (uint8_t)(intensity * 255);

      led_set_all_internal(level, level, level);
    }
    break;
  }
}

// ============================================================================
// SMART LED REPORTING FUNCTIONS
// ============================================================================

/**
 * @brief Print compact LED status showing only lit LEDs
 */
void led_print_compact_status(void) {
  if (quiet_period_active) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (current_time - quiet_period_start < quiet_period_duration) {
      ESP_LOGI(TAG, "LED: Quiet period active (%" PRIu32 " ms remaining)",
               quiet_period_duration - (current_time - quiet_period_start));
      return;
    }
    quiet_period_active = false;
  }

  // Count active LEDs by type
  uint32_t board_leds_count[8] = {0}; // Color counts for board LEDs
  uint32_t button_leds_active = 0;
  uint32_t total_active = 0;

  // Analyze board LEDs (0-63)
  for (int i = 0; i < 64; i++) {
    if (led_states[i] != 0) {
      total_active++;
      uint32_t color = led_states[i];
      uint8_t red = (color >> 16) & 0xFF;
      uint8_t green = (color >> 8) & 0xFF;
      uint8_t blue = color & 0xFF;

      // Determine color category
      if (red == 0 && green == 0 && blue == 0) {
        board_leds_count[0]++; // Black
      } else if (red == 0 && green == 0 && blue > 0) {
        board_leds_count[1]++; // Blue
      } else if (red == 0 && green > 0 && blue == 0) {
        board_leds_count[2]++; // Green
      } else if (red == 0 && green > 0 && blue > 0) {
        board_leds_count[3]++; // Cyan
      } else if (red > 0 && green == 0 && blue == 0) {
        board_leds_count[4]++; // Red
      } else if (red > 0 && green == 0 && blue > 0) {
        board_leds_count[5]++; // Magenta
      } else if (red > 0 && green > 0 && blue == 0) {
        board_leds_count[6]++; // Yellow
      } else if (red > 0 && green > 0 && blue > 0) {
        board_leds_count[7]++; // White
      }
    }
  }

  // Analyze button LEDs (64-72)
  char button_status[128] = "";
  int button_pos = 0;
  for (int i = 64; i < 73; i++) {
    if (led_states[i] != 0) {
      button_leds_active++;
      total_active++;

      // Get button color name
      uint32_t color = led_states[i];
      uint8_t red = (color >> 16) & 0xFF;
      uint8_t green = (color >> 8) & 0xFF;
      uint8_t blue = color & 0xFF;

      const char *color_name = "Unknown";
      if (red == 0xFF && green == 0x00 && blue == 0xFF)
        color_name = "Mag";
      else if (red == 0xFF && green == 0x00 && blue == 0x00)
        color_name = "Red";
      else if (red == 0x00 && green == 0xFF && blue == 0xFF)
        color_name = "Cya";
      else if (red == 0xFF && green == 0x80 && blue == 0x00)
        color_name = "Ora";
      else if (red == 0x00 && green == 0xFF && blue == 0x00)
        color_name = "Gre";
      else if (red == 0x00 && green == 0x00 && blue == 0xFF)
        color_name = "Blu";
      else if (red == 0xFF && green == 0xFF && blue == 0x00)
        color_name = "Yel";
      else if (red == 0xFF && green == 0xFF && blue == 0xFF)
        color_name = "Whi";

      // Add to button status string
      if (button_pos > 0)
        button_pos += snprintf(button_status + button_pos,
                               sizeof(button_status) - button_pos, " ");
      button_pos += snprintf(button_status + button_pos,
                             sizeof(button_status) - button_pos, "%s=%s",
                             button_names[i - 64], color_name);
    }
  }

  // Print compact status
  if (total_active == 0) {
    ESP_LOGI(TAG, "LED: All LEDs off");
    return;
  }

  // Board LEDs summary
  char board_summary[256] = "";
  int board_pos = 0;
  for (int i = 0; i < 8; i++) {
    if (board_leds_count[i] > 0) {
      if (board_pos > 0)
        board_pos += snprintf(board_summary + board_pos,
                              sizeof(board_summary) - board_pos, ", ");
      board_pos +=
          snprintf(board_summary + board_pos, sizeof(board_summary) - board_pos,
                   "%" PRIu32 "x %s", board_leds_count[i], color_names[i]);
    }
  }

  // Final compact output
  if (button_leds_active > 0) {
    ESP_LOGI(TAG, "LED: Board: %s | Buttons: %s | Total: %" PRIu32 " active",
             board_summary, button_status, total_active);
  } else {
    ESP_LOGI(TAG, "LED: Board: %s | Total: %" PRIu32 " active", board_summary,
             total_active);
  }

  // Update tracking
  last_status_report_time = esp_timer_get_time() / 1000;
  memcpy(previous_led_states, led_states, sizeof(led_states));
  led_states_initialized = true;
}

/**
 * @brief Print detailed LED status showing all LED states
 */
void led_print_detailed_status(void) {
  ESP_LOGI(TAG, "=== Detailed LED Status ===");
  ESP_LOGI(TAG, "Total LEDs: %d (64 board + 9 buttons)", CHESS_LED_COUNT_TOTAL);

  // Board LEDs
  ESP_LOGI(TAG, "Board LEDs (0-63):");
  for (int row = 7; row >= 0; row--) {
    char row_status[256] = "";
    int pos = 0;
    for (int col = 0; col < 8; col++) {
      int led_index = chess_pos_to_led_index(row, col);
      uint32_t color = led_states[led_index];
      if (color != 0) {
        uint8_t red = (color >> 16) & 0xFF;
        uint8_t green = (color >> 8) & 0xFF;
        uint8_t blue = color & 0xFF;
        pos += snprintf(row_status + pos, sizeof(row_status) - pos,
                        "%d=R(%d,%d,%d) ", led_index, red, green, blue);
      }
    }
    if (pos > 0) {
      ESP_LOGI(TAG, "  Row %d: %s", 8 - row, row_status);
    }
  }

  // Button LEDs
  ESP_LOGI(TAG, "Button LEDs (64-72):");
  for (int i = 64; i < 73; i++) {
    if (led_states[i] != 0) {
      uint32_t color = led_states[i];
      uint8_t red = (color >> 16) & 0xFF;
      uint8_t green = (color >> 8) & 0xFF;
      uint8_t blue = color & 0xFF;
      ESP_LOGI(TAG, "  %d (%s): R(%d,%d,%d)", i, button_names[i - 64], red,
               green, blue);
    }
  }

  ESP_LOGI(TAG, "=== End LED Status ===");
}

/**
 * @brief Send LED status to UART task immediately without delays (for real-time
 * response)
 */
static void led_send_status_to_uart_immediate(QueueHandle_t response_queue) {
  ESP_LOGI(TAG,
           "🔍 led_send_status_to_uart_immediate called (STREAMING OPTIMIZED)");
  if (response_queue == NULL) {
    ESP_LOGW(TAG, "No response queue available for LED status");
    return;
  }

  // Reset WDT before starting operation (only if registered)
  esp_err_t wdt_ret = led_task_wdt_reset_safe();
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
    // Task not registered with TWDT yet - this is normal during startup
  }

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(1536)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG,
           "📡 Using streaming output for immediate LED status (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret = streaming_set_queue_output(response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
  // Stream LED status header
  stream_writeln("🔍 LED Board Status (Real-time)");
  stream_writeln(
      "═══════════════════════════════════════════════════════════════");
  stream_writeln("📊 Board LEDs (64) - Chessboard Layout:");

  // Stream board LEDs in chessboard layout (8x8)
  for (int row = 7; row >= 0; row--) { // Start from row 8 (top)
    // Reset watchdog every row to prevent timeouts (only if registered)
    wdt_ret = led_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    // Row header
    stream_printf("  %d │ ", row + 1);

    for (int col = 0; col < 8; col++) {
      int led_index =
          chess_pos_to_led_index(7 - row, col); // Convert to LED index
      uint32_t color = led_states[led_index];
      uint8_t red = (color >> 16) & 0xFF;
      uint8_t green = (color >> 8) & 0xFF;
      uint8_t blue = color & 0xFF;

      // Convert to chess notation
      char square[4];
      snprintf(square, sizeof(square), "%c%d", 'a' + col, row + 1);

      // Get ANSI color for square name and RGB values
      const char *ansi_color = get_ansi_color_from_rgb(red, green, blue);

      // Display square with colored name and simplified RGB values
      stream_printf("%s%s\033[0m:%s(%d,%d,%d)\033[0m ", ansi_color, square,
                    ansi_color, red, green, blue);
    }

    // End of row
    stream_writeln("");
  }

  // Stream column headers
  stream_printf("    └─");
  for (int col = 0; col < 8; col++) {
    stream_printf("───");
  }
  stream_writeln("");

  // Stream column labels
  stream_printf("     ");
  for (int col = 0; col < 8; col++) {
    stream_printf("  %c  ", 'a' + col);
  }
  stream_writeln("");

  // Stream button LEDs (64-72)
  stream_writeln("\n📊 Button LEDs (9):");

  const char *button_names[] = {"NEW_GAME", "SAVE",     "LOAD",
                                "UNDO",     "REDO",     "HINT",
                                "ANALYZE",  "SETTINGS", "HELP"};

  for (int i = 64; i < 73; i++) {
    uint32_t color = led_states[i];
    uint8_t red = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;

    // Get ANSI color for button name and RGB values
    const char *ansi_color = get_ansi_color_from_rgb(red, green, blue);

    stream_printf("  %d (%s%s\033[0m): %sR(%d,%d,%d)\033[0m\n", i, ansi_color,
                  button_names[i - 64], ansi_color, red, green, blue);
  }

  // Send LED data to UART task as single response (no chunking for immediate
  // response) MEMORY OPTIMIZATION: Streaming output handles data transmission
  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ LED status streaming completed immediately");

  // Final WDT reset (only if registered)
  wdt_ret = led_task_wdt_reset_safe();
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
    // Task not registered with TWDT yet - this is normal during startup
  }

  // Cleanup hardware on task exit
  led_hardware_cleanup();
}

// ============================================================================
// GAME STATE INTEGRATION FUNCTIONS
// ============================================================================

/**
 * @brief Update LEDs based on current game state
 */
void led_update_game_state(void) {
  ESP_LOGI(TAG, "🎮 Updating LEDs based on current game state");

  // Clear all board highlights first
  led_clear_all_highlights();

  // Highlight pieces that can move for current player
  led_highlight_pieces_that_can_move();

  ESP_LOGI(TAG, "✅ Game state LED update completed");
}

/**
 * @brief Highlight pieces that can move for current player
 */
void led_highlight_pieces_that_can_move(void) {
  // Check if system is booting - if so, DO NOT highlight anything
  if (led_is_booting()) {
    ESP_LOGW(TAG, "⚠️ Attempted to highlight pieces during BOOT - blocked");
    return;
  }

  ESP_LOGI(TAG, "🎯 Highlighting pieces that can move");

  // Clear all highlights first
  led_clear_all_highlights();

  // Use real game logic instead of simulation
  // We call game_highlight_movable_pieces() from game_task.c
  extern void game_highlight_movable_pieces(void);
  game_highlight_movable_pieces();

  ESP_LOGI(TAG,
           "✅ Called game_highlight_movable_pieces() for real highlighting");
}

/**
 * @brief Highlight possible moves for selected piece
 * @param from_square Square index (0-63) of the selected piece
 */
void led_highlight_possible_moves(uint8_t from_square) {
  if (from_square >= 64) {
    ESP_LOGE(TAG, "Invalid square index: %d", from_square);
    return;
  }

  ESP_LOGI(TAG, "🎯 Highlighting possible moves from square %d", from_square);

  // Clear previous highlights
  led_clear_all_highlights();

  // Highlight source square in yellow
  led_set_pixel_internal(from_square, 255, 255, 0);

  // Simulate highlighting some possible moves
  // In real implementation, this would query game_task for valid moves
  uint8_t example_moves[] = {from_square + 8,
                             from_square + 16}; // Forward moves for pawns

  for (int i = 0; i < 2; i++) {
    uint8_t dest_square = example_moves[i];
    if (dest_square < 64) {
      // Highlight possible destinations in green
      led_set_pixel_internal(dest_square, 0, 255, 0);
    }
  }

  ESP_LOGI(TAG, "✅ Highlighted possible moves from square %d", from_square);
}

/**
 * @brief Clear all board highlights and return to chess board pattern
 */
void led_clear_all_highlights(void) {
  ESP_LOGI(TAG, "🧹 Clearing all board highlights");

  // Clear all board LEDs (0-63) - they should be off by default
  for (int i = 0; i < 64; i++) {
    led_states[i] = 0x000000; // All off
  }

  ESP_LOGI(TAG, "✅ All board highlights cleared");
}

// ============================================================================
// HARDWARE INTEGRATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize RMT hardware for WS2812B LED control
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t led_hardware_init(void) {
  ESP_LOGI(
      TAG,
      "Initializing SK6812MINI RGBW strip (GRBW)...");

  /*
   * OPSCO SK6812MINIRGBW T1H is 0.65–1.00 µs (typical 0.75). The IDF
   * SK6812 preset uses T1H 0.6 µs, under that minimum. WS2812 timing in
   * this driver is T0H 0.3 / T1H 0.9 µs, inside the window, and the reset
   * is 280 µs (datasheet wants >200 µs). Byte order is GRBW, 4 bytes.
   */
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_DATA_PIN,
      .max_leds = CHESS_LED_COUNT_TOTAL,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
  };

  // RMT BACKEND CONFIGURATION - Optimized for WS2812B timing
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz =
          10 * 1000 * 1000,     // 10MHz resolution for precise WS2812B timing
      .mem_block_symbols = 128, // More buffer for stability
      .flags =
          {
              .with_dma = false, // ESP32-C6 doesn't support DMA for RMT
          },
  };

  // CREATE LED STRIP OBJECT
  esp_err_t ret =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ LED strip creation failed: %s", esp_err_to_name(ret));
    led_strip = NULL; // Ensure handle is NULL on failure
    return ret;
  }

  // Verify LED strip handle is valid
  if (led_strip == NULL) {
    ESP_LOGE(TAG, "❌ LED strip handle is NULL after creation");
    return ESP_ERR_INVALID_STATE;
  }

  // CLEAR ALL LEDS, then one refresh of that finished black frame.
  ret = led_strip_clear(led_strip);
  if (ret == ESP_OK) {
    ret = led_strip_refresh(led_strip);
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ LED strip clear failed: %s", esp_err_to_name(ret));
    led_strip_del(led_strip);
    return ret;
  }

  led_initialized = true;

  // Initialize batch update system
  led_changes_pending = false;
  memset(led_changed_flags, false, sizeof(led_changed_flags));
  memset(led_pending_changes, 0, sizeof(led_pending_changes));

  ESP_LOGI(TAG, "LED pixel format GRBW, 4 bytes per LED");
  ESP_LOGI(TAG, "  • GPIO: %d", LED_DATA_PIN);
  ESP_LOGI(TAG, "  • LEDs: %d total", CHESS_LED_COUNT_TOTAL);
  ESP_LOGI(TAG, "  • Driver: espressif/led_strip, led_strip_set_pixel_rgbw");
  ESP_LOGI(TAG, "  • Batch update system: initialized");

  return ESP_OK;
}

// ============================================================================
// OBSOLETE FUNCTION - REMOVED
// ============================================================================

/**
 * @brief OBSOLETE: Hardware update function no longer needed
 * All LED updates now happen immediately via led_set_pixel_internal()
 * This function kept only for compatibility - does nothing useful
 */
// static void led_hardware_update(void) - REMOVED

/**
 * @brief Cleanup hardware resources
 */
static void led_hardware_cleanup(void) {
  if (led_initialized && led_strip) {
    ESP_LOGI(TAG, "🧹 Cleaning up WS2812B hardware...");
    led_strip_clear(led_strip);
    led_strip_del(led_strip);
    led_strip = NULL;
    led_initialized = false;
    ESP_LOGI(TAG, "✅ Hardware cleanup completed");
  }
}

/**
 * @brief Print only LED changes since last status
 */
void led_print_changes_only(void) {
  if (!led_states_initialized) {
    ESP_LOGI(TAG, "LED: No previous state to compare");
    return;
  }

  bool changes_found = false;
  char changes_summary[512] = "";
  int pos = 0;

  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    if (led_states[i] != previous_led_states[i]) {
      changes_found = true;

      uint32_t old_color = previous_led_states[i];
      uint32_t new_color = led_states[i];

      uint8_t old_red = (old_color >> 16) & 0xFF;
      uint8_t old_green = (old_color >> 8) & 0xFF;
      uint8_t old_blue = old_color & 0xFF;

      uint8_t new_red = (new_color >> 16) & 0xFF;
      uint8_t new_green = (new_color >> 8) & 0xFF;
      uint8_t new_blue = (new_color & 0xFF);

      if (pos < sizeof(changes_summary) - 100) {
        if (i < 64) {
          // Board LED
          pos += snprintf(changes_summary + pos, sizeof(changes_summary) - pos,
                          "%d:B(%d,%d,%d)->(%d,%d,%d) ", i, old_red, old_green,
                          old_blue, new_red, new_green, new_blue);
        } else {
          // Button LED
          pos += snprintf(changes_summary + pos, sizeof(changes_summary) - pos,
                          "%d:%s(%d,%d,%d)->(%d,%d,%d) ", i,
                          button_names[i - 64], old_red, old_green, old_blue,
                          new_red, new_green, new_blue);
        }
      }
    }
  }

  if (changes_found) {
    ESP_LOGI(TAG, "LED Changes: %s", changes_summary);
    led_changes_count++;
  } else {
    ESP_LOGI(TAG, "LED: No changes detected");
  }
}

/**
 * @brief Start quiet period (suppress LED status reports)
 */
void led_start_quiet_period(uint32_t duration_ms) {
  quiet_period_active = true;
  quiet_period_start = esp_timer_get_time() / 1000;
  quiet_period_duration = duration_ms;
  ESP_LOGI(TAG, "LED: Quiet period started (%" PRIu32 " ms)", duration_ms);
}

/**
 * @brief Check if LED states have changed significantly
 */
bool led_has_significant_changes(void) {
  if (!led_states_initialized)
    return true;

  uint32_t changed_count = 0;
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    if (led_states[i] != previous_led_states[i]) {
      changed_count++;
      if (changed_count > 5)
        return true; // More than 5 LEDs changed
    }
  }

  return changed_count > 0;
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

static bool s_forced_blackout;

void led_set_forced_blackout(bool on) { s_forced_blackout = on; }

void led_task_start(void *pvParameters) {
  led_task_handle = xTaskGetCurrentTaskHandle();
  ESP_LOGI(TAG, "🚀 LED task starting...");

  // CRITICAL: Register with TWDT from within task
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGE(TAG, "Failed to register LED task with TWDT: %s",
             esp_err_to_name(wdt_ret));
  } else {
    ESP_LOGI(TAG, "✅ LED task registered with TWDT");
  }

  // Initialize hardware
  esp_err_t hw_ret = led_hardware_init();
  if (hw_ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Hardware initialization failed: %s",
             esp_err_to_name(hw_ret));
    ESP_LOGE(TAG, "❌ LED strip handle is NULL - switching to simulation mode");
    // Continue in simulation mode
    simulation_mode = true;
    led_initialized = false;
    ESP_LOGW(
        TAG,
        "⚠️ Continuing in simulation mode - LED commands will be logged only");
  } else {
    ESP_LOGI(TAG, "✅ Hardware initialized successfully");
    simulation_mode = false;
    led_initialized = true;
  }

  // BOOTING ANIMATION: Controlled by main.c (not here!)
  // BACKUP FIX: Animation controlled from main.c via led_boot_animation_step()
  // If this is uncommented, led_task will block for 2-3s and main.c animation
  // nefunguje
  ESP_LOGI(TAG, "🌟 Boot animation ready (controlled by main.c)...");
  // led_booting_animation(); // ← COMMENTED as in backup!
  ESP_LOGI(TAG, "LED task started successfully (%s)",
           simulation_mode ? "SIMULATION MODE" : "HARDWARE MODE");
  ESP_LOGI(TAG, "Features:");
  ESP_LOGI(
      TAG,
      "  • WS2812B %s: 73 LEDs (64 board + 9 buttons: 8 promotion + 1 reset)",
      simulation_mode ? "simulation" : "hardware");
  ESP_LOGI(TAG, "  • Chess board pattern: alternating black/white squares");
  ESP_LOGI(TAG, "  • Button LED feedback: availability-based colors");
  ESP_LOGI(TAG, "  • Animation support: rainbow wave, breathing, fade");
  ESP_LOGI(TAG, "  • Command queue processing: LED commands from other tasks");
  ESP_LOGI(TAG, "  • Time-multiplexed updates: 5ms cycle");

  ESP_LOGI(TAG, "🔄 Initializing LED states...");
  task_running = true;

  // Initialize LED unified mutex for synchronization
  led_unified_mutex = xSemaphoreCreateMutex();
  if (led_unified_mutex == NULL) {
    ESP_LOGE(TAG, "❌ CRITICAL: Failed to create LED unified mutex!");
    return;
  }
  ESP_LOGI(TAG, "✅ LED unified mutex created");

  /* Stay at 100%. A stored NVS level must not dim the strip at boot. */
  global_brightness = 100;
  ESP_LOGI(TAG, "Global brightness: 100%%");

  // Initialize duration management system
  led_init_duration_system();

  // Initialize unified animation manager for non-blocking endgame animations
  animation_config_t anim_config = {.max_concurrent_animations = 8,
                                    .update_frequency_hz =
                                        30, // 30 FPS for smooth animations
                                    .enable_smooth_interpolation = true,
                                    .enable_trail_effects = true,
                                    .default_duration_ms = 1000};
  esp_err_t anim_ret = animation_manager_init(&anim_config);
  if (anim_ret != ESP_OK) {
    ESP_LOGW(TAG, "⚠️ Animation manager init failed: %s",
             esp_err_to_name(anim_ret));
  } else {
    ESP_LOGI(TAG, "✅ Animation manager initialized");
  }

  // Initialize LED states
  ESP_LOGI(TAG, "🔄 Clearing all LEDs...");
  led_clear_all_internal();
  ESP_LOGI(TAG, "✅ LED initialization complete, board stays dark until boot ends");

  // Main task loop
  uint32_t loop_count = 0;
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    // Reset watchdog - only if task is registered with WDT
    esp_err_t wdt_ret = led_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    // Watchdog logging every 1000 iterations (5 seconds)
    if (loop_count % 1000 == 0) {
      ESP_LOGI(TAG, "LED Task Watchdog: loop=%" PRIu32 ", heap=%" PRIu32,
               loop_count, esp_get_free_heap_size());
    }

    // Process LED commands from queue
    led_frame_begin();
    led_game_events_report_overlay();
    led_game_events_process();
    led_process_commands();

    /* Game animations paint and refresh their own frames. Drawing the board
     * again here mixes the two pictures onto the strip. */
    if (s_board_show == 0) {
      led_update_animation();
      led_update_endgame_wave();
      if (!led_board_show_active()) {
        led_board_restore_guide_tick();
        led_required_step_guide_tick();
      }
      led_process_duration_expirations();
      led_gesture_anim_tick();
    }

    led_process_button_blink_timers();
    if (s_forced_blackout) {
      led_clear_all_internal();
    }
    led_frame_end();

    // One finished frame. Nothing above is allowed to refresh the strip.
    led_privileged_batch_commit();

    // Update hardware (simulation mode)

    // Periodic status update - reduced frequency for cleaner UART
    if (loop_count % 10000 == 0) { // Every 10000 loops (50 seconds)
      ESP_LOGI(TAG, "LED Task Status: loop=%" PRIu32 ", animation=%s", loop_count,
               animation_active ? "active" : "inactive");
    }

    loop_count++;

    /* 33 ms cadence, or sooner when a picture closes (lift snap). */
    {
      uint32_t woke = 0;
      TickType_t now_tick = xTaskGetTickCount();
      TickType_t step = pdMS_TO_TICKS(led_visual_busy() ? 16 : 33);
      TickType_t next_tick = last_wake_time + step;
      if (next_tick > now_tick) {
        (void)xTaskNotifyWait(0, UINT32_MAX, &woke, next_tick - now_tick);
      } else {
        (void)xTaskNotifyWait(0, UINT32_MAX, &woke, 0);
      }
      if (woke != 0U) {
        last_wake_time = xTaskGetTickCount();
      } else {
        last_wake_time = next_tick;
      }
    }
  }
}

// Global endgame animation state
static bool endgame_animation_active = false;
// ============================================================================
// ADVANCED CHESS ANIMATION IMPLEMENTATIONS
// ============================================================================

static float led_smoothstepf(float t) {
  if (t < 0.f) {
    return 0.f;
  }
  if (t > 1.f) {
    return 1.f;
  }
  return t * t * (3.f - 2.f * t);
}

/** Slide a color across the four squares around a fractional board point. */
static void led_blend_add(uint16_t acc[64][3], float row, float col, float red,
                          float green, float blue) {
  if (row < 0.f) {
    row = 0.f;
  } else if (row > 7.f) {
    row = 7.f;
  }
  if (col < 0.f) {
    col = 0.f;
  } else if (col > 7.f) {
    col = 7.f;
  }
  int row0 = (int)row;
  int col0 = (int)col;
  float fr = row - (float)row0;
  float fc = col - (float)col0;
  for (int dr = 0; dr <= 1; dr++) {
    for (int dc = 0; dc <= 1; dc++) {
      int rr = row0 + dr;
      int cc = col0 + dc;
      if (rr > 7 || cc > 7) {
        continue;
      }
      float w = (dr ? fr : (1.f - fr)) * (dc ? fc : (1.f - fc));
      if (w <= 0.001f) {
        continue;
      }
      uint8_t led = chess_pos_to_led_index((uint8_t)rr, (uint8_t)cc);
      acc[led][0] = (uint16_t)fminf(255.f, (float)acc[led][0] + red * w);
      acc[led][1] = (uint16_t)fminf(255.f, (float)acc[led][1] + green * w);
      acc[led][2] = (uint16_t)fminf(255.f, (float)acc[led][2] + blue * w);
    }
  }
}

static void led_blend_flush(uint16_t acc[64][3]) {
  led_frame_begin();
  led_clear_board_only();
  for (int i = 0; i < 64; i++) {
    if (acc[i][0] == 0 && acc[i][1] == 0 && acc[i][2] == 0) {
      continue;
    }
    led_set_pixel_safe((uint8_t)i, (uint8_t)acc[i][0], (uint8_t)acc[i][1],
                       (uint8_t)acc[i][2]);
  }
  led_frame_end();
  led_force_immediate_update();
}

void led_anim_move_path(const led_command_t *cmd) {
  uint8_t from_led;
  uint8_t to_led;
  uint8_t from_row;
  uint8_t from_col;
  uint8_t to_row;
  uint8_t to_col;
  if (!cmd || cmd->led_index >= 64) {
    return;
  }
  from_led = cmd->led_index;
  to_led = (cmd->data ? *((uint8_t *)cmd->data) : 63);
  if (to_led >= 64) {
    return;
  }
  led_index_to_chess_pos(from_led, &from_row, &from_col);
  led_index_to_chess_pos(to_led, &to_row, &to_col);
  ESP_LOGI(TAG, "🎬 Move trail: %d -> %d", from_led, to_led);
  led_move_trail_start(from_row, from_col, to_row, to_col);
}

void led_anim_castle(const led_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏰 Starting castling animation");

  // Get castling data from command
  uint8_t king_from = cmd->led_index;
  uint8_t king_to = (cmd->data ? *((uint8_t *)cmd->data) : king_from + 2);

  /* Rook squares from the chess rank. LED indices are not linear on a rank. */
  uint8_t king_from_row, king_from_col, king_to_row, king_to_col;
  led_index_to_chess_pos(king_from, &king_from_row, &king_from_col);
  led_index_to_chess_pos(king_to, &king_to_row, &king_to_col);
  if (king_from_row != king_to_row) {
    return;
  }
  bool kingside = king_to_col > king_from_col;
  uint8_t rook_from =
      chess_pos_to_led_index(king_from_row, kingside ? 7 : 0);
  uint8_t rook_to = chess_pos_to_led_index(king_from_row, kingside ? 5 : 3);

  // ED: Ensure valid LED indices
  if (rook_from >= 64)
    rook_from = 63;
  if (rook_to >= 64)
    rook_to = 63;

  // Validate LED indices
  if (king_from >= 64 || king_to >= 64 || rook_from >= 64 || rook_to >= 64) {
    ESP_LOGE(TAG,
             "❌ Invalid LED indices: king_from=%d, king_to=%d, rook_from=%d, "
             "rook_to=%d",
             king_from, king_to, rook_from, rook_to);
    return;
  }

  uint8_t rook_from_row, rook_from_col, rook_to_row, rook_to_col;
  led_index_to_chess_pos(rook_from, &rook_from_row, &rook_from_col);
  led_index_to_chess_pos(rook_to, &rook_to_row, &rook_to_col);

  static uint16_t acc[64][3];
  const int frames = 12;
  for (int frame = 0; frame < frames; frame++) {
    memset(acc, 0, sizeof(acc));
    float progress = (float)frame / (float)(frames - 1);
    for (int trail = 0; trail < 3; trail++) {
      float trail_progress = progress - (trail * 0.14f);
      if (trail_progress < 0.f) {
        continue;
      }
      if (trail_progress > 1.f) {
        break;
      }
      float eased = led_smoothstepf(trail_progress);
      float bright = fmaxf(0.f, 1.f - (trail * 0.28f));
      float king_row = (float)king_from_row +
                       ((float)king_to_row - (float)king_from_row) * eased;
      float king_col = (float)king_from_col +
                       ((float)king_to_col - (float)king_from_col) * eased;
      float rook_row = (float)rook_from_row +
                       ((float)rook_to_row - (float)rook_from_row) * eased;
      float rook_col = (float)rook_from_col +
                       ((float)rook_to_col - (float)rook_from_col) * eased;
      led_blend_add(acc, king_row, king_col, 255.f * bright, 215.f * bright,
                    0.f);
      led_blend_add(acc, rook_row, rook_col, 192.f * bright, 192.f * bright,
                    192.f * bright);
    }
    led_blend_flush(acc);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  const int breaths = 5;
  for (int breath = 0; breath < breaths; breath++) {
    float t = (float)breath / (float)(breaths - 1);
    float level = sinf(t * (float)M_PI);
    led_frame_begin();
    led_clear_board_only();
    led_set_pixel_safe(king_to, (uint8_t)(255.f * level),
                       (uint8_t)(215.f * level), 0);
    led_set_pixel_safe(rook_to, (uint8_t)(192.f * level),
                       (uint8_t)(192.f * level), (uint8_t)(192.f * level));
    led_frame_end();
    led_force_immediate_update();
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  led_clear_board_only();
  led_force_immediate_update();
  ESP_LOGI(TAG, "🏰 Castling animation completed");
}

void led_anim_promote(const led_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "👑 Starting NON-BLOCKING promotion animation");

  uint8_t promotion_led = cmd->led_index;

  // Non-blocking promotion animation using unified_animation_manager
  uint32_t anim_id =
      unified_animation_create(ANIM_TYPE_PROMOTION, ANIM_PRIORITY_HIGH);
  if (anim_id != 0) {
    esp_err_t ret = animation_start_promotion(anim_id, promotion_led);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "✅ Non-blocking promotion animation started (ID: %lu)",
               anim_id);
    } else {
      ESP_LOGE(TAG, "❌ Failed to start promotion animation: %s",
               esp_err_to_name(ret));
    }
  } else {
    ESP_LOGE(TAG, "❌ Failed to create promotion animation");
  }
}

/**
 * @brief Booting animation - progressive LED illumination
 */
static bool led_booting_active = true;

bool led_is_booting(void) { return led_booting_active; }

/**
 * @brief Booting animation - progressive LED illumination
 */
void led_booting_animation(void) {
  led_booting_active = true;
  ESP_LOGI(TAG, "🌟 Starting booting animation...");

  // Progressive illumination of board LEDs (0-63) - start at 20% for visibility
  for (int brightness = 20; brightness <= 100; brightness += 5) {
    led_frame_begin();
    for (int led = 0; led < 64; led++) {
      // Calculate color based on position
      uint8_t row = led / 8;
      uint8_t col = led % 8;

      // Chess board pattern colors
      uint8_t r, g, b;
      if ((row + col) % 2 == 0) {
        // Light squares - warm white
        r = (uint8_t)(255 * brightness / 100);
        g = (uint8_t)(240 * brightness / 100);
        b = (uint8_t)(200 * brightness / 100);
      } else {
        // Dark squares - cool white
        r = (uint8_t)(200 * brightness / 100);
        g = (uint8_t)(220 * brightness / 100);
        b = (uint8_t)(255 * brightness / 100);
      }

      led_set_pixel_internal(led, r, g, b);
    }

    // Pulse buttons green (indices 64-72)
    uint8_t btn_brightness = (uint8_t)(255 * brightness / 100);
    for (int btn = 64; btn < 73; btn++) {
      led_set_pixel_internal(btn, 0, btn_brightness, 0); // Green pulse
    }
    led_frame_end();
    led_commit_pending_changes();
    vTaskDelay(pdMS_TO_TICKS(50));

    // Reset WDT during animation
    led_task_wdt_reset_safe();
  }

  // Brief pause at full brightness
  vTaskDelay(pdMS_TO_TICKS(500));

  // Fade out
  for (int brightness = 100; brightness >= 0; brightness -= 10) {
    led_frame_begin();
    for (int led = 0; led < 64; led++) {
      uint8_t row = led / 8;
      uint8_t col = led % 8;

      uint8_t r, g, b;
      if ((row + col) % 2 == 0) {
        r = (uint8_t)(255 * brightness / 100);
        g = (uint8_t)(240 * brightness / 100);
        b = (uint8_t)(200 * brightness / 100);
      } else {
        r = (uint8_t)(200 * brightness / 100);
        g = (uint8_t)(220 * brightness / 100);
        b = (uint8_t)(255 * brightness / 100);
      }

      led_set_pixel_internal(led, r, g, b);
    }

    // Fade out buttons too
    uint8_t btn_brightness = (uint8_t)(255 * brightness / 100);
    for (int btn = 64; btn < 73; btn++) {
      led_set_pixel_internal(btn, 0, btn_brightness, 0);
    }
    led_frame_end();
    led_commit_pending_changes();
    vTaskDelay(pdMS_TO_TICKS(30));
    led_task_wdt_reset_safe();
  }

  // Clear board LEDs
  led_frame_begin();
  led_clear_board_only();

  // Set default button state (dim blue) so they are not dark
  // Game logic will update this shortly
  for (int btn = 64; btn < 73; btn++) {
    led_set_pixel_internal(btn, 0, 0, 32);
  }
  led_frame_end();
  led_commit_pending_changes();

  // NOW clear boot flag - animation is fully done (including fade-out)
  led_booting_active = false;
  ESP_LOGI(TAG, "🌟 Booting animation completed - boot flag cleared");
}

// Endgame animation state structure (based on old project)
typedef struct {
  bool active;
  uint8_t win_king_led;
  uint8_t win_king_row;
  uint8_t win_king_col;
  uint8_t lose_king_row;
  uint8_t lose_king_col;
  float radius;
  float reveal;
  uint32_t last_update;
  bool initialized;
  bool centre;
  uint8_t reason;
  uint8_t loops;
  piece_t winner_piece; // Store the actual winner piece for proper detection
} endgame_wave_state_t;

static endgame_wave_state_t endgame_wave = {0};

bool led_board_show_active(void) {
  return s_board_show > 0 || endgame_wave.active;
}

/**
 * @brief Initialize AVR-style wave endgame animation (according to old project)
 */
static bool endgame_draw_reason(uint8_t reason) {
  return reason == 10U || reason == 11U || reason == 12U || reason == 13U ||
         reason == 22U;
}

void led_anim_endgame(const led_command_t *cmd) {
  uint8_t sq;
  uint8_t row;
  uint8_t col;
  if (!cmd) {
    return;
  }

  ESP_LOGI(TAG, "🏆 Starting AVR-style wave endgame animation");

  /* Callers pass a row-major square (rank*8+file), not a chain LED index. */
  sq = cmd->led_index;
  if (sq >= 64U) {
    sq = 28U;
  }
  row = (uint8_t)(sq / 8U);
  col = (uint8_t)(sq % 8U);
  endgame_wave.reason = game_endgame_reason();
  endgame_wave.centre = endgame_draw_reason(endgame_wave.reason);
  if (endgame_wave.centre) {
    endgame_wave.win_king_row = 3U;
    endgame_wave.win_king_col = 3U;
    endgame_wave.win_king_led = chess_pos_to_led_index(3U, 3U);
    endgame_wave.winner_piece = PIECE_EMPTY;
  } else {
    endgame_wave.win_king_row = row;
    endgame_wave.win_king_col = col;
    endgame_wave.win_king_led = chess_pos_to_led_index(row, col);
    endgame_wave.winner_piece = game_session_snap_piece(row, col);
  }
  endgame_wave.lose_king_row = 0;
  endgame_wave.lose_king_col = 0;

  ESP_LOGI(TAG,
           "🎯 Wave origin (%d,%d) piece=%d centre=%d reason=%u",
           endgame_wave.win_king_row, endgame_wave.win_king_col,
           endgame_wave.winner_piece, (int)endgame_wave.centre,
           (unsigned)endgame_wave.reason);

  endgame_wave.loops = 0;
  endgame_wave.radius = 0.f;
  endgame_wave.reveal = 0.f;
  endgame_wave.last_update = (uint32_t)(esp_timer_get_time() / 1000);
  endgame_wave.active = true;
  endgame_wave.initialized = true;
  led_board_restore_guide_yield();
  led_required_step_guide_yield();

  // Set global endgame state
  endgame_animation_active = true;

  ESP_LOGI(TAG, "🌊 AVR-style wave endgame animation initialized");
}

/**
 * @brief Update enhanced wave endgame animation with improved colors, faster
 * animation and perfect piece highlighting (according to the old project)
 */
static float endgame_ring(float dist, float radius) {
  float d = dist - radius;
  return expf(-(d * d) / 1.8f);
}

static float endgame_amp(float radius) {
  if (radius < 0.f || radius > 16.f) {
    return 0.f;
  }
  if (radius < 1.6f) {
    return radius / 1.6f;
  }
  if (radius > 13.5f) {
    return (16.f - radius) / 2.5f;
  }
  return 1.f;
}

/* Radius 0.28 per 16 ms frame was one lap in ~914 ms. Half of that. */
#define ENDGAME_WAVE_CYCLE_MS 1829U
#define ENDGAME_WAVE_REVEAL_MS 400U

void led_update_endgame_wave(void) {
  uint32_t now;
  uint32_t dt;
  if (!endgame_wave.active || !endgame_wave.initialized) {
    return;
  }
  now = (uint32_t)(esp_timer_get_time() / 1000);
  if (game_ep_capture_waiting()) {
    endgame_wave.last_update = now;
    return;
  }
  dt = (uint32_t)(now - endgame_wave.last_update);
  endgame_wave.last_update = now;
  if (dt > 500U) {
    dt = 33U;
  }

  /* Two soft rings, half a cycle apart. Speed follows the clock, not the
   * LED loop period. */
  endgame_wave.radius += (16.f * (float)dt) / (float)ENDGAME_WAVE_CYCLE_MS;
  while (endgame_wave.radius >= 16.f) {
    endgame_wave.radius -= 16.f;
  }
  if (endgame_wave.reveal < 1.f) {
    endgame_wave.reveal += (float)dt / (float)ENDGAME_WAVE_REVEAL_MS;
    if (endgame_wave.reveal > 1.f) {
      endgame_wave.reveal = 1.f;
    }
  }

  float r0 = endgame_wave.radius;
  float r1 = r0 + 8.f;
  float origin_row;
  float origin_col;
  if (r1 >= 16.f) {
    r1 -= 16.f;
  }
  float a0 = endgame_amp(r0);
  float a1 = endgame_amp(r1);

  piece_t winner_king = endgame_wave.winner_piece;
  bool winner_is_white = (winner_king == PIECE_WHITE_KING);
  origin_row = endgame_wave.centre ? 3.5f : (float)endgame_wave.win_king_row;
  origin_col = endgame_wave.centre ? 3.5f : (float)endgame_wave.win_king_col;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      float dx = (float)col - origin_col;
      float dy = (float)row - origin_row;
      float dist = sqrtf(dx * dx + dy * dy);
      float intensity =
          fmaxf(endgame_ring(dist, r0) * a0, endgame_ring(dist, r1) * a1);
      intensity *= endgame_wave.reveal;
      if (intensity < 0.02f) {
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 0, 0, 0);
        continue;
      }
      if (intensity > 1.f) {
        intensity = 1.f;
      }

      piece_t piece = game_session_snap_piece(row, col);
      uint8_t red;
      uint8_t green;
      uint8_t blue;
      uint8_t reason = endgame_wave.reason;
      if (reason == 10 || reason == 11 || reason == 12 || reason == 13 ||
          reason == 20 || reason == 21 || reason == 22) {
        uint8_t cr = 180, cg = 180, cb = 180;
        if (reason == 11) {
          cr = 255;
          cg = 160;
          cb = 0;
        } else if (reason == 12) {
          cr = 40;
          cg = 80;
          cb = 255;
        } else if (reason == 13) {
          cr = 220;
          cg = 220;
          cb = 220;
        } else if (reason == 20) {
          cr = 255;
          cg = 0;
          cb = 0;
        } else if (reason == 21) {
          cr = 255;
          cg = 60;
          cb = 0;
        } else if (reason == 22) {
          cr = 200;
          cg = 220;
          cb = 255;
        }
        led_set_pixel_safe(chess_pos_to_led_index(row, col),
                           (uint8_t)(cr * intensity), (uint8_t)(cg * intensity),
                           (uint8_t)(cb * intensity));
        continue;
      }
      if (piece != PIECE_EMPTY) {
        bool opponent = winner_is_white
                            ? (piece >= PIECE_BLACK_PAWN &&
                               piece <= PIECE_BLACK_KING)
                            : (piece >= PIECE_WHITE_PAWN &&
                               piece <= PIECE_WHITE_KING);
        if (opponent) {
          red = (uint8_t)(255.f * intensity);
          green = (uint8_t)(30.f * intensity);
          blue = (uint8_t)(30.f * intensity);
        } else {
          red = (uint8_t)(30.f * intensity);
          green = (uint8_t)(255.f * intensity);
          blue = (uint8_t)(80.f * intensity);
        }
      } else {
        red = (uint8_t)(30.f * intensity);
        green = (uint8_t)(100.f * intensity);
        blue = (uint8_t)(255.f * intensity);
      }
      led_set_pixel_safe(chess_pos_to_led_index(row, col), red, green, blue);
    }
  }

  if (!endgame_wave.centre) {
    led_set_pixel_safe(endgame_wave.win_king_led,
                       (uint8_t)(255.f * endgame_wave.reveal),
                       (uint8_t)(215.f * endgame_wave.reveal), 0);
  }
}

/**
 * @brief Stop endless endgame animation
 */
void led_stop_endgame_animation(void) {
  ESP_LOGI(TAG, "🛑 Stopping endless endgame animation...");
  endgame_animation_active = false;

  // Zastavit endgame wave animaci
  endgame_wave.active = false;
  endgame_wave.initialized = false;
  endgame_wave.radius = 0;

  led_clear_board_only();
  ESP_LOGI(TAG, "✅ Endless endgame animation stopped");
}

/**
 * @brief Setup animation after endgame - reset pieces to starting positions
 */
void led_setup_animation_after_endgame(void) {
  ESP_LOGI(TAG,
           "🔄 Starting MATRIX-MONITORED setup animation after endgame...");

  // Step 1: Clear board
  led_clear_board_only();

  // Step 2: Show starting position setup
  ESP_LOGI(TAG, "🎯 Step 1: Setting up starting position");

  // White pieces (bottom)
  uint8_t white_pieces[] = {0, 1, 2,  3,  4,  5,  6,  7,
                            8, 9, 10, 11, 12, 13, 14, 15};
  for (int i = 0; i < 16; i++) {
    led_set_pixel_safe(white_pieces[i], 255, 255, 255); // White
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  // Black pieces (top)
  uint8_t black_pieces[] = {48, 49, 50, 51, 52, 53, 54, 55,
                            56, 57, 58, 59, 60, 61, 62, 63};
  for (int i = 0; i < 16; i++) {
    led_set_pixel_safe(black_pieces[i], 0, 0, 0); // Black (dark)
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  // Step 3: Wait for matrix monitoring to confirm pieces are in place
  ESP_LOGI(TAG,
           "⏳ Step 2: Waiting for matrix to confirm pieces are in place...");
  ESP_LOGI(TAG, "💡 Please place pieces on rows 1, 2, 7, 8 as shown by LEDs");

  // Keep showing the setup until matrix confirms all pieces are in place
  // This will be handled by the actual game logic when matrix scanning detects
  // pieces

  // Step 4: Show legal moves for white
  ESP_LOGI(TAG, "🎯 Step 3: Highlighting legal moves for white");
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Clear board and show legal moves
  led_clear_board_only();

  // Show some legal moves (example: pawn moves)
  uint8_t legal_moves[] = {16, 17, 18, 19, 20, 21, 22, 23,
                           24, 25, 26, 27, 28, 29, 30, 31};
  for (int i = 0; i < 16; i++) {
    led_set_pixel_safe(legal_moves[i], 0, 255, 0); // Green for legal moves
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  // Step 5: Final setup complete
  ESP_LOGI(
      TAG,
      "✅ MATRIX-MONITORED setup animation completed - ready for new game");
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Clear board
  led_clear_board_only();
}

void led_anim_check(const led_command_t *cmd) {
  if (!cmd)
    return;

  /* Pink on the king. Breathe; do not clear the rest of the board. */
  led_target_set(cmd->led_index, 255, 192, 203, LED_VIS_BREATHE);

  ESP_LOGI(TAG, "Check: pink breathe at king LED %u", (unsigned)cmd->led_index);
}

void led_anim_checkmate(const led_command_t *cmd) {
  if (!cmd)
    return;

  for (int flash = 0; flash < 8; flash++) {
    uint8_t red = 255;
    uint8_t green = (flash % 2 == 0) ? 0 : 255;
    uint8_t blue = (flash % 2 == 0) ? 0 : 255;
    led_frame_begin();
    for (int i = 0; i < 64; i++) {
      led_set_pixel_internal((uint8_t)i, red, green, blue);
    }
    led_frame_end();
    led_force_immediate_update();
    vTaskDelay(pdMS_TO_TICKS(180));
  }

  led_frame_begin();
  for (int i = 0; i < 64; i++) {
    led_set_pixel_internal((uint8_t)i, 0, 0, 0);
  }
  led_frame_end();
  led_force_immediate_update();
}

// ============================================================================
// LED UTILITY FUNCTIONS
// ============================================================================

static bool s_lamp_hold;

void led_set_lamp_hold(bool hold) {
  bool was = s_lamp_hold;
  s_lamp_hold = hold;
  if (was && !hold) {
    /* The lamp overwrote the strip. The guide still remembers the old
     * colors and would otherwise leave the pieces dark. */
    led_board_restore_guide_yield();
    led_required_step_guide_yield();
  }
}

bool led_lamp_board_held(void) { return s_lamp_hold; }

static bool led_game_pixels_blocked(void) {
  return s_lamp_hold || led_gesture_anim_busy();
}

static bool led_tgt_lock(void) {
  if (led_unified_mutex == NULL) {
    return true;
  }
  return xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) ==
         pdTRUE;
}

static void led_tgt_unlock(void) {
  if (led_unified_mutex != NULL) {
    xSemaphoreGive(led_unified_mutex);
  }
}

static void led_target_kick(void) {
  if (led_task_handle != NULL &&
      xTaskGetCurrentTaskHandle() != led_task_handle) {
    (void)xTaskNotify(led_task_handle, 1U, eSetBits);
  }
}

static uint8_t s_cap[LED_BOARD_N];
static uint8_t s_cap_n;

void led_move_collapse_capture(void) {
  uint8_t i;
  s_cap_n = 0U;
  if (!led_tgt_lock()) {
    return;
  }
  for (i = 0; i < LED_BOARD_N; i++) {
    led_tgt_t *t = &s_tgt[i];
    if (t->tr != 0U || t->tg != 0U || t->tb != 0U) {
      s_cap[s_cap_n] = i;
      s_cap_n++;
    }
  }
  led_tgt_unlock();
}

void led_move_trail_cancel(void) { s_cap_n = 0U; }

void led_move_trail_start(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                          uint8_t to_col) {
  uint8_t dest;
  uint8_t n;
  uint8_t set[LED_BOARD_N];
  uint8_t i;
  uint8_t drow = 0;
  uint8_t dcol = 0;
  int maxd = 0;
  int dist[LED_BOARD_N];
  uint16_t span;
  uint16_t now;
  (void)from_row;
  (void)from_col;
  n = s_cap_n;
  s_cap_n = 0U;
  if (n == 0U || to_row > 7U || to_col > 7U) {
    return;
  }
  if (n > LED_BOARD_N) {
    n = LED_BOARD_N;
  }
  memcpy(set, s_cap, n);
  dest = chess_pos_to_led_index(to_row, to_col);
  for (i = 0; i < n; i++) {
    if (set[i] == dest) {
      break;
    }
  }
  if (i == n && n < LED_BOARD_N) {
    set[n] = dest;
    n++;
  }
  led_index_to_chess_pos(dest, &drow, &dcol);
  for (i = 0; i < n; i++) {
    uint8_t row = 0;
    uint8_t col = 0;
    int dr;
    int dc;
    int d;
    led_index_to_chess_pos(set[i], &row, &col);
    dr = (int)row - (int)drow;
    dc = (int)col - (int)dcol;
    if (dr < 0) {
      dr = -dr;
    }
    if (dc < 0) {
      dc = -dc;
    }
    d = dr > dc ? dr : dc;
    dist[i] = d;
    if (d > maxd) {
      maxd = d;
    }
  }
  span = (uint16_t)(MOVE_COLLAPSE_MS - MOVE_COLLAPSE_FADE_MS);
  now = (uint16_t)(esp_timer_get_time() / 1000);
  if (!led_tgt_lock()) {
    return;
  }
  for (i = 0; i < n; i++) {
    led_tgt_t *t = &s_tgt[set[i]];
    uint16_t delay = 0;
    if (set[i] >= LED_BOARD_N) {
      continue;
    }
    /* Already the post-move colour: leave a piece that stays lit alone. */
    if (!t->br_fading && t->tr == t->out_r && t->tg == t->out_g &&
        t->tb == t->out_b) {
      continue;
    }
    if (maxd > 0) {
      delay = (uint16_t)(((maxd - dist[i]) * (int)span) / maxd);
    }
    if (!t->br_fading) {
      t->sr = t->out_r;
      t->sg = t->out_g;
      t->sb = t->out_b;
      t->br_fading = 1U;
    }
    t->br_delay = delay;
    t->br_fade_ms = MOVE_COLLAPSE_FADE_MS;
    t->t0 = now;
  }
  led_tgt_unlock();
  led_target_kick();
}

static void led_tgt_store(led_tgt_t *t, uint8_t r, uint8_t g, uint8_t b,
                          uint8_t mode, bool mark, uint8_t floor,
                          uint16_t period, uint16_t phase, uint16_t fade_ms) {
  /* Same picture again must not restart a fade or the breathe phase. */
  if (!t->pulse && t->mode == mode && t->tr == r && t->tg == g && t->tb == b &&
      t->br_floor == floor && t->br_period == period && t->br_phase == phase &&
      t->br_fade_ms == fade_ms) {
    if (mark && s_picture_open) {
      t->touched = 1U;
    }
    return;
  }
  t->pulse = 0U;
  if (fade_ms != 0U) {
    /* From the colour on the strip now, so a cut-off fade does not jump. */
    t->sr = t->out_r;
    t->sg = t->out_g;
    t->sb = t->out_b;
    t->t0 = (uint16_t)(esp_timer_get_time() / 1000);
    t->br_fading = 1U;
    t->br_delay = 0U;
  } else if (mode == LED_VIS_FADE) {
    t->sr = t->out_r;
    t->sg = t->out_g;
    t->sb = t->out_b;
    t->t0 = (uint16_t)(esp_timer_get_time() / 1000);
    t->br_fading = 0U;
    t->br_delay = 0U;
  } else {
    t->br_fading = 0U;
    t->br_delay = 0U;
  }
  t->tr = r;
  t->tg = g;
  t->tb = b;
  t->mode = mode;
  t->br_floor = floor;
  t->br_period = period;
  t->br_phase = phase;
  t->br_fade_ms = fade_ms;
  if (mark && s_picture_open) {
    t->touched = 1U;
  }
}

uint32_t led_board_picture_gen(void) { return s_pic_gen; }

void led_target_begin(void) {
  uint8_t i;
  s_pic_gen++;
  if (!led_tgt_lock()) {
    s_picture_open = 1U;
    return;
  }
  s_picture_open = 1U;
  s_frame_fade = 0U;
  for (i = 0; i < LED_BOARD_N; i++) {
    s_tgt[i].touched = 0U;
  }
  led_tgt_unlock();
}

void led_target_begin_fade(uint16_t fade_ms) {
  led_target_begin();
  s_frame_fade = fade_ms;
}

void led_arm_direct_handover(void) {
  uint8_t i;
  s_hold_direct = true;
  if (!led_tgt_lock()) {
    return;
  }
  for (i = 0; i < LED_BOARD_N; i++) {
    uint32_t color = led_states[i];
    led_tgt_t *t = &s_tgt[i];
    t->pulse = 0U;
    t->br_fading = 0U;
    t->br_fade_ms = 0U;
    t->br_floor = 0U;
    t->br_period = 0U;
    t->br_phase = 0U;
    t->mode = LED_VIS_SNAP;
    t->tr = (uint8_t)((color >> 16) & 0xFFU);
    t->tg = (uint8_t)((color >> 8) & 0xFFU);
    t->tb = (uint8_t)(color & 0xFFU);
    t->out_r = t->tr;
    t->out_g = t->tg;
    t->out_b = t->tb;
  }
  s_hw_resync = 0U;
  led_tgt_unlock();
}

void led_release_direct_hold(void) { s_hold_direct = false; }

void led_target_set(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue,
                    led_vis_mode_t mode) {
  if (led_index >= LED_BOARD_N) {
    return;
  }
  s_pic_gen++;
  if (!led_tgt_lock()) {
    return;
  }
  led_tgt_store(&s_tgt[led_index], red, green, blue, (uint8_t)mode, true, 0, 0,
                0, s_frame_fade);
  led_tgt_unlock();
}

void led_target_set_breathe_sq(uint8_t led_index, uint8_t red, uint8_t green,
                               uint8_t blue, uint8_t floor, uint16_t period_ms,
                               uint16_t phase_ms, uint16_t fade_ms) {
  if (led_index >= LED_BOARD_N || fade_ms == 0U) {
    return;
  }
  s_pic_gen++;
  if (!led_tgt_lock()) {
    return;
  }
  led_tgt_store(&s_tgt[led_index], red, green, blue, LED_VIS_BREATHE, true,
                floor, period_ms, phase_ms, fade_ms);
  led_tgt_unlock();
}

void led_target_end(void) {
  uint8_t i;
  if (!led_tgt_lock()) {
    s_picture_open = 0U;
    s_frame_fade = 0U;
    led_target_kick();
    return;
  }
  if (!s_picture_open) {
    s_frame_fade = 0U;
    led_tgt_unlock();
    return;
  }
  for (i = 0; i < LED_BOARD_N; i++) {
    if (!s_tgt[i].touched) {
      s_tgt[i].pulse = 0U;
      /* Hint and guide frames ease off. A plain begin (pickup) snaps. */
      led_tgt_store(&s_tgt[i], 0, 0, 0, LED_VIS_SNAP, false, 0, 0, 0,
                    s_frame_fade);
    }
    s_tgt[i].touched = 0U;
  }
  s_picture_open = 0U;
  s_frame_fade = 0U;
  led_tgt_unlock();
  led_target_kick();
}

bool led_target_is_open(void) { return s_picture_open != 0U; }

void led_target_pulse(uint8_t led_index, uint8_t red, uint8_t green,
                      uint8_t blue) {
  led_tgt_t *t;
  if (led_index >= LED_BOARD_N) {
    return;
  }
  if (!led_tgt_lock()) {
    return;
  }
  t = &s_tgt[led_index];
  t->sr = t->out_r;
  t->sg = t->out_g;
  t->sb = t->out_b;
  t->pr = red;
  t->pg = green;
  t->pb = blue;
  t->pulse = 1U;
  t->br_fading = 0U;
  t->t0 = (uint16_t)(esp_timer_get_time() / 1000);
  if (s_picture_open) {
    t->touched = 1U;
  }
  led_tgt_unlock();
  led_target_kick();
}

void led_target_pulse_cancel(uint8_t led_index) {
  if (led_index >= LED_BOARD_N) {
    return;
  }
  if (!led_tgt_lock()) {
    return;
  }
  s_tgt[led_index].pulse = 0U;
  led_tgt_unlock();
  led_target_kick();
}

void led_set_pixel_safe(uint8_t led_index, uint8_t red, uint8_t green,
                        uint8_t blue) {
  if (led_index >= CHESS_LED_COUNT_TOTAL)
    return;
  /* Boot animation in main.c uses led_set_pixel_internal + force update. */
  if (led_is_booting() || led_game_pixels_blocked()) {
    return;
  }

  /* Endgame writes the strip itself. Board squares otherwise only set targets. */
  if (led_index >= LED_BOARD_N || endgame_wave.active) {
    led_set_pixel_internal(led_index, red, green, blue);
  } else {
    led_target_set(led_index, red, green, blue, LED_VIS_SNAP);
  }
  if (s_held_picture && led_task_handle != NULL &&
      xTaskGetCurrentTaskHandle() != led_task_handle) {
    s_picture_touch = xTaskGetTickCount();
  }
}

void led_clear_all_safe(void) {
  led_frame_begin();
  for (int i = 0; i < 64; i++) {
    led_set_pixel_internal(i, 0, 0, 0);
  }
  led_frame_end();
}

void led_set_all_safe(uint8_t red, uint8_t green, uint8_t blue) {
  led_frame_begin();
  for (int i = 0; i < 64; i++) {
    led_set_pixel_internal(i, red, green, blue);
  }
  led_frame_end();
}

void led_clear_board_only(void) {
  int i;
  bool hold = false;
  if (s_hold_direct || led_game_pixels_blocked()) {
    return;
  }
  led_move_trail_cancel();
  /* The wave owns the strip until it stops. A target clear would snap back. */
  if (endgame_wave.active) {
    for (i = 0; i < LED_BOARD_N; i++) {
      led_set_pixel_internal((uint8_t)i, 0, 0, 0);
    }
    return;
  }
  for (i = 0; i < LED_BOARD_N; i++) {
    led_target_set((uint8_t)i, 0, 0, 0, LED_VIS_SNAP);
  }
  if (s_picture_open) {
    led_board_restore_guide_yield();
  led_required_step_guide_yield();
    return;
  }
  /* Other task's clear opens one frame. Release drops it only while that
   * count is 1, so the count has to be raised here or the hold never ends
   * and led_render_targets stays dark after boot fade. */
  portENTER_CRITICAL(&s_led_compose_mux);
  hold = (s_led_compose == 0) && (led_task_handle != NULL) &&
         (xTaskGetCurrentTaskHandle() != led_task_handle);
  if (s_led_compose < 255) {
    s_led_compose++;
  }
  if (hold) {
    s_held_picture = true;
    s_picture_touch = xTaskGetTickCount();
  }
  portEXIT_CRITICAL(&s_led_compose_mux);
  if (!hold) {
    led_frame_end();
  }
  led_board_restore_guide_yield();
  led_required_step_guide_yield();
}

void led_clear_buttons_only(void) {
  led_frame_begin();
  // Clear only button LEDs (64+), preserve board (0-63)
  for (int i = 64; i < 80; i++) {
    led_set_pixel_internal(i, 0, 0, 0);
  }
  led_frame_end();
}

void led_preserve_buttons(void) {
  // This function preserves button states during board operations
  // Implementation depends on your button LED management system
}

void led_show_legal_moves(const led_command_t *cmd) {
  if (!cmd)
    return;

  // Show legal moves in green
  for (int i = 0; i < 64; i++) {
    if (cmd->data && ((uint8_t *)cmd->data)[i]) {
      led_set_pixel_safe(i, 0, 255, 0); // Green
    }
  }
}

void led_error_invalid_move(const led_command_t *cmd) {
  if (!cmd)
    return;

  // Flash red for invalid move
  for (int flash = 0; flash < 3; flash++) {
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(200));

    for (int i = 0; i < 64; i++) {
      led_set_pixel_safe(i, 255, 0, 0); // Red
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  led_clear_board_only();
}

void led_error_return_piece(const led_command_t *cmd) {
  if (!cmd)
    return;

  // Flash yellow for return piece
  for (int flash = 0; flash < 4; flash++) {
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(150));

    for (int i = 0; i < 64; i++) {
      led_set_pixel_safe(i, 255, 255, 0); // Yellow
    }
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  led_clear_board_only();
}

void led_error_recovery(const led_command_t *cmd) {
  if (!cmd)
    return;

  // Flash blue for recovery
  for (int flash = 0; flash < 5; flash++) {
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(100));

    for (int i = 0; i < 64; i++) {
      led_set_pixel_safe(i, 0, 0, 255); // Blue
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  led_clear_board_only();
}

void led_set_button_promotion_available(uint8_t button_id, bool available) {
  if (button_id >= 4)
    return;

  button_available[button_id] = available;
  if (!button_pressed[button_id] && !button_blinking[button_id]) {
    led_update_button_led_state(button_id);
  }
}

void led_update_button_availability_from_game(void) {
  // Get current player and promotion status from game
  player_t current_player =
      PLAYER_WHITE; // Default, should be updated from game state
  bool white_promotion_possible = false; // Should be updated from game state
  bool black_promotion_possible = false; // Should be updated from game state

  // Update button LEDs based on current state
  for (int i = 0; i < 4; i++) {
    uint8_t led_index = 64 + i;

    if (current_player == PLAYER_WHITE && white_promotion_possible) {
      led_set_pixel_internal(led_index, 0, 255, 0); // Green - available
    } else if (current_player == PLAYER_BLACK && black_promotion_possible) {
      led_set_pixel_internal(led_index, 0, 255, 0); // Green - available
    } else {
      led_set_pixel_internal(led_index, 0, 0, 255); // Blue - unavailable
    }
  }

  // CRITICAL: Force immediate update to ensure changes are visible
  led_force_immediate_update();

  ESP_LOGI(TAG,
           "✅ Button availability updated - White promotion: %s, Black "
           "promotion: %s",
           white_promotion_possible ? "YES (green)" : "NO (blue)",
           black_promotion_possible ? "YES (green)" : "NO (blue)");
}

/**
 * @brief Get ANSI color code from RGB values
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ANSI color escape sequence
 */
static const char *get_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
  // Simple color mapping based on dominant color
  if (r > 200 && g < 100 && b < 100)
    return "\033[31m"; // Red
  if (g > 200 && r < 100 && b < 100)
    return "\033[32m"; // Green
  if (b > 200 && r < 100 && g < 100)
    return "\033[34m"; // Blue
  if (r > 200 && g > 200 && b < 100)
    return "\033[33m"; // Yellow
  if (r > 200 && g < 100 && b > 200)
    return "\033[35m"; // Magenta
  if (r < 100 && g > 200 && b > 200)
    return "\033[36m"; // Cyan
  if (r > 200 && g > 200 && b > 200)
    return "\033[37m"; // White
  if (r < 50 && g < 50 && b < 50)
    return "\033[30m"; // Black
  return "\033[0m";    // Default (reset)
}

static uint8_t led_smoothstep_u8(uint8_t x) {
  uint32_t v = (uint32_t)x;
  uint32_t s = (v * v * (765U - 2U * v)) / (255U * 255U);
  return (uint8_t)s;
}

/**
 * @brief Update button LED state based on current conditions
 * @param button_id Button ID (0-8)
 */
static void led_update_button_led_state(uint8_t button_id) {
  if (button_id >= CHESS_BUTTON_COUNT)
    return;

  // Correct mapping of button IDs to LED indices
  uint8_t led_index = led_get_button_led_index(button_id);
  uint32_t current_time = esp_timer_get_time() / 1000;

  /* Release eases from red back to the resting color. Hard on/off blink
   * only updated when the 500 ms window ended, so the flash never ran. */
  if (button_blinking[button_id]) {
    uint32_t age = current_time - button_release_time[button_id];
    uint8_t rest_g = button_available[button_id] ? 255 : 0;
    uint8_t rest_b = button_available[button_id] ? 0 : 255;
    if (age < 420U) {
      uint8_t t = led_smoothstep_u8((uint8_t)((age * 255U) / 420U));
      led_set_pixel_internal(led_index, led_lerp_u8(255, 0, t),
                             led_lerp_u8(0, rest_g, t),
                             led_lerp_u8(0, rest_b, t));
      return;
    }
    button_blinking[button_id] = false;
  }

  // Set LED color based on state
  if (button_pressed[button_id]) {
    // Button is pressed - red
    led_set_pixel_internal(led_index, 255, 0, 0);
  } else if (button_available[button_id]) {
    // Button is available - green
    led_set_pixel_internal(led_index, 0, 255, 0);
  } else {
    // Button is not available - blue
    led_set_pixel_internal(led_index, 0, 0, 255);
  }
}

/**
 * @brief Set button pressed state
 * @param button_id Button ID (0-8)
 * @param pressed True if button is pressed
 */
static void led_set_button_pressed(uint8_t button_id, bool pressed) {
  if (button_id >= CHESS_BUTTON_COUNT)
    return;

  uint32_t current_time = esp_timer_get_time() / 1000;

  if (pressed) {
    button_pressed[button_id] = true;
    button_blinking[button_id] = false; // Stop any blinking
  } else {
    button_pressed[button_id] = false;
    button_release_time[button_id] = current_time;
    button_blinking[button_id] = true; // Start blinking
  }

  led_update_button_led_state(button_id);

  ESP_LOGI(TAG, "Button %d %s", button_id,
           pressed ? "PRESSED (red)" : "RELEASED");
}

/**
 * @brief Process button blink timers (call from main loop)
 */
static void led_process_button_blink_timers(void) {
  for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
    if (button_blinking[i]) {
      led_update_button_led_state((uint8_t)i);
    }
  }
}

static uint32_t led_logical_color(int index) {
  if (led_changed_flags[index]) {
    return led_pending_changes[index];
  }
  return led_states[index];
}

/**
 * Neutral RGB uses the white die. (200,180,180) spreads 20 counts, which is
 * within 20% of the peak, so W = 180 and RGB becomes (20,0,0). A saturated
 * color has a zero channel, or a wider spread (yellow, orange, pink), and
 * keeps W = 0. A caller that already stored W (lamp) is left alone.
 */
static void led_move_neutral_white(uint8_t *red, uint8_t *green, uint8_t *blue,
                                   uint8_t *white) {
  if (*white != 0) {
    return;
  }
  uint8_t lo = *red;
  uint8_t hi = *red;
  if (*green < lo) {
    lo = *green;
  }
  if (*blue < lo) {
    lo = *blue;
  }
  if (*green > hi) {
    hi = *green;
  }
  if (*blue > hi) {
    hi = *blue;
  }
  if (lo == 0 || hi == 0) {
    return;
  }
  if ((uint16_t)(hi - lo) * 5u > hi) {
    return;
  }
  *white = lo;
  *red = (uint8_t)(*red - lo);
  *green = (uint8_t)(*green - lo);
  *blue = (uint8_t)(*blue - lo);
}

static void led_decode_output(uint32_t color, uint8_t *red, uint8_t *green,
                              uint8_t *blue, uint8_t *white) {
  *red = (color >> 16) & 0xFF;
  *green = (color >> 8) & 0xFF;
  *blue = color & 0xFF;
  *white = (color >> 24) & 0xFF;
  unsigned scale = global_brightness;
  if (power_guard_output_half()) {
    scale /= 2u;
  }
  if (scale < 100u) {
    *red = (uint8_t)((*red * scale) / 100u);
    *green = (uint8_t)((*green * scale) / 100u);
    *blue = (uint8_t)((*blue * scale) / 100u);
    *white = (uint8_t)((*white * scale) / 100u);
  }
  led_move_neutral_white(red, green, blue, white);
}

static void led_frame_begin(void) {
  portENTER_CRITICAL(&s_led_compose_mux);
  if (s_led_compose < 255) {
    s_led_compose++;
  }
  portEXIT_CRITICAL(&s_led_compose_mux);
}

static void led_frame_end(void) {
  portENTER_CRITICAL(&s_led_compose_mux);
  if (s_led_compose > 0) {
    s_led_compose--;
  }
  portEXIT_CRITICAL(&s_led_compose_mux);
}

static void led_release_held_picture(bool force) {
  TickType_t now = xTaskGetTickCount();
  portENTER_CRITICAL(&s_led_compose_mux);
  if (s_held_picture && s_led_compose == 1 &&
      (force || (now - s_picture_touch) >= pdMS_TO_TICKS(LED_PICTURE_SETTLE_MS))) {
    s_held_picture = false;
    s_led_compose = 0;
  }
  portEXIT_CRITICAL(&s_led_compose_mux);
}

static bool led_frame_busy(void) {
  portENTER_CRITICAL(&s_led_compose_mux);
  bool busy = s_led_compose > 0;
  portEXIT_CRITICAL(&s_led_compose_mux);
  return busy;
}

/**
 * @brief Commit all pending LED changes atomically with watchdog safety
 * This function ensures all LED updates are sent to hardware in one critical
 * section
 */
static void led_commit_pending_changes(void) {
  if (!led_changes_pending || !led_initialized || led_strip == NULL ||
      simulation_mode) {
    return;
  }
  /* A picture is still being assembled. The caller sends it after end(). */
  if (led_frame_busy()) {
    return;
  }

  if (led_unified_mutex != NULL &&
      xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) !=
          pdTRUE) {
    ESP_LOGW(TAG, "Failed to take LED mutex for strip commit");
    return;
  }
  if (led_frame_busy() || !led_changes_pending) {
    if (led_unified_mutex != NULL) {
      xSemaphoreGive(led_unified_mutex);
    }
    return;
  }

  bool dirty[CHESS_LED_COUNT_TOTAL];
  uint32_t snap[CHESS_LED_COUNT_TOTAL];
  uint32_t changed_count = 0;
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    dirty[i] = led_changed_flags[i];
    if (!dirty[i]) {
      continue;
    }
    snap[i] = led_logical_color(i);
    changed_count++;
  }
  if (changed_count == 0) {
    led_changes_pending = false;
    if (led_unified_mutex != NULL) {
      xSemaphoreGive(led_unified_mutex);
    }
    return;
  }

  esp_err_t wdt_ret = led_task_wdt_reset_safe();
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
  }

  /* Driver buffer is filled from the frozen snapshot, then sent once.
   * Nothing else can refresh or rewrite it: this task holds the mutex. */
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    if (!dirty[i]) {
      continue;
    }
    if ((i % LED_WATCHDOG_RESET_INTERVAL) == 0) {
      led_task_wdt_reset_safe();
    }
    uint8_t red, green, blue, white;
    led_decode_output(snap[i], &red, &green, &blue, &white);
    esp_err_t set_ret =
        led_strip_set_pixel_rgbw(led_strip, i, red, green, blue, white);
    if (set_ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set LED %d: %s", i, esp_err_to_name(set_ret));
    }
  }

  led_task_wdt_reset_safe();
  esp_err_t ret = led_strip_refresh(led_strip);
  if (ret == ESP_OK) {
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      if (dirty[i]) {
        led_changed_flags[i] = false;
      }
    }
    led_changes_pending = false;
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      if (led_changed_flags[i]) {
        led_changes_pending = true;
        break;
      }
    }
  } else if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGD(TAG, "LED strip refresh skipped (RMT not ready)");
  } else {
    ESP_LOGW(TAG, "LED strip refresh warning: %s", esp_err_to_name(ret));
  }

  if (led_unified_mutex != NULL) {
    xSemaphoreGive(led_unified_mutex);
  }
}

/**
 * @brief Force immediate LED update for critical operations
 * Use this only for startup tests or critical visual feedback
 */
void led_force_immediate_update(void) {
  if (!led_initialized || led_strip == NULL || simulation_mode) {
    return;
  }

  // BOOT FIX: ALLOW update even during booting!
  // The main.c animation MUST have access to the LED to render the progress bar.
  // Original check if (led_is_booting()) return; BLOCKED the animation.
  // Race condition protection is solved by skipping LED operations in game_task
  // (total_games check).

  /* The LED task is inside its own frame. Sending here would publish that
   * half-built picture. The loop commit sends it when the frame closes.
   * Another task waits out that frame, then sends the finished one. */
  if (led_task_handle != NULL &&
      xTaskGetCurrentTaskHandle() == led_task_handle) {
    led_commit_pending_changes();
    return;
  }
  led_release_held_picture(true);
  for (int wait = 0; wait < 40 && led_frame_busy(); wait++) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  led_commit_pending_changes();
}

/**
 * @brief Optimized batch commit with privileged mutex synchronization
 * This function provides maximum stability for LED updates
 */
#define LED_TAKEBACK_MS 600U
#define LED_RESIGN_SPREAD_MS 6000U
#define LED_RESIGN_FADE_MS 400U

static uint8_t s_tb_led[8];
static uint8_t s_tb_n;
static uint32_t s_tb_t0;
static uint32_t s_tb_last;
static uint32_t s_tb_phase;
static bool s_tb_on;

static bool s_ring_on;
static bool s_ring_fade;
static uint8_t s_ring_king_led;
static uint8_t s_ring_n;
static uint8_t s_ring_led[32];
static uint8_t s_ring_cheb[32];
static uint8_t s_ring_max;
static uint32_t s_ring_t0;
static uint32_t s_ring_fade_t0;

void led_takeback_arm(const uint8_t *led_index, uint8_t count) {
  uint8_t i;
  uint8_t n = 0;
  if (led_index == NULL || count == 0) {
    led_takeback_clear();
    return;
  }
  for (i = 0; i < count && n < 8; i++) {
    if (led_index[i] < LED_BOARD_N) {
      s_tb_led[n++] = led_index[i];
    }
  }
  s_tb_n = n;
  s_tb_t0 = (uint32_t)(esp_timer_get_time() / 1000);
  s_tb_last = s_tb_t0;
  s_tb_phase = 0;
  s_tb_on = n > 0;
  led_target_kick();
}

void led_takeback_clear(void) {
  s_tb_on = false;
  s_tb_n = 0;
}

static uint8_t led_takeback_gain(uint32_t now) {
  uint32_t elapsed = (uint32_t)(now - s_tb_t0);
  uint32_t period;
  uint32_t dt;
  if (elapsed > LED_TAKEBACK_MS) {
    elapsed = LED_TAKEBACK_MS;
  }
  /* 1400 ms down to 280 ms across the hold. One cycle is 16 breathe steps. */
  period = 1400U - ((1400U - 280U) * elapsed) / LED_TAKEBACK_MS;
  if (period < 280U) {
    period = 280U;
  }
  dt = (uint32_t)(now - s_tb_last);
  s_tb_last = now;
  if (dt > 80U) {
    dt = 33U;
  }
  s_tb_phase += (dt * 16U * 256U) / period;
  return k_breathe_gain[(s_tb_phase / 256U) & 15U];
}

static bool led_takeback_has(uint8_t led) {
  uint8_t i;
  for (i = 0; i < s_tb_n; i++) {
    if (s_tb_led[i] == led) {
      return true;
    }
  }
  return false;
}

void led_resign_ring_start(uint8_t king_row, uint8_t king_col, bool white_side) {
  uint8_t row0 = white_side ? 0U : 4U;
  uint8_t row1 = white_side ? 4U : 8U;
  uint8_t row;
  uint8_t col;
  int kr = (int)king_row;
  int kc = (int)king_col;
  (void)white_side;
  s_ring_king_led = chess_pos_to_led_index(king_row, king_col);
  s_ring_n = 0;
  s_ring_max = 1;
  for (row = row0; row < row1; row++) {
    for (col = 0; col < 8U; col++) {
      int cheb = (int)row - kr;
      int dc = (int)col - kc;
      uint8_t led;
      if (cheb < 0) {
        cheb = -cheb;
      }
      if (dc < 0) {
        dc = -dc;
      }
      if (dc > cheb) {
        cheb = dc;
      }
      if ((uint8_t)cheb > s_ring_max) {
        s_ring_max = (uint8_t)cheb;
      }
      if (s_ring_n >= 32) {
        continue;
      }
      led = chess_pos_to_led_index(row, col);
      s_ring_led[s_ring_n] = led;
      s_ring_cheb[s_ring_n] = (uint8_t)cheb;
      s_ring_n++;
    }
  }
  s_ring_t0 = (uint32_t)(esp_timer_get_time() / 1000);
  s_ring_fade = false;
  s_ring_on = true;
  led_target_kick();
}

void led_resign_ring_cancel(void) {
  if (!s_ring_on) {
    return;
  }
  if (!s_ring_fade) {
    s_ring_fade = true;
    s_ring_fade_t0 = (uint32_t)(esp_timer_get_time() / 1000);
  }
  led_target_kick();
}

void led_resign_ring_stop(void) {
  s_ring_on = false;
  s_ring_fade = false;
}

bool led_resign_ring_busy(void) { return s_ring_on; }

static int led_ring_slot(uint8_t led) {
  uint8_t i;
  for (i = 0; i < s_ring_n; i++) {
    if (s_ring_led[i] == led) {
      return (int)i;
    }
  }
  return -1;
}

static void led_overlay_square(uint8_t led, uint8_t tb_gain, uint32_t elapsed,
                               uint32_t radius, uint8_t fade, uint8_t breathe,
                               uint8_t *r, uint8_t *g, uint8_t *b) {
  if (s_tb_on && led_takeback_has(led)) {
    *r = 0;
    *g = led_scale_u8(255, tb_gain);
    *b = led_scale_u8(255, tb_gain);
  }
  if (!s_ring_on || endgame_wave.active) {
    return;
  }
  {
    int slot = led_ring_slot(led);
    uint8_t rr;
    uint8_t gg;
    uint8_t bb;
    if (slot < 0 && led != s_ring_king_led) {
      return;
    }
    if (led == s_ring_king_led) {
      rr = led_scale_u8(255, breathe);
      gg = led_scale_u8(160, breathe);
      bb = 0;
    } else {
      uint8_t cheb = s_ring_cheb[slot];
      uint32_t t = (elapsed * 255U) / LED_RESIGN_SPREAD_MS;
      uint8_t green;
      uint8_t level = 0;
      if (t > 255U) {
        t = 255U;
      }
      green = (uint8_t)((140U * (255U - t)) / 255U);
      if (cheb < radius) {
        level = 56;
      } else if (cheb == radius) {
        level = 255;
      }
      rr = led_scale_u8(255, level);
      gg = led_scale_u8(green, level);
      bb = 0;
    }
    *r = (uint8_t)(((uint16_t)rr * (255U - fade) + (uint16_t)(*r) * fade) / 255U);
    *g = (uint8_t)(((uint16_t)gg * (255U - fade) + (uint16_t)(*g) * fade) / 255U);
    *b = (uint8_t)(((uint16_t)bb * (255U - fade) + (uint16_t)(*b) * fade) / 255U);
  }
}

static void led_render_one(const led_tgt_t *t, uint16_t now16, uint8_t gain,
                          uint8_t *r, uint8_t *g, uint8_t *b) {
  uint16_t elapsed = (uint16_t)(now16 - t->t0);
  if (t->pulse) {
    uint16_t half = LED_PULSE_MS / 2U;
    if (elapsed >= LED_PULSE_MS) {
      *r = t->tr;
      *g = t->tg;
      *b = t->tb;
      return;
    }
    if (elapsed < half) {
      uint8_t u = led_smoothstep(elapsed, half);
      *r = led_lerp_u8(t->sr, t->pr, u);
      *g = led_lerp_u8(t->sg, t->pg, u);
      *b = led_lerp_u8(t->sb, t->pb, u);
      return;
    }
    {
      uint8_t u = led_smoothstep((uint16_t)(elapsed - half), half);
      *r = led_lerp_u8(t->pr, t->tr, u);
      *g = led_lerp_u8(t->pg, t->tg, u);
      *b = led_lerp_u8(t->pb, t->tb, u);
    }
    return;
  }
  if (t->mode == LED_VIS_FADE) {
    uint8_t u = led_smoothstep(elapsed, LED_FADE_MS);
    *r = led_lerp_u8(t->sr, t->tr, u);
    *g = led_lerp_u8(t->sg, t->tg, u);
    *b = led_lerp_u8(t->sb, t->tb, u);
    return;
  }
  if (t->mode == LED_VIS_BREATHE) {
    uint8_t gnow = gain;
    if (t->br_period != 0U) {
      gnow = led_breathe_sq_gain(t->br_period, t->br_phase, t->br_floor);
    }
    *r = led_scale_u8(t->tr, gnow);
    *g = led_scale_u8(t->tg, gnow);
    *b = led_scale_u8(t->tb, gnow);
  } else {
    *r = t->tr;
    *g = t->tg;
    *b = t->tb;
  }
  if (t->br_fading && t->br_fade_ms != 0U) {
    uint16_t delay = t->br_delay;
    uint8_t u;
    if (elapsed < delay) {
      *r = t->sr;
      *g = t->sg;
      *b = t->sb;
    } else {
      u = led_smoothstep((uint16_t)(elapsed - delay), t->br_fade_ms);
      if (t->sr == 0U && t->sg == 0U && t->sb == 0U) {
        *r = led_scale_u8(*r, u);
        *g = led_scale_u8(*g, u);
        *b = led_scale_u8(*b, u);
      } else {
        *r = led_lerp_u8(t->sr, *r, u);
        *g = led_lerp_u8(t->sg, *g, u);
        *b = led_lerp_u8(t->sb, *b, u);
      }
    }
  }
}

static bool led_visual_busy(void) {
  uint8_t i;
  for (i = 0; i < LED_BOARD_N; i++) {
    if (s_tgt[i].br_fading) {
      return true;
    }
  }
  return false;
}

static void led_render_targets(void) {
  uint8_t rgb[LED_BOARD_N][3];
  uint8_t dirty[LED_BOARD_N];
  uint8_t gain;
  uint16_t now16;
  uint8_t i;
  if (s_picture_open || s_held_picture || s_board_show > 0 ||
      endgame_wave.active || s_forced_blackout || led_is_booting() ||
      led_game_pixels_blocked()) {
    return;
  }
  if (!led_tgt_lock()) {
    return;
  }
  {
    uint8_t force_now = s_hw_resync;
    s_hw_resync = 0U;
    s_force_hw = force_now;
  }
  gain = led_breathe_gain_now();
  now16 = (uint16_t)(esp_timer_get_time() / 1000);
  {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint8_t tb_gain = 0;
    uint32_t ring_elapsed = 0;
    uint32_t ring_radius = 0;
    uint8_t ring_fade = 0;
    if (s_tb_on) {
      tb_gain = led_takeback_gain(now_ms);
    }
    if (s_ring_on && !endgame_wave.active) {
      uint32_t span;
      ring_elapsed = (uint32_t)(now_ms - s_ring_t0);
      if (ring_elapsed > LED_RESIGN_SPREAD_MS) {
        ring_elapsed = LED_RESIGN_SPREAD_MS;
      }
      if (s_ring_fade) {
        uint32_t fe = (uint32_t)(now_ms - s_ring_fade_t0);
        if (fe >= LED_RESIGN_FADE_MS) {
          s_ring_on = false;
          s_ring_fade = false;
        } else {
          ring_fade = (uint8_t)((fe * 255U) / LED_RESIGN_FADE_MS);
        }
      }
      span = s_ring_max == 0 ? 1U : s_ring_max;
      ring_radius = (ring_elapsed * span) / LED_RESIGN_SPREAD_MS;
    }
    for (i = 0; i < LED_BOARD_N; i++) {
    led_tgt_t *t = &s_tgt[i];
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint16_t elapsed = (uint16_t)(now16 - t->t0);
    if (t->pulse && elapsed >= LED_PULSE_MS) {
      t->pulse = 0U;
    }
    if (t->mode == LED_VIS_FADE && elapsed >= LED_FADE_MS && !t->pulse) {
      t->mode = LED_VIS_SNAP;
    }
    led_render_one(t, now16, gain, &r, &g, &b);
    if (t->br_fading && t->br_fade_ms != 0U &&
        (uint16_t)(now16 - t->t0) >=
            (uint16_t)(t->br_delay + t->br_fade_ms)) {
      t->br_fading = 0U;
    }
    if (s_ring_on || s_tb_on) {
      led_overlay_square(i, tb_gain, ring_elapsed, ring_radius, ring_fade, gain,
                         &r, &g, &b);
    }
    dirty[i] = (s_force_hw || r != t->out_r || g != t->out_g || b != t->out_b)
                   ? 1U
                   : 0U;
    if (dirty[i]) {
      t->out_r = r;
      t->out_g = g;
      t->out_b = b;
      rgb[i][0] = r;
      rgb[i][1] = g;
      rgb[i][2] = b;
    }
    }
  }
  led_tgt_unlock();
  s_target_write = 1U;
  for (i = 0; i < LED_BOARD_N; i++) {
    if (!dirty[i]) {
      continue;
    }
    led_set_pixel_channels(i, rgb[i][0], rgb[i][1], rgb[i][2], 0);
  }
  s_target_write = 0U;
  s_force_hw = 0U;
}

static void led_privileged_batch_commit(void) {
  led_release_held_picture(false);
  led_render_targets();
  led_commit_pending_changes();
}

/**
 * @brief Set LED pixel with duration support
 * @param led_index LED index (0-72)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param duration_ms Duration in milliseconds
 */
static void led_set_pixel_with_duration(uint8_t led_index, uint8_t r, uint8_t g,
                                        uint8_t b, uint32_t duration_ms) {
  if (led_index >= CHESS_LED_COUNT_TOTAL) {
    ESP_LOGE(TAG, "Invalid LED index: %d", led_index);
    return;
  }

  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) ==
        pdTRUE) {
      uint32_t new_color = (r << 16) | (g << 8) | b;
      uint32_t current_time = esp_timer_get_time() / 1000;

      // Save the original color if the duration is not active
      if (!led_durations[led_index].is_active) {
        led_durations[led_index].original_color = led_states[led_index];
      }

      // Nastavit duration tracking
      led_durations[led_index].led_index = led_index;
      led_durations[led_index].duration_color = new_color;
      led_durations[led_index].start_time = current_time;
      led_durations[led_index].duration_ms = duration_ms;
      led_durations[led_index].is_active = true;
      led_durations[led_index].restore_original = true;

      // Aplikovat novou barvu
      led_states[led_index] = new_color;

      ESP_LOGD(TAG, "LED[%d] set with duration: RGB(%d,%d,%d) for %lums",
               led_index, r, g, b, duration_ms);

      xSemaphoreGive(led_unified_mutex);
    } else {
      ESP_LOGW(TAG, "Failed to take LED mutex for duration operation");
    }
  }
}

/**
 * @brief Process expired durations (runs in LED task context)
 *
 * IMPORTANT:
 * - Do NOT run duration logic from FreeRTOS timer callbacks ("Tmr Svc").
 *   Timer callbacks have small stacks and must not block on mutexes.
 * - We process expirations in the main LED task loop (33ms), then commit via
 * the existing batch system.
 */
static void led_process_duration_expirations(void) {
  if (!led_duration_system_enabled) {
    return;
  }

  uint32_t current_time = esp_timer_get_time() / 1000;
  bool state_changed = false;

  // Take mutex (task context -> safe to wait briefly)
  if (led_unified_mutex != NULL) {
    if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) !=
        pdTRUE) {
      return;
    }
  }

  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    if (!led_durations[i].is_active) {
      continue;
    }

    uint32_t elapsed = current_time - led_durations[i].start_time;
    if (elapsed < led_durations[i].duration_ms) {
      continue;
    }

    // Restore original color if requested
    if (led_durations[i].restore_original) {
      uint32_t restore_color = led_durations[i].original_color;
      led_states[i] = restore_color;

      // Mark pending changes so batch commit can refresh HW
      if (led_initialized && led_strip != NULL && !simulation_mode) {
        led_pending_changes[i] = restore_color;
        led_changed_flags[i] = true;
        led_changes_pending = true;
      }
    }

    led_durations[i].is_active = false;
    state_changed = true;
  }

  if (led_unified_mutex != NULL) {
    xSemaphoreGive(led_unified_mutex);
  }

  // No immediate refresh here — main loop already calls
  // `led_privileged_batch_commit()`.
  (void)state_changed;
}

/**
 * @brief Initialize duration management system
 */
static void led_init_duration_system(void) {
  // Clear all duration states
  memset(led_durations, 0, sizeof(led_durations));

  // PRODUCTION STABILITY:
  // Duration expirations are processed in LED task main loop, not via FreeRTOS
  // timers.
  led_duration_system_enabled = true;
  ESP_LOGI(TAG, "✅ LED duration management system initialized (task-driven)");
}

// ============================================================================
// ENHANCED CASTLING SYSTEM LED FUNCTIONS
// ============================================================================

/**
 * @brief Enhanced castling guidance LED function
 */
void led_enhanced_castling_guidance(const led_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏰 Enhanced castling guidance at LED %d", cmd->led_index);

  // Clear board first
  led_clear_board_only();

  // Show guidance based on command data
  // cmd->data should contain castling guidance data
  if (cmd->data) {
    // Parse guidance data and show appropriate guidance
    // This would be called from enhanced_castling_system.c
    led_set_pixel_safe(cmd->led_index, cmd->red, cmd->green, cmd->blue);
  } else {
    // Default guidance - show source and destination
    led_set_pixel_safe(cmd->led_index, 255, 215, 0); // Gold for source
  }
}

/**
 * @brief Enhanced castling error LED function
 */
void led_enhanced_castling_error(const led_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "❌ Enhanced castling error at LED %d", cmd->led_index);

  // Flash error indication
  for (int flash = 0; flash < 3; flash++) {
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(200));

    led_set_pixel_safe(cmd->led_index, 255, 0, 0); // Red error
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  led_clear_board_only();
}

/**
 * @brief Enhanced castling celebration LED function
 */
void led_enhanced_castling_celebration(const led_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🎉 Enhanced castling celebration");

  // Rainbow celebration effect
  for (int cycle = 0; cycle < 3; cycle++) {
    uint8_t colors[][3] = {
        {255, 0, 0},   // Red
        {0, 255, 0},   // Green
        {0, 0, 255},   // Blue
        {255, 255, 0}, // Yellow
        {255, 0, 255}, // Magenta
        {0, 255, 255}  // Cyan
    };

    for (int i = 0; i < 6; i++) {
      led_clear_board_only();
      led_set_pixel_safe(cmd->led_index, colors[i][0], colors[i][1],
                         colors[i][2]);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  // Final success state
  led_clear_board_only();
  led_set_pixel_safe(cmd->led_index, 0, 255, 0); // Green for success
}

/**
 * @brief Enhanced castling tutorial LED function
 */
void led_enhanced_castling_tutorial(const led_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "📖 Enhanced castling tutorial");

  // Step-by-step tutorial
  for (int step = 0; step < 3; step++) {
    led_clear_board_only();

    if (step == 0) {
      // Show king position
      led_set_pixel_safe(cmd->led_index, 255, 215, 0); // Gold
    } else if (step == 1) {
      // Show rook position
      led_set_pixel_safe(cmd->led_index, 192, 192, 192); // Silver
    } else {
      // Show both positions
      led_set_pixel_safe(cmd->led_index, 255, 215, 0); // Gold
      if (cmd->data) {
        uint8_t *rook_pos = (uint8_t *)cmd->data;
        led_set_pixel_safe(*rook_pos, 192, 192, 192); // Silver
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1500));
  }

  led_clear_board_only();
}

/**
 * @brief Clear enhanced castling indications
 */
void led_enhanced_castling_clear(void) {
  ESP_LOGI(TAG, "🧹 Clearing enhanced castling indications");
  led_clear_board_only();
}

// ============================================================================
// OTA PROGRESS LED (HTTPS / HTTP / BLE stream update)
// ============================================================================

void led_ota_progress_ui(uint8_t progress_percent) {
  if (!led_initialized || simulation_mode || led_is_booting()) {
    return;
  }
  if (!led_component_enabled) {
    return;
  }
  if (progress_percent > 100) {
    progress_percent = 100;
  }

  uint32_t t = xTaskGetTickCount();
  uint32_t pulse = 96U + (uint32_t)((t * 3U) % 48U);

  int filled = ((int)progress_percent * CHESS_LED_COUNT_BOARD) / 100;
  if (progress_percent > 0 && filled == 0) {
    filled = 1;
  }

  if (filled == 0) {
    uint32_t phase = (t / 6U) % 14U;
    if (phase > 7U) {
      phase = 14U - phase;
    }
    int sweep_led = (int)phase;
    for (int i = 0; i < CHESS_LED_COUNT_BOARD; i++) {
      if (i == sweep_led) {
        led_set_pixel_internal((uint8_t)i, (uint8_t)pulse,
                               (uint8_t)(pulse / 2U), 0);
      } else if (i < 8) {
        led_set_pixel_internal((uint8_t)i, 0, 4, 18);
      } else {
        led_set_pixel_internal((uint8_t)i, 0, 2, 12);
      }
    }
  } else {
    int head = filled - 1;
    uint32_t chase = (uint32_t)((t / 5U) % (uint32_t)filled);
    for (int i = 0; i < CHESS_LED_COUNT_BOARD; i++) {
      if (i < filled) {
        int dist = i - head;
        if (dist < 0) {
          dist = -dist;
        }
        uint32_t dim =
            40U + (pulse * (uint32_t)(8 - min(dist, 7))) / 40U;
        if ((uint32_t)i == chase) {
          led_set_pixel_internal((uint8_t)i, 255, 230, 80);
        } else {
          led_set_pixel_internal(
              (uint8_t)i, (uint8_t)(dim + 60U), (uint8_t)(dim / 4U), 0);
        }
      } else {
        led_set_pixel_internal((uint8_t)i, 0, 4, 22);
      }
    }
  }

  uint32_t bphase = (t / 12U) % (uint32_t)CHESS_LED_COUNT_BUTTONS;
  for (int b = 0; b < CHESS_LED_COUNT_BUTTONS; b++) {
    uint8_t li = led_get_button_led_index((uint8_t)b);
    if ((uint32_t)b == bphase) {
      led_set_pixel_internal(li, 0, (uint8_t)pulse, 200);
    } else {
      led_set_pixel_internal(li, 0, 12, 40);
    }
  }

  led_force_immediate_update();
}

void led_ota_restore_board_after_update_abort(void) {
  if (!led_initialized || simulation_mode || led_is_booting()) {
    return;
  }
  led_show_chess_board();
}

// ============================================================================
// BOOT ANIMATION LED FUNCTIONS
// ============================================================================

/**
 * @brief LED boot animation step - light up the LED according to progress
 * @param progress_percent Progress in percent (0-100)
 * @details
 * Bloom a warm checkerboard out from the center. The percent is unused; the
 * picture advances one smooth step per call (about 25 ms).
 * @note The function is safe - it checks initialization and simulation mode
 */
/* Clock units per 25 ms. Rings overlap, so the bloom stays smooth. */
#define BOOT_CLOCK_STEP 18u
#define BOOT_FRAME_MS 25u
#define BOOT_RING_GAP 22u
#define BOOT_SQUARE_SPAN 110u
#define BOOT_CLOCK_END ((7u * BOOT_RING_GAP) + BOOT_SQUARE_SPAN)

static uint16_t s_boot_clock;
static uint32_t s_boot_t0;
static bool s_boot_timed;
static bool s_boot_faded;

static uint8_t boot_center_dist(uint8_t row, uint8_t col) {
  int dr = (int)row * 2 - 7;
  int dc = (int)col * 2 - 7;
  if (dr < 0) {
    dr = -dr;
  }
  if (dc < 0) {
    dc = -dc;
  }
  return (uint8_t)(dr > dc ? dr : dc);
}

static uint8_t boot_scale(uint8_t channel, uint8_t amount, uint8_t gain) {
  return (uint8_t)(((uint32_t)channel * amount * gain) / (255u * 255u));
}

/** Warm light squares, quiet dark squares, brightness blooming from the center. */
static void boot_paint_board(uint16_t clock, uint8_t gain) {
  for (uint8_t row = 0; row < 8u; row++) {
    for (uint8_t col = 0; col < 8u; col++) {
      int local = (int)clock - (int)boot_center_dist(row, col) * (int)BOOT_RING_GAP;
      uint8_t amount = 0;
      if (local >= (int)BOOT_SQUARE_SPAN) {
        amount = 255;
      } else if (local > 0) {
        amount = led_smoothstep_u8((uint8_t)((local * 255) / (int)BOOT_SQUARE_SPAN));
      }
      bool light = ((row + col) & 1u) != 0u;
      uint8_t r = boot_scale(light ? 230u : 22u, amount, gain);
      uint8_t g = boot_scale(light ? 186u : 30u, amount, gain);
      uint8_t b = boot_scale(light ? 120u : 52u, amount, gain);
      led_set_pixel_internal(chess_pos_to_led_index(row, col), r, g, b);
    }
  }
}

bool led_boot_animation_step(uint8_t progress_percent) {
  (void)progress_percent;
  if (!led_initialized || simulation_mode || s_boot_faded) {
    return s_boot_faded;
  }

  bool covered = false;
  {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t elapsed;
    uint32_t clock;
    if (!s_boot_timed) {
      s_boot_t0 = now;
      s_boot_timed = true;
    }
    elapsed = (uint32_t)(now - s_boot_t0);
    clock = BOOT_CLOCK_STEP + (elapsed * BOOT_CLOCK_STEP) / BOOT_FRAME_MS;
    if (clock > BOOT_CLOCK_END) {
      clock = BOOT_CLOCK_END;
    }
    s_boot_clock = (uint16_t)clock;
    covered = (s_boot_clock >= BOOT_CLOCK_END);
  }

  boot_paint_board(s_boot_clock, 255);
  led_force_immediate_update();
  return covered;
}

/**
 * @brief LED boot animation fade out - gradually fade all LEDs to 0
 * @details
 * Ease the finished checkerboard down to black.
 * @note The function is safe - it checks initialization and simulation mode
 */
void led_boot_animation_fade_out(void) {
  // Security checks
  if (!led_initialized || simulation_mode) {
    ESP_LOGD(TAG, "LED boot fade out: %s",
             simulation_mode ? "simulation mode" : "not initialized");
    return;
  }
  if (s_boot_faded) {
    return;
  }
  s_boot_faded = true;

  ESP_LOGI(TAG, "Starting LED boot animation fade out...");

  /* Ease the checker back to black. Short, so boot does not sit on the picture. */
  const int fade_steps = 8;
  const int step_delay_ms = 12;

  for (int step = fade_steps; step >= 0; step--) {
    uint8_t lin = (uint8_t)((step * 255) / fade_steps);
    boot_paint_board(BOOT_CLOCK_END, led_smoothstep_u8(lin));
    led_force_immediate_update();
    vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    led_task_wdt_reset_safe();
  }

  led_clear_board_only();
  s_boot_clock = 0;
  s_boot_timed = false;
  led_booting_active = false;
  ESP_LOGI(TAG, "LED boot animation fade out completed");
}
