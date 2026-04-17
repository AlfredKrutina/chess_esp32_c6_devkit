/**
 * @file ble_nimble_impl.c
 * @brief NimBLE GATT — kompiluje se jen při CONFIG_BT_ENABLED.
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
#include "host/ble_uuid.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_task_wdt.h"
#include "web_server_task.h"
#include <assert.h>
#include <string.h>

#if CONFIG_ESP_COEX_ENABLED
#include "esp_coexist.h"
#endif

static bool s_snap_notify_enabled;

static const char *TAG = "BLE_NIMBLE";

/*
 * UUID: A0B40001-9267-4AB6-BDCC-E8336F8A8D9E
 *
 * DŮLEŽITÉ: BLE_UUID128_INIT vyžaduje LITTLE-ENDIAN pořadí bajtů —
 * tedy OBRÁCENÉ oproti čitelnému UUID stringu (RFC 4122 = big-endian).
 *
 * Chybná (big-endian, nefunkční):
 *   0xa0,0xb4,0x00,0x01, 0x92,0x67, 0x4a,0xb6, 0xbd,0xcc,
 * 0xe8,0x33,0x6f,0x8a,0x8d,0x9e Správná (little-endian, funkční):
 *   0x9e,0x8d,0x8a,0x6f, 0x33,0xe8, 0xcc,0xbd, 0xb6,0x4a, 0x67,0x92,
 * 0x01,0x00,0xb4,0xa0
 *
 * Původní big-endian verze způsobovala: iOS skenoval UUID "A0B40001-..."
 * ale ESP inzerovalo "9E8D8A6F-..." → telefon nikdy nenašel šachovnici!
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

/** A0B40005-… — JSON výsledek posledního CMD (notify); iOS parsuje channel=cmd_ack */
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

/** Periodicky ověřit GAP advertising, když není žádný centrál připojený. */
static esp_timer_handle_t s_adv_watchdog_timer;
/** Naplánování watchdog logiky na výchozí NimBLE event queue (GAP jen z host
 * kontextu). ble_hs_sched() není ve veřejném API ESP-IDF 5.5 / NimBLE. */
static struct ble_npl_event s_idle_adv_watchdog_npl_ev;
static bool s_idle_adv_watchdog_npl_ev_inited;

/** Jedno GATT write najednou na host tasku — šetří stack oproti lokálnímu tmp[768]. */
static char s_ble_cmd_copy[768];

static int czechmate_gap_event(struct ble_gap_event *event, void *arg);

