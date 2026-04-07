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
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
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
 *   0xa0,0xb4,0x00,0x01, 0x92,0x67, 0x4a,0xb6, 0xbd,0xcc, 0xe8,0x33,0x6f,0x8a,0x8d,0x9e
 * Správná (little-endian, funkční):
 *   0x9e,0x8d,0x8a,0x6f, 0x33,0xe8, 0xcc,0xbd, 0xb6,0x4a, 0x67,0x92, 0x01,0x00,0xb4,0xa0
 *
 * Původní big-endian verze způsobovala: iOS skenoval UUID "A0B40001-..."
 * ale ESP inzerovalo "9E8D8A6F-..." → telefon nikdy nenašel šachovnici!
 */
static const ble_uuid128_t czechmate_svc_uuid = BLE_UUID128_INIT(
    0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd,
    0xb6, 0x4a, 0x67, 0x92, 0x01, 0x00, 0xb4, 0xa0);

static const ble_uuid128_t czechmate_snap_chr_uuid = BLE_UUID128_INIT(
    0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd,
    0xb6, 0x4a, 0x67, 0x92, 0x02, 0x00, 0xb4, 0xa0);

static const ble_uuid128_t czechmate_cmd_chr_uuid = BLE_UUID128_INIT(
    0x9e, 0x8d, 0x8a, 0x6f, 0x33, 0xe8, 0xcc, 0xbd,
    0xb6, 0x4a, 0x67, 0x92, 0x03, 0x00, 0xb4, 0xa0);

static uint16_t g_snap_val_handle;
static uint16_t g_cmd_val_handle;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static int czechmate_gap_event(struct ble_gap_event *event, void *arg);

