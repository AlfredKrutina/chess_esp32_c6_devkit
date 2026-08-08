/**
 * @file game_led_animations.c
 * @brief ESP32-C6 Chess System - Advanced LED animation
 *
 * Complete implementation of advanced LED animations for the Sacha system:
 * - 5 endgame animations including wave animation
 * - Soft animations for figurines and buttons
 * - Full error handling
 * - RGB optimization for nice color transitions
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-04
 *
 * @details
 * This module contains all advanced LED animations for the sach system.
 * Contains endgame animations, smooth effects and optimized color transitions.
 * All animations are smooth and memory efficient.
 */

#include "game_led_animations.h"
#include "../../game_task/include/game_task.h" // Needed for game_get_piece
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "led_mapping.h"
#include "led_task_simple.h"
#include <math.h>
#include <string.h>

static const char *TAG = "GAME_LED_ANIMATIONS";

// ============================================================================
// GLOBAL VARIABLES FOR ANIMATIONS
// ============================================================================

// The state of the animation system
static bool animation_system_active = false;
static bool endgame_animation_running = false;
static endgame_animation_type_t current_endgame_animation =
    ENDGAME_ANIM_VICTORY_WAVE;
static uint8_t winning_king_position = 0;

// Timer pro animace
static TimerHandle_t animation_timer = NULL;
static TimerHandle_t subtle_animation_timer = NULL;

// Stav pro vlnovou animaci
static wave_animation_state_t wave_state = {0};

// Smooth animations
static subtle_animation_state_t subtle_pieces[64] = {0};
static subtle_animation_state_t subtle_buttons[9] = {0};

// ============================================================================
// BARVY A PALETY PRO ANIMACE
// ============================================================================

// Basic RGB colors
static const rgb_color_t COLOR_RED = {255, 0, 0};
static const rgb_color_t COLOR_GREEN = {0, 255, 0};
static const rgb_color_t COLOR_BLUE = {0, 0, 255};
static const rgb_color_t COLOR_YELLOW = {255, 255, 0};
static const rgb_color_t COLOR_ORANGE = {255, 165, 0};
static const rgb_color_t COLOR_PURPLE = {128, 0, 128};
static const rgb_color_t COLOR_WHITE = {255, 255, 255};
static const rgb_color_t COLOR_GOLD = {255, 215, 0};

// Palety pro vlnovou animaci
static const rgb_color_t wave_blue_palette[] = {
    {0, 100, 255},   // Light blue
    {0, 150, 255},   // Blue
    {0, 200, 255},   // Intense blue
    {100, 220, 255}, // Light blue
    {0, 255, 255}    // Cyan
};

static const rgb_color_t enemy_red_palette[] = {
    {255, 0, 0},    // Red
    {255, 50, 0},   // Orange-red
    {255, 100, 0},  // Orange
    {255, 150, 0},  // Light orange
    {255, 200, 100} // Creamy
};

// ============================================================================
// HELPFUL FUNCTIONS FOR COLOR GRADES
// ============================================================================

/**
 * @brief Interpolate between two colors
 *
 * Auxiliary function for a smooth transition between two colors.
 * Then not used, but prepared for future extended animation.
 *
 * @param from Initial color
 * @param the End color
 * @param progress Progress (0.0-1.0)
 * @return The interpolated color
 */
__attribute__((unused)) static rgb_color_t
interpolate_color(rgb_color_t from, rgb_color_t to, float progress) {
  rgb_color_t result;
  result.r = from.r + (to.r - from.r) * progress;
  result.g = from.g + (to.g - from.g) * progress;
  result.b = from.b + (to.b - from.b) * progress;
  return result;
}

/**
 * @brief Aplikuje barvu na LED s error handlingem
 */
static esp_err_t apply_color_safe(uint8_t led_index, rgb_color_t color) {
  if (led_index >= 73) {
    ESP_LOGW(TAG, "Invalid LED index: %d", led_index);
    return ESP_ERR_INVALID_ARG;
  }

  return led_set_pixel_safe(led_index, color.r, color.g, color.b);
}

/**
 * @brief Gets the distance between two positions on the chessboard
 */
static float get_board_distance(uint8_t pos1, uint8_t pos2) {
  uint8_t row1, col1, row2, col2;
  led_index_to_chess_pos(pos1, &row1, &col1);
  led_index_to_chess_pos(pos2, &row2, &col2);

  float dx = col2 - col1;
  float dy = row2 - row1;
  return sqrtf(dx * dx + dy * dy);
}

