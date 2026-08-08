/**
 * @file ble_nimble_impl.c
 * @brief NimBLE GATT — only compiles when CONFIG_BT_ENABLED.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if CONFIG_BT_ENABLED

#include "ble_task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/** NimBLE store/config — in ESP-IDF examples (bleprph) without a prototype in the public header. */
void ble_store_config_init(void);
#include "esp_task_wdt.h"
#include "game_state_notify.h"
#include "ota_update.h"
#include "web_server_task.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if CONFIG_ESP_COEX_ENABLED
#include "esp_coexist.h"
#endif

static bool s_snap_notify_enabled;

static const char *TAG = "BLE_NIMBLE";

/*
 * UUID: A0B40001-9267-4AB6-BDCC-E8336F8A8D9E
 *
 * IMPORTANT: BLE_UUID128_INIT requires LITTLE-ENDIAN byte order —
 * i.e. REVERSE compared to a readable UUID string (RFC 4122 = big-endian).
 *
 * Bad (big-endian, broken):
 * 0xa0,0xb4,0x00,0x01, 0x92,0x67, 0x4a,0xb6, 0xbd,0xcc,
 * 0xe8,0x33,0x6f,0x8a,0x8d,0x9e Correct (little-endian, functional):
 * 0x9e,0x8d,0x8a,0x6f, 0x33,0xe8, 0xcc,0xbd, 0xb6,0x4a, 0x67,0x92,
 * 0x01.0x00.0xb4.0x0
 *
 * The original big-endian version caused: iOS to scan UUID "A0B40001-..."
 * but ESP advertised "9E8D8A6F-..." → the phone never found the checkerboard!
 */
static const ble_uuid128_t czechmate_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd, 0xb6, 0x4a,
                     0x67, 0x92, 0x01, 0x00, 0xb4, 0xa0);

static const ble_uuid128_t czechmate_snap_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd, 0xb6, 0x4a,
                     0x67, 0x92, 0x02, 0x00, 0xb4, 0xa0);

static const ble_uuid128_t czechmate_cmd_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd, 0xb6, 0x4a,
                     0x67, 0x92, 0x03, 0x00, 0xb4, 0xa0);

static const ble_uuid128_t czechmate_net_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd, 0xb6, 0x4a,
                     0x67, 0x92, 0x04, 0x00, 0xb4, 0xa0);

/** A0B40005-… — JSON result of last CMD (notify); iOS parses channel=cmd_ack */
static const ble_uuid128_t czechmate_cmd_ack_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd, 0xb6, 0x4a,
                     0x67, 0x92, 0x05, 0x00, 0xb4, 0xa0);

static uint16_t g_snap_val_handle;
static uint16_t g_cmd_val_handle;
static uint16_t g_net_val_handle;
static uint16_t g_cmd_ack_val_handle;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_net_notify_enabled = false;
static bool s_cmd_ack_notify_enabled = false;

#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
/** Deferred retry of SMP after CONNECT — iOS sometimes does not respond to the first security request. */
static struct ble_npl_callout s_sec_retry_co;
static bool s_sec_retry_co_ready;
static uint8_t s_sec_deferred_attempts;
#endif

/** Periodically verify GAP advertising when no central office is connected. */
static esp_timer_handle_t s_adv_watchdog_timer;
/** Schedule watchdog logic to the default NimBLE event queue (GAP only from host
 * context). ble_hs_sched() is not in the ESP-IDF 5.5 / NimBLE public API. */
static struct ble_npl_event s_idle_adv_watchdog_npl_ev;
static bool s_idle_adv_watchdog_npl_ev_inited;

/** One GATT write at a time per guest task — saves stack compared to local tmp[768]. */
static char s_ble_cmd_copy[768];

static int czechmate_gap_event(struct ble_gap_event *event, void *arg);

/** Leave only safe characters for JSON string in cmd. */
static void ble_sanitize_cmd_token(const char *src, char *dst, size_t dst_cap) {
  if (dst_cap == 0) {
    return;
  }
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && j + 1 < dst_cap; i++) {
    unsigned char c = (unsigned char)src[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
      dst[j++] = (char)c;
    }
  }
  dst[j] = '\0';
}

#ifndef BLE_CMD_ACK_JSON_MAX_LEN
#define BLE_CMD_ACK_JSON_MAX_LEN 2048
#endif

void ble_task_notify_cmd_ack_json(const char *json_utf8) {
  if (json_utf8 == NULL) {
    return;
  }
  if (g_cmd_ack_val_handle == 0 ||
      s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_cmd_ack_notify_enabled) {
    ESP_LOGD(TAG, "cmd_ack_json skipped (no conn/sub)");
    return;
  }
  size_t len = strlen(json_utf8);
  if (len > BLE_CMD_ACK_JSON_MAX_LEN) {
    ESP_LOGW(TAG, "cmd_ack_json truncating %u to %u B",
             (unsigned)len, (unsigned)BLE_CMD_ACK_JSON_MAX_LEN);
    len = BLE_CMD_ACK_JSON_MAX_LEN;
  }
  struct os_mbuf *om = ble_hs_mbuf_from_flat(json_utf8, (uint16_t)len);
  if (om == NULL) {
    ESP_LOGW(TAG, "cmd_ack_json: mbuf alloc failed");
    return;
  }
  int rc = ble_gatts_notify_custom(s_conn_handle, g_cmd_ack_val_handle, om);
  if (rc != 0) {
    ESP_LOGW(TAG, "cmd_ack_json: notify_custom rc=%d", rc);
  }
}

