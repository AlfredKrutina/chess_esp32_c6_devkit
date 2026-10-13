/**
 * @file ota_guard.c
 * @brief OTA state machine, NVS journal of the last attempt and boot guard.
 *
 * Power-loss model:
 *  - Image bytes go only into the inactive slot (esp_ota_get_next_update_partition).
 *  - otadata is switched (esp_ota_set_boot_partition) only after esp_ota_end
 *    verified the whole image and the app descriptor matches this project.
 *  - The bootloader (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) boots a freshly
 *    switched image as PENDING_VERIFY; any reset before
 *    esp_ota_mark_app_valid_cancel_rollback() makes it fall back to the previous slot.
 *  - The journal (NVS blob, atomic per write) records the phase of the last attempt
 *    so the next boot can report receiving→aborted, verifying→aborted /
 *    rolled_back, rebooting→verifying / success / rolled_back truthfully.
 *  - Crash-loop counter lives in RTC memory (no flash writes, cleared on power-on).
 */
#include "ota_guard.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "ota_guard";

#ifndef CHESS_DEBUG_MODE
#define CHESS_DEBUG_MODE 0
#endif

#define OTA_JOURNAL_NS "ota_guard"
#define OTA_JOURNAL_KEY "journal"
#define OTA_JOURNAL_VER 1

#define OTA_GUARD_RTC_MAGIC 0x07A6D0A5u

typedef struct {
  uint8_t ver;
  uint8_t phase; /**< ota_guard_state_t */
  uint8_t reserved[2];
  uint32_t seq;
  char transport[8];
  char target_slot[17];
  char target_version[32];
  char reason[48];
} ota_journal_t;

RTC_NOINIT_ATTR static uint32_t s_rtc_magic;
RTC_NOINIT_ATTR static uint32_t s_rtc_crash_streak;
RTC_NOINIT_ATTR static uint32_t s_rtc_fallback_done;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static ota_guard_status_t s_st;
static bool s_live;           /**< transfer in progress in this boot */
static bool s_confirm_done;   /**< confirm step finished in this boot */
static esp_timer_handle_t s_deadline_timer;
static esp_timer_handle_t s_restart_timer;
static int s_last_logged_decile = -1;

const char *ota_guard_state_str(ota_guard_state_t st) {
  switch (st) {
  case OTA_GUARD_RECEIVING:
    return "receiving";
  case OTA_GUARD_VERIFYING:
    return "verifying";
  case OTA_GUARD_REBOOTING:
    return "rebooting";
  case OTA_GUARD_SUCCESS:
    return "success";
  case OTA_GUARD_ABORTED:
    return "aborted";
  case OTA_GUARD_ROLLED_BACK:
    return "rolled_back";
  case OTA_GUARD_ERROR:
    return "error";
  case OTA_GUARD_IDLE:
  default:
    return "idle";
  }
}

/** Copy for JSON output: quotes, backslashes and control chars become '_'. */
static void safe_copy(char *dst, size_t cap, const char *src) {
  if (dst == NULL || cap == 0) {
    return;
  }
  size_t i = 0;
  if (src != NULL) {
    for (; src[i] != '\0' && i + 1 < cap; i++) {
      char c = src[i];
      dst[i] = (c == '"' || c == '\\' || (unsigned char)c < 0x20) ? '_' : c;
    }
  }
  dst[i] = '\0';
}

static bool is_crash_reset(esp_reset_reason_t rr) {
  return rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT ||
         rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT;
}

static void staging_log_state(const char *where) {
#if CHESS_DEBUG_MODE
  ota_guard_status_t s;
  ota_guard_get_status(&s);
  ESP_LOGI(TAG,
           "[STAGING] OTA %s: state=%s pct=%d seq=%u tr=%s target=%s/%s "
           "running=%s/%s confirmed=%d awaiting=%d reason=%s",
           where, ota_guard_state_str(s.state), s.percent, (unsigned)s.seq,
           s.transport, s.target_slot, s.target_version, s.running_slot,
           s.running_version, s.confirmed, s.awaiting_confirm, s.reason);
#else
  (void)where;
#endif
}

/* ---------------------------------------------------------------- journal */