static int czechmate_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
  (void)arg;
  ESP_LOGD(TAG,
           "[STAGING] GATT access conn=%u op=%d attr=%u snap_h=%u cmd_h=%u",
           (unsigned)conn_handle, (int)ctxt->op, (unsigned)attr_handle,
           (unsigned)g_snap_val_handle, (unsigned)g_cmd_val_handle);
  switch (ctxt->op) {
  case BLE_GATT_ACCESS_OP_READ_CHR:
    if (attr_handle == g_snap_val_handle) {
      char *snap = NULL;
      size_t len = 0;
      esp_err_t e = web_server_build_game_snapshot_json_shared(&snap, &len);
      if (e != ESP_OK || len == 0 || snap == NULL) {
        ESP_LOGW(TAG,
                 "[STAGING] BLE read snapshot: build failed (%s) — neposíláme ne-GameSnapshot JSON",
                 esp_err_to_name(e));
        return BLE_ATT_ERR_UNLIKELY;
      }
      /* os_mbuf_append: 0 = úspěch, nenulové = chyba (viz os_mbuf.h). Dřívější ternární výraz to měl
       * obráceně → iOS dostával ATT chybu při úspěchu. Dlouhý atribut: NimBLE + klient řeší Read Blob. */
      if (len > 65535U) {
        ESP_LOGW(TAG, "[STAGING] BLE read snapshot: JSON > 65535 B (limit os_mbuf_append)");
        return BLE_ATT_ERR_UNLIKELY;
      }
      int rc = os_mbuf_append(ctxt->om, snap, (uint16_t)len);
      return rc != 0 ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    break;
  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    if (attr_handle == g_cmd_val_handle) {
      uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
      char tmp[384];
      if (om_len >= sizeof(tmp)) {
        om_len = (uint16_t)(sizeof(tmp) - 1);
      }
      if (om_len > 0) {
        os_mbuf_copydata(ctxt->om, 0, om_len, tmp);
      }
      tmp[om_len] = '\0';
      esp_err_t derr = web_server_ble_command_dispatch(tmp, om_len);
      if (derr == ESP_OK) {
        ESP_LOGD(TAG, "[STAGING] CMD dispatch ok (%u B)", (unsigned)om_len);
        return 0;
      }
      if (derr == ESP_ERR_INVALID_STATE) {
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
      if (derr == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "CMD unknown");
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
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
                    /* Write (s odpovědí) — odpovídá iOS writeWithResponse */
                    .flags = BLE_GATT_CHR_F_WRITE,
                    .val_handle = &g_cmd_val_handle,
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
   * Name přesunuto do Scan Response (uvolní místo, iOS 13+ posílá ScanReq automaticky).
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

  /* Scan Response: plné jméno (iOS ho čte jako CBAdvertisementDataLocalNameKey). */
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
    /* Fallback: RPA (random private address) — pokud ESP nemá platnou public BD addr. */
    ESP_LOGW(TAG, "adv_start PUBLIC rc=%d — fallback na RPA", rc);
    rc = ble_gap_adv_start(BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT, NULL, BLE_HS_FOREVER,
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


static int czechmate_gap_event(struct ble_gap_event *event, void *arg) {
  (void)arg;
  ESP_LOGD(TAG, "[STAGING] GAP event type=%d", (int)event->type);
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
      s_conn_handle = event->connect.conn_handle;
      ESP_LOGI(TAG, "connected handle=%d", s_conn_handle);
#if CONFIG_ESP_COEX_ENABLED
      /* Jedno rádio Wi‑Fi + BLE: bez posunu k BLE často ATT vůbec neodpoví (iOS „0 služeb“).
       * PREFER_BALANCE u některých C6 + AP+STA nestačilo — dáváme BLE větší váhu po dobu spoje. */
      {
        esp_err_t ce = esp_coex_preference_set(ESP_COEX_PREFER_BT);
        if (ce != ESP_OK) {
          ESP_LOGW(TAG, "esp_coex_preference_set(BT) failed: %s",
                   esp_err_to_name(ce));
        } else {
          ESP_LOGI(TAG, "coexistence: PREFER_BT while BLE connected (ATT/GATT)");
        }
      }
#endif
    } else {
      s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      czechmate_advertise();
    }
    return 0;
  case BLE_GAP_EVENT_DISCONNECT:
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_snap_notify_enabled = false;
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
    czechmate_advertise();
    return 0;
  case BLE_GAP_EVENT_SUBSCRIBE:
    if (event->subscribe.attr_handle == g_snap_val_handle) {
      s_snap_notify_enabled = event->subscribe.cur_notify;
      ESP_LOGI(TAG, "snapshot notify=%d", (int)s_snap_notify_enabled);
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
 * gatt_svr_init() před host taskem. ble_gatts_start() se volá uvnitř ble_hs_start()
 * jen jednou; služby přidané až v sync_cb by se do ATT tabulky nikdy nedostaly
 * (handles snap/cmd by zůstaly 0, iOS: „0 služeb“).
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
  ESP_LOGI(TAG,
           "GATT defs queued (gap+gatt+CZECHMATE) — handles se nastaví v "
           "ble_gatts_start()");
}

static void ble_on_sync(void) {
  ESP_LOGI(TAG,
           "ble_on_sync: ATT hotový — snap_h=%u cmd_h=%u, start advertising",
           (unsigned)g_snap_val_handle, (unsigned)g_cmd_val_handle);
  czechmate_advertise();
}

static void ble_host_task(void *param) {
  (void)param;
  ESP_LOGD(TAG,
           "[STAGING] ble_host_task: entered (nimble_port_run — stack event loop)");
  nimble_port_run();
  ESP_LOGD(TAG, "[STAGING] ble_host_task: nimble_port_run returned");
  nimble_port_freertos_deinit();
}

void ble_nimble_stack_init(void) {
  /* ESP32-C6: nimble_port_init() inicializuje controller + host (bez esp_nimble_hci.h,
   * které je jen při CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE na starších cílech). */
  ESP_LOGD(TAG, "[STAGING] ble_nimble_stack_init: nimble_port_init + freertos host");
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
      "BLE: %s | snapshot_notify=%s | handles snap=%u cmd=%u | stack NimBLE",
      s_conn_handle == BLE_HS_CONN_HANDLE_NONE ? "disconnected" : "connected",
      s_snap_notify_enabled ? "on" : "off", (unsigned)g_snap_val_handle,
      (unsigned)g_cmd_val_handle);
}

#define CHUNK_PAYLOAD 400

void ble_task_push_snapshot_json(const uint8_t *data, size_t len) {
  if (data == NULL || len == 0 || g_snap_val_handle == 0) {
    ESP_LOGV(TAG, "push_snapshot: skip (no data or snap handle 0)");
    return;
  }
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_snap_notify_enabled) {
    ESP_LOGV(TAG, "push_snapshot: skip (conn=%d notify=%d)",
             (int)s_conn_handle, (int)s_snap_notify_enabled);
    return;
  }
  ESP_LOGD(TAG, "[STAGING] push_snapshot: %u B → notify chunk(s)", (unsigned)len);
  size_t off = 0;
  uint8_t part = 0;
  uint8_t total = (uint8_t)((len + CHUNK_PAYLOAD - 1) / CHUNK_PAYLOAD);
  if (total < 1) {
    total = 1;
  }
  while (off < len) {
    part++;
    size_t chunk = len - off;
    if (chunk > CHUNK_PAYLOAD) {
      chunk = CHUNK_PAYLOAD;
    }
    uint8_t pkt[4 + CHUNK_PAYLOAD];
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

void ble_task_format_status(char *buf, size_t cap) {
  if (buf && cap) {
    snprintf(buf, cap, "BLE: disabled (CONFIG_BT_ENABLED=n in sdkconfig)");
  }
}

#endif
