/**
 * @file ha_light_task.c
 * @brief HA Light Task - Home Assistant RGB Light Integration via MQTT
 *
 * @details
 * =============================================================================
 * CO TENTO SOUBOR DELA?
 * =============================================================================
 *
 * Tento task integruje sachovnici jako RGB svetlo do Home Assistant pres MQTT.
 * Po pripojeni na WiFi STA se board automaticky prepne do HA modu po 5 minutach
 * necinosti. V HA modu se vsechny LED desky chovaji jako jedno RGB svetlo.
 *
 * Rezimy:
 * - GAME MODE: LED zobrazuji sachovnici (vychozi)
 * - HA MODE: Vsech 64 LED jako RGB svetlo ovladane pres HA (po 5 min necinosti)
 *
 * Automaticke prepinani:
 * - GAME -> HA: Po 5 minutach bez aktivity (pohyb figurky nebo herni prikaz)
 * - HA -> GAME: Okamzite pri detekci pohybu figurky (PICKUP/DROP)
 *
 * @author Alfred Krutina
 * @version 1.7.3
 * @date 2025-01-XX
 */

#include "ha_light_task.h"
#include "cJSON.h"
#include "chess_types.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "freertos_chess.h"
#include "game_task.h"
#include "led_task.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "HA_LIGHT_TASK";

/** Min. volný heap před esp_mqtt_client_start (interní úloha + zásobník). */
#define HA_MQTT_MIN_FREE_HEAP_BYTES (18 * 1024)
/** Pod tímto prahem ukončíme MQTT klienta (uvolní sockety / úlohy) — prevence
 * errno 11 / RST. */
#define HA_MQTT_STOP_HEAP_BYTES (10 * 1024)

// ============================================================================
// NVS KONFIGURACE PRO MQTT
// ============================================================================

// NVS namespace a klíče pro MQTT konfiguraci
#define MQTT_NVS_NAMESPACE "mqtt_config"
#define MQTT_NVS_KEY_HOST "broker_host"
#define MQTT_NVS_KEY_PORT "broker_port"
#define MQTT_NVS_KEY_USERNAME "broker_username"
#define MQTT_NVS_KEY_PASSWORD "broker_password"

// NVS pro ulozeni stavu lampy (web / lokální režim)
#define LAMP_NVS_NAMESPACE "lamp_cfg"
#define LAMP_NVS_KEY_STATE "state"
#define LAMP_NVS_KEY_R "r"
#define LAMP_NVS_KEY_G "g"
#define LAMP_NVS_KEY_B "b"
#define LAMP_NVS_KEY_AUTO_TIMEOUT_SEC "auto_sec"

// Rozsah pro automatické přepnutí do režimu lampa: 5 s .. 120 min (7200 s)
#define HA_ACTIVITY_TIMEOUT_AUTO_MIN_SEC 5
#define HA_ACTIVITY_TIMEOUT_AUTO_MAX_SEC 7200
#define HA_ACTIVITY_TIMEOUT_AUTO_DEFAULT_SEC 300

// Default hodnoty pro MQTT konfiguraci
#define MQTT_DEFAULT_HOST "homeassistant.local"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_USERNAME ""
#define MQTT_DEFAULT_PASSWORD ""

// ============================================================================
// GLOBALNI PROMENNE A STAV
// ============================================================================

// Task state
static bool task_running = false;
static ha_mode_t current_mode = HA_MODE_GAME;

// Activity tracking – doba (s) po které se přepne do režimu lampa (načteno z
// NVS)
static uint32_t activity_timeout_auto_sec =
    HA_ACTIVITY_TIMEOUT_AUTO_DEFAULT_SEC;
static uint32_t last_activity_time_ms = 0;

// WiFi STA status (shared with web_server_task via events)
static bool sta_connected = false;

// MQTT client
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// HA Light state
static struct {
  bool state;         // on/off
  uint8_t brightness; // 0-255
  uint8_t r, g, b;    // RGB color
  char effect[32];    // effect name
} ha_light_state = {.state = true,
                    .brightness = 255,
                    .r = 255,
                    .g = 255,
                    .b = 255,
                    .effect = "solid"};

// MQTT konfigurace (načítá se z NVS)
static struct {
  char host[128];    // Broker hostname/IP
  uint16_t port;     // Broker port
  char username[64]; // MQTT username (prázdné = bez auth)
  char password[64]; // MQTT password (prázdné = bez auth)
  bool loaded;       // Zda byla konfigurace načtena z NVS
} mqtt_config = {.host = MQTT_DEFAULT_HOST,
                 .port = MQTT_DEFAULT_PORT,
                 .username = MQTT_DEFAULT_USERNAME,
                 .password = MQTT_DEFAULT_PASSWORD,
                 .loaded = false};

// Externi fronty
extern QueueHandle_t game_command_queue;
extern QueueHandle_t matrix_event_queue;

// Interní fronta pro HA task
static QueueHandle_t ha_light_cmd_queue = NULL;

// Mutex pro thread-safe pristup k ha_light_state (HTTP handler cte, HA task
// pise)
static SemaphoreHandle_t ha_light_state_mutex = NULL;

// Forward declarations
static void ha_light_check_activity_timeout(void);
static void ha_light_switch_to_ha_mode(void);
static void ha_light_switch_to_game_mode(void);
static void ha_light_monitor_game_activity(void);
static bool ha_light_check_wifi_sta_connected(void);
static esp_err_t ha_light_init_mqtt(void);
static void ha_light_mqtt_event_handler(void *handler_args,
                                        esp_event_base_t base, int32_t event_id,
                                        void *event_data);
static void ha_light_handle_mqtt_command(const char *topic, const char *data,
                                         int data_len);
static void ha_light_publish_state(void);
static void lamp_nvs_load(void);
static void lamp_nvs_save(void);

// ============================================================================
// WDT HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecny reset WDT
 */
static esp_err_t ha_light_task_wdt_reset_safe(void) {
  esp_err_t ret = esp_task_wdt_reset();
  if (ret == ESP_ERR_NOT_FOUND) {
    ESP_LOGW(TAG, "WDT reset: task not registered yet");
    return ESP_OK;
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
    return ret;
  }
  return ESP_OK;
}