void ble_task_notify_command_result(esp_err_t err, const char *json_body) {
  char cmd_safe[48] = {0};
  if (json_body != NULL) {
    char raw[44] = {0};
    if (web_server_ble_extract_cmd_for_ack(json_body, raw, sizeof(raw))) {
      ble_sanitize_cmd_token(raw, cmd_safe, sizeof(cmd_safe));
    }
  }

  const char *code = "internal_error";
  const char *ok_str = "false";
  const char *msg = "Internal error while processing command.";

  switch (err) {
  case ESP_OK:
    ok_str = "true";
    code = "ok";
    msg = "Command accepted.";
    break;
  case ESP_ERR_NOT_SUPPORTED:
    code = "unknown_command";
    msg = "Board does not recognize this command.";
    break;
  case ESP_ERR_NOT_FOUND:
    code = "missing_cmd";
    msg = "JSON is missing the cmd field.";
    break;
  case ESP_ERR_INVALID_ARG:
    code = "invalid_argument";
    msg = "Invalid command parameters.";
    break;
  case ESP_ERR_INVALID_STATE:
    code = "blocked";
    msg = "Action is blocked (web lock or board state).";
    break;
  case ESP_ERR_NOT_FINISHED:
    code = "tutorial_finish_conflict";
    msg =
        "Physical board does not match the starting position (ranks 1-2 and 7-8 full).";
    break;
  case ESP_FAIL:
    code = "internal_error";
    msg = "Internal error while processing command.";
    break;
  default:
    code = "error";
    msg = "Error while processing command.";
    break;
  }

  char out[256];
  int n = snprintf(out, sizeof(out),
                   "{\"channel\":\"cmd_ack\",\"ok\":%s,\"code\":\"%s\","
                   "\"cmd\":\"%s\",\"message\":\"%s\",\"esp\":%d}",
                   ok_str, code, cmd_safe, msg, (int)err);
  if (n < 0 || (size_t)n >= sizeof(out)) {
    ESP_LOGW(TAG, "cmd_ack JSON truncate");
  }

  if (err == ESP_OK) {
    ESP_LOGD(TAG, "[cmd_ack] %s", out);
  } else {
    ESP_LOGI(TAG, "[cmd_ack] %s", out);
  }

  if (g_cmd_ack_val_handle == 0 ||
      s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_cmd_ack_notify_enabled) {
    ESP_LOGD(TAG,
             "cmd_ack notify skipped (h=%u conn=%d sub=%d)",
             (unsigned)g_cmd_ack_val_handle, (int)s_conn_handle,
             (int)s_cmd_ack_notify_enabled);
    return;
  }

  size_t len = strlen(out);
  struct os_mbuf *om = ble_hs_mbuf_from_flat(out, (uint16_t)len);
  if (om == NULL) {
    ESP_LOGW(TAG, "cmd_ack: mbuf alloc failed");
    return;
  }
  int rc = ble_gatts_notify_custom(s_conn_handle, g_cmd_ack_val_handle, om);
  if (rc != 0) {
    ESP_LOGW(TAG, "cmd_ack: notify_custom rc=%d", rc);
  }
}

static int czechmate_build_network_json(char *buf, size_t cap) {
  char sta_ip[16] = {0};
  char sta_ssid[33] = {0};
  esp_err_t ip_ret = wifi_get_sta_ip(sta_ip, sizeof(sta_ip));
  esp_err_t ssid_ret = wifi_get_sta_ssid(sta_ssid, sizeof(sta_ssid));
  bool sta_connected = (ip_ret == ESP_OK && sta_ip[0] != '\0');
  const bool ap_on = wifi_ap_is_broadcasting();
  const char *ap_ip_lit = ap_on ? "192.168.4.1" : "";
  const char *ap_ssid_lit = wifi_ap_effective_ssid();

  snprintf(buf, cap,
           "{"
           "\"sta_connected\":%s,"
           "\"sta_ip\":\"%s\","
           "\"sta_ssid\":\"%s\","
           "\"ap_ip\":\"%s\","
           "\"ap_ssid\":\"%s\","
           "\"ap_active\":%s,"
           "\"online\":%s"
           "}",
           sta_connected ? "true" : "false",
           sta_connected ? sta_ip : "",
           (ssid_ret == ESP_OK && sta_ssid[0] != '\0') ? sta_ssid : "",
           ap_ip_lit,
           ap_ssid_lit,
           ap_on ? "true" : "false",
           sta_connected ? "true" : "false");
  return 0;
}

