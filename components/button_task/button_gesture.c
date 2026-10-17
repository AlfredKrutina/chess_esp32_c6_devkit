/**
 * @file button_gesture.c
 * @brief Debounced one-button gestures for SW1 on the V2 hall PCB.
 *
 * Short click is a press shorter than 400 ms. A second press inside 400 ms
 * that is held becomes power-off (it wins over a triple-click and over
 * new-game). Three short clicks start pairing once; a fourth click in the
 * same burst does not start it again. Two clicks do nothing. A hold that
 * begins with no click in the previous 400 ms starts the new-game snake.
 * The 1 s legacy long-press threshold is not a commit point.
 */

#include "button_gesture.h"

#include "sdkconfig.h"

#if CONFIG_CHESS_GESTURE_BUTTON

#include "led_gesture_anim.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "BTN_GESTURE";

#define GESTURE_DEBOUNCE_MS 25
#define GESTURE_CLICK_MAX_MS 400
#define GESTURE_GAP_MS 400
#define GESTURE_LOCK_FALLBACK_MS 800

typedef enum {
  GS_IDLE = 0,
  GS_DOWN_IDLE,
  GS_GAP,
  GS_DOWN_SEQ,
  GS_SWALLOW,
  GS_LOCK,
  GS_WAIT_UP
} gesture_phase_t;

static QueueHandle_t s_queue;
static bool s_raw_down;
static bool s_down;
static uint32_t s_change_ms;
static bool s_change_valid;
static gesture_phase_t s_phase;
static uint8_t s_clicks;
static uint32_t s_press_ms;
static uint32_t s_up_ms;
static uint32_t s_lock_ms;
static bool s_saw_busy;
static bool s_gpio_ready;

static uint32_t gesture_now_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

static void gesture_emit(gesture_event_t id, const char *staging) {
  ESP_LOGI(TAG, "[STAGING] gesture: %s", staging);
  if (id == GESTURE_SNAKE_START) {
    (void)led_gesture_start_snake();
  } else if (id == GESTURE_SHUTDOWN_START) {
    (void)led_gesture_start_shutdown();
  }
  if (s_queue == NULL) {
    return;
  }
  if (xQueueSend(s_queue, &id, 0) != pdTRUE) {
    ESP_LOGW(TAG, "gesture queue full, drop %d", (int)id);
  }
  s_phase = GS_LOCK;
  s_lock_ms = gesture_now_ms();
  s_saw_busy = false;
}

static void gesture_on_press(uint32_t now) {
  if (s_phase == GS_LOCK || s_phase == GS_SWALLOW || s_phase == GS_WAIT_UP ||
      s_phase == GS_DOWN_IDLE || s_phase == GS_DOWN_SEQ) {
    return;
  }
  s_press_ms = now;
  if (s_phase == GS_GAP) {
    s_phase = GS_DOWN_SEQ;
    return;
  }
  s_clicks = 0;
  s_phase = GS_DOWN_IDLE;
}

static void gesture_on_release(uint32_t now) {
  if (led_gesture_anim_busy() || s_phase == GS_LOCK) {
    led_gesture_note_release();
  }
  s_up_ms = now;
  if (s_phase == GS_DOWN_IDLE) {
    uint32_t held = now - s_press_ms;
    if (held < GESTURE_CLICK_MAX_MS) {
      s_clicks = 1;
      s_phase = GS_GAP;
    } else {
      s_phase = GS_WAIT_UP;
    }
    return;
  }
  if (s_phase == GS_DOWN_SEQ) {
    uint32_t held = now - s_press_ms;
    if (held >= GESTURE_CLICK_MAX_MS) {
      s_phase = GS_WAIT_UP;
      return;
    }
    if (s_clicks < 255) {
      s_clicks++;
    }
    if (s_clicks >= 3) {
      gesture_emit(GESTURE_PAIR, "triple click -> BLE pairing");
      return;
    }
    s_phase = GS_GAP;
  }
}