/** Handle hlavní smyčky — esp_task_wdt_reset jen z ha_light_task (ne z MQTT).
 */
static TaskHandle_t s_ha_light_task_hdl;

static void ha_light_wdt_feed_if_own_task(void) {
  if (s_ha_light_task_hdl != NULL &&
      xTaskGetCurrentTaskHandle() == s_ha_light_task_hdl) {
    (void)ha_light_task_wdt_reset_safe();
  }
}

// ============================================================================
// WIFI STA STATUS CHECKING
// ============================================================================

/**
 * @brief Zkontroluj zda je WiFi STA pripojeno
 *
 * @return true pokud je STA pripojeno a ma IP, false jinak
 */
static bool ha_light_check_wifi_sta_connected(void) {
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta_netif == NULL) {
    return false;
  }

  esp_netif_ip_info_t ip_info;
  esp_err_t ret = esp_netif_get_ip_info(sta_netif, &ip_info);
  if (ret != ESP_OK) {
    return false;
  }

  // Check if IP is valid (not 0.0.0.0)
  if (ip_info.ip.addr == 0) {
    return false;
  }

  return true;
}

// ============================================================================
// ACTIVITY TRACKING
// ============================================================================

/**
 * @brief Monitoruje aktivitu hry a resetuje timer
 *
 * Tato funkce se spoléhá na ha_light_report_activity() volané z jiných tasků.
 * Samotné monitorování front zde neděláme, abychom nekonzumovali příkazy.
 */
static void ha_light_monitor_game_activity(void) {
  // Activity monitoring is done via ha_light_report_activity() calls from:
  // - game_task: when processing game commands
  // - matrix_task: when detecting piece movement
  // We just check if we need to switch modes based on activity timeout
}

/**
 * @brief Zprava o aktivity hry (volano z jinych tasku)
 */
void ha_light_report_activity(const char *activity_type) {
  if (!task_running) {
    return;
  }

  // Keep this function lightweight: it may be called from input paths.
  // Mode switching and LED refresh must happen in HA task context.
  last_activity_time_ms = esp_timer_get_time() / 1000;

  /* Rate limit: max 1 queued activity / 500 ms (šetří frontu a MQTT).
   * Výjimka: POST /api/light/game_mode — musí vždy projít, jinak uživatel
   * nevrátí desku z režimu lampy po rychlé sérii jiných událostí. */
  static uint32_t last_report_time = 0;
  uint32_t current_time = esp_timer_get_time() / 1000;
  const bool force_queue =
      (activity_type != NULL && strcmp(activity_type, "web_game_mode") == 0);
  if (!force_queue && current_time - last_report_time < 500) {
    return;
  }
  last_report_time = current_time;

  ha_light_command_t cmd;
  cmd.type = HA_CMD_ACTIVITY;
  cmd.u.data = (void *)activity_type;

  if (ha_light_cmd_queue != NULL) {
    xQueueSend(ha_light_cmd_queue, &cmd, 0);
  }
}

/**
 * @brief Kontroluje zda byl prekrocen 5 minutovy timeout
 */
static void ha_light_check_activity_timeout(void) {
  if (!task_running || current_mode == HA_MODE_HA) {
    return; // Already in HA mode or task not running
  }

  if (last_activity_time_ms == 0) {
    // First run - initialize timer
    last_activity_time_ms = esp_timer_get_time() / 1000;
    return;
  }

  uint32_t current_time_ms = esp_timer_get_time() / 1000;
  uint32_t elapsed_ms = current_time_ms - last_activity_time_ms;
  uint32_t timeout_ms = (uint32_t)activity_timeout_auto_sec * 1000;

  if (elapsed_ms >= timeout_ms) {
    if (ha_light_check_wifi_sta_connected()) {
      ESP_LOGI(TAG,
               "Activity timeout (%lu s) reached - auto-switching to HA mode",
               (unsigned long)activity_timeout_auto_sec);
      ha_light_switch_to_ha_mode();
    } else {
      ESP_LOGD(
          TAG,
          "Timeout reached but WiFi STA not connected - staying in GAME mode");
    }
  }
}

// ============================================================================
// MODE SWITCHING
// ============================================================================

/**
 * @brief Prepne do HA modu
 *
 * Vsech 64 LED desky se nastavi na barvu z ha_light_state.
 */
static void ha_light_switch_to_ha_mode(void) {
  ha_light_wdt_feed_if_own_task();
  bool mode_changed = (current_mode != HA_MODE_HA);

  if (mode_changed) {
    ESP_LOGI(TAG, "Switching to HA MODE");
  }
  current_mode = HA_MODE_HA;

  uint8_t r, g, b, brightness;
  bool state;
  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    r = ha_light_state.r;
    g = ha_light_state.g;
    b = ha_light_state.b;
    brightness = ha_light_state.brightness;
    state = ha_light_state.state;
    xSemaphoreGive(ha_light_state_mutex);
  } else {
    r = g = b = 255;
    brightness = 255;
    state = true;
  }

  // Apply HA color/state to all 64 board LEDs
  if (state) {
    led_set_ha_color(r, g, b, brightness);
  } else {
    led_set_ha_color(0, 0, 0, 0);
  }
  ha_light_wdt_feed_if_own_task();

  ha_light_publish_state();
  ha_light_wdt_feed_if_own_task();
}

/**
 * @brief Prepne do herniho modu
 *
 * Obnovi sachovnici (led_show_chess_board).
 */
static void ha_light_switch_to_game_mode(void) {
  if (current_mode == HA_MODE_GAME) {
    return; // Already in game mode
  }

  ESP_LOGI(TAG, "Switching to GAME MODE");
  ha_light_wdt_feed_if_own_task();
  current_mode = HA_MODE_GAME;

  // Restore game LED state (refresh based on current game status)
  game_refresh_leds();
  ha_light_wdt_feed_if_own_task();

  // Obnovit tlačítka (zelená/modrá/červená podle dostupnosti a stisku)
  led_refresh_all_button_leds();
  ha_light_wdt_feed_if_own_task();

  // Publish state update
  ha_light_publish_state();
  ha_light_wdt_feed_if_own_task();
}

// ============================================================================
// MQTT NVS KONFIGURACE FUNKCE
// ============================================================================