static int czechmate_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
  (void)arg;
  ESP_LOGD(TAG,
           "[STAGING] GATT access conn=%u op=%d attr=%u snap_h=%u cmd_h=%u net_h=%u",
           (unsigned)conn_handle, (int)ctxt->op, (unsigned)attr_handle,
           (unsigned)g_snap_val_handle, (unsigned)g_cmd_val_handle,
           (unsigned)g_net_val_handle);
  switch (ctxt->op) {
  case BLE_GATT_ACCESS_OP_READ_CHR:
    if (attr_handle == g_snap_val_handle) {
      char *snap = NULL;
      size_t len = 0;
      esp_err_t e = web_server_build_game_snapshot_json_shared(&snap, &len);
      if (e != ESP_OK || len == 0 || snap == NULL) {
        ESP_LOGW(TAG,
                 "[STAGING] BLE read snapshot: build failed (%s)",
                 esp_err_to_name(e));
        return BLE_ATT_ERR_UNLIKELY;
      }
      if (len > 65535U) {
        ESP_LOGW(TAG, "[STAGING] BLE read snapshot: JSON > 65535 B");
        return BLE_ATT_ERR_UNLIKELY;
      }
      int rc = os_mbuf_append(ctxt->om, snap, (uint16_t)len);
      if (rc != 0) {
        ESP_LOGW(TAG, "[STAGING] os_mbuf_append failed rc=%d", rc);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      return 0;
    }
    if (attr_handle == g_net_val_handle) {
      char net_json[384];
      czechmate_build_network_json(net_json, sizeof(net_json));
      size_t len = strlen(net_json);
      int rc = os_mbuf_append(ctxt->om, net_json, (uint16_t)len);
      if (rc != 0) {
        ESP_LOGW(TAG, "[STAGING] os_mbuf_append network failed rc=%d", rc);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      ESP_LOGI(TAG, "[STAGING] BLE read network: %s", net_json);
      return 0;
    }
    break;
  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    if (attr_handle == g_cmd_val_handle) {
      /*
       * hint_highlight / hint_clear: synchronous web_server_ble_command_dispatch in frame
       * ATT write; after it cmd_ack notify on another characteristic. Snapshot notify is running
       * separately (game task → ble_task_push_snapshot_json) — different GATT handle; NimBLE
       * serializes notifications; long snapshot chunks may delay further notifications, but
       * must not drop CMD itself (verify when debugging MTU and board load).
       */
      uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
      uint8_t tmp[768];
      uint16_t n = om_len > sizeof(tmp) ? (uint16_t)sizeof(tmp) : om_len;
      if (n > 0) {
        os_mbuf_copydata(ctxt->om, 0, n, tmp);
      }
      /* Raw firmware chunky: magic OB + idx/total + payload (nezapisovat do JSON bufferu). */
      if (n >= 7 && tmp[0] == 'O' && tmp[1] == 'B') {
        if (!ble_task_conn_is_encrypted()) {
          ESP_LOGW(TAG, "OTA BLE chunk rejected: link not encrypted");
          return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
        }
        esp_err_t derr = ota_update_ble_feed_chunk(tmp, n);
        static const char ack_chunk_bad[] =
            "{\"cmd\":\"ota_ble_chunk\",\"ok\":false}";
        /*
         * Success chunk: don't send cmd_ack notify — OTA client doesn't wait for ACK,
         * only on ATT write response; notify each chunk overflows the queue on iOS.
         */
        if (derr != ESP_OK) {
          ble_task_notify_command_result(derr, ack_chunk_bad);
        }
        if (derr == ESP_OK || derr == ESP_ERR_NOT_SUPPORTED) {
          return 0;
        }
        if (derr == ESP_ERR_INVALID_STATE) {
          ESP_LOGW(TAG, "OTA BLE chunk blocked: %s", esp_err_to_name(derr));
          return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
        }
        if (derr == ESP_ERR_INVALID_ARG || derr == ESP_ERR_INVALID_SIZE) {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        ESP_LOGW(TAG, "OTA BLE chunk: %s", esp_err_to_name(derr));
        return BLE_ATT_ERR_UNLIKELY;
      }
      /* Must conform to web_server_ble_command_dispatch (larger JSON: timer/virtual). */
      if (om_len >= sizeof(s_ble_cmd_copy)) {
        om_len = (uint16_t)(sizeof(s_ble_cmd_copy) - 1);
      }
      if (om_len > 0) {
        os_mbuf_copydata(ctxt->om, 0, om_len, s_ble_cmd_copy);
      }
      s_ble_cmd_copy[om_len] = '\0';
      esp_err_t derr =
          web_server_ble_command_dispatch(s_ble_cmd_copy, om_len);
      if (!web_server_ble_dispatch_custom_ack_was_sent()) {
        ble_task_notify_command_result(derr, s_ble_cmd_copy);
      }
      if (derr == ESP_OK || derr == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGD(TAG,
                 "[STAGING] CMD dispatch %s (%u B) — ATT write response OK",
                 esp_err_to_name(derr), (unsigned)om_len);
        return 0;
      }
      if (derr == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "CMD blocked: %s", esp_err_to_name(derr));
        return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
      }
      if (derr == ESP_ERR_NOT_ALLOWED) {
        ESP_LOGW(TAG, "CMD not allowed: %s", esp_err_to_name(derr));
        return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
      }
      if (derr == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "CMD missing field");
        return BLE_ATT_ERR_ATTR_NOT_FOUND;
      }
      if (derr == ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "CMD invalid JSON/args");
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      ESP_LOGW(TAG, "CMD dispatch: %s", esp_err_to_name(derr));
      return BLE_ATT_ERR_UNLIKELY;
    }
    break;
  default:
    break;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def czechmate_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &czechmate_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &czechmate_snap_chr_uuid.u,
                    .access_cb = czechmate_gatt_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &g_snap_val_handle,
                },
                {
                    .uuid = &czechmate_cmd_chr_uuid.u,
                    .access_cb = czechmate_gatt_access,
                    .flags = BLE_GATT_CHR_F_WRITE,
                    .val_handle = &g_cmd_val_handle,
                },
                {
                    .uuid = &czechmate_net_chr_uuid.u,
                    .access_cb = czechmate_gatt_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &g_net_val_handle,
                },
                {
                    .uuid = &czechmate_cmd_ack_chr_uuid.u,
                    .access_cb = czechmate_gatt_access,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &g_cmd_ack_val_handle,
                },
                {
                    0,
                },
            },
    },
    {
        0,
    },
};