static esp_err_t journal_open(nvs_open_mode_t mode, nvs_handle_t *h) {
  esp_err_t e = nvs_open(OTA_JOURNAL_NS, mode, h);
  if (e == ESP_ERR_NVS_NOT_INITIALIZED) {
    /* Never erase here: NVS holds Wi‑Fi, tokens and game settings. */
    if (nvs_flash_init() == ESP_OK) {
      e = nvs_open(OTA_JOURNAL_NS, mode, h);
    }
  }
  return e;
}

static bool journal_load(ota_journal_t *j) {
  nvs_handle_t h;
  if (journal_open(NVS_READONLY, &h) != ESP_OK) {
    return false;
  }
  size_t len = sizeof(*j);
  esp_err_t e = nvs_get_blob(h, OTA_JOURNAL_KEY, j, &len);
  nvs_close(h);
  if (e != ESP_OK || len != sizeof(*j) || j->ver != OTA_JOURNAL_VER) {
    return false;
  }
  j->transport[sizeof(j->transport) - 1] = '\0';
  j->target_slot[sizeof(j->target_slot) - 1] = '\0';
  j->target_version[sizeof(j->target_version) - 1] = '\0';
  j->reason[sizeof(j->reason) - 1] = '\0';
  return true;
}

static void journal_save(void) {
  ota_journal_t j;
  memset(&j, 0, sizeof(j));
  j.ver = OTA_JOURNAL_VER;
  taskENTER_CRITICAL(&s_mux);
  j.phase = (uint8_t)s_st.state;
  j.seq = s_st.seq;
  memcpy(j.transport, s_st.transport, sizeof(j.transport));
  memcpy(j.target_slot, s_st.target_slot, sizeof(j.target_slot));
  memcpy(j.target_version, s_st.target_version, sizeof(j.target_version));
  memcpy(j.reason, s_st.reason, sizeof(j.reason));
  taskEXIT_CRITICAL(&s_mux);

  nvs_handle_t h;
  esp_err_t e = journal_open(NVS_READWRITE, &h);
  if (e == ESP_OK) {
    e = nvs_set_blob(h, OTA_JOURNAL_KEY, &j, sizeof(j));
    if (e == ESP_OK) {
      e = nvs_commit(h);
    }
    nvs_close(h);
  }
  if (e != ESP_OK) {
    ESP_LOGW(TAG, "OTA journal write failed: %s", esp_err_to_name(e));
  }
}

/* ---------------------------------------------------------- state helpers */

static void set_state(ota_guard_state_t st, const char *reason) {
  taskENTER_CRITICAL(&s_mux);
  s_st.state = st;
  if (reason != NULL) {
    safe_copy(s_st.reason, sizeof(s_st.reason), reason);
  }
  taskEXIT_CRITICAL(&s_mux);
}

static void set_target(const char *slot, const char *version) {
  taskENTER_CRITICAL(&s_mux);
  if (slot != NULL) {
    safe_copy(s_st.target_slot, sizeof(s_st.target_slot), slot);
  }
  if (version != NULL) {
    safe_copy(s_st.target_version, sizeof(s_st.target_version), version);
  }
  taskEXIT_CRITICAL(&s_mux);
}

void ota_guard_get_status(ota_guard_status_t *out) {
  if (out == NULL) {
    return;
  }
  taskENTER_CRITICAL(&s_mux);
  *out = s_st;
  taskEXIT_CRITICAL(&s_mux);
}

int ota_guard_format_json_fields(char *buf, size_t cap) {
  if (buf == NULL || cap == 0) {
    return -1;
  }
  ota_guard_status_t s;
  ota_guard_get_status(&s);
  return snprintf(
      buf, cap,
      "\"state\":\"%s\",\"percent\":%d,\"confirmed\":%s,"
      "\"awaiting_confirm\":%s,\"rollback_active\":%s,\"resumable\":%s,"
      "\"seq\":%u,\"transport\":\"%s\",\"reason\":\"%s\","
      "\"target_slot\":\"%s\",\"target_version\":\"%s\","
      "\"running_slot\":\"%s\",\"running_version\":\"%s\"",
      ota_guard_state_str(s.state), s.percent, s.confirmed ? "true" : "false",
      s.awaiting_confirm ? "true" : "false",
      s.rollback_active ? "true" : "false", s.resumable ? "true" : "false",
      (unsigned)s.seq, s.transport, s.reason, s.target_slot, s.target_version,
      s.running_slot, s.running_version);
}