/**
 * @brief Načte MQTT konfiguraci z NVS
 *
 * @param host Buffer pro broker host (min 128 bytes)
 * @param host_len Velikost host bufferu
 * @param port Ukazatel na port (1-65535)
 * @param username Buffer pro username (min 64 bytes, může být prázdné)
 * @param username_len Velikost username bufferu
 * @param password Buffer pro password (min 64 bytes, může být prázdné)
 * @param password_len Velikost password bufferu
 * @return ESP_OK při úspěchu, ESP_ERR_NOT_FOUND pokud není v NVS (vrátí
 * defaults)
 */
static esp_err_t mqtt_load_config_from_nvs(char *host, size_t host_len,
                                           uint16_t *port, char *username,
                                           size_t username_len, char *password,
                                           size_t password_len) {
  if (host == NULL || port == NULL || username == NULL || password == NULL ||
      host_len == 0 || username_len == 0 || password_len == 0) {
    ESP_LOGE(TAG, "Invalid parameters for mqtt_load_config_from_nvs");
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(MQTT_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "MQTT config not found in NVS, using defaults");
    // Vrátit default hodnoty
    strncpy(host, MQTT_DEFAULT_HOST, host_len - 1);
    host[host_len - 1] = '\0';
    *port = MQTT_DEFAULT_PORT;
    strncpy(username, MQTT_DEFAULT_USERNAME, username_len - 1);
    username[username_len - 1] = '\0';
    strncpy(password, MQTT_DEFAULT_PASSWORD, password_len - 1);
    password[password_len - 1] = '\0';
    return ESP_ERR_NOT_FOUND; // Není v NVS, ale vrátil defaults
  }

  // Načíst host
  size_t required_size = host_len;
  ret = nvs_get_str(nvs_handle, MQTT_NVS_KEY_HOST, host, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to get MQTT host from NVS, using default");
    strncpy(host, MQTT_DEFAULT_HOST, host_len - 1);
    host[host_len - 1] = '\0';
  }

  // Načíst port
  uint32_t port_val = MQTT_DEFAULT_PORT;
  ret = nvs_get_u32(nvs_handle, MQTT_NVS_KEY_PORT, &port_val);
  if (ret != ESP_OK || port_val == 0 || port_val > 65535) {
    ESP_LOGW(TAG, "Failed to get MQTT port from NVS, using default");
    port_val = MQTT_DEFAULT_PORT;
  }
  *port = (uint16_t)port_val;

  // Načíst username (volitelné - může být prázdné)
  required_size = username_len;
  ret =
      nvs_get_str(nvs_handle, MQTT_NVS_KEY_USERNAME, username, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "MQTT username not in NVS, using empty");
    strncpy(username, MQTT_DEFAULT_USERNAME, username_len - 1);
    username[username_len - 1] = '\0';
  }

  // Načíst password (volitelné - může být prázdné)
  required_size = password_len;
  ret =
      nvs_get_str(nvs_handle, MQTT_NVS_KEY_PASSWORD, password, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "MQTT password not in NVS, using empty");
    strncpy(password, MQTT_DEFAULT_PASSWORD, password_len - 1);
    password[password_len - 1] = '\0';
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "MQTT config loaded from NVS: host=%s, port=%d, username=%s",
           host, *port, username[0] ? username : "(empty)");

  return ESP_OK;
}

/**
 * @brief Uloží MQTT konfiguraci do NVS
 *
 * @param host Broker hostname/IP
 * @param port Broker port (1-65535)
 * @param username MQTT username (NULL nebo prázdné = bez auth)
 * @param password MQTT password (NULL nebo prázdné = bez auth)
 * @return ESP_OK při úspěchu, chybový kód při chybě
 */