static void czechmate_advertise(void) {
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  struct ble_hs_adv_fields rsp_fields;
  int rc;

  ESP_LOGD(TAG, "[STAGING] czechmate_advertise: set ADV + scanRsp fields");

  /*
   * Primary ADV packet (max 31 B):
   * Flags: 3 B (2 header + 1 value)
   * UUID128: 18 B (2 header + 16 UUID)
   * = 21 B — OK, below the 31 B limit.
   * Name moved to Scan Response (makes room, iOS 13+ sends ScanReq
   * automatically).
   */
  memset(&fields, 0, sizeof(fields));
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.uuids128 = &czechmate_svc_uuid;
  fields.num_uuids128 = 1;
  fields.uuids128_is_complete = 1;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
    return;
  }

  /* Scan Response: full name (iOS reads it as
   * CBAdvertisementDataLocalNameKey). */
  memset(&rsp_fields, 0, sizeof(rsp_fields));
  const char *name = "CZECHMATE";
  rsp_fields.name = (uint8_t *)name;
  rsp_fields.name_len = strlen(name);
  rsp_fields.name_is_complete = 1;

  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  if (rc != 0) {
    /* Scan response is not critical — log but continue */
    ESP_LOGW(TAG, "adv_rsp_set_fields rc=%d", rc);
  }

  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params,
                         czechmate_gap_event, NULL);
  if (rc != 0) {
    /* Fallback: RPA (random private address) — if the ESP does not have a valid public BD
     * addr. */
    ESP_LOGW(TAG, "adv_start PUBLIC rc=%d — fallback na RPA", rc);
    rc =
        ble_gap_adv_start(BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT, NULL, BLE_HS_FOREVER,
                          &adv_params, czechmate_gap_event, NULL);
    if (rc != 0) {
      ESP_LOGE(TAG, "adv_start RPA fallback rc=%d", rc);
      return;
    }
    ESP_LOGD(TAG, "[STAGING] adv_start ok (RPA address)");
  } else {
    ESP_LOGD(TAG, "[STAGING] adv_start ok (PUBLIC address)");
  }
}

/**
 * Discoverable advertising (undirected GAP) will start again.
 * Call after a connection drop or when the watchdog detects that ADV has stopped — iOS then board
 * will see again in the scan even after hours of uptime.
 */
static void czechmate_gap_restart_advertising(void) {
  int sr = ble_gap_adv_stop();
  if (sr != 0) {
    ESP_LOGD(TAG, "adv_stop rc=%d (typically OK if ADV was no longer running)", sr);
  }
  czechmate_advertise();
}

static void czechmate_idle_adv_watchdog_host(void *param) {
  (void)param;
  if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
    return;
  }
  if (ble_gap_adv_active()) {
    return;
  }
  ESP_LOGW(TAG,
           "GAP: no BLE connection and advertising is not running — restart GAP (visibility "
           "pro sken v aplikaci)");
  czechmate_gap_restart_advertising();
}

static void czechmate_idle_adv_watchdog_npl_cb(struct ble_npl_event *ev) {
  (void)ev;
  czechmate_idle_adv_watchdog_host(NULL);
}

static void adv_watchdog_timer_cb(void *arg) {
  (void)arg;
  if (!ble_hs_synced()) {
    return;
  }
  struct ble_npl_eventq *evq = nimble_port_get_dflt_eventq();
  if (evq == NULL) {
    return;
  }
  if (!s_idle_adv_watchdog_npl_ev_inited) {
    ble_npl_event_init(&s_idle_adv_watchdog_npl_ev,
                       czechmate_idle_adv_watchdog_npl_cb, NULL);
    s_idle_adv_watchdog_npl_ev_inited = true;
  }
  ble_npl_eventq_put(evq, &s_idle_adv_watchdog_npl_ev);
}