const char *ota_guard_reason_from_err(esp_err_t err) {
  switch (err) {
  case ESP_OK:
    return "ok";
  case ESP_ERR_OTA_VALIDATE_FAILED:
    return "image_invalid";
  case ESP_ERR_OTA_ROLLBACK_INVALID_STATE:
    return "confirm_pending";
  case ESP_ERR_OTA_PARTITION_CONFLICT:
    return "partition_conflict";
  case ESP_ERR_OTA_SELECT_INFO_INVALID:
    return "otadata_invalid";
  case ESP_ERR_INVALID_SIZE:
    return "image_too_large";
  case ESP_ERR_NOT_FOUND:
    return "no_ota_partition";
  case ESP_ERR_NO_MEM:
    return "no_mem";
  case ESP_ERR_TIMEOUT:
    return "timeout";
  default:
    return esp_err_to_name(err);
  }
}

/* ------------------------------------------------------------- boot side */

static void deadline_cb(void *arg) {
  (void)arg;
  if (s_confirm_done) {
    return;
  }
  ESP_LOGE(TAG,
           "OTA: image not confirmed within %u s — marking invalid, rollback",
           (unsigned)(OTA_GUARD_CONFIRM_DEADLINE_MS / 1000));
  set_state(OTA_GUARD_ROLLED_BACK, "confirm_timeout");
  journal_save();
  staging_log_state("deadline");
  esp_err_t e = esp_ota_mark_app_invalid_rollback_and_reboot();
  ESP_LOGE(TAG, "rollback_and_reboot failed: %s — plain restart",
           esp_err_to_name(e));
  /* Reset while PENDING_VERIFY → bootloader aborts this image anyway. */
  esp_restart();
}

void ota_guard_boot_early(void) {
  esp_reset_reason_t rr = esp_reset_reason();
  if (s_rtc_magic != OTA_GUARD_RTC_MAGIC || rr == ESP_RST_POWERON) {
    s_rtc_magic = OTA_GUARD_RTC_MAGIC;
    s_rtc_crash_streak = 0;
    s_rtc_fallback_done = 0;
  } else if (is_crash_reset(rr)) {
    s_rtc_crash_streak++;
  } else if (rr == ESP_RST_BROWNOUT || rr == ESP_RST_EXT) {
    /* Battery / external reset is not a firmware crash. */
    s_rtc_crash_streak = 0;
  }

  memset(&s_st, 0, sizeof(s_st));
  s_st.state = OTA_GUARD_IDLE;
  s_st.confirmed = true;

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_app_desc_t *app = esp_app_get_description();
  if (running != NULL) {
    safe_copy(s_st.running_slot, sizeof(s_st.running_slot), running->label);
  }
  if (app != NULL) {
    safe_copy(s_st.running_version, sizeof(s_st.running_version), app->version);
  }

  esp_ota_img_states_t ist = ESP_OTA_IMG_UNDEFINED;
  if (running != NULL &&
      esp_ota_get_state_partition(running, &ist) == ESP_OK &&
      ist == ESP_OTA_IMG_PENDING_VERIFY) {
    s_st.rollback_active = true;
    s_st.confirmed = false;
    const esp_timer_create_args_t targs = {.callback = deadline_cb,
                                           .name = "ota_confirm_dl"};
    if (esp_timer_create(&targs, &s_deadline_timer) == ESP_OK) {
      esp_timer_start_once(s_deadline_timer,
                           (uint64_t)OTA_GUARD_CONFIRM_DEADLINE_MS * 1000ULL);
    }
    ESP_LOGW(TAG,
             "OTA: running %s is PENDING_VERIFY — confirm after %u s stable, "
             "rollback after %u s",
             s_st.running_slot, (unsigned)(OTA_GUARD_CONFIRM_STABLE_MS / 1000),
             (unsigned)(OTA_GUARD_CONFIRM_DEADLINE_MS / 1000));
  } else if (ist == ESP_OTA_IMG_NEW) {
    /* Bootloader without rollback support never moves NEW → PENDING_VERIFY. */
    s_st.confirmed = false;
    ESP_LOGW(TAG, "OTA: running %s in state NEW — bootloader built without "
                  "APP_ROLLBACK, no automatic rollback on this board",
             s_st.running_slot);
  }
  ESP_LOGI(TAG, "OTA guard: reset=%d crash_streak=%u fallback_done=%u",
           (int)rr, (unsigned)s_rtc_crash_streak, (unsigned)s_rtc_fallback_done);
}