// ============================================================================
// ENDGAME ANIMATION - IMPLEMENTATION OF ALL 5 TYPES
// ============================================================================

/**
 * @brief 1. Victory Wave - Victory wave from the king
 */
static void endgame_animation_victory_wave(uint32_t frame) {
  if (wave_state.frame == 0) {
    // Wave initialization
    wave_state.center_pos = winning_king_position;
    wave_state.max_radius = 10.0f;
    wave_state.current_radius = 0.0f;
    wave_state.wave_speed = 0.3f;
    wave_state.active_waves = 3;

    for (int i = 0; i < MAX_WAVES; i++) {
      wave_state.waves[i].radius = -2.0f * i; // Gradual launch of waves
      wave_state.waves[i].active = true;
    }

    ESP_LOGI(TAG, "🌊 Victory Wave animation started from position %d",
             winning_king_position);
  }

  // Update all waves
  for (int wave_idx = 0; wave_idx < wave_state.active_waves; wave_idx++) {
    if (!wave_state.waves[wave_idx].active)
      continue;

    wave_state.waves[wave_idx].radius += wave_state.wave_speed;

    // A wave is only active if it has a positive radius
    if (wave_state.waves[wave_idx].radius < 0)
      continue;

    // Aplikace vlny na LED
    for (int led = 0; led < 64; led++) {
      float distance = get_board_distance(wave_state.center_pos, led);
      float wave_radius = wave_state.waves[wave_idx].radius;

      // Kontrola zda je LED v dosahu vlny
      if (distance >= wave_radius - 0.5f && distance <= wave_radius + 0.5f) {
        rgb_color_t wave_color;

        // Get piece at this position
        uint8_t r, c;
        led_index_to_chess_pos(led, &r, &c);
        piece_t piece = game_get_piece(r, c);

        // Determine if it's an enemy piece
        bool is_enemy = false;
        if (piece != PIECE_EMPTY) {
          // Get winner color from the king at center_pos
          uint8_t king_r, king_c;
          led_index_to_chess_pos(wave_state.center_pos, &king_r, &king_c);
          piece_t winner_king = game_get_piece(king_r, king_c);
          bool winner_is_white = game_is_white_piece(winner_king);

          // Compare with current piece
          if (winner_is_white) {
            is_enemy = game_is_black_piece(piece);
          } else {
            is_enemy = game_is_white_piece(piece);
          }
        }

        if (is_enemy) {
          // Use the red palette for enemy pieces
          int palette_idx = wave_idx % (sizeof(enemy_red_palette) /
                                        sizeof(enemy_red_palette[0]));
          wave_color = enemy_red_palette[palette_idx];
        } else {
          // Use blue palette for normal wave (and friendly figures)
          int palette_idx = wave_idx % (sizeof(wave_blue_palette) /
                                        sizeof(wave_blue_palette[0]));
          wave_color = wave_blue_palette[palette_idx];
        }

        // Fade effect for a nicer wave
        float fade = 1.0f - fabsf(distance - wave_radius) / 0.5f;
        wave_color.r *= fade;
        wave_color.g *= fade;
        wave_color.b *= fade;

        apply_color_safe(led, wave_color);
      }
    }

    // Wave deactivation if off board
    if (wave_state.waves[wave_idx].radius > wave_state.max_radius) {
      wave_state.waves[wave_idx].active = false;
    }
  }

  wave_state.frame++;

  // Restart animation when all waves have reached the edge
  bool any_active = false;
  for (int i = 0; i < wave_state.active_waves; i++) {
    if (wave_state.waves[i].active) {
      any_active = true;
      break;
    }
  }

  if (!any_active) {
    // Restart with a slight delay
    wave_state.frame = 0;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/**
 * @brief 2. Victory Circles - Expanding circles
 */
static void endgame_animation_victory_circles(uint32_t frame) {
  static float circle_radius = 0.0f;
  static int circle_phase = 0;

  // Clear the board
  led_clear_all_safe();

  circle_radius += 0.2f;

  for (int led = 0; led < 64; led++) {
    float distance = get_board_distance(winning_king_position, led);

    // Several circles with different phases
    for (int c = 0; c < 3; c++) {
      float circle_r = circle_radius - c * 1.5f;
      if (circle_r <= 0)
        continue;

      if (fabsf(distance - circle_r) < 0.7f) {
        rgb_color_t color = COLOR_GOLD;

        // Different colors for different circles
        switch (c) {
        case 0:
          color = COLOR_GOLD;
          break;
        case 1:
          color = COLOR_ORANGE;
          break;
        case 2:
          color = COLOR_YELLOW;
          break;
        }

        // Fade efekt
        float fade = 1.0f - fabsf(distance - circle_r) / 0.7f;
        color.r *= fade;
        color.g *= fade;
        color.b *= fade;

        apply_color_safe(led, color);
      }
    }
  }

  // Reset when the circles reach the edge
  if (circle_radius > 10.0f) {
    circle_radius = 0.0f;
    circle_phase++;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/**
 * @brief 3. Victory Cascade - Cascading fall
 */
static void endgame_animation_victory_cascade(uint32_t frame) {
  static int cascade_row = 7;
  static int cascade_phase = 0;

  // Apply the cascade line by line
  for (int col = 0; col < 8; col++) {
    uint8_t led = chess_pos_to_led_index(cascade_row, col);

    rgb_color_t cascade_color;
    switch (cascade_phase % 3) {
    case 0:
      cascade_color = COLOR_PURPLE;
      break;
    case 1:
      cascade_color = COLOR_BLUE;
      break;
    case 2:
      cascade_color = COLOR_WHITE;
      break;
    }

    apply_color_safe(led, cascade_color);
  }

  // Move to the next line
  cascade_row--;
  if (cascade_row < 0) {
    cascade_row = 7;
    cascade_phase++;

    // Clear the board between phases
    led_clear_all_safe();
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

/**
 * @brief 4. Victory Fireworks
 */
static void endgame_animation_victory_fireworks(uint32_t frame) {
  static firework_t fireworks[MAX_FIREWORKS];
  static bool fireworks_initialized = false;

  if (!fireworks_initialized) {
    // Initialization of fireworks
    for (int i = 0; i < MAX_FIREWORKS; i++) {
      fireworks[i].center_x = rand() % 8;
      fireworks[i].center_y = rand() % 8;
      fireworks[i].radius = 0.0f;
      fireworks[i].max_radius = 2.0f + (rand() % 3);
      fireworks[i].color_idx = rand() % 3;
      fireworks[i].active = (i == 0); // Set off the first fireworks
      fireworks[i].delay = i * 10;    // Gradual launch
    }
    fireworks_initialized = true;
  }

  // Clear the board
  led_clear_all_safe();

  // Fireworks update
  for (int f = 0; f < MAX_FIREWORKS; f++) {
    if (!fireworks[f].active) {
      // Delayed activation
      if (fireworks[f].delay > 0) {
        fireworks[f].delay--;
      } else if (fireworks[f].radius == 0) {
        fireworks[f].active = true;
      }
      continue;
    }

    fireworks[f].radius += 0.15f;

    // Rendering of fireworks
    for (int led = 0; led < 64; led++) {
      uint8_t led_x, led_y;
      led_index_to_chess_pos(led, &led_y, &led_x);

      float dx = led_x - fireworks[f].center_x;
      float dy = led_y - fireworks[f].center_y;
      float distance = sqrtf(dx * dx + dy * dy);

      if (fabsf(distance - fireworks[f].radius) < 0.8f) {
        rgb_color_t firework_color;
        switch (fireworks[f].color_idx) {
        case 0:
          firework_color = COLOR_RED;
          break;
        case 1:
          firework_color = COLOR_GREEN;
          break;
        case 2:
          firework_color = COLOR_BLUE;
          break;
        default:
          firework_color = COLOR_WHITE;
          break;
        }

        // Fade efekt
        float fade = 1.0f - (fireworks[f].radius / fireworks[f].max_radius);
        firework_color.r *= fade;
        firework_color.g *= fade;
        firework_color.b *= fade;

        apply_color_safe(led, firework_color);
      }
    }

    // Deactivation when fireworks reach maximum
    if (fireworks[f].radius > fireworks[f].max_radius) {
      fireworks[f].active = false;
      fireworks[f].radius = 0.0f;

      // Restart with new parameters
      fireworks[f].center_x = rand() % 8;
      fireworks[f].center_y = rand() % 8;
      fireworks[f].max_radius = 2.0f + (rand() % 3);
      fireworks[f].color_idx = rand() % 3;
      fireworks[f].delay = rand() % 30;
    }
  }
}

/**
 * @brief 5. Victory Crown - Crown for the winner
 */
static void endgame_animation_victory_crown(uint32_t frame) {
  static int crown_phase = 0;
  // Crown pattern - calculated at runtime due to serpentine layout
  uint8_t crown_pattern[21];
  crown_pattern[0] = chess_pos_to_led_index(7, 0);
  crown_pattern[1] = chess_pos_to_led_index(7, 1);
  crown_pattern[2] = chess_pos_to_led_index(7, 2);
  crown_pattern[3] = chess_pos_to_led_index(7, 3);
  crown_pattern[4] = chess_pos_to_led_index(7, 4);
  crown_pattern[5] = chess_pos_to_led_index(7, 5);
  crown_pattern[6] = chess_pos_to_led_index(7, 6);
  crown_pattern[7] = chess_pos_to_led_index(7, 7); // Top line
  crown_pattern[8] = chess_pos_to_led_index(6, 1);
  crown_pattern[9] = chess_pos_to_led_index(6, 3);
  crown_pattern[10] = chess_pos_to_led_index(6, 5);
  crown_pattern[11] = chess_pos_to_led_index(6, 7);
  crown_pattern[12] = chess_pos_to_led_index(6, 1);
  crown_pattern[13] = chess_pos_to_led_index(6, 3);
  crown_pattern[14] = chess_pos_to_led_index(6, 5); // Vrcholky korunky
  crown_pattern[15] = chess_pos_to_led_index(5, 1);
  crown_pattern[16] = chess_pos_to_led_index(5, 2);
  crown_pattern[17] = chess_pos_to_led_index(5, 3);
  crown_pattern[18] = chess_pos_to_led_index(5, 4);
  crown_pattern[19] = chess_pos_to_led_index(5, 5);
  crown_pattern[20] = chess_pos_to_led_index(5, 6); // Spodek korunky

  // Clear the board
  led_clear_all_safe();

  // Draw the crown step by step
  int crown_size = sizeof(crown_pattern) / sizeof(crown_pattern[0]);
  int visible_parts = (frame / 3) % (crown_size + 10);

  for (int i = 0; i < visible_parts && i < crown_size; i++) {
    rgb_color_t crown_color;

    // Different colors according to phase
    switch (crown_phase % 4) {
    case 0:
      crown_color = COLOR_GOLD;
      break;
    case 1:
      crown_color = COLOR_YELLOW;
      break;
    case 2:
      crown_color = COLOR_ORANGE;
      break;
    case 3:
      crown_color = COLOR_WHITE;
      break;
    }

    // Flashing for dramatic effect
    if ((frame / 5) % 2 == 0) {
      crown_color.r = crown_color.r * 0.7f;
      crown_color.g = crown_color.g * 0.7f;
      crown_color.b = crown_color.b * 0.7f;
    }

    apply_color_safe(crown_pattern[i], crown_color);
  }

  // Phase change
  if (visible_parts >= crown_size) {
    crown_phase++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================================
// SMOOTH ANIMATIONS FOR FIGURES AND BUTTONS
// ============================================================================

/**
 * @brief Start a soft animation for the figurine
 */
esp_err_t start_subtle_piece_animation(uint8_t piece_position,
                                       subtle_anim_type_t anim_type) {
  if (piece_position >= 64) {
    return ESP_ERR_INVALID_ARG;
  }

  subtle_pieces[piece_position].active = true;
  subtle_pieces[piece_position].type = anim_type;
  subtle_pieces[piece_position].frame = 0;
  subtle_pieces[piece_position].base_color =
      COLOR_YELLOW; // Basic color for moving figures

  ESP_LOGD(TAG, "Started subtle animation for piece at %d, type %d",
           piece_position, anim_type);
  return ESP_OK;
}

/**
 * @brief Start a soft animation for the button
 */
esp_err_t start_subtle_button_animation(uint8_t button_id,
                                        subtle_anim_type_t anim_type) {
  if (button_id >= 9) {
    return ESP_ERR_INVALID_ARG;
  }

  subtle_buttons[button_id].active = true;
  subtle_buttons[button_id].type = anim_type;
  subtle_buttons[button_id].frame = 0;
  subtle_buttons[button_id].base_color =
      COLOR_GREEN; // Base color for available buttons

  ESP_LOGD(TAG, "Started subtle animation for button %d, type %d", button_id,
           anim_type);
  return ESP_OK;
}

/**
 * @brief Aplikuje jemnou animaci na LED
 */
static void apply_subtle_animation(uint8_t led_index,
                                   subtle_animation_state_t *anim) {
  if (!anim->active)
    return;

  rgb_color_t result_color = anim->base_color;
  float wave_progress = sinf(anim->frame * 0.1f);

  switch (anim->type) {
  case SUBTLE_ANIM_GENTLE_WAVE:
    // Soft wave - slight changes in saturation
    result_color.r = anim->base_color.r * (0.9f + 0.1f * wave_progress);
    result_color.g = anim->base_color.g * (0.9f + 0.1f * wave_progress);
    result_color.b = anim->base_color.b * (0.9f + 0.1f * wave_progress);
    break;

  case SUBTLE_ANIM_WARM_GLOW:
    // Warm radiation - mixed with yellow/orange
    float glow_intensity = (wave_progress + 1.0f) / 2.0f * 0.15f;
    result_color.r = anim->base_color.r + 40 * glow_intensity;
    result_color.g = anim->base_color.g + 20 * glow_intensity;
    // The blue remains the same
    break;

  case SUBTLE_ANIM_COOL_PULSE:
    // Cool pulsing - mixed with blue/purple
    float pulse_intensity = (wave_progress + 1.0f) / 2.0f * 0.1f;
    result_color.b = anim->base_color.b + 30 * pulse_intensity;
    result_color.r = anim->base_color.r * (1.0f - pulse_intensity * 0.2f);
    break;

  case SUBTLE_ANIM_WHITE_WINS:
    // White wins - white animation with gentle pulsation
    float white_intensity = (wave_progress + 1.0f) / 2.0f * 0.2f;
    result_color.r = 255 * (0.8f + white_intensity);
    result_color.g = 255 * (0.8f + white_intensity);
    result_color.b = 255 * (0.8f + white_intensity);
    break;

  case SUBTLE_ANIM_BLACK_WINS:
    // Black wins - black animation with subtle pulsation
    float black_intensity = (wave_progress + 1.0f) / 2.0f * 0.1f;
    result_color.r = 50 * (0.5f + black_intensity);
    result_color.g = 50 * (0.5f + black_intensity);
    result_color.b = 50 * (0.5f + black_intensity);
    break;

  case SUBTLE_ANIM_DRAW:
    // Draw - neutral animation with gray
    float draw_intensity = (wave_progress + 1.0f) / 2.0f * 0.15f;
    result_color.r = 128 * (0.8f + draw_intensity);
    result_color.g = 128 * (0.8f + draw_intensity);
    result_color.b = 128 * (0.8f + draw_intensity);
    break;
  }

  // Limit colors to valid range
  result_color.r = fminf(255, fmaxf(0, result_color.r));
  result_color.g = fminf(255, fmaxf(0, result_color.g));
  result_color.b = fminf(255, fmaxf(0, result_color.b));

  apply_color_safe(led_index, result_color);
  anim->frame++;
}

/**
 * @brief Timer callback for fine animations
 */
static void subtle_animation_timer_callback(TimerHandle_t timer) {
  // Apply subtle animations to figures
  for (int i = 0; i < 64; i++) {
    if (subtle_pieces[i].active) {
      apply_subtle_animation(i, &subtle_pieces[i]);
    }
  }

  // Apply subtle animations to buttons
  for (int i = 0; i < 9; i++) {
    if (subtle_buttons[i].active) {
      apply_subtle_animation(64 + i, &subtle_buttons[i]);
    }
  }
}

// ============================================================================
// MAIN ANIMATION FEATURE
// ============================================================================

/**
 * @brief Timer callback pro endgame animace
 */
static void animation_timer_callback(TimerHandle_t timer) {
  if (!endgame_animation_running)
    return;

  static uint32_t frame_counter = 0;

  switch (current_endgame_animation) {
  case ENDGAME_ANIM_VICTORY_WAVE:
    endgame_animation_victory_wave(frame_counter);
    break;

  case ENDGAME_ANIM_VICTORY_CIRCLES:
    endgame_animation_victory_circles(frame_counter);
    break;

  case ENDGAME_ANIM_VICTORY_CASCADE:
    endgame_animation_victory_cascade(frame_counter);
    break;

  case ENDGAME_ANIM_VICTORY_FIREWORKS:
    endgame_animation_victory_fireworks(frame_counter);
    break;

  case ENDGAME_ANIM_VICTORY_CROWN:
    endgame_animation_victory_crown(frame_counter);
    break;

  default:
    ESP_LOGW(TAG, "Unknown endgame animation type: %d",
             current_endgame_animation);
    break;
  }

  frame_counter++;
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

esp_err_t game_led_animations_init(void) {
  if (animation_system_active) {
    ESP_LOGW(TAG, "Animation system already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing advanced LED animation system...");

  // Create timers for animations
  animation_timer =
      xTimerCreate("EndgameAnim",
                   pdMS_TO_TICKS(100), // 100ms interval for smooth animations
                   pdTRUE,             // Auto-reload
                   NULL, animation_timer_callback);

  subtle_animation_timer =
      xTimerCreate("SubtleAnim",
                   pdMS_TO_TICKS(50), // 50ms interval for smooth animations
                   pdTRUE, NULL, subtle_animation_timer_callback);

  if (!animation_timer || !subtle_animation_timer) {
    ESP_LOGE(TAG, "Failed to create animation timers");
    return ESP_ERR_NO_MEM;
  }

  // Initialize states
  memset(&wave_state, 0, sizeof(wave_state));
  memset(subtle_pieces, 0, sizeof(subtle_pieces));
  memset(subtle_buttons, 0, sizeof(subtle_buttons));

  // Start a timer for gentle animations
  xTimerStart(subtle_animation_timer, 0);

  animation_system_active = true;
  ESP_LOGI(TAG, "✅ Advanced LED animation system initialized successfully");

  return ESP_OK;
}

esp_err_t start_endgame_animation(endgame_animation_type_t animation_type,
                                  uint8_t king_position) {
  if (!animation_system_active) {
    ESP_LOGE(TAG, "Animation system not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (animation_type >= ENDGAME_ANIM_MAX) {
    ESP_LOGE(TAG, "Invalid animation type: %d", animation_type);
    return ESP_ERR_INVALID_ARG;
  }

  if (king_position >= 64) {
    ESP_LOGE(TAG, "Invalid king position: %d", king_position);
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "🎬 Starting endgame animation %d from position %d",
           animation_type, king_position);

  // Stops the previous animation if it is running
  if (endgame_animation_running) {
    stop_endgame_animation();
  }

  current_endgame_animation = animation_type;
  winning_king_position = king_position;
  endgame_animation_running = true;

  // Reset states for new animation
  memset(&wave_state, 0, sizeof(wave_state));

  // Starts a timer
  xTimerStart(animation_timer, 0);

  return ESP_OK;
}

esp_err_t stop_endgame_animation(void) {
  if (!endgame_animation_running) {
    return ESP_OK;
  }

  ESP_LOGI(TAG, "🛑 Stopping endgame animation");

  endgame_animation_running = false;
  xTimerStop(animation_timer, 0);

  // Clear the board
  led_clear_all_safe();

  return ESP_OK;
}

bool is_endgame_animation_running(void) { return endgame_animation_running; }

const char *
get_endgame_animation_name(endgame_animation_type_t animation_type) {
  switch (animation_type) {
  case ENDGAME_ANIM_VICTORY_WAVE:
    return "Victory Wave";
  case ENDGAME_ANIM_VICTORY_CIRCLES:
    return "Victory Circles";
  case ENDGAME_ANIM_VICTORY_CASCADE:
    return "Victory Cascade";
  case ENDGAME_ANIM_VICTORY_FIREWORKS:
    return "Victory Fireworks";
  case ENDGAME_ANIM_VICTORY_CROWN:
    return "Victory Crown";
  default:
    return "Unknown";
  }
}

esp_err_t stop_all_subtle_animations(void) {
  memset(subtle_pieces, 0, sizeof(subtle_pieces));
  memset(subtle_buttons, 0, sizeof(subtle_buttons));
  ESP_LOGI(TAG, "All subtle animations stopped");
  return ESP_OK;
}