static void czechmate_start_adv_watchdog_timer(void) {
  if (s_adv_watchdog_timer != NULL) {
    return;
  }
  const esp_timer_create_args_t args = {
      .callback = &adv_watchdog_timer_cb,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "ble_adv_wd",
      .skip_unhandled_events = true,
  };
  esp_err_t e = esp_timer_create(&args, &s_adv_watchdog_timer);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "adv watchdog esp_timer_create: %s", esp_err_to_name(e));
    return;
  }
  /* 45 s: longer than Wi‑Fi coexistence jitter; shorter than the typical "I forget to scan". */
  e = esp_timer_start_periodic(s_adv_watchdog_timer, 45 * 1000 * 1000);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "adv watchdog esp_timer_start_periodic: %s", esp_err_to_name(e));
    return;
  }
  ESP_LOGI(TAG,
           "GAP advertising watchdog: every 45 s check (only without active "
           "BLE connection = no "pair"/central)");
}

#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
static void czechmate_security_retry_cb(struct ble_npl_event *ev) {
  (void)ev;
  if (!s_sec_retry_co_ready || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }
  if (ble_task_conn_is_encrypted()) {
    ESP_LOGI(TAG, "[STAGING] SMP: encryption OK (after delayed retry)");
    s_sec_deferred_attempts = 0;
    return;
  }
  int rc = ble_gap_security_initiate(s_conn_handle);
  ESP_LOGI(TAG,
           "[STAGING] SMP deferred attempt %u: ble_gap_security_initiate rc=%d",
           (unsigned)(s_sec_deferred_attempts + 1), rc);
  s_sec_deferred_attempts++;
  if (s_sec_deferred_attempts < 6 && !ble_task_conn_is_encrypted()) {
    ble_npl_error_t e = ble_npl_callout_reset(
        &s_sec_retry_co, ble_npl_time_ms_to_ticks32(2500));
    if (e != BLE_NPL_OK) {
      ESP_LOGW(TAG, "SMP callout_reset rc=%d", (int)e);
    }
  } else if (!ble_task_conn_is_encrypted()) {
    ESP_LOGW(TAG,
             "[STAGING] SMP: still no encryption after %u retries",
             (unsigned)s_sec_deferred_attempts);
    s_sec_deferred_attempts = 0;
  }
}
#endif

static int czechmate_gap_event(struct ble_gap_event *event, void *arg) {
  (void)arg;
  ESP_LOGD(TAG, "[STAGING] GAP event type=%d", (int)event->type);
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
      s_conn_handle = event->connect.conn_handle;
      ESP_LOGI(TAG, "connected handle=%d", s_conn_handle);
#if CONFIG_ESP_COEX_ENABLED
      /* One Wi‑Fi + BLE radio: without moving to BLE, ATT often does not respond at all (iOS
       * "0 services"). PREFER_BALANCE was not enough for some C6 + AP+STA — we give
       * BLE more weight for the duration of the connection. */
      {
        esp_err_t ce = esp_coex_preference_set(ESP_COEX_PREFER_BT);
        if (ce != ESP_OK) {
          ESP_LOGW(TAG, "esp_coex_preference_set(BT) failed: %s",
                   esp_err_to_name(ce));
        } else {
          ESP_LOGI(TAG,
                   "coexistence: PREFER_BT while BLE connected (ATT/GATT)");
        }
      }
#endif
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
      /* iOS: SMP before central office completes GATT discovery often terminates disconnect 531 /
       * enc_change status≠0. Application typically starts discovery ~0.5-1.5s after LINK —
       * delay first ble_gap_security_initiate (~1.8s). */
      {
        s_sec_deferred_attempts = 0;
        if (s_sec_retry_co_ready) {
          ble_npl_callout_stop(&s_sec_retry_co);
          ble_npl_error_t ce = ble_npl_callout_reset(
              &s_sec_retry_co, ble_npl_time_ms_to_ticks32(1800));
          if (ce != BLE_NPL_OK) {
            ESP_LOGW(TAG, "SMP defer callout_reset rc=%d — immediate initiate",
                     (int)ce);
            int src = ble_gap_security_initiate(event->connect.conn_handle);
            ESP_LOGI(TAG, "[STAGING] ble_gap_security_initiate(fallback) rc=%d",
                     src);
            if (src != 0 && !ble_task_conn_is_encrypted()) {
              (void)ble_npl_callout_reset(
                  &s_sec_retry_co, ble_npl_time_ms_to_ticks32(450));
            }
          }
        } else {
          int src = ble_gap_security_initiate(event->connect.conn_handle);
          ESP_LOGI(TAG, "[STAGING] ble_gap_security_initiate(no_timer) rc=%d",
                   src);
        }
      }
#endif
    } else {
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      czechmate_gap_restart_advertising();
    }
    return 0;
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
  case BLE_GAP_EVENT_ENC_CHANGE:
    ESP_LOGI(TAG,
             "GAP enc_change status=%d handle=%u (0=ok; otherwise see ble_hs_errno /"
             "hci)",
             (int)event->enc_change.status,
             (unsigned)event->enc_change.conn_handle);
    if (event->enc_change.status == 0 &&
        event->enc_change.conn_handle == s_conn_handle) {
      struct ble_gap_conn_desc edesc = {0};
      if (ble_gap_conn_find(s_conn_handle, &edesc) == 0) {
        ESP_LOGI(TAG,
                 "[STAGING] po enc_change: encrypted=%u authenticated=%u bonded=%u "
                 "key_size=%u",
                 (unsigned)edesc.sec_state.encrypted,
                 (unsigned)edesc.sec_state.authenticated,
                 (unsigned)edesc.sec_state.bonded,
                 (unsigned)edesc.sec_state.key_size);
        if (edesc.sec_state.encrypted && s_sec_retry_co_ready) {
          ble_npl_callout_stop(&s_sec_retry_co);
          s_sec_deferred_attempts = 0;
        }
      }
    }
    return 0;
  case BLE_GAP_EVENT_PARING_COMPLETE:
    ESP_LOGI(TAG,
             "GAP pairing_complete status=%d handle=%u",
             (int)event->pairing_complete.status,
             (unsigned)event->pairing_complete.conn_handle);
    return 0;
  case BLE_GAP_EVENT_REPEAT_PAIRING: {
    struct ble_gap_conn_desc desc;
    int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
    if (rc == 0) {
      ble_store_util_delete_peer(&desc.peer_id_addr);
    }
    return BLE_GAP_REPEAT_PAIRING_RETRY;
  }
  case BLE_GAP_EVENT_PASSKEY_ACTION: {
    const uint8_t act = event->passkey.params.action;
    ESP_LOGI(TAG, "[STAGING] PASSKEY_ACTION act=%u numcmp=%" PRIu32,
             (unsigned)act, (uint32_t)event->passkey.params.numcmp);
    if (act == BLE_SM_IOACT_NONE) {
      return 0;
    }
    struct ble_sm_io io = {0};
    io.action = act;
    switch (act) {
    case BLE_SM_IOACT_NUMCMP:
      /* Headless peripherals: the user verifies the value on the phone (iOS). */
      io.numcmp_accept = 1;
      break;
    case BLE_SM_IOACT_DISP:
      io.passkey = event->passkey.params.numcmp;
      break;
    case BLE_SM_IOACT_INPUT:
      io.passkey = event->passkey.params.numcmp;
      break;
    default:
      ESP_LOGW(TAG, "PASSKEY_ACTION act=%u — not supported (eg OOB)", (unsigned)act);
      return 0;
    }
    int rc = ble_sm_inject_io(event->passkey.conn_handle, &io);
    if (rc != 0) {
      ESP_LOGW(TAG, "ble_sm_inject_io act=%u rc=%d", (unsigned)act, rc);
    }
    return 0;
  }
