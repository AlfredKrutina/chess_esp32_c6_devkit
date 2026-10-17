#include "power_guard.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "PWR_GUARD";

#define PWR_GUARD_NS "pwr_guard"
#define PWR_GUARD_KEY_STREAK "streak"
#define PWR_GUARD_KEY_HALF "half"
#define PWR_GUARD_KEY_TOTAL "total"

/** Half output starts on the boot after more than this many brownouts in a row. */
#define PWR_GUARD_STREAK_LIMIT 3

static uint8_t s_streak;
static uint8_t s_half;
static uint16_t s_total;
static bool s_noted;

static void power_guard_load(void) {
  nvs_handle_t h;
  if (nvs_open(PWR_GUARD_NS, NVS_READONLY, &h) != ESP_OK) {
    return;
  }
  uint8_t streak = 0;
  uint8_t half = 0;
  uint16_t total = 0;
  if (nvs_get_u8(h, PWR_GUARD_KEY_STREAK, &streak) != ESP_OK) {
    streak = 0;
  }
  if (nvs_get_u8(h, PWR_GUARD_KEY_HALF, &half) != ESP_OK) {
    half = 0;
  }
  if (nvs_get_u16(h, PWR_GUARD_KEY_TOTAL, &total) != ESP_OK) {
    total = 0;
  }
  nvs_close(h);
  s_streak = streak;
  s_half = half ? 1u : 0u;
  s_total = total;
}

static void power_guard_save(void) {
  nvs_handle_t h;
  if (nvs_open(PWR_GUARD_NS, NVS_READWRITE, &h) != ESP_OK) {
    ESP_LOGW(TAG, "nvs_open failed");
    return;
  }
  esp_err_t err = nvs_set_u8(h, PWR_GUARD_KEY_STREAK, s_streak);
  if (err == ESP_OK) {
    err = nvs_set_u8(h, PWR_GUARD_KEY_HALF, s_half);
  }
  if (err == ESP_OK) {
    err = nvs_set_u16(h, PWR_GUARD_KEY_TOTAL, s_total);
  }
  if (err == ESP_OK) {
    err = nvs_commit(h);
  }
  nvs_close(h);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs save: %s", esp_err_to_name(err));
  }
}

void power_guard_on_boot(esp_reset_reason_t reason) {
  if (s_noted) {
    return;
  }
  s_noted = true;

  esp_err_t nvs_err = nvs_flash_init();
  if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
      nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS needs erase; brownout counter skipped this boot");
    return;
  }
  if (nvs_err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_flash_init: %s", esp_err_to_name(nvs_err));
    return;
  }

  power_guard_load();

  if (reason == ESP_RST_POWERON) {
    s_streak = 0;
    s_half = 0;
    ESP_LOGI(TAG,
             "Power-on: LED cap cleared, trying full brightness (brownouts total=%u)",
             (unsigned)s_total);
  } else if (reason == ESP_RST_BROWNOUT) {
    if (s_streak < 255u) {
      s_streak++;
    }
    if (s_total < 65535u) {
      s_total++;
    }
    if (s_streak > PWR_GUARD_STREAK_LIMIT) {
      s_half = 1;
    }
    ESP_LOGW(TAG, "Brownout streak=%u total=%u led_scale=%u%%",
             (unsigned)s_streak, (unsigned)s_total,
             s_half ? 50u : 100u);
  } else {
    s_streak = 0;
    ESP_LOGI(TAG, "Reset %d clears the brownout streak; LED cap %s",
             (int)reason, s_half ? "stays at half" : "is off");
  }

  power_guard_save();
}

bool power_guard_output_half(void) { return s_half != 0; }

uint8_t power_guard_scale_percent(void) { return s_half ? 50u : 100u; }

uint8_t power_guard_brownout_streak(void) { return s_streak; }

uint16_t power_guard_brownout_total(void) { return s_total; }