/** Switch to the other (valid) slot after a crash loop. Restarts on success. */
static bool try_crash_loop_fallback(const char *reason) {
  if (s_rtc_fallback_done) {
    ESP_LOGW(TAG, "OTA: crash loop, fallback already used this power cycle");
    return false;
  }
  if (!esp_ota_check_rollback_is_possible()) {
    ESP_LOGW(TAG, "OTA: crash loop, no valid image in the other slot");
    return false;
  }
  const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
  esp_app_desc_t d;
  if (other == NULL || esp_ota_get_partition_description(other, &d) != ESP_OK) {
    return false;
  }
  char why[48];
  if (ota_guard_validate_desc(d.project_name, NULL, why, sizeof(why)) != ESP_OK) {
    ESP_LOGW(TAG, "OTA: crash loop, other slot rejected: %s", why);
    return false;
  }
  ESP_LOGE(TAG, "OTA: %u crash resets in a row — switching to %s (%s)",
           (unsigned)s_rtc_crash_streak, other->label, d.version);
  set_target(s_st.running_slot, s_st.running_version);
  set_state(OTA_GUARD_ROLLED_BACK, reason);
  journal_save();
  s_rtc_fallback_done = 1;
  s_rtc_crash_streak = 0;
  esp_err_t e = esp_ota_set_boot_partition(other);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "OTA: set_boot_partition(%s) failed: %s", other->label,
             esp_err_to_name(e));
    return false;
  }
  staging_log_state("crash_loop_fallback");
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
  return true;
}

void ota_guard_init(void) {
  ota_journal_t j;
  bool have = journal_load(&j);
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (!have) {
    const esp_partition_t *inv = esp_ota_get_last_invalid_partition();
    if (inv != NULL && inv != running) {
      esp_app_desc_t d;
      memset(&d, 0, sizeof(d));
      (void)esp_ota_get_partition_description(inv, &d);
      set_target(inv->label, d.version);
      set_state(OTA_GUARD_ROLLED_BACK, "bootloader_rollback");
    }
  } else {
    taskENTER_CRITICAL(&s_mux);
    s_st.seq = j.seq;
    memcpy(s_st.transport, j.transport, sizeof(s_st.transport));
    memcpy(s_st.target_slot, j.target_slot, sizeof(s_st.target_slot));
    memcpy(s_st.target_version, j.target_version, sizeof(s_st.target_version));
    memcpy(s_st.reason, j.reason, sizeof(s_st.reason));
    taskEXIT_CRITICAL(&s_mux);

    bool on_target = running != NULL && j.target_slot[0] != '\0' &&
                     strcmp(running->label, j.target_slot) == 0;
    switch ((ota_guard_state_t)j.phase) {
    case OTA_GUARD_RECEIVING:
      set_state(OTA_GUARD_ABORTED, "reset_during_transfer");
      journal_save();
      break;
    case OTA_GUARD_VERIFYING:
    case OTA_GUARD_REBOOTING:
      if (on_target) {
        taskENTER_CRITICAL(&s_mux);
        s_st.awaiting_confirm = true;
        s_st.percent = 100;
        taskEXIT_CRITICAL(&s_mux);
        set_state(OTA_GUARD_VERIFYING, "awaiting_confirm");
      } else {
        const esp_partition_t *t = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, j.target_slot);
        esp_ota_img_states_t ts = ESP_OTA_IMG_UNDEFINED;
        bool failed_boot = t != NULL &&
                           esp_ota_get_state_partition(t, &ts) == ESP_OK &&
                           (ts == ESP_OTA_IMG_ABORTED || ts == ESP_OTA_IMG_INVALID);
        if (failed_boot) {
          set_state(OTA_GUARD_ROLLED_BACK, "reset_before_confirm");
        } else {
          set_state(OTA_GUARD_ABORTED, "not_activated");
        }
        journal_save();
      }
      break;
    case OTA_GUARD_SUCCESS:
      if (on_target) {
        set_state(OTA_GUARD_SUCCESS, NULL);
      } else {
        set_state(OTA_GUARD_IDLE, "running_other_slot");
      }
      break;
    case OTA_GUARD_ABORTED:
    case OTA_GUARD_ROLLED_BACK:
    case OTA_GUARD_ERROR:
      set_state((ota_guard_state_t)j.phase, NULL);
      break;
    default:
      set_state(OTA_GUARD_IDLE, NULL);
      break;
    }
  }

  staging_log_state("boot");

  if (s_rtc_crash_streak >= OTA_GUARD_CRASH_LOOP_LIMIT && !s_st.rollback_active) {
    (void)try_crash_loop_fallback("crash_loop");
  }
}