#endif
  case BLE_GAP_EVENT_DISCONNECT:
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
    if (s_sec_retry_co_ready) {
      ble_npl_callout_stop(&s_sec_retry_co);
    }
    s_sec_deferred_attempts = 0;
#endif
    /* Release ongoing BLE OTA stream — otherwise hang s_ble_ota_rx + s_ota_sem. */
    ota_update_ble_on_disconnect();
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_snap_notify_enabled = false;
    s_net_notify_enabled = false;
    s_cmd_ack_notify_enabled = false;
    ESP_LOGI(TAG, "disconnect reason=%d", event->disconnect.reason);
#if CONFIG_ESP_COEX_ENABLED
    {
      esp_err_t ce = esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
      if (ce != ESP_OK) {
        ESP_LOGW(TAG, "esp_coex_preference_set(WIFI) failed: %s",
                 esp_err_to_name(ce));
      } else {
        ESP_LOGI(TAG, "coexistence: PREFER_WIFI after BLE disconnect (web)");
      }
    }
#endif
    czechmate_gap_restart_advertising();
    return 0;
  case BLE_GAP_EVENT_SUBSCRIBE:
    if (event->subscribe.attr_handle == g_snap_val_handle) {
      s_snap_notify_enabled = event->subscribe.cur_notify;
      ESP_LOGI(TAG, "snapshot notify=%d", (int)s_snap_notify_enabled);
      if (s_snap_notify_enabled) {
        czechmate_on_game_state_changed();
      }
    }
    if (event->subscribe.attr_handle == g_net_val_handle) {
      s_net_notify_enabled = event->subscribe.cur_notify;
      ESP_LOGI(TAG, "network notify=%d", (int)s_net_notify_enabled);
    }
    if (event->subscribe.attr_handle == g_cmd_ack_val_handle) {
      s_cmd_ack_notify_enabled = event->subscribe.cur_notify;
      ESP_LOGI(TAG, "cmd_ack notify=%d", (int)s_cmd_ack_notify_enabled);
    }
    return 0;
  case BLE_GAP_EVENT_CONN_UPDATE_REQ:
    if (event->conn_update_req.peer_params != NULL) {
      ESP_LOGI(TAG,
               "[STAGING] conn_update_req h=%u itvl %u-%u lat=%u sup=%u (iOS "
               "— accept)",
               (unsigned)event->conn_update_req.conn_handle,
               event->conn_update_req.peer_params->itvl_min,
               event->conn_update_req.peer_params->itvl_max,
               event->conn_update_req.peer_params->latency,
               event->conn_update_req.peer_params->supervision_timeout);
    } else {
      ESP_LOGI(TAG, "[STAGING] conn_update_req h=%u",
               (unsigned)event->conn_update_req.conn_handle);
    }
    return 0;
  case BLE_GAP_EVENT_MTU:
    ESP_LOGI(TAG, "[STAGING] MTU h=%u cid=%u value=%u",
             (unsigned)event->mtu.conn_handle, (unsigned)event->mtu.channel_id,
             (unsigned)event->mtu.value);
    return 0;
  default:
    ESP_LOGD(TAG, "[STAGING] GAP event (unhandled in switch) type=%d",
             (int)event->type);
    return 0;
  }
}

