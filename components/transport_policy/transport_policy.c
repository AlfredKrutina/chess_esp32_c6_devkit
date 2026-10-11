/**
 * @file transport_policy.c
 * @brief V2 session policy: one place for BLE-vs-WiFi transport decisions.
 *
 * Rules:
 * - Boot with BLE advertising on; start WiFi only if NVS already has STA
 *   credentials or the user hotspot (AP) was explicitly enabled.
 * - BLE is the default realtime path into the game queue.
 * - WiFi is for bulk (OTA, large JSON) and automatic failover when BLE drops.
 * - Coexistence preference follows the active realtime link.
 * - No password or token dumps in logs.
 */
#include "transport_policy.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs.h"

#if CONFIG_BT_ENABLED
#include "host/ble_gap.h"
#endif

#if CONFIG_ESP_COEX_ENABLED
#include "esp_coexist.h"
#endif

#include <string.h>

static const char *TAG = "TRANSPORT_POLICY";

#define WIFI_NVS_NAMESPACE "wifi_config"
#define WIFI_NVS_KEY_SSID "sta_ssid"
#define WIFI_NVS_KEY_AP_USER "ap_user_en"

/* Preferred BLE connection parameters for low-latency moves.
 * interval units are 1.25 ms: 0x0006 = 7.5 ms, 0x000C = 15 ms.
 * supervision timeout units are 10 ms: 0x0032 = 500 ms.
 * The central (phone) may override these, but the request lets iOS/Android
 * know the peripheral wants a tight interval for realtime packets. */
#define TP_BLE_PREFERRED_ITVL_MIN 0x0006
#define TP_BLE_PREFERRED_ITVL_MAX 0x000C
#define TP_BLE_PREFERRED_LATENCY  0
#define TP_BLE_PREFERRED_SUPERVISION_TIMEOUT 0x0032

static bool s_wifi_started = false;

void transport_policy_init(void) {
  const bool wifi_on_boot = transport_policy_should_enable_wifi_on_boot();
  ESP_LOGI(TAG,
           "V2 session policy init: BLE primary realtime, WiFi bulk/failover; "
           "WiFi on boot=%s",
           wifi_on_boot ? "yes" : "no (lazy start)");
}

bool transport_policy_should_enable_wifi_on_boot(void) {
  /* Start WiFi at boot if the board has STA credentials or the user has
   * previously enabled the AP hotspot. Otherwise stay quiet to save airtime
   * until the app provisions credentials over BLE. */
  char ssid[33] = {0};
  bool has_sta_creds = false;
  nvs_handle_t h;
  if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
    size_t ssid_len = sizeof(ssid);
    if (nvs_get_str(h, WIFI_NVS_KEY_SSID, ssid, &ssid_len) == ESP_OK &&
        ssid[0] != '\0') {
      has_sta_creds = true;
    }
    nvs_close(h);
  }

  uint8_t ap_user = 0;
  if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
    (void)nvs_get_u8(h, WIFI_NVS_KEY_AP_USER, &ap_user);
    nvs_close(h);
  }

  /* Password intentionally not logged. */
  ESP_LOGI(TAG,
           "boot check: STA credentials=%s, AP user enabled=%s",
           has_sta_creds ? "yes" : "no",
           ap_user ? "yes" : "no");
  return has_sta_creds || (ap_user != 0);
}

void transport_policy_ensure_wifi_started(void) {
  if (s_wifi_started) {
    return;
  }
  esp_err_t err = esp_wifi_start();
  if (err == ESP_OK) {
    s_wifi_started = true;
    ESP_LOGI(TAG, "WiFi started lazily (provisioning or reconnect request)");
  } else {
    ESP_LOGW(TAG, "Lazy WiFi start failed: %s", esp_err_to_name(err));
  }
}

static void transport_policy_prefer_ble_coex(void) {
#if CONFIG_ESP_COEX_ENABLED
  esp_err_t err = esp_coex_preference_set(ESP_COEX_PREFER_BT);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "coexistence PREFER_BT failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "coexistence: PREFER_BT while BLE connected");
  }
#else
  ESP_LOGD(TAG, "coexistence not enabled in this build");
#endif
}

static void transport_policy_prefer_wifi_coex(void) {
#if CONFIG_ESP_COEX_ENABLED
  esp_err_t err = esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "coexistence PREFER_WIFI failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "coexistence: PREFER_WIFI after BLE disconnect");
  }
#else
  ESP_LOGD(TAG, "coexistence not enabled in this build");
#endif
}

static void transport_policy_request_ble_params(uint16_t conn_handle) {
#if CONFIG_BT_ENABLED
  struct ble_gap_upd_params params = {
      .itvl_min = TP_BLE_PREFERRED_ITVL_MIN,
      .itvl_max = TP_BLE_PREFERRED_ITVL_MAX,
      .latency = TP_BLE_PREFERRED_LATENCY,
      .supervision_timeout = TP_BLE_PREFERRED_SUPERVISION_TIMEOUT,
      .min_ce_len = 0,
      .max_ce_len = 0,
  };
  int rc = ble_gap_update_params(conn_handle, &params);
  if (rc != 0) {
    ESP_LOGW(TAG,
             "BLE connection parameter request failed rc=%d "
             "(phone may choose its own interval)",
             rc);
  } else {
    ESP_LOGI(TAG,
             "BLE requested interval %u-%u (x1.25 ms), latency=%u, timeout=%u (x10 ms)",
             (unsigned)params.itvl_min, (unsigned)params.itvl_max,
             (unsigned)params.latency,
             (unsigned)params.supervision_timeout);
  }
#else
  (void)conn_handle;
#endif
}

void transport_policy_on_ble_connect(uint16_t conn_handle) {
  ESP_LOGI(TAG,
           "realtime path: BLE (handle=%u) — commands go through GATT",
           (unsigned)conn_handle);
  transport_policy_request_ble_params(conn_handle);
  transport_policy_prefer_ble_coex();
}

void transport_policy_on_ble_disconnect(void) {
  ESP_LOGI(TAG, "BLE realtime path lost; WiFi remains available for failover");
  transport_policy_prefer_wifi_coex();
}

void transport_policy_log_realtime_path_ble(void) {
  ESP_LOGI(TAG, "realtime path: BLE");
}

void transport_policy_log_realtime_path_wifi(void) {
  ESP_LOGI(TAG, "realtime path: WiFi");
}

void transport_policy_log_failover_to_wifi(void) {
  ESP_LOGI(TAG, "failover: realtime path switching from BLE to WiFi");
}