void button_gesture_init(void) {
  if (s_queue == NULL) {
    s_queue = xQueueCreate(8, sizeof(gesture_event_t));
  }
  gpio_reset_pin((gpio_num_t)CONFIG_CHESS_GESTURE_BUTTON_GPIO);
  gpio_config_t cfg = {
      .pin_bit_mask = 1ULL << CONFIG_CHESS_GESTURE_BUTTON_GPIO,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t err = gpio_config(&cfg);
  s_gpio_ready = (err == ESP_OK);
  s_raw_down = false;
  s_down = false;
  s_change_valid = false;
  s_phase = GS_IDLE;
  s_clicks = 0;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPIO%d config failed: %s", CONFIG_CHESS_GESTURE_BUTTON_GPIO,
             esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG,
           "Gesture button GPIO%d active-low, debounce %d ms "
           "(promotion buttons stay at 50 ms)",
           CONFIG_CHESS_GESTURE_BUTTON_GPIO, GESTURE_DEBOUNCE_MS);
}

bool button_gesture_is_down(void) { return s_down; }

QueueHandle_t button_gesture_queue(void) { return s_queue; }

void button_gesture_poll(void) {
  if (!s_gpio_ready) {
    return;
  }
  uint32_t now = gesture_now_ms();
  bool raw = (gpio_get_level((gpio_num_t)CONFIG_CHESS_GESTURE_BUTTON_GPIO) == 0);
  if (!s_change_valid || raw != s_raw_down) {
    s_raw_down = raw;
    s_change_ms = now;
    s_change_valid = true;
  }
  if ((now - s_change_ms) >= GESTURE_DEBOUNCE_MS && s_down != s_raw_down) {
    s_down = s_raw_down;
    led_gesture_set_button_held(s_down);
    ESP_LOGI(TAG, "SW1 %s GPIO%d raw=%d", s_down ? "down" : "up",
             CONFIG_CHESS_GESTURE_BUTTON_GPIO, (int)raw);
    if (s_down) {
      gesture_on_press(now);
    } else {
      gesture_on_release(now);
    }
  }

  if (s_phase == GS_LOCK) {
    if (led_gesture_anim_busy()) {
      s_saw_busy = true;
    }
    if (s_saw_busy && !led_gesture_anim_busy()) {
      s_phase = s_down ? GS_WAIT_UP : GS_IDLE;
      s_clicks = 0;
    } else if (!s_saw_busy && (now - s_lock_ms) > GESTURE_LOCK_FALLBACK_MS) {
      s_phase = s_down ? GS_WAIT_UP : GS_IDLE;
      s_clicks = 0;
    }
    return;
  }
  if (s_phase == GS_WAIT_UP) {
    if (!s_down) {
      s_phase = GS_IDLE;
      s_clicks = 0;
    }
    return;
  }
  if (s_phase == GS_SWALLOW) {
    if (!s_down && (now - s_up_ms) >= GESTURE_GAP_MS) {
      s_phase = GS_IDLE;
      s_clicks = 0;
    }
    return;
  }
  if (s_phase == GS_DOWN_IDLE && s_down &&
      (now - s_press_ms) >= GESTURE_CLICK_MAX_MS) {
    gesture_emit(GESTURE_SNAKE_START, "hold from idle -> new game snake");
    return;
  }
  if (s_phase == GS_DOWN_SEQ && s_down && s_clicks == 1 &&
      (now - s_press_ms) >= GESTURE_CLICK_MAX_MS) {
    gesture_emit(GESTURE_SHUTDOWN_START, "click then hold -> power off");
    return;
  }
  if (s_phase == GS_GAP && !s_down && (now - s_up_ms) >= GESTURE_GAP_MS) {
    if (s_clicks == 1) {
      gesture_emit(GESTURE_TOGGLE, "single click -> game/lamp toggle");
    } else if (s_clicks == 2) {
      ESP_LOGI(TAG,
               "[STAGING] gesture: two clicks ignored "
               "(not a second toggle, not pairing)");
      s_phase = GS_IDLE;
      s_clicks = 0;
    } else {
      s_phase = GS_IDLE;
      s_clicks = 0;
    }
  }
}

#else /* !CONFIG_CHESS_GESTURE_BUTTON */

void button_gesture_init(void) {}
void button_gesture_poll(void) {}
bool button_gesture_is_down(void) { return false; }
QueueHandle_t button_gesture_queue(void) { return NULL; }

#endif