bool ota_guard_confirm_if_healthy(bool transport_ready) {
  if (s_confirm_done) {
    return true;
  }
  int64_t up_ms = esp_timer_get_time() / 1000;
  if (up_ms < OTA_GUARD_CONFIRM_STABLE_MS || !transport_ready) {
    return false;
  }

  ota_guard_status_t s;
  ota_guard_get_status(&s);
  if (s.rollback_active || s.awaiting_confirm || !s.confirmed) {
    esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
    if (e != ESP_OK) {
      ESP_LOGE(TAG, "OTA: mark_app_valid failed: %s (retry)", esp_err_to_name(e));
      return false;
    }
  }
  if (s_deadline_timer != NULL) {
    esp_timer_stop(s_deadline_timer);
  }
  s_rtc_crash_streak = 0;
  s_rtc_fallback_done = 0;
  s_confirm_done = true;

  bool report_success = false;
  taskENTER_CRITICAL(&s_mux);
  s_st.confirmed = true;
  if (s_st.awaiting_confirm && !s_live && s_st.state == OTA_GUARD_VERIFYING) {
    s_st.awaiting_confirm = false;
    s_st.state = OTA_GUARD_SUCCESS;
    safe_copy(s_st.reason, sizeof(s_st.reason), "confirmed");
    safe_copy(s_st.target_version, sizeof(s_st.target_version),
              s_st.running_version);
    report_success = true;
  }
  taskEXIT_CRITICAL(&s_mux);

  if (report_success) {
    journal_save();
    ESP_LOGI(TAG, "OTA: new image %s (%s) confirmed after %lld ms — success",
             s.running_slot, s.running_version, (long long)up_ms);
  } else {
    ESP_LOGI(TAG, "OTA guard: stable start confirmed (%lld ms)", (long long)up_ms);
  }
  staging_log_state("confirm");
  return true;
}

void ota_guard_on_init_failure(const char *what) {
  ESP_LOGE(TAG, "OTA guard: init failure (%s)", what != NULL ? what : "?");
  ota_guard_status_t s;
  ota_guard_get_status(&s);
  if (s.rollback_active && !s_confirm_done) {
    set_state(OTA_GUARD_ROLLED_BACK, "init_failed");
    journal_save();
    staging_log_state("init_failed");
    esp_err_t e = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGE(TAG, "rollback_and_reboot failed: %s — plain restart",
             esp_err_to_name(e));
    esp_restart();
  }
  if (s_rtc_fallback_done || !esp_ota_check_rollback_is_possible()) {
    return;
  }
  s_rtc_crash_streak++;
  if (s_rtc_crash_streak >= OTA_GUARD_CRASH_LOOP_LIMIT) {
    (void)try_crash_loop_fallback("crash_loop_init_failed");
    return;
  }
  ESP_LOGW(TAG, "OTA guard: restart %u/%u before falling back to other slot",
           (unsigned)s_rtc_crash_streak, (unsigned)OTA_GUARD_CRASH_LOOP_LIMIT);
  vTaskDelay(pdMS_TO_TICKS(3000));
  esp_restart();
}

/* -------------------------------------------------------- transport side */

void ota_guard_begin(const char *transport, const esp_partition_t *target) {
  taskENTER_CRITICAL(&s_mux);
  s_live = true;
  s_st.state = OTA_GUARD_RECEIVING;
  s_st.percent = 0;
  s_st.seq++;
  s_st.resumable = false;
  s_st.awaiting_confirm = false;
  safe_copy(s_st.transport, sizeof(s_st.transport), transport);
  safe_copy(s_st.target_slot, sizeof(s_st.target_slot),
            target != NULL ? target->label : "");
  s_st.target_version[0] = '\0';
  s_st.reason[0] = '\0';
  taskEXIT_CRITICAL(&s_mux);
  s_last_logged_decile = -1;
  journal_save();
  staging_log_state("begin");
}

