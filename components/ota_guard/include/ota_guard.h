/**
 * @file ota_guard.h
 * @brief OTA state machine and boot guard shared by all OTA transports.
 *
 * Single source of truth for the state reported to the app
 * (`GET /api/system/ota/status`, BLE `ota_ble_status`, `GET /api/system/firmware`).
 *
 * Boot side (called from main):
 *  - ota_guard_boot_early(): first thing in app_main, no NVS needed. Arms the
 *    confirm deadline when the running image is PENDING_VERIFY and counts
 *    crash resets in RTC memory.
 *  - ota_guard_init(): after NVS init. Resolves the outcome of the previous
 *    OTA attempt from the NVS journal + otadata and performs the crash-loop
 *    fallback to the other slot.
 *  - ota_guard_confirm_if_healthy(): from the main loop. Confirms the image
 *    (esp_ota_mark_app_valid_cancel_rollback) only after a stable uptime with
 *    an OTA transport up.
 *  - ota_guard_on_init_failure(): from safe-mode branches.
 *
 * Transport side (ota_update.c): begin → progress → verifying → validate →
 * rebooting, or aborted / error. Success is reported only after the new image
 * confirmed itself on the next boot.
 */
#ifndef OTA_GUARD_H
#define OTA_GUARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OTA_GUARD_IDLE = 0,
  OTA_GUARD_RECEIVING,
  OTA_GUARD_VERIFYING,
  OTA_GUARD_REBOOTING,
  OTA_GUARD_SUCCESS,
  OTA_GUARD_ABORTED,
  OTA_GUARD_ROLLED_BACK,
  OTA_GUARD_ERROR,
} ota_guard_state_t;

/** Uptime the new image must survive with an OTA transport up before it is confirmed. */
#define OTA_GUARD_CONFIRM_STABLE_MS 30000
/** PENDING_VERIFY image not confirmed by then → mark invalid + reboot into previous slot. */
#define OTA_GUARD_CONFIRM_DEADLINE_MS 180000
/** Consecutive crash resets (panic / WDT / init failure) before switching to the other slot. */
#define OTA_GUARD_CRASH_LOOP_LIMIT 3

typedef struct {
  ota_guard_state_t state;
  int percent;
  /** Running image is confirmed (VALID or rollback not applicable). */
  bool confirmed;
  /** Running image is the freshly flashed one and waits for confirm. */
  bool awaiting_confirm;
  /** BLE session kept after disconnect; client may resume from `bytes`. */
  bool resumable;
  /** Bootloader marked the running image PENDING_VERIFY (rollback really active). */
  bool rollback_active;
  uint32_t seq;
  char transport[8];
  char reason[48];
  char target_slot[17];
  char target_version[32];
  char running_slot[17];
  char running_version[32];
} ota_guard_status_t;

const char *ota_guard_state_str(ota_guard_state_t st);

/* ---- boot side ---- */
void ota_guard_boot_early(void);
void ota_guard_init(void);
/**
 * @param transport_ready at least one OTA transport (HTTP server or BLE host) is up
 * @return true when the running image is confirmed (now or earlier)
 */
bool ota_guard_confirm_if_healthy(bool transport_ready);
/**
 * Safe-mode hook. PENDING_VERIFY image → rollback + reboot (does not return).
 * Confirmed image with a valid other slot → counts toward crash loop and restarts
 * (does not return). Otherwise returns and the caller keeps its UART safe mode.
 */
void ota_guard_on_init_failure(const char *what);

/* ---- transport side ---- */
/** Start of a transfer into @p target. @p transport: "https" | "http" | "ble". */
void ota_guard_begin(const char *transport, const esp_partition_t *target);
void ota_guard_progress(int percent);
/** Transfer interrupted; image is not activated. @p resumable: BLE session kept. */
void ota_guard_aborted(const char *reason, bool resumable);
/** BLE session resumed after reconnect. */
void ota_guard_resumed(void);
void ota_guard_error(const char *reason);
/** All bytes received; esp_ota_end / image verification follows. */
void ota_guard_verifying(void);
/**
 * Checks the app descriptor of a fully written image against the running app
 * (project name). Fills @p why on failure.
 */
esp_err_t ota_guard_validate_image(const esp_partition_t *target, char *why,
                                   size_t why_cap);
/** Same check from an already parsed descriptor (HTTPS: esp_https_ota_get_img_desc). */
esp_err_t ota_guard_validate_desc(const char *project_name, const char *version,
                                  char *why, size_t why_cap);
/** Boot partition switched to @p target; restart follows. */
void ota_guard_rebooting(const esp_partition_t *target);
/** esp_restart after @p delay_ms from an esp_timer (lets BLE/HTTP responses go out). */
void ota_guard_schedule_restart(uint32_t delay_ms);

/** Human readable mapping of esp_ota / esp_https_ota errors to stable reason codes. */
const char *ota_guard_reason_from_err(esp_err_t err);

void ota_guard_get_status(ota_guard_status_t *out);
/**
 * Compact JSON object body fields (no braces), e.g.
 * `"state":"receiving","percent":12,...` — callers wrap it.
 * @return number of characters written (snprintf semantics) or <0
 */
int ota_guard_format_json_fields(char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