/**
 * Must run BEFORE nimble_port_freertos_init() — same as ESP-IDF bleprph
 * gatt_svr_init() before guest task. ble_gatts_start() is called internally
 * ble_hs_start() only once; services added up in sync_cb would be in the ATT table
 * never received (snap/cmd handles would remain 0, iOS: "0 services").
 */
static void czechmate_gatt_register_before_host_start(void) {
  int rc;
  ble_svc_gap_init();
  ble_svc_gatt_init();
  rc = ble_gatts_count_cfg(czechmate_svcs);
  assert(rc == 0);
  rc = ble_gatts_add_svcs(czechmate_svcs);
  assert(rc == 0);
  rc = ble_svc_gap_device_name_set("CZECHMATE");
  assert(rc == 0);
  ESP_LOGI(TAG, "GATT defs queued (gap+gatt+CZECHMATE) — handles are set in "
                "ble_gatts_start()");
}

static void ble_on_sync(void) {
  ESP_LOGI(TAG,
           "ble_on_sync: ATT done — snap=%u cmd=%u net=%u ack=%u, start advertising",
           (unsigned)g_snap_val_handle, (unsigned)g_cmd_val_handle,
           (unsigned)g_net_val_handle, (unsigned)g_cmd_ack_val_handle);
  czechmate_advertise();
  czechmate_start_adv_watchdog_timer();
}

static void ble_host_task(void *param) {
  (void)param;
  ESP_LOGD(
      TAG,
      "[STAGING] ble_host_task: entered (nimble_port_run — stack event loop)");
  nimble_port_run();
  ESP_LOGD(TAG, "[STAGING] ble_host_task: nimble_port_run returned");
  nimble_port_freertos_deinit();
}

void ble_nimble_stack_init(void) {
  /* ESP32-C6: nimble_port_init() initializes controller + host (without
   * esp_nimble_hci.h which is only when CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE is on
   * older targets). */
  ESP_LOGD(TAG,
           "[STAGING] ble_nimble_stack_init: nimble_port_init + freertos host");
  ESP_ERROR_CHECK(nimble_port_init());
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
  if (!s_sec_retry_co_ready) {
    struct ble_npl_eventq *eq = nimble_port_get_dflt_eventq();
    if (eq != NULL) {
      int ci = ble_npl_callout_init(&s_sec_retry_co, eq, czechmate_security_retry_cb,
                                    NULL);
      if (ci != 0) {
        ESP_LOGW(TAG, "ble_npl_callout_init(SMP retry) failed rc=%d", ci);
      } else {
        s_sec_retry_co_ready = true;
      }
    }
  }
  /* Just Works + bonding — central (iOS) then uses encrypted ATT for long writes. */
  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_mitm = 0;
  /* ENC + ID — iOS often expects an identity when bonding (see SMP pairing_complete). */
  ble_hs_cfg.sm_our_key_dist |=
      (BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  ble_hs_cfg.sm_their_key_dist |=
      (BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
#endif
  ble_hs_cfg.sync_cb = ble_on_sync;
  czechmate_gatt_register_before_host_start();
  ble_store_config_init();
  ESP_LOGI(TAG, "[STAGING] ble_store_config_init (SMP store callbacks — required for enc/link)");
  nimble_port_freertos_init(ble_host_task);
}

void ble_task_format_status(char *buf, size_t cap) {
  if (buf == NULL || cap == 0) {
    return;
  }
  snprintf(
      buf, cap,
      "BLE: %s | adv=%s | snap=%s net=%s ack=%s | h snap=%u cmd=%u net=%u ack=%u | NimBLE",
      s_conn_handle == BLE_HS_CONN_HANDLE_NONE ? "disconnected" : "connected",
      ble_gap_adv_active() ? "on" : "off",
      s_snap_notify_enabled ? "on" : "off",
      s_net_notify_enabled ? "on" : "off",
      s_cmd_ack_notify_enabled ? "on" : "off",
      (unsigned)g_snap_val_handle,
      (unsigned)g_cmd_val_handle,
      (unsigned)g_net_val_handle,
      (unsigned)g_cmd_ack_val_handle);
}

bool ble_task_conn_is_encrypted(void) {
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    return false;
  }
  struct ble_gap_conn_desc desc;
  int rc = ble_gap_conn_find(s_conn_handle, &desc);
  if (rc != 0) {
    return false;
  }
  return desc.sec_state.encrypted != 0;
}

void ble_task_push_network_info(void) {
  if (g_net_val_handle == 0) {
    ESP_LOGV(TAG, "push_network: skip (net handle 0)");
    return;
  }
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_net_notify_enabled) {
    ESP_LOGV(TAG, "push_network: skip (conn=%d notify=%d)",
             (int)s_conn_handle, (int)s_net_notify_enabled);
    return;
  }
  char net_json[384];
  czechmate_build_network_json(net_json, sizeof(net_json));
  size_t len = strlen(net_json);
  struct os_mbuf *om = ble_hs_mbuf_from_flat(net_json, (uint16_t)len);
  if (om == NULL) {
    ESP_LOGW(TAG, "push_network: mbuf alloc failed");
    return;
  }
  int rc = ble_gatts_notify_custom(s_conn_handle, g_net_val_handle, om);
  if (rc != 0) {
    ESP_LOGW(TAG, "push_network: notify_custom rc=%d", rc);
    return;
  }
  ESP_LOGI(TAG, "[STAGING] network notify: %s", net_json);
}