/** Ponechá v cmd jen bezpečné znaky pro JSON řetězec. */
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
  const char *msg = "Interní chyba při zpracování příkazu.";

  switch (err) {
  case ESP_OK:
    ok_str = "true";
    code = "ok";
    msg = "Příkaz přijat.";
    break;
  case ESP_ERR_NOT_SUPPORTED:
    code = "unknown_command";
    msg = "Deska tento příkaz nezná.";
    break;
  case ESP_ERR_NOT_FOUND:
    code = "missing_cmd";
    msg = "V JSON chybí pole cmd.";
    break;
  case ESP_ERR_INVALID_ARG:
    code = "invalid_argument";
    msg = "Neplatné parametry příkazu.";
    break;
  case ESP_ERR_INVALID_STATE:
    code = "blocked";
    msg = "Akce je zablokovaná (zámek webu nebo stav desky).";
    break;
  case ESP_ERR_NOT_FINISHED:
    code = "tutorial_finish_conflict";
    msg =
        "Fyzická deska neodpovídá základnímu postavení (řádky 1–2 a 7–8 plné).";
    break;
  case ESP_FAIL:
    code = "internal_error";
    msg = "Interní chyba při zpracování příkazu.";
    break;
  default:
    code = "error";
    msg = "Chyba při zpracování příkazu.";
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
  
  snprintf(buf, cap,
           "{"
           "\"sta_connected\":%s,"
           "\"sta_ip\":\"%s\","
           "\"sta_ssid\":\"%s\","
           "\"ap_ip\":\"192.168.4.1\","
           "\"online\":%s"
           "}",
           sta_connected ? "true" : "false",
           sta_connected ? sta_ip : "",
           (ssid_ret == ESP_OK && sta_ssid[0] != '\0') ? sta_ssid : "",
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
      char net_json[256];
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
       * hint_highlight / hint_clear: synchronní web_server_ble_command_dispatch v rámci
       * ATT write; po něm cmd_ack notify na jiné charakteristice. Snapshot notify jede
       * zvlášť (game task → ble_task_push_snapshot_json) — jiný GATT handle; NimBLE
       * serializuje notify; dlouhé snapshot chunky mohou zpoždět další notify, ale
       * nesmí zahodit samotný CMD (ověřovat při ladění MTU a zatížení desky).
       */
      uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
      /* Musí být v souladu s web_server_ble_command_dispatch (větší JSON: timer/virtual). */
      if (om_len >= sizeof(s_ble_cmd_copy)) {
        om_len = (uint16_t)(sizeof(s_ble_cmd_copy) - 1);
      }
      if (om_len > 0) {
        os_mbuf_copydata(ctxt->om, 0, om_len, s_ble_cmd_copy);
      }
      s_ble_cmd_copy[om_len] = '\0';
      esp_err_t derr =
          web_server_ble_command_dispatch(s_ble_cmd_copy, om_len);
      ble_task_notify_command_result(derr, s_ble_cmd_copy);
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
   *   Flags:    3 B  (2 header + 1 value)
   *   UUID128: 18 B  (2 header + 16 UUID)
   *   = 21 B — OK, pod limit 31 B.
   * Name přesunuto do Scan Response (uvolní místo, iOS 13+ posílá ScanReq
   * automaticky).
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

  /* Scan Response: plné jméno (iOS ho čte jako
   * CBAdvertisementDataLocalNameKey). */
  memset(&rsp_fields, 0, sizeof(rsp_fields));
  const char *name = "CZECHMATE";
  rsp_fields.name = (uint8_t *)name;
  rsp_fields.name_len = strlen(name);
  rsp_fields.name_is_complete = 1;

  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  if (rc != 0) {
    /* Scan response není kritický — loguj ale pokračuj */
    ESP_LOGW(TAG, "adv_rsp_set_fields rc=%d", rc);
  }

  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params,
                         czechmate_gap_event, NULL);
  if (rc != 0) {
    /* Fallback: RPA (random private address) — pokud ESP nemá platnou public BD
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
 * Znovu rozběhne discoverable advertising (undirected GAP).
 * Volat po pádu spoje nebo když watchdog zjistí, že ADV ustal — iOS pak desku
 * znovu uvidí ve skenu i po hodinách uptime.
 */
static void czechmate_gap_restart_advertising(void) {
  int sr = ble_gap_adv_stop();
  if (sr != 0) {
    ESP_LOGD(TAG, "adv_stop rc=%d (typicky OK, pokud ADV už neběžel)", sr);
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
           "GAP: žádné BLE spojení a advertising neběží — restart GAP (viditelnost "
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
  /* 45 s: delší než Wi‑Fi coexistence jitter; kratší než typický „zapomenu skenovat“. */
  e = esp_timer_start_periodic(s_adv_watchdog_timer, 45 * 1000 * 1000);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "adv watchdog esp_timer_start_periodic: %s", esp_err_to_name(e));
    return;
  }
  ESP_LOGI(TAG,
           "GAP advertising watchdog: každých 45 s kontrola (jen bez aktivního "
           "BLE spojení = žádný „pár“/centrál)");
}

static int czechmate_gap_event(struct ble_gap_event *event, void *arg) {
  (void)arg;
  ESP_LOGD(TAG, "[STAGING] GAP event type=%d", (int)event->type);
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
      s_conn_handle = event->connect.conn_handle;
      ESP_LOGI(TAG, "connected handle=%d", s_conn_handle);
#if CONFIG_ESP_COEX_ENABLED
      /* Jedno rádio Wi‑Fi + BLE: bez posunu k BLE často ATT vůbec neodpoví (iOS
       * „0 služeb“). PREFER_BALANCE u některých C6 + AP+STA nestačilo — dáváme
       * BLE větší váhu po dobu spoje. */
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
    } else {
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      czechmate_gap_restart_advertising();
    }
    return 0;
  case BLE_GAP_EVENT_DISCONNECT:
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
 * Musí běžet PŘED nimble_port_freertos_init() — stejně jako ESP-IDF bleprph
 * gatt_svr_init() před host taskem. ble_gatts_start() se volá uvnitř
 * ble_hs_start() jen jednou; služby přidané až v sync_cb by se do ATT tabulky
 * nikdy nedostaly (handles snap/cmd by zůstaly 0, iOS: „0 služeb“).
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
  ESP_LOGI(TAG, "GATT defs queued (gap+gatt+CZECHMATE) — handles se nastaví v "
                "ble_gatts_start()");
}