esp_err_t mqtt_save_config_to_nvs(const char *host, uint16_t port,
                                  const char *username, const char *password) {
  if (host == NULL || port == 0) {
    ESP_LOGE(TAG, "Invalid parameters: host is NULL or port is 0");
    return ESP_ERR_INVALID_ARG;
  }

  size_t host_len = strlen(host);
  if (host_len == 0 || host_len > 127) {
    ESP_LOGE(TAG, "Invalid host length: %zu (must be 1-127)", host_len);
    return ESP_ERR_INVALID_ARG;
  }

  // Validovat username/password pokud jsou nastavené
  if (username != NULL) {
    size_t username_len = strlen(username);
    if (username_len > 63) {
      ESP_LOGE(TAG, "Invalid username length: %zu (max 63)", username_len);
      return ESP_ERR_INVALID_ARG;
    }
  }

  if (password != NULL) {
    size_t password_len = strlen(password);
    if (password_len > 63) {
      ESP_LOGE(TAG, "Invalid password length: %zu (max 63)", password_len);
      return ESP_ERR_INVALID_ARG;
    }
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(MQTT_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    return ret;
  }

  // Uložit host
  ret = nvs_set_str(nvs_handle, MQTT_NVS_KEY_HOST, host);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT host in NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Uložit port
  ret = nvs_set_u32(nvs_handle, MQTT_NVS_KEY_PORT, (uint32_t)port);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT port in NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Uložit username (pokud je nastavený, jinak uložit prázdný string)
  const char *username_to_save =
      (username != NULL && strlen(username) > 0) ? username : "";
  ret = nvs_set_str(nvs_handle, MQTT_NVS_KEY_USERNAME, username_to_save);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT username in NVS: %s",
             esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Uložit password (pokud je nastavený, jinak uložit prázdný string)
  const char *password_to_save =
      (password != NULL && strlen(password) > 0) ? password : "";
  ret = nvs_set_str(nvs_handle, MQTT_NVS_KEY_PASSWORD, password_to_save);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT password in NVS: %s",
             esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_commit(nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "MQTT config saved to NVS: host=%s, port=%d, username=%s", host,
           port, username_to_save[0] ? username_to_save : "(empty)");

  return ESP_OK;
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================

/**
 * @brief Publish Home Assistant Auto-Discovery configuration
 */
static void ha_light_publish_discovery(void) {
  if (!mqtt_connected || mqtt_client == NULL) {
    return;
  }
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    ESP_LOGD(TAG, "[STAGING] ha_light_publish_discovery: skip (low heap)");
    return;
  }

  // Get MAC address for unique ID
  uint8_t mac_raw[6];
  esp_read_mac(mac_raw, ESP_MAC_WIFI_STA);
  char mac_str[13];
  snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x", mac_raw[0],
           mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);

  // Construct unique ID and name
  char unique_id[64];
  snprintf(unique_id, sizeof(unique_id), "esp32_chess_light_%s", mac_str);

  // Construct discovery topic: homeassistant/light/[node_id]/config
  char discovery_topic[128];
  snprintf(discovery_topic, sizeof(discovery_topic), "%s/%s/%s/config",
           HA_DISCOVERY_PREFIX, HA_COMPONENT_LIGHT, unique_id);

  ESP_LOGI(TAG, "Publishing HA Discovery to %s", discovery_topic);

  // Create JSON payload
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "name", "CzechMate");
  cJSON_AddStringToObject(json, "unique_id", unique_id);
  cJSON_AddStringToObject(json, "command_topic", HA_TOPIC_LIGHT_COMMAND);
  cJSON_AddStringToObject(json, "state_topic", HA_TOPIC_LIGHT_STATE);
  cJSON_AddStringToObject(json, "availability_topic",
                          HA_TOPIC_LIGHT_AVAILABILITY);
  cJSON_AddStringToObject(json, "payload_available", HA_MQTT_PAYLOAD_ONLINE);
  cJSON_AddStringToObject(json, "payload_not_available",
                          HA_MQTT_PAYLOAD_OFFLINE);
  cJSON_AddStringToObject(json, "schema", "json");
  cJSON_AddBoolToObject(json, "brightness", true);
  cJSON_AddBoolToObject(json, "color_mode", true);
  cJSON *color_modes = cJSON_CreateArray();
  cJSON_AddItemToArray(color_modes, cJSON_CreateString("rgb"));
  cJSON_AddItemToObject(json, "supported_color_modes", color_modes);

  // Effect support
  cJSON_AddBoolToObject(json, "effect", true);
  cJSON *effects = cJSON_CreateArray();
  cJSON_AddItemToArray(effects, cJSON_CreateString("rainbow"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("pulse"));
  cJSON_AddItemToArray(effects, cJSON_CreateString("static"));
  cJSON_AddItemToObject(json, "effect_list", effects);

  // Device info
  cJSON *device = cJSON_CreateObject();
  cJSON_AddStringToObject(device, "identifiers", unique_id);
  cJSON_AddStringToObject(device, "name", "ESP32 Chess System");
  cJSON_AddStringToObject(device, "manufacturer", HA_DEVICE_MANUFACTURER);
  cJSON_AddStringToObject(device, "model", HA_DEVICE_MODEL);
  cJSON_AddStringToObject(device, "sw_version", HA_DEVICE_SW_VERSION);
  cJSON_AddItemToObject(json, "device", device);

  char *json_str = cJSON_PrintUnformatted(json);
  if (json_str != NULL) {
    // Publish with retain=true so HA picks it up anytime
    esp_mqtt_client_publish(mqtt_client, discovery_topic, json_str, 0, 1, 1);
    free(json_str);
  }
  cJSON_Delete(json);
}

/**
 * @brief MQTT event handler
 */
static void ha_light_mqtt_event_handler(void *handler_args,
                                        esp_event_base_t base, int32_t event_id,
                                        void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t client = event->client;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT Connected");
    mqtt_connected = true;

    // Subscribe to light command topic
    esp_mqtt_client_subscribe(client, HA_TOPIC_LIGHT_COMMAND, 0);
    ESP_LOGI(TAG, "Subscribed to %s", HA_TOPIC_LIGHT_COMMAND);

    // Publish Auto-Discovery Config
    ha_light_publish_discovery();

    // Publish availability: online
    esp_mqtt_client_publish(client, HA_TOPIC_LIGHT_AVAILABILITY,
                            HA_MQTT_PAYLOAD_ONLINE, 0, 1, 1);

    // Publish initial state
    ha_light_publish_state();
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT Disconnected");
    mqtt_connected = false;
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT DATA: topic=%.*s, data=%.*s", event->topic_len,
             event->topic, event->data_len, event->data);

    // Handle command
    char topic[128] = {0};
    char data[512] = {0};

    if (event->topic_len < sizeof(topic) && event->data_len < sizeof(data)) {
      memcpy(topic, event->topic, event->topic_len);
      memcpy(data, event->data, event->data_len);
      ha_light_handle_mqtt_command(topic, data, event->data_len);
    }
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGW(TAG, "MQTT Error");
    break;

  default:
    break;
  }
}

/**
 * @brief Zpracuje MQTT prikaz
 */