/*
 * One GATT notify = one ATT PDU with the value max (MTU − 3) B (NimBLE/iOS).
 * Old fixed chunk 400 + 4 B header "CM" = 404 B → on MTU 247 value only
 * 244 B → iOS truncates end, iOS assembles truncated parts → broken JSON ("…p\",me":0…).
 * Payload = min(remaining, ble_att_mtu − 3 − 4).
 */
#define SNAPSHOT_NOTIFY_CHUNK_CAP 508

bool ble_task_should_push_snapshot(void) {
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_snap_notify_enabled) {
    return false;
  }
  if (ota_update_ble_is_rx_active()) {
    return false;
  }
  return true;
}

void ble_task_push_snapshot_json(const uint8_t *data, size_t len) {
  if (data == NULL || len == 0 || g_snap_val_handle == 0) {
    ESP_LOGV(TAG, "push_snapshot: skip (no data or snap handle 0)");
    return;
  }
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_snap_notify_enabled) {
    ESP_LOGV(TAG, "push_snapshot: skip (conn=%d notify=%d)", (int)s_conn_handle,
             (int)s_snap_notify_enabled);
    return;
  }
  uint16_t mtu = ble_att_mtu(s_conn_handle);
  if (mtu < 27) {
    mtu = 23;
  }
  size_t chunk_cap = (size_t)mtu - 7U;
  if (chunk_cap < 8U) {
    chunk_cap = 8U;
  }
  if (chunk_cap > SNAPSHOT_NOTIFY_CHUNK_CAP) {
    chunk_cap = SNAPSHOT_NOTIFY_CHUNK_CAP;
  }
  ESP_LOGD(TAG, "[STAGING] push_snapshot: %u B → notify chunk(s) mtu=%u cap=%u",
           (unsigned)len, (unsigned)mtu, (unsigned)chunk_cap);
  size_t off = 0;
  uint8_t part = 0;
  uint32_t total_u = (uint32_t)((len + chunk_cap - 1) / chunk_cap);
  if (total_u < 1U) {
    total_u = 1U;
  }
  /* The protocol has part/total in uint8 — max 255 notifications per snapshot. */
  if (total_u > 255U) {
    size_t need = (len + 254U) / 255U;
    if (need <= SNAPSHOT_NOTIFY_CHUNK_CAP && need + 7U <= (size_t)mtu) {
      chunk_cap = need;
      total_u = (uint32_t)((len + chunk_cap - 1) / chunk_cap);
    }
  }
  if (total_u > 255U) {
    ESP_LOGE(TAG,
             "push_snapshot: %u B @ mtu=%u cap=%u → %u chunks (max 255); MTU "
             "still or too small",
             (unsigned)len, (unsigned)mtu, (unsigned)chunk_cap,
             (unsigned)total_u);
    return;
  }
  uint8_t total = (uint8_t)total_u;
  while (off < len) {
    /* Called typically from web_server_task — a long line of notify; TWDT can't keep up otherwise. */
    (void)esp_task_wdt_reset();
    part++;
    size_t chunk = len - off;
    if (chunk > chunk_cap) {
      chunk = chunk_cap;
    }
    uint8_t pkt[4 + SNAPSHOT_NOTIFY_CHUNK_CAP];
    pkt[0] = 0x43;
    pkt[1] = 0x4D;
    pkt[2] = part;
    pkt[3] = total;
    memcpy(pkt + 4, data + off, chunk);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(pkt, (uint16_t)(4 + chunk));
    if (om == NULL) {
      return;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, g_snap_val_handle, om);
    if (rc != 0) {
      ESP_LOGW(TAG, "notify_custom rc=%d", rc);
      return;
    }
    ESP_LOGD(TAG, "[STAGING] notify chunk %u/%u (%u B)", (unsigned)part,
             (unsigned)total, (unsigned)chunk);
    off += chunk;
  }
}

#else /* !CONFIG_BT_ENABLED */

void ble_nimble_stack_init(void) {}
bool ble_task_conn_is_encrypted(void) { return false; }
bool ble_task_should_push_snapshot(void) { return false; }
void ble_task_push_snapshot_json(const uint8_t *data, size_t len) {
  (void)data;
  (void)len;
}
void ble_task_push_network_info(void) {}

void ble_task_notify_command_result(esp_err_t err, const char *json_body) {
  (void)err;
  (void)json_body;
}

void ble_task_notify_cmd_ack_json(const char *json_utf8) { (void)json_utf8; }

void ble_task_format_status(char *buf, size_t cap) {
  if (buf && cap) {
    snprintf(buf, cap, "BLE: disabled (CONFIG_BT_ENABLED=n in sdkconfig)");
  }
}

#endif