void ota_guard_progress(int percent) {
  if (percent < 0) {
    percent = 0;
  }
  if (percent > 100) {
    percent = 100;
  }
  taskENTER_CRITICAL(&s_mux);
  s_st.percent = percent;
  taskEXIT_CRITICAL(&s_mux);
#if CHESS_DEBUG_MODE
  int dec = percent / 10;
  if (dec != s_last_logged_decile) {
    s_last_logged_decile = dec;
    ESP_LOGI(TAG, "[STAGING] OTA receiving %d%%", percent);
  }
#endif
}

void ota_guard_aborted(const char *reason, bool resumable) {
  taskENTER_CRITICAL(&s_mux);
  s_live = resumable;
  s_st.resumable = resumable;
  taskEXIT_CRITICAL(&s_mux);
  set_state(OTA_GUARD_ABORTED, reason);
  journal_save();
  staging_log_state("aborted");
}

void ota_guard_resumed(void) {
  taskENTER_CRITICAL(&s_mux);
  s_live = true;
  s_st.resumable = false;
  taskEXIT_CRITICAL(&s_mux);
  set_state(OTA_GUARD_RECEIVING, "");
  journal_save();
  staging_log_state("resumed");
}

void ota_guard_error(const char *reason) {
  taskENTER_CRITICAL(&s_mux);
  s_live = false;
  s_st.resumable = false;
  taskEXIT_CRITICAL(&s_mux);
  set_state(OTA_GUARD_ERROR, reason);
  journal_save();
  staging_log_state("error");
}

void ota_guard_verifying(void) {
  set_state(OTA_GUARD_VERIFYING, "");
  journal_save();
  staging_log_state("verifying");
}

esp_err_t ota_guard_validate_desc(const char *project_name, const char *version,
                                  char *why, size_t why_cap) {
  const esp_app_desc_t *self = esp_app_get_description();
  if (project_name == NULL || self == NULL ||
      strncmp(project_name, self->project_name, sizeof(self->project_name)) != 0) {
    if (why != NULL && why_cap > 0) {
      snprintf(why, why_cap, "wrong_project");
    }
    ESP_LOGE(TAG, "OTA image project '%s' != running '%s'",
             project_name != NULL ? project_name : "?",
             self != NULL ? self->project_name : "?");
    return ESP_ERR_INVALID_VERSION;
  }
  if (version != NULL) {
    set_target(NULL, version);
  }
  return ESP_OK;
}

esp_err_t ota_guard_validate_image(const esp_partition_t *target, char *why,
                                   size_t why_cap) {
  esp_app_desc_t d;
  if (target == NULL || esp_ota_get_partition_description(target, &d) != ESP_OK) {
    if (why != NULL && why_cap > 0) {
      snprintf(why, why_cap, "image_invalid");
    }
    return ESP_ERR_OTA_VALIDATE_FAILED;
  }
  return ota_guard_validate_desc(d.project_name, d.version, why, why_cap);
}

void ota_guard_rebooting(const esp_partition_t *target) {
  taskENTER_CRITICAL(&s_mux);
  s_live = false;
  s_st.percent = 100;
  if (target != NULL) {
    safe_copy(s_st.target_slot, sizeof(s_st.target_slot), target->label);
  }
  taskEXIT_CRITICAL(&s_mux);
  set_state(OTA_GUARD_REBOOTING, "");
  journal_save();
  staging_log_state("rebooting");
}

static void restart_cb(void *arg) {
  (void)arg;
  ESP_LOGI(TAG, "OTA: restarting into new image");
  esp_restart();
}

void ota_guard_schedule_restart(uint32_t delay_ms) {
  if (s_restart_timer == NULL) {
    const esp_timer_create_args_t targs = {.callback = restart_cb,
                                           .name = "ota_restart"};
    if (esp_timer_create(&targs, &s_restart_timer) != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
      esp_restart();
      return;
    }
  }
  esp_timer_stop(s_restart_timer);
  esp_timer_start_once(s_restart_timer, (uint64_t)delay_ms * 1000ULL);
}