static void ha_light_handle_mqtt_command(const char *topic, const char *data,
                                         int data_len) {
  if (strcmp(topic, HA_TOPIC_LIGHT_COMMAND) != 0) {
    return; // Not our topic
  }

  // Check if we can apply HA command during GAME mode
  bool can_apply_command = true;
  if (current_mode == HA_MODE_GAME) {
    uint32_t current_time_ms = esp_timer_get_time() / 1000;
    uint32_t idle_ms = current_time_ms - last_activity_time_ms;

    if (idle_ms >= HA_ACTIVITY_TIMEOUT_COMMAND_MS ||
        game_get_move_count() == 0) {
      // Idle ≥ 2min OR no moves yet → allow switch to HA mode
      if (game_get_move_count() == 0) {
        ESP_LOGI(TAG,
                 "HA command accepted immediately (game not started, moves=0)");
      } else {
        ESP_LOGI(
            TAG,
            "HA command after %lu ms idle (≥2min) - will switch to HA mode",
            idle_ms);
      }
      can_apply_command = true;
    } else {
      // Idle < 2min AND game in progress → ignore command (game has priority)
      ESP_LOGI(
          TAG,
          "HA command IGNORED (game active, moves=%lu, idle=%lu ms < 2min)",
          game_get_move_count(), idle_ms);
      can_apply_command = false;
    }
  }

  // Parse JSON
  cJSON *json = cJSON_ParseWithLength(data, data_len);
  if (json == NULL) {
    ESP_LOGE(TAG, "Failed to parse MQTT JSON");
    return;
  }

  bool new_state = ha_light_state.state;
  uint8_t new_r = ha_light_state.r, new_g = ha_light_state.g,
          new_b = ha_light_state.b;
  uint8_t new_brightness = ha_light_state.brightness;

  cJSON *state_item = cJSON_GetObjectItem(json, "state");
  if (cJSON_IsString(state_item)) {
    if (strcasecmp(state_item->valuestring, "ON") == 0) {
      new_state = true;
    } else if (strcasecmp(state_item->valuestring, "OFF") == 0) {
      new_state = false;
    }
  }

  cJSON *brightness_item = cJSON_GetObjectItem(json, "brightness");
  if (cJSON_IsNumber(brightness_item)) {
    int brightness_val = (int)cJSON_GetNumberValue(brightness_item);
    if (brightness_val < 0)
      brightness_val = 0;
    if (brightness_val > 255)
      brightness_val = 255;
    new_brightness = (uint8_t)brightness_val;
  }

  cJSON *color_item = cJSON_GetObjectItem(json, "color");
  if (cJSON_IsObject(color_item)) {
    cJSON *r = cJSON_GetObjectItem(color_item, "r");
    cJSON *g = cJSON_GetObjectItem(color_item, "g");
    cJSON *b = cJSON_GetObjectItem(color_item, "b");
    if (cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
      new_r = (uint8_t)cJSON_GetNumberValue(r);
      new_g = (uint8_t)cJSON_GetNumberValue(g);
      new_b = (uint8_t)cJSON_GetNumberValue(b);
    }
  }

  cJSON *rgb_item = cJSON_GetObjectItem(json, "rgb_color");
  if (cJSON_IsArray(rgb_item) && cJSON_GetArraySize(rgb_item) >= 3) {
    new_r = (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(rgb_item, 0));
    new_g = (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(rgb_item, 1));
    new_b = (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(rgb_item, 2));
  }

  char new_effect[32] = "solid";
  cJSON *effect_item = cJSON_GetObjectItem(json, "effect");
  if (cJSON_IsString(effect_item) && effect_item->valuestring) {
    strncpy(new_effect, effect_item->valuestring, sizeof(new_effect) - 1);
    new_effect[sizeof(new_effect) - 1] = '\0';
  }

  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ha_light_state.state = new_state;
    ha_light_state.r = new_r;
    ha_light_state.g = new_g;
    ha_light_state.b = new_b;
    ha_light_state.brightness = new_brightness;
    strncpy(ha_light_state.effect, new_effect,
            sizeof(ha_light_state.effect) - 1);
    ha_light_state.effect[sizeof(ha_light_state.effect) - 1] = '\0';
    xSemaphoreGive(ha_light_state_mutex);
  }

  cJSON_Delete(json);

  // Apply changes based on mode and idle time
  if (can_apply_command) {
    if (current_mode == HA_MODE_GAME) {
      // Switch from GAME to HA mode
      ESP_LOGI(TAG, "Switching from GAME to HA mode (HA command accepted)");
      ha_light_switch_to_ha_mode();
    } else {
      // Already in HA mode, just re-apply
      ha_light_switch_to_ha_mode(); // Re-apply with new settings
    }
  }

  // Always publish state update (virtual state)
  ha_light_publish_state();
}

/**
 * @brief Publikuje stav do MQTT
 */
static void ha_light_publish_state(void) {
  if (!mqtt_connected || mqtt_client == NULL) {
    return;
  }
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    ESP_LOGD(TAG, "[STAGING] ha_light_publish_state: skip (heap < %d B)",
             HA_MQTT_STOP_HEAP_BYTES);
    return;
  }

  bool st;
  uint8_t br, rr, rg, rb;
  char eff[32];
  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    st = ha_light_state.state;
    br = ha_light_state.brightness;
    rr = ha_light_state.r;
    rg = ha_light_state.g;
    rb = ha_light_state.b;
    strncpy(eff, ha_light_state.effect, sizeof(eff) - 1);
    eff[sizeof(eff) - 1] = '\0';
    xSemaphoreGive(ha_light_state_mutex);
  } else {
    st = true;
    br = 255;
    rr = rg = rb = 255;
    eff[0] = '\0';
  }

  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "state", st ? "ON" : "OFF");
  cJSON_AddNumberToObject(json, "brightness", br);
  cJSON_AddStringToObject(json, "color_mode", "rgb");
  cJSON *color = cJSON_CreateObject();
  cJSON_AddNumberToObject(color, "r", rr);
  cJSON_AddNumberToObject(color, "g", rg);
  cJSON_AddNumberToObject(color, "b", rb);
  cJSON_AddItemToObject(json, "color", color);
  cJSON_AddStringToObject(json, "effect", eff);

  // Mode
  cJSON_AddStringToObject(json, "mode",
                          (current_mode == HA_MODE_GAME) ? "game" : "ha");

  // Activity timeout
  uint32_t current_time_ms = esp_timer_get_time() / 1000;
  uint32_t time_until_timeout = 0;
  if (last_activity_time_ms > 0 && current_mode == HA_MODE_GAME) {
    uint32_t elapsed = current_time_ms - last_activity_time_ms;
    if (elapsed < HA_ACTIVITY_TIMEOUT_AUTO_MS) {
      time_until_timeout = HA_ACTIVITY_TIMEOUT_AUTO_MS - elapsed;
    }
  }
  cJSON_AddNumberToObject(json, "activity_timeout_ms", time_until_timeout);

  char *json_str = cJSON_Print(json);
  if (json_str != NULL) {
    esp_mqtt_client_publish(mqtt_client, HA_TOPIC_LIGHT_STATE, json_str, 0, 1,
                            0);
    free(json_str);
  }
  cJSON_Delete(json);
}

/**
 * @brief Načte MQTT konfiguraci z NVS (pokud ještě nebyla načtena)
 */