static void ble_on_sync(void) {
  ESP_LOGI(TAG,
           "ble_on_sync: ATT hotový — snap=%u cmd=%u net=%u ack=%u, start advertising",
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
  /* ESP32-C6: nimble_port_init() inicializuje controller + host (bez
   * esp_nimble_hci.h, které je jen při CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE na
   * starších cílech). */
  ESP_LOGD(TAG,
           "[STAGING] ble_nimble_stack_init: nimble_port_init + freertos host");
  ESP_ERROR_CHECK(nimble_port_init());
  ble_hs_cfg.sync_cb = ble_on_sync;
  czechmate_gatt_register_before_host_start();
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
  char net_json[256];
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
 * Jedno GATT notify = jedna ATT PDU s hodnotou max (MTU − 3) B (NimBLE/iOS).
 * Starý fixní chunk 400 + 4 B hlavička „CM“ = 404 B → na MTU 247 hodnota jen
 * 244 B → iOS usekne konec, iOS skládá zkrácené díly → rozbitý JSON („…p\",me":0…).
 * Payload = min(zbývá, ble_att_mtu − 3 − 4).
 */
#define SNAPSHOT_NOTIFY_CHUNK_CAP 508

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
  /* Protokol má part/total v uint8 — max 255 notify na jeden snapshot. */
  if (total_u > 255U) {
    size_t need = (len + 254U) / 255U;
    if (need <= SNAPSHOT_NOTIFY_CHUNK_CAP && need + 7U <= (size_t)mtu) {
      chunk_cap = need;
      total_u = (uint32_t)((len + chunk_cap - 1) / chunk_cap);
    }
  }
  if (total_u > 255U) {
    ESP_LOGE(TAG,
             "push_snapshot: %u B @ mtu=%u cap=%u → %u chunků (max 255); MTU "
             "ještě nebo moc malé",
             (unsigned)len, (unsigned)mtu, (unsigned)chunk_cap,
             (unsigned)total_u);
    return;
  }
  uint8_t total = (uint8_t)total_u;
  while (off < len) {
    /* Voláno typicky z web_server_task — dlouhá řada notify; TWDT jinak nestíhá. */
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
void ble_task_push_snapshot_json(const uint8_t *data, size_t len) {
  (void)data;
  (void)len;
}
void ble_task_push_network_info(void) {}

void ble_task_notify_command_result(esp_err_t err, const char *json_body) {
  (void)err;
  (void)json_body;
}

void ble_task_format_status(char *buf, size_t cap) {
  if (buf && cap) {
    snprintf(buf, cap, "BLE: disabled (CONFIG_BT_ENABLED=n in sdkconfig)");
  }
}

#endif