static void mqtt_ensure_config_loaded(void) {
  if (mqtt_config.loaded) {
    return; // Already loaded
  }

  char host[128] = {0};
  char username[64] = {0};
  char password[64] = {0};
  uint16_t port = 1883;

  esp_err_t ret =
      mqtt_load_config_from_nvs(host, sizeof(host), &port, username,
                                sizeof(username), password, sizeof(password));
  if (ret == ESP_OK || ret == ESP_ERR_NOT_FOUND) {
    // Config loaded (or defaults used)
    strncpy(mqtt_config.host, host, sizeof(mqtt_config.host) - 1);
    mqtt_config.host[sizeof(mqtt_config.host) - 1] = '\0';
    mqtt_config.port = port;
    strncpy(mqtt_config.username, username, sizeof(mqtt_config.username) - 1);
    mqtt_config.username[sizeof(mqtt_config.username) - 1] = '\0';
    strncpy(mqtt_config.password, password, sizeof(mqtt_config.password) - 1);
    mqtt_config.password[sizeof(mqtt_config.password) - 1] = '\0';
    mqtt_config.loaded = true;
    ESP_LOGI(TAG, "MQTT config loaded: host=%s, port=%d", mqtt_config.host,
             mqtt_config.port);
  } else {
    ESP_LOGW(TAG, "Failed to load MQTT config from NVS, using defaults");
    // Keep defaults
    mqtt_config.loaded = true;
  }
}

/**
 * @brief Inicializuje MQTT klienta
 *
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
static esp_err_t ha_light_init_mqtt(void) {
  // Prevent multiple clients/memory leaks
  if (mqtt_client != NULL) {
    ESP_LOGI(TAG, "MQTT client already initialized, skipping re-init");
    return ESP_OK;
  }

  size_t free_heap = esp_get_free_heap_size();
  if (free_heap < HA_MQTT_MIN_FREE_HEAP_BYTES) {
    ESP_LOGW(TAG,
             "MQTT deferred: free heap %zu B < %d B (need RAM for mqtt task)",
             free_heap, HA_MQTT_MIN_FREE_HEAP_BYTES);
    return ESP_ERR_NO_MEM;
  }

  // Ensure config is loaded from NVS (first time only)
  mqtt_ensure_config_loaded();

  // Build MQTT broker URI (buffer size 512 to avoid format-truncation warning)
  char mqtt_broker_uri[512];
  if (mqtt_config.username[0] != '\0' && mqtt_config.password[0] != '\0') {
    // MQTT URI with username/password: mqtt://user:pass@host:port
    snprintf(mqtt_broker_uri, sizeof(mqtt_broker_uri), "mqtt://%s:%s@%s:%d",
             mqtt_config.username, mqtt_config.password, mqtt_config.host,
             mqtt_config.port);
  } else {
    // MQTT URI without auth: mqtt://host:port
    snprintf(mqtt_broker_uri, sizeof(mqtt_broker_uri), "mqtt://%s:%d",
             mqtt_config.host, mqtt_config.port);
  }

  // Generate unique Client ID combined from Base + MAC
  uint8_t mac_raw[6];
  esp_read_mac(mac_raw, ESP_MAC_WIFI_STA);
  char client_id[64];
  snprintf(client_id, sizeof(client_id), "%s-%02x%02x%02x%02x%02x%02x",
           HA_MQTT_CLIENT_ID, mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3],
           mac_raw[4], mac_raw[5]);

  ESP_LOGI(TAG, "Initializing MQTT client: %s (ID: %s)", mqtt_broker_uri,
           client_id);

  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = mqtt_broker_uri,
      .credentials.client_id = client_id,
      .session.last_will.topic = HA_TOPIC_LIGHT_AVAILABILITY,
      .session.last_will.msg = HA_MQTT_PAYLOAD_OFFLINE,
      .session.last_will.qos = 1,
      .session.last_will.retain = true,
      .session.keepalive = 120, // Increase keepalive to 120s for stability
      .network.disable_auto_reconnect =
          false, // Ensure auto-reconnect is enabled
  };

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (mqtt_client == NULL) {
    ESP_LOGE(TAG, "Failed to initialize MQTT client");
    return ESP_FAIL;
  }

  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 ha_light_mqtt_event_handler, NULL);

  esp_err_t ret = esp_mqtt_client_start(mqtt_client);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    return ret;
  }

  ESP_LOGI(TAG, "MQTT client initialized and started");
  return ESP_OK;
}

// ============================================================================
// LAMP NVS PERSISTENCE
// ============================================================================

static void lamp_nvs_load(void) {
  nvs_handle_t handle;
  esp_err_t ret = nvs_open(LAMP_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (ret != ESP_OK) {
    return;
  }

  uint8_t state_u8 = 1, r = 255, g = 255, b = 255;
  uint32_t auto_sec = HA_ACTIVITY_TIMEOUT_AUTO_DEFAULT_SEC;
  nvs_get_u8(handle, LAMP_NVS_KEY_STATE, &state_u8);
  nvs_get_u8(handle, LAMP_NVS_KEY_R, &r);
  nvs_get_u8(handle, LAMP_NVS_KEY_G, &g);
  nvs_get_u8(handle, LAMP_NVS_KEY_B, &b);
  if (nvs_get_u32(handle, LAMP_NVS_KEY_AUTO_TIMEOUT_SEC, &auto_sec) == ESP_OK &&
      auto_sec >= HA_ACTIVITY_TIMEOUT_AUTO_MIN_SEC &&
      auto_sec <= HA_ACTIVITY_TIMEOUT_AUTO_MAX_SEC) {
    activity_timeout_auto_sec = auto_sec;
  }
  nvs_close(handle);

  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ha_light_state.state = (state_u8 != 0);
    ha_light_state.r = r;
    ha_light_state.g = g;
    ha_light_state.b = b;
    ha_light_state.brightness = 255;
    xSemaphoreGive(ha_light_state_mutex);
  }
}

static void lamp_nvs_save(void) {
  ha_light_task_wdt_reset_safe();
  bool st;
  uint8_t r, g, b;
  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    st = ha_light_state.state;
    r = ha_light_state.r;
    g = ha_light_state.g;
    b = ha_light_state.b;
    xSemaphoreGive(ha_light_state_mutex);
  } else {
    return;
  }

  nvs_handle_t handle;
  if (nvs_open(LAMP_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
    ESP_LOGW(TAG, "lamp_nvs_save: nvs_open failed");
    return;
  }
  nvs_set_u8(handle, LAMP_NVS_KEY_STATE, st ? 1 : 0);
  nvs_set_u8(handle, LAMP_NVS_KEY_R, r);
  nvs_set_u8(handle, LAMP_NVS_KEY_G, g);
  nvs_set_u8(handle, LAMP_NVS_KEY_B, b);
  ha_light_task_wdt_reset_safe();
  esp_err_t err = nvs_commit(handle);
  ha_light_task_wdt_reset_safe();
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "lamp_nvs_save: nvs_commit failed (%s)",
             esp_err_to_name(err));
  }
}

static void lamp_nvs_save_auto_timeout(void) {
  nvs_handle_t handle;
  if (nvs_open(LAMP_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
    return;
  }
  nvs_set_u32(handle, LAMP_NVS_KEY_AUTO_TIMEOUT_SEC, activity_timeout_auto_sec);
  nvs_commit(handle);
  nvs_close(handle);
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * @brief Ziskej aktualni rezim
 */
ha_mode_t ha_light_get_mode(void) { return current_mode; }

uint32_t ha_light_get_activity_timeout_sec(void) {
  return activity_timeout_auto_sec;
}

esp_err_t ha_light_set_activity_timeout_sec(uint32_t sec) {
  if (sec < HA_ACTIVITY_TIMEOUT_AUTO_MIN_SEC) {
    sec = HA_ACTIVITY_TIMEOUT_AUTO_MIN_SEC;
  }
  if (sec > HA_ACTIVITY_TIMEOUT_AUTO_MAX_SEC) {
    sec = HA_ACTIVITY_TIMEOUT_AUTO_MAX_SEC;
  }
  activity_timeout_auto_sec = sec;
  lamp_nvs_save_auto_timeout();
  ESP_LOGI(TAG, "Auto lamp timeout set to %lu s (saved to NVS)",
           (unsigned long)sec);
  return ESP_OK;
}

/**
 * @brief Ziskej aktualni stav lampy (thread-safe pro HTTP handler)
 */
void ha_light_get_state(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *brightness,
                        bool *state) {
  if (r)
    *r = 255;
  if (g)
    *g = 255;
  if (b)
    *b = 255;
  if (brightness)
    *brightness = 255;
  if (state)
    *state = true;

  if (ha_light_state_mutex == NULL) {
    return;
  }
  if (xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }
  if (r)
    *r = ha_light_state.r;
  if (g)
    *g = ha_light_state.g;
  if (b)
    *b = ha_light_state.b;
  if (brightness)
    *brightness = ha_light_state.brightness;
  if (state)
    *state = ha_light_state.state;
  xSemaphoreGive(ha_light_state_mutex);
}

/**
 * @brief Pozadavek na nastaveni lampy z webu
 * @return true pokud prikaz odeslan do fronty, false pokud fronta NULL nebo
 * plna
 */
bool ha_light_request_web_lamp(bool state, uint8_t r, uint8_t g, uint8_t b) {
  ha_light_command_t cmd;
  cmd.type = HA_CMD_WEB_LAMP;
  cmd.u.web_lamp.state = state ? 1 : 0;
  cmd.u.web_lamp.r = r;
  cmd.u.web_lamp.g = g;
  cmd.u.web_lamp.b = b;

  if (ha_light_cmd_queue == NULL) {
    return false;
  }
  return xQueueSend(ha_light_cmd_queue, &cmd, 0) == pdTRUE;
}

/**
 * @brief Zjisti zda je HA rezim dostupny (WiFi STA pripojeno)
 */
bool ha_light_is_available(void) { return ha_light_check_wifi_sta_connected(); }

/**
 * @brief Získá MQTT konfiguraci (načte z NVS pokud ještě nebyla načtena)
 */
esp_err_t mqtt_get_config(char *host, size_t host_len, uint16_t *port,
                          char *username, size_t username_len, char *password,
                          size_t password_len) {
  if (host == NULL || port == NULL || username == NULL || password == NULL ||
      host_len == 0 || username_len == 0 || password_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Ensure config is loaded
  mqtt_ensure_config_loaded();

  // Copy current config
  strncpy(host, mqtt_config.host, host_len - 1);
  host[host_len - 1] = '\0';
  *port = mqtt_config.port;
  strncpy(username, mqtt_config.username, username_len - 1);
  username[username_len - 1] = '\0';
  strncpy(password, mqtt_config.password, password_len - 1);
  password[password_len - 1] = '\0';

  return ESP_OK;
}

/**
 * @brief Zjisti zda je MQTT klient pripojen
 */
bool ha_light_is_mqtt_connected(void) { return mqtt_connected; }

/**
 * @brief Odpoji a reinicializuje MQTT klienta s aktualni konfiguraci z NVS
 *
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t ha_light_reinit_mqtt(void) {
  // Check WiFi STA connection
  if (!ha_light_check_wifi_sta_connected()) {
    ESP_LOGW(TAG, "Cannot reinit MQTT: WiFi STA not connected");
    return ESP_ERR_INVALID_STATE;
  }

  // Stop and destroy existing MQTT client if exists
  if (mqtt_client != NULL) {
    ESP_LOGI(TAG, "Stopping existing MQTT client for reinit");
    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_connected = false;
  }

  // Reset config loaded flag to force reload from NVS
  mqtt_config.loaded = false;

  // Initialize new MQTT client with updated config
  esp_err_t ret = ha_light_init_mqtt();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to reinit MQTT client: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "MQTT client reinicialized successfully");
  return ESP_OK;
}

// ============================================================================
// MAIN TASK LOOP
// ============================================================================

/**
 * @brief Hlavni funkce HA Light tasku
 */
void ha_light_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "Starting HA Light Task...");
  s_ha_light_task_hdl = xTaskGetCurrentTaskHandle();

  // Register with WDT
  esp_err_t ret = esp_task_wdt_add(NULL);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGW(TAG, "Failed to register with WDT: %s", esp_err_to_name(ret));
  }

  task_running = true;
  current_mode = HA_MODE_GAME;
  last_activity_time_ms = esp_timer_get_time() / 1000;

  ha_light_state_mutex = xSemaphoreCreateMutex();
  if (ha_light_state_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create ha_light_state mutex");
  }

  ha_light_cmd_queue = xQueueCreate(10, sizeof(ha_light_command_t));
  if (ha_light_cmd_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create HA command queue");
  }

  lamp_nvs_load();

  // Main loop
  // TickType_t last_wake_time = xTaskGetTickCount(); // Unused in queue-based
  // loop

  for (;;) {
    // =========================================================================
    // 🛑 BOOT ANIMATION PROTECTION - BLOKUJE HA OPERACE BĚHEM BOOT
    // =========================================================================
    // Pokud běží boot animace, HA task nesmí posílat LED příkazy!
    // (např. "zhasni světlo" při rychlém WiFi připojení)
    if (led_is_booting()) {
      ha_light_task_wdt_reset_safe();
      vTaskDelay(pdMS_TO_TICKS(10)); // 10ms - rychlé probuzení po fade_out!
      continue;
    }
    // =========================================================================

    // Reset WDT
    ha_light_task_wdt_reset_safe();

    // Process queue with timeout (100ms) - replaces vTaskDelay
    ha_light_command_t cmd;
    if (ha_light_cmd_queue &&
        xQueueReceive(ha_light_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (cmd.type == HA_CMD_WEB_LAMP) {
        if (ha_light_state_mutex != NULL &&
            xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) ==
                pdTRUE) {
          ha_light_state.state = (cmd.u.web_lamp.state != 0);
          ha_light_state.r = cmd.u.web_lamp.r;
          ha_light_state.g = cmd.u.web_lamp.g;
          ha_light_state.b = cmd.u.web_lamp.b;
          ha_light_state.brightness = 255;
          xSemaphoreGive(ha_light_state_mutex);
        }
        lamp_nvs_save();
        ha_light_switch_to_ha_mode();
      } else if (cmd.type == HA_CMD_ACTIVITY) {
        const char *activity_name = (const char *)cmd.u.data;
        // Reset activity timer in HA task context
        last_activity_time_ms = esp_timer_get_time() / 1000;

        // If currently in HA mode, switch back to GAME mode on first activity
        if (current_mode == HA_MODE_HA) {
          ESP_LOGI(TAG, "Switching from HA mode to GAME mode (activity: %s)",
                   activity_name ? activity_name : "unknown");
          ha_light_switch_to_game_mode();
        }

        // Check rate limit logic again here (in HA task context) to save MQTT
        // bandwidth
        static uint32_t last_mqtt_activity = 0;
        uint32_t now = esp_timer_get_time() / 1000;

        if (now - last_mqtt_activity > 500) { // 500ms limit
          last_mqtt_activity = now;

          if (mqtt_connected && mqtt_client != NULL) {
            char topic[128];
            snprintf(topic, sizeof(topic), "%s", HA_TOPIC_GAME_ACTIVITY);
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "event",
                                    activity_name ? activity_name : "unknown");
            cJSON_AddNumberToObject(json, "timestamp_ms", now);
            char *json_str = cJSON_PrintUnformatted(json);
            if (json_str) {
              esp_mqtt_client_publish(mqtt_client, topic, json_str, 0, 0, 0);
              free(json_str);
            }
            cJSON_Delete(json);
          }
        }
      }
      ha_light_task_wdt_reset_safe();
    }

    // POLL: Check WiFi STA status periodically (every 5 seconds)
    static uint32_t last_wifi_check = 0;
    static uint32_t last_mqtt_retry_ms = 0;
    uint32_t current_time_ms = esp_timer_get_time() / 1000;
    if (current_time_ms - last_wifi_check >= 5000) {
      bool wifi_connected = ha_light_check_wifi_sta_connected();
      if (wifi_connected && sta_connected && mqtt_client != NULL) {
        size_t fh = esp_get_free_heap_size();
        if (fh < HA_MQTT_STOP_HEAP_BYTES) {
          ESP_LOGW(TAG,
                   "[STAGING] MQTT stop: critical heap %zu B (<%d B) — release "
                   "client",
                   fh, HA_MQTT_STOP_HEAP_BYTES);
          esp_mqtt_client_stop(mqtt_client);
          esp_mqtt_client_destroy(mqtt_client);
          mqtt_client = NULL;
          mqtt_connected = false;
        }
      }
      if (wifi_connected && !sta_connected) {
        sta_connected = true;
        ESP_LOGI(TAG, "WiFi STA connected - initializing MQTT");
        ha_light_task_wdt_reset_safe();
        ha_light_init_mqtt();
      } else if (wifi_connected && sta_connected && mqtt_client == NULL) {
        /* Opakovat po uvolnění heap; při NO_MEM nečastěji než 1×/30 s (šetří
         * CPU a WDT) */
        if (current_time_ms - last_mqtt_retry_ms >= 30000) {
          last_mqtt_retry_ms = current_time_ms;
          ha_light_task_wdt_reset_safe();
          esp_err_t mr = ha_light_init_mqtt();
          if (mr == ESP_ERR_NO_MEM) {
            ESP_LOGW(TAG, "MQTT retry skipped/low heap: free=%zu B",
                     (size_t)esp_get_free_heap_size());
          }
        }
      } else if (!wifi_connected && sta_connected) {
        // WiFi disconnected
        sta_connected = false;
        mqtt_connected = false; // Just mark disconnected, don't destroy client
                                // to avoid complex cleanup
        ESP_LOGI(TAG, "WiFi STA disconnected - MQTT unavailable");
        if (current_mode == HA_MODE_HA) {
          // Switch back to game mode
          ha_light_switch_to_game_mode();
        }
      }
      last_wifi_check = current_time_ms;
    }

    // Monitor game activity (checks queues for activity)
    ha_light_monitor_game_activity();

    // Check activity timeout (5 minutes or value that is set in ap or web)
    ha_light_check_activity_timeout();

    // Periodically publish state (every 30 seconds)
    static uint32_t last_state_publish = 0;
    if (mqtt_connected && (current_time_ms - last_state_publish >= 30000)) {
      ha_light_publish_state();
      last_state_publish = current_time_ms;

      // Periodically enforce "online" status (every 60s) to fix "Unknown"
      // status issues
      static uint32_t last_avail_publish = 0;
      if (current_time_ms - last_avail_publish >= 60000) {
        esp_mqtt_client_publish(mqtt_client, HA_TOPIC_LIGHT_AVAILABILITY,
                                HA_MQTT_PAYLOAD_ONLINE, 0, 1, 1);
        last_avail_publish = current_time_ms;
      }
    }

    // Minimal yield not needed because xQueueReceive handles waiting
  }
}
