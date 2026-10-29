/**
 * @file ha_light_task.c
 * @brief HA Light Task - Home Assistant RGB Light Integration via MQTT
 *
 * @details
 * =============================================================================
 * WHAT DID THIS FILE DO?
 * =============================================================================
 *
 * This task integrates the box as an RGB light into the Home Assistant via MQTT.
 * After connecting to the WiFi STA, the board automatically switches to HA mode after 5 minutes
 * idleness. In HA mode, all LED boards behave as one RGB light.
 *
 * Modes:
 * - GAME MODE: LED displays chest (default)
 * - HA MODE: All 64 LEDs as RGB light controlled via HA (after 5 min of inactivity)
 *
 * Automatic switching:
 * - GAME -> HA: After 5 minutes of no activity (figure movement or game command)
 * - HA -> GAME: Immediately upon detection of piece movement (PICKUP/DROP)
 *
 * @author Alfred Krutina
 * @version 1.8.0
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
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "freertos_chess.h"
#include "led_mapping.h"
#include "task_wdt.h"
#include "game_task.h"
#include "game_gesture.h"
#include "led_task.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "HA_LIGHT_TASK";

/** esp-mqtt uses this when task.stack_size is 0. Custom menuconfig is off,
 * so mqtt_config.h falls back to 6144. xTaskCreate needs one internal block
 * of that size. In buffer is 512, out buffer is HA_MQTT_PAYLOAD_MAX.
 * URI is mqtt:// so mbedtls content buffers are not taken
 * (CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN is 16384 and does not fit a 14 KiB block).
 * tcp_snd is the sdkconfig send-buffer cap the broker socket grows toward. */
#if CONFIG_MQTT_TASK_STACK_SIZE
#define HA_MQTT_TASK_STACK_BYTES CONFIG_MQTT_TASK_STACK_SIZE
#else
#define HA_MQTT_TASK_STACK_BYTES (6 * 1024)
#endif
#define HA_MQTT_IN_BYTES 512
/* HA_MQTT_PAYLOAD_MAX is the out buffer; defined just below. */
#define HA_MQTT_OUT_BYTES 768
#define HA_MQTT_TCP_SND_BYTES CONFIG_LWIP_TCP_SND_BUF_DEFAULT
#define HA_MQTT_CLIENT_OVERHEAD_BYTES 2048
#define HA_MQTT_NEED_BYTES                                                     \
  ((size_t)HA_MQTT_TASK_STACK_BYTES + (size_t)HA_MQTT_IN_BYTES +              \
   (size_t)HA_MQTT_OUT_BYTES + (size_t)HA_MQTT_TCP_SND_BYTES +                \
   (size_t)HA_MQTT_CLIENT_OVERHEAD_BYTES)
/** Biggest single internal alloc is the task stack, not the TCP cap. */
#define HA_MQTT_BIGGEST_BLOCK_BYTES ((size_t)HA_MQTT_TASK_STACK_BYTES)
/** Heap must stay above the start gate this long before a start. */
#define HA_MQTT_CALM_MS 8000u
/** Release the client as soon as WiFi/lwIP lose their floor. */
#define HA_MQTT_STOP_HEAP_BYTES CHESS_WIFI_HEAP_FLOOR_BYTES
/* One reused payload. The client copies it before publish returns. */
#define HA_MQTT_PAYLOAD_MAX 768
_Static_assert(HA_MQTT_OUT_BYTES == HA_MQTT_PAYLOAD_MAX,
               "MQTT out buffer and payload slot must match");
static char s_mqtt_payload[HA_MQTT_PAYLOAD_MAX];

// ============================================================================
// NVS KONFIGURACE PRO MQTT
// ============================================================================

// NVS namespace and keys for MQTT configuration
#define MQTT_NVS_NAMESPACE "mqtt_config"
#define MQTT_NVS_KEY_HOST "broker_host"
#define MQTT_NVS_KEY_PORT "broker_port"
#define MQTT_NVS_KEY_USERNAME "broker_username"
#define MQTT_NVS_KEY_PASSWORD "broker_password"

// NVS to save lamp state (web / local mode)
#define LAMP_NVS_NAMESPACE "lamp_cfg"
#define LAMP_NVS_KEY_STATE "state"
#define LAMP_NVS_KEY_R "r"
#define LAMP_NVS_KEY_G "g"
#define LAMP_NVS_KEY_B "b"
#define LAMP_NVS_KEY_BRIGHTNESS "br"
#define LAMP_NVS_KEY_AUTO_TIMEOUT_SEC "auto_sec"
#define LAMP_NVS_KEY_MODE "mode"

// Range for automatic switching to lamp mode: 5 s .. 120 min (7200 s)
#define HA_ACTIVITY_TIMEOUT_AUTO_MIN_SEC 5
#define HA_ACTIVITY_TIMEOUT_AUTO_MAX_SEC 7200
#define HA_ACTIVITY_TIMEOUT_AUTO_DEFAULT_SEC 300

// Default values ​​for MQTT configuration
#define MQTT_DEFAULT_HOST "homeassistant.local"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_USERNAME "alfred"
#define MQTT_DEFAULT_PASSWORD "alfred"

// ============================================================================
// GLOBAL VARIABLES AND STATUS
// ============================================================================

// Task state
static bool task_running = false;
static ha_mode_t current_mode = HA_MODE_GAME;
static bool s_nvs_mode_found = false;
static ha_mode_t s_nvs_mode = HA_MODE_GAME;
static bool s_boot_mode_applied = false;

// Activity tracking – time (s) after which the lamp switches to mode (read from
// NVS)
static uint32_t activity_timeout_auto_sec =
    HA_ACTIVITY_TIMEOUT_AUTO_DEFAULT_SEC;
static uint32_t last_activity_time_ms = 0;

// WiFi STA status (shared with web_server_task via events)
static bool sta_connected = false;

// MQTT client
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool s_mqtt_down = false;
/** Next discovery message, or -1 when idle. Written from the MQTT task too. */
static volatile int s_disc_step = -1;
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

/** Color that was on the lamp before it was switched off. Never black. */
static uint8_t s_lit_r = 255, s_lit_g = 255, s_lit_b = 255, s_lit_br = 255;

static void remember_lit_color(uint8_t r, uint8_t g, uint8_t b, uint8_t br) {
  if ((r | g | b) == 0) {
    return;
  }
  s_lit_r = r;
  s_lit_g = g;
  s_lit_b = b;
  if (br > 0) {
    s_lit_br = br;
  }
}

// MQTT configuration (loaded from NVS)
static struct {
  char host[128];    // Broker hostname/IP
  uint16_t port;     // Broker port
  char username[64]; // MQTT username (empty = no auth)
  char password[64]; // MQTT password (empty = no auth)
  bool loaded;       // Whether the configuration was loaded from NVS
} mqtt_config = {.host = MQTT_DEFAULT_HOST,
                 .port = MQTT_DEFAULT_PORT,
                 .username = MQTT_DEFAULT_USERNAME,
                 .password = MQTT_DEFAULT_PASSWORD,
                 .loaded = false};

// Externi fronty
extern QueueHandle_t game_command_queue;
extern QueueHandle_t squares_event_queue;

// Internal queue for HA task
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
static bool ha_light_publish_state(void);
static void ha_game_apply(uint8_t fact, uint8_t reason);
static void ha_discovery_pump(void);
static bool ha_game_publish_state(void);
static void lamp_nvs_load(void);
static void lamp_nvs_save(void);
static void lamp_nvs_save_mode(void);
static void ha_pattern_nvs_erase_once(void);

// ============================================================================
// WDT HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe reset WDT
 */
/** Main loop handle — esp_task_wdt_reset only from ha_light_task (not from MQTT).
 */
static TaskHandle_t s_ha_light_task_hdl;

static void ha_light_wdt_feed_if_own_task(void) {
  (void)chess_task_wdt_reset_if_owner(TAG, s_ha_light_task_hdl);
}

// ============================================================================
// WIFI STA STATUS CHECKING
// ============================================================================

/**
 * @brief Check if WiFi STA is connected
 *
 * @return true if the STA is connected and has an IP, false otherwise
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
 * @brief Monitors game activity and resets the timer
 *
 * This function relies on ha_light_report_activity() called from other tasks.
 * We do not monitor the queues here, so as not to consume commands.
 */
static void ha_light_monitor_game_activity(void) {
  // Activity monitoring is done via ha_light_report_activity() calls from:
  // - game_task: when processing game commands
  // - squares_task: when detecting piece movement
  // We just check if we need to switch modes based on activity timeout
}

/**
 * @brief Game activity report (called from other tasks)
 */
void ha_light_report_activity(const char *activity_type) {
  if (!task_running) {
    return;
  }

  // Keep this function lightweight: it may be called from input paths.
  // Mode switching and LED refresh must happen in HA task context.
  last_activity_time_ms = esp_timer_get_time() / 1000;

  /* Rate limit: max 1 queued activity / 500 ms (saves the queue and MQTT).
   * Exception: POST /api/light/game_mode — must always pass, otherwise the user
   * will not bring the board back from lamp mode after a rapid series of other events. */
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
  /* Idle must not switch the board into the lamp. The click gesture still does. */
  (void)activity_timeout_auto_sec;
}

// ============================================================================
// MODE SWITCHING
// ============================================================================

/**
 * @brief Switches to HA mode
 *
 * All 64 LED boards will be set to the color from ha_light_state.
 */
static void ha_light_switch_to_ha_mode(void) {
  ha_light_wdt_feed_if_own_task();
  bool mode_changed = (current_mode != HA_MODE_HA);

  if (mode_changed) {
    ESP_LOGI(TAG, "Switching to HA MODE");
    lamp_nvs_save_mode();
  }
  current_mode = HA_MODE_HA;
  /* Game LED writes stop before the lamp color is applied. */
  led_set_lamp_hold(true);

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

  remember_lit_color(r, g, b, brightness);

  /* Light off releases the board: a lift then continues the game, or the
   * starting-position piece colors come back. */
  if (state) {
    led_set_ha_color(r, g, b, brightness);
  } else {
    led_set_ha_color(0, 0, 0, 0);
    led_set_lamp_hold(false);
  }
  ha_light_wdt_feed_if_own_task();

  ha_light_publish_state();
  ha_light_wdt_feed_if_own_task();
  if (mode_changed) {
    ha_game_apply(HA_GAME_LAMP, HA_REASON_NONE);
  }
}

/**
 * @brief Switches to game mode
 *
 * Restore the chessboard (led_show_chess_board).
 */
static void ha_light_switch_to_game_mode(void) {
  if (current_mode == HA_MODE_GAME) {
    return; // Already in game mode
  }

  ESP_LOGI(TAG, "Switching to GAME MODE");
  ha_light_wdt_feed_if_own_task();
  current_mode = HA_MODE_GAME;
  led_set_lamp_hold(false);
  /* Idle time already expired while the lamp was on. Without a fresh
   * timestamp the next loop would switch straight back to the lamp. */
  last_activity_time_ms = esp_timer_get_time() / 1000;
  lamp_nvs_save_mode();

  // Restore game LED state (refresh based on current game status)
  game_refresh_leds();
  ha_light_wdt_feed_if_own_task();

  // Reset buttons (green/blue/red depending on availability and press)
  led_refresh_all_button_leds();
  ha_light_wdt_feed_if_own_task();

  // Publish state update
  ha_light_publish_state();
  ha_light_wdt_feed_if_own_task();
  ha_game_apply(HA_GAME_BOARD, HA_REASON_NONE);
}

// ============================================================================
// MQTT NVS CONFIGURATION FUNCTION
// ============================================================================

/**
 * @brief Loads MQTT configuration from NVS
 *
 * @param host Buffer for broker host (min 128 bytes)
 * @param host_len Host buffer size
 * @param port Pointer to port (1-65535)
 * @param username Buffer for username (min 64 bytes, can be empty)
 * @param username_len The size of the username buffer
 * @param password Buffer for password (min 64 bytes, can be empty)
 * @param password_len Password buffer size
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not in NVS (return
 *defaults)
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
    // Return default values
    strncpy(host, MQTT_DEFAULT_HOST, host_len - 1);
    host[host_len - 1] = '\0';
    *port = MQTT_DEFAULT_PORT;
    strncpy(username, MQTT_DEFAULT_USERNAME, username_len - 1);
    username[username_len - 1] = '\0';
    strncpy(password, MQTT_DEFAULT_PASSWORD, password_len - 1);
    password[password_len - 1] = '\0';
    return ESP_ERR_NOT_FOUND; // Not in NVS, but returned defaults
  }

  // Load guest
  size_t required_size = host_len;
  ret = nvs_get_str(nvs_handle, MQTT_NVS_KEY_HOST, host, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to get MQTT host from NVS, using default");
    strncpy(host, MQTT_DEFAULT_HOST, host_len - 1);
    host[host_len - 1] = '\0';
  }

  // Load the port
  uint32_t port_val = MQTT_DEFAULT_PORT;
  ret = nvs_get_u32(nvs_handle, MQTT_NVS_KEY_PORT, &port_val);
  if (ret != ESP_OK || port_val == 0 || port_val > 65535) {
    ESP_LOGW(TAG, "Failed to get MQTT port from NVS, using default");
    port_val = MQTT_DEFAULT_PORT;
  }
  *port = (uint16_t)port_val;

  // Load username (optional - can be empty)
  required_size = username_len;
  ret =
      nvs_get_str(nvs_handle, MQTT_NVS_KEY_USERNAME, username, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "MQTT username not in NVS, using default");
    strncpy(username, MQTT_DEFAULT_USERNAME, username_len - 1);
    username[username_len - 1] = '\0';
  }

  // Load password (optional - can be empty)
  required_size = password_len;
  ret =
      nvs_get_str(nvs_handle, MQTT_NVS_KEY_PASSWORD, password, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "MQTT password not in NVS, using default");
    strncpy(password, MQTT_DEFAULT_PASSWORD, password_len - 1);
    password[password_len - 1] = '\0';
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "MQTT config loaded from NVS: host=%s, port=%d, username=%s",
           host, *port, username[0] ? username : "(empty)");

  return ESP_OK;
}

/**
 * @brief Saves MQTT configuration to NVS
 *
 * @param host Broker hostname/IP
 * @param port Broker port (1-65535)
 * @param username MQTT username (NULL or empty = no auth)
 * @param password MQTT password (NULL or empty = no auth)
 * @return ESP_OK on success, error code on failure
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

  // Validate username/password if set
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

  // Save guest
  ret = nvs_set_str(nvs_handle, MQTT_NVS_KEY_HOST, host);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT host in NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Save port
  ret = nvs_set_u32(nvs_handle, MQTT_NVS_KEY_PORT, (uint32_t)port);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT port in NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Save username (if set, otherwise save empty string)
  const char *username_to_save =
      (username != NULL && strlen(username) > 0) ? username : "";
  ret = nvs_set_str(nvs_handle, MQTT_NVS_KEY_USERNAME, username_to_save);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set MQTT username in NVS: %s",
             esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Save password (if set, otherwise save empty string)
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

static bool ha_mqtt_set(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(s_mqtt_payload, sizeof(s_mqtt_payload), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)sizeof(s_mqtt_payload)) {
    ESP_LOGW(TAG, "MQTT payload truncated");
    s_mqtt_payload[0] = '\0';
    return false;
  }
  return true;
}

/* QoS 0: the outbox does not keep a copy until PUBACK. */
static bool ha_mqtt_send(const char *topic, int retain) {
  if (!mqtt_connected || mqtt_client == NULL || s_mqtt_payload[0] == '\0') {
    return false;
  }
  return esp_mqtt_client_publish(mqtt_client, topic, s_mqtt_payload, 0, 0,
                                 retain) >= 0;
}

/**
 * @brief Publish Home Assistant Auto-Discovery configuration
 */
static bool ha_light_publish_discovery(void) {
  if (!mqtt_connected || mqtt_client == NULL) {
    return false;
  }
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    ESP_LOGD(TAG, "[STAGING] ha_light_publish_discovery: skip (low heap)");
    return false;
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
  if (!ha_mqtt_set(
          "{\"name\":\"CzechMate\",\"unique_id\":\"%s\","
          "\"command_topic\":\"" HA_TOPIC_LIGHT_COMMAND "\","
          "\"state_topic\":\"" HA_TOPIC_LIGHT_STATE "\","
          "\"availability_topic\":\"" HA_TOPIC_LIGHT_AVAILABILITY "\","
          "\"payload_available\":\"" HA_MQTT_PAYLOAD_ONLINE "\","
          "\"payload_not_available\":\"" HA_MQTT_PAYLOAD_OFFLINE "\","
          "\"schema\":\"json\","
          "\"json_attributes_topic\":\"" HA_TOPIC_LIGHT_STATE "\","
          "\"brightness\":true,\"supported_color_modes\":[\"rgb\"],"
          "\"device\":{\"identifiers\":\"%s\",\"name\":\"ESP32 Chess System\","
          "\"manufacturer\":\"" HA_DEVICE_MANUFACTURER "\","
          "\"model\":\"" HA_DEVICE_MODEL "\","
          "\"sw_version\":\"" HA_DEVICE_SW_VERSION "\"}}",
          unique_id, unique_id)) {
    return false;
  }
  return ha_mqtt_send(discovery_topic, 1);
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
    s_mqtt_down = false;

    // Subscribe to light command topic
    esp_mqtt_client_subscribe(client, HA_TOPIC_LIGHT_COMMAND, 0);
    ESP_LOGI(TAG, "Subscribed to %s", HA_TOPIC_LIGHT_COMMAND);

    /* Discovery and state go out one message per loop, not in this callback. */
    s_disc_step = 0;
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT Disconnected");
    mqtt_connected = false;
    s_mqtt_down = true;
    s_disc_step = -1;
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT DATA: topic=%.*s, data=%.*s", event->topic_len,
             event->topic, event->data_len, event->data);

    // Handle command
    char topic[128] = {0};
    char data[512] = {0};

    if (event->topic != NULL && event->data != NULL &&
        event->topic_len > 0 && event->topic_len < (int)sizeof(topic) &&
        event->data_len > 0 && event->data_len < (int)sizeof(data)) {
      memcpy(topic, event->topic, event->topic_len);
      memcpy(data, event->data, event->data_len);
      ha_light_handle_mqtt_command(topic, data, event->data_len);
    }
    break;

  case MQTT_EVENT_ERROR:
    mqtt_connected = false;
    s_mqtt_down = true;
    s_disc_step = -1;
    if (event->error_handle != NULL) {
      ESP_LOGW(TAG, "MQTT Error type=%d sock=%d tls=%s",
               (int)event->error_handle->error_type,
               (int)event->error_handle->esp_transport_sock_errno,
               esp_err_to_name(event->error_handle->esp_tls_last_esp_err));
    } else {
      ESP_LOGW(TAG, "MQTT Error");
    }
    break;

  default:
    break;
  }
}

/**
 * @brief Processes an MQTT command
 */
static void ha_light_handle_mqtt_command(const char *topic, const char *data,
                                         int data_len) {
  if (strcmp(topic, HA_TOPIC_LIGHT_COMMAND) != 0) {
    return; // Not our topic
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

  cJSON_Delete(json);

  if (ha_light_cmd_queue == NULL) {
    return;
  }

  ha_light_command_t cmd = {0};
  cmd.type = HA_CMD_MQTT_STATE;
  cmd.u.web_lamp.state = new_state ? 1 : 0;
  cmd.u.web_lamp.r = new_r;
  cmd.u.web_lamp.g = new_g;
  cmd.u.web_lamp.b = new_b;
  cmd.u.web_lamp.brightness = new_brightness;
  cmd.u.web_lamp.set_brightness = 1;
  if (xQueueSend(ha_light_cmd_queue, &cmd, 0) != pdTRUE) {
    ESP_LOGW(TAG, "MQTT state queue full");
  }
}

/**
 * @brief Publikuje stav do MQTT
 */
static bool ha_light_publish_state(void) {
  if (!mqtt_connected || mqtt_client == NULL) {
    return false;
  }
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    ESP_LOGD(TAG, "[STAGING] ha_light_publish_state: skip (heap < %d B)",
             HA_MQTT_STOP_HEAP_BYTES);
    return false;
  }

  bool st;
  uint8_t br, rr, rg, rb;
  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    st = ha_light_state.state;
    br = ha_light_state.brightness;
    rr = ha_light_state.r;
    rg = ha_light_state.g;
    rb = ha_light_state.b;
    xSemaphoreGive(ha_light_state_mutex);
  } else {
    st = true;
    br = 255;
    rr = rg = rb = 255;
  }

  /* Remembered lamp switch stays in NVS. While chess is on the LEDs, HA must
   * show the lamp off and mode "game", matching the button. */
  bool lamp_showing = st && (current_mode == HA_MODE_HA);
  uint32_t current_time_ms = esp_timer_get_time() / 1000;
  uint32_t time_until_timeout = 0;
  if (last_activity_time_ms > 0 && current_mode == HA_MODE_GAME) {
    uint32_t elapsed = current_time_ms - last_activity_time_ms;
    if (elapsed < HA_ACTIVITY_TIMEOUT_AUTO_MS) {
      time_until_timeout = HA_ACTIVITY_TIMEOUT_AUTO_MS - elapsed;
    }
  }
  if (!ha_mqtt_set(
          "{\"state\":\"%s\",\"brightness\":%u,\"color_mode\":\"rgb\","
          "\"color\":{\"r\":%u,\"g\":%u,\"b\":%u},\"mode\":\"%s\","
          "\"activity_timeout_ms\":%lu}",
          lamp_showing ? "ON" : "OFF", (unsigned)br, (unsigned)rr,
          (unsigned)rg, (unsigned)rb,
          (current_mode == HA_MODE_GAME) ? "game" : "ha",
          (unsigned long)time_until_timeout)) {
    return false;
  }
  return ha_mqtt_send(HA_TOPIC_LIGHT_STATE, 0);
}

static const char *s_game_state = "idle";
static const char *s_game_reason = "none";
static const char *s_game_result = "none";
static const char *s_armed_event;

static const char *ha_game_reason_name(uint8_t reason) {
  switch (reason) {
  case HA_REASON_CHECKMATE:
    return "checkmate";
  case HA_REASON_STALEMATE:
    return "stalemate";
  case HA_REASON_FIFTY:
    return "fifty_move";
  case HA_REASON_REPETITION:
    return "repetition";
  case HA_REASON_INSUFFICIENT:
    return "insufficient";
  case HA_REASON_TIMEOUT:
    return "timeout";
  case HA_REASON_RESIGNATION:
    return "resignation";
  case HA_REASON_AGREEMENT:
    return "agreement";
  default:
    return "none";
  }
}

static const char *ha_game_event_name(uint8_t fact) {
  switch (fact) {
  case HA_GAME_MOVE:
    return "move";
  case HA_GAME_WHITE_WIN:
    return "white_win";
  case HA_GAME_BLACK_WIN:
    return "black_win";
  case HA_GAME_DRAW:
    return "draw";
  case HA_GAME_LAMP:
    return "lamp_on";
  case HA_GAME_BOARD:
    return "game_mode";
  default:
    return NULL;
  }
}

static void ha_game_remember(uint8_t fact, uint8_t reason) {
  switch (fact) {
  case HA_GAME_MOVE:
    if (strcmp(s_game_result, "none") == 0) {
      s_game_state = "playing";
    }
    break;
  case HA_GAME_WHITE_WIN:
    s_game_state = "white_win";
    s_game_result = "white_win";
    s_game_reason = ha_game_reason_name(reason);
    break;
  case HA_GAME_BLACK_WIN:
    s_game_state = "black_win";
    s_game_result = "black_win";
    s_game_reason = ha_game_reason_name(reason);
    break;
  case HA_GAME_DRAW:
    s_game_state = "draw";
    s_game_result = "draw";
    s_game_reason = ha_game_reason_name(reason);
    break;
  case HA_GAME_LAMP:
    s_game_state = "lamp";
    break;
  case HA_GAME_BOARD:
    s_game_state =
        (strcmp(s_game_result, "none") != 0) ? s_game_result : "playing";
    break;
  case HA_GAME_NEW:
    s_game_state = "playing";
    s_game_result = "none";
    s_game_reason = "none";
    break;
  default:
    break;
  }
}

static void ha_game_publish_event(const char *event) {
  if (!mqtt_connected || mqtt_client == NULL || event == NULL) {
    return;
  }
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    return;
  }
  esp_mqtt_client_publish(mqtt_client, HA_TOPIC_GAME_EVENT, event, 0, 0, 0);
}

static bool ha_game_publish_state(void) {
  if (!mqtt_connected || mqtt_client == NULL) {
    return false;
  }
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    return false;
  }
  if (!ha_mqtt_set("{\"state\":\"%s\",\"reason\":\"%s\",\"result\":\"%s\","
                   "\"mode\":\"%s\"}",
                   s_game_state, s_game_reason, s_game_result,
                   (current_mode == HA_MODE_GAME) ? "game" : "ha")) {
    return false;
  }
  return ha_mqtt_send(HA_TOPIC_GAME_STATE, 1);
}

static void ha_game_apply(uint8_t fact, uint8_t reason) {
  ha_game_remember(fact, reason);
  const char *event = ha_game_event_name(fact);
  if (!mqtt_connected || mqtt_client == NULL) {
    if (fact == HA_GAME_NEW) {
      s_armed_event = NULL;
    } else if (event != NULL && fact != HA_GAME_MOVE) {
      s_armed_event = event;
    }
    return;
  }
  ha_game_publish_state();
  if (event != NULL) {
    ha_game_publish_event(event);
    ESP_LOGI(TAG, "HA game event %s (%s)", event, s_game_reason);
  }
}

void ha_game_report(uint8_t fact, uint8_t reason) {
  if (!task_running || ha_light_cmd_queue == NULL) {
    ha_game_apply(fact, reason);
    return;
  }
  ha_light_command_t cmd = {0};
  cmd.type = HA_CMD_GAME_FACT;
  cmd.u.web_lamp.state = fact;
  cmd.u.web_lamp.r = reason;
  if (xQueueSend(ha_light_cmd_queue, &cmd, 0) != pdTRUE) {
    ha_game_apply(fact, reason);
    ESP_LOGW(TAG, "game fact queue full");
  }
}

static const char *s_game_events[] = {"move",     "white_win", "black_win",
                                      "draw",     "lamp_on",   "game_mode"};

static void ha_game_ids(char *light_id, size_t light_cap, char *game_id,
                        size_t game_cap) {
  uint8_t mac_raw[6];
  esp_read_mac(mac_raw, ESP_MAC_WIFI_STA);
  char mac_str[13];
  snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x", mac_raw[0],
           mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);
  snprintf(light_id, light_cap, "esp32_chess_light_%s", mac_str);
  snprintf(game_id, game_cap, "esp32_chess_game_%s", mac_str);
}

static bool ha_game_publish_sensor(void) {
  char light_id[64];
  char game_id[64];
  char topic[128];
  ha_game_ids(light_id, sizeof(light_id), game_id, sizeof(game_id));
  snprintf(topic, sizeof(topic), "%s/sensor/%s/config", HA_DISCOVERY_PREFIX,
           game_id);
  if (!ha_mqtt_set(
          "{\"name\":\"Game\",\"unique_id\":\"%s\","
          "\"state_topic\":\"" HA_TOPIC_GAME_STATE "\","
          "\"value_template\":\"{{ value_json.state }}\","
          "\"json_attributes_topic\":\"" HA_TOPIC_GAME_STATE "\","
          "\"availability_topic\":\"" HA_TOPIC_LIGHT_AVAILABILITY "\","
          "\"payload_available\":\"" HA_MQTT_PAYLOAD_ONLINE "\","
          "\"payload_not_available\":\"" HA_MQTT_PAYLOAD_OFFLINE "\","
          "\"icon\":\"mdi:chess-king\","
          "\"device\":{\"identifiers\":\"%s\",\"name\":\"ESP32 Chess System\","
          "\"manufacturer\":\"" HA_DEVICE_MANUFACTURER "\","
          "\"model\":\"" HA_DEVICE_MODEL "\","
          "\"sw_version\":\"" HA_DEVICE_SW_VERSION "\"}}",
          game_id, light_id)) {
    return false;
  }
  return ha_mqtt_send(topic, 1);
}

static bool ha_game_publish_trigger(unsigned index) {
  if (index >= sizeof(s_game_events) / sizeof(s_game_events[0])) {
    return false;
  }
  char light_id[64];
  char game_id[64];
  char topic[128];
  ha_game_ids(light_id, sizeof(light_id), game_id, sizeof(game_id));
  snprintf(topic, sizeof(topic), "%s/device_automation/%s_%s/config",
           HA_DISCOVERY_PREFIX, game_id, s_game_events[index]);
  if (!ha_mqtt_set("{\"automation_type\":\"trigger\","
                   "\"topic\":\"" HA_TOPIC_GAME_EVENT "\","
                   "\"type\":\"%s\",\"subtype\":\"chess\",\"payload\":\"%s\","
                   "\"device\":{\"identifiers\":\"%s\","
                   "\"name\":\"ESP32 Chess System\","
                   "\"manufacturer\":\"" HA_DEVICE_MANUFACTURER "\","
                   "\"model\":\"" HA_DEVICE_MODEL "\","
                   "\"sw_version\":\"" HA_DEVICE_SW_VERSION "\"}}",
                   s_game_events[index], s_game_events[index], light_id)) {
    return false;
  }
  return ha_mqtt_send(topic, 1);
}

/** One retained config per call. Steps follow s_game_events, so a new
 *  trigger is published without editing this ladder. */
static void ha_discovery_pump(void) {
  int step = s_disc_step;
  if (step < 0 || !mqtt_connected || mqtt_client == NULL) {
    return;
  }
  /* Skip publishes once free heap is under the WiFi floor. */
  if (esp_get_free_heap_size() < HA_MQTT_STOP_HEAP_BYTES) {
    return;
  }
  static int64_t s_disc_next_us;
  int64_t now = esp_timer_get_time();
  if (now < s_disc_next_us) {
    return;
  }
  const int trigger0 = 4;
  const int trigger_n =
      (int)(sizeof(s_game_events) / sizeof(s_game_events[0]));
  const int state_step = trigger0 + trigger_n;
  bool sent = false;
  if (step == 0) {
    sent = ha_light_publish_discovery();
  } else if (step == 1) {
    /* One QoS 1 so Home Assistant keeps the online state. */
    sent = esp_mqtt_client_publish(mqtt_client, HA_TOPIC_LIGHT_AVAILABILITY,
                                   HA_MQTT_PAYLOAD_ONLINE, 0, 1, 1) >= 0;
  } else if (step == 2) {
    sent = ha_light_publish_state();
  } else if (step == 3) {
    sent = ha_game_publish_sensor();
  } else if (step < state_step) {
    sent = ha_game_publish_trigger((unsigned)(step - trigger0));
  } else {
    sent = ha_game_publish_state();
    if (sent && s_disc_step == step) {
      s_disc_step = -1;
    }
    s_disc_next_us = now + 100000;
    return;
  }
  s_disc_next_us = now + 100000;
  /* A failed send stays on this step. Disconnect must not be overwritten. */
  if (sent && s_disc_step == step) {
    s_disc_step = step + 1;
  }
}

/**
 * @brief Loads MQTT configuration from NVS (if not already loaded)
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

static bool s_mqtt_hold_logged;
static uint32_t s_mqtt_gate_since_ms;

static void ha_mqtt_read_mem(size_t *free_b, size_t *largest) {
  *free_b = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  *largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static bool ha_mqtt_start_ok(size_t free_b, size_t largest) {
  return free_b >= (HA_MQTT_NEED_BYTES + (size_t)CHESS_WIFI_HEAP_FLOOR_BYTES) &&
         largest >= (HA_MQTT_BIGGEST_BLOCK_BYTES +
                     (size_t)CHESS_WIFI_LARGEST_FLOOR_BYTES);
}

static void ha_mqtt_log_hold(size_t free_b, size_t largest) {
  if (s_mqtt_hold_logged) {
    return;
  }
  s_mqtt_hold_logged = true;
  ESP_LOGW(TAG,
           "MQTT held: free %u B, largest %u B, mqtt need %u B "
           "(stack %d + in %d + out %d + tcp_snd %d + ovh %d, tls 0 mqtt://), "
           "floor %d B, need free >= %u B and largest >= %u B",
           (unsigned)free_b, (unsigned)largest, (unsigned)HA_MQTT_NEED_BYTES,
           HA_MQTT_TASK_STACK_BYTES, HA_MQTT_IN_BYTES, HA_MQTT_OUT_BYTES,
           HA_MQTT_TCP_SND_BYTES, HA_MQTT_CLIENT_OVERHEAD_BYTES,
           CHESS_WIFI_HEAP_FLOOR_BYTES,
           (unsigned)(HA_MQTT_NEED_BYTES + (size_t)CHESS_WIFI_HEAP_FLOOR_BYTES),
           (unsigned)(HA_MQTT_BIGGEST_BLOCK_BYTES +
                      (size_t)CHESS_WIFI_LARGEST_FLOOR_BYTES));
}

static void ha_mqtt_release_client(size_t free_b, size_t largest) {
  ESP_LOGW(TAG,
           "MQTT stop: free %u B largest %u B crossed WiFi floor "
           "(free < %d or largest < %d)",
           (unsigned)free_b, (unsigned)largest, CHESS_WIFI_HEAP_FLOOR_BYTES,
           CHESS_WIFI_LARGEST_FLOOR_BYTES);
  esp_mqtt_client_stop(mqtt_client);
  esp_mqtt_client_destroy(mqtt_client);
  mqtt_client = NULL;
  mqtt_connected = false;
  s_mqtt_down = false;
  s_mqtt_gate_since_ms = 0;
  s_mqtt_hold_logged = false;
}

static void ha_mqtt_stop_if_low(void) {
  if (mqtt_client == NULL) {
    return;
  }
  size_t free_b = 0;
  size_t largest = 0;
  ha_mqtt_read_mem(&free_b, &largest);
  if (free_b < (size_t)CHESS_WIFI_HEAP_FLOOR_BYTES ||
      largest < (size_t)CHESS_WIFI_LARGEST_FLOOR_BYTES) {
    ha_mqtt_release_client(free_b, largest);
  }
}

/**
 * @brief Initializes the MQTT client
 *
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t ha_light_init_mqtt(void) {
  // Prevent multiple clients/memory leaks
  if (mqtt_client != NULL) {
    ESP_LOGI(TAG, "MQTT client already initialized, skipping re-init");
    return ESP_OK;
  }

  size_t free_heap = 0;
  size_t largest = 0;
  ha_mqtt_read_mem(&free_heap, &largest);
  if (!ha_mqtt_start_ok(free_heap, largest)) {
    ha_mqtt_log_hold(free_heap, largest);
    return ESP_ERR_NO_MEM;
  }

  // Ensure config is loaded from NVS (first time only)
  mqtt_ensure_config_loaded();

  // Build MQTT broker URI (buffer size 512 to avoid format-truncation warning)
  char mqtt_broker_uri[512];
  snprintf(mqtt_broker_uri, sizeof(mqtt_broker_uri), "mqtt://%s:%d",
           mqtt_config.host, mqtt_config.port);

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
      .credentials.username =
          mqtt_config.username[0] != '\0' ? mqtt_config.username : NULL,
      .credentials.authentication.password =
          mqtt_config.password[0] != '\0' ? mqtt_config.password : NULL,
      .session.last_will.topic = HA_TOPIC_LIGHT_AVAILABILITY,
      .session.last_will.msg = HA_MQTT_PAYLOAD_OFFLINE,
      .session.last_will.qos = 1,
      .session.last_will.retain = true,
      .session.keepalive = 120, // Increase keepalive to 120s for stability
      .network.timeout_ms = 4000,
      .network.disable_auto_reconnect = false,
      .task.stack_size = HA_MQTT_TASK_STACK_BYTES,
      /* Incoming commands are small. Outgoing discovery fits in 768. */
      .buffer.size = HA_MQTT_IN_BYTES,
      .buffer.out_size = HA_MQTT_OUT_BYTES,
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
  s_mqtt_hold_logged = false;
  s_mqtt_gate_since_ms = 0;
  return ESP_OK;
}

static void ha_mqtt_consider_start(uint32_t now_ms) {
  if (mqtt_client != NULL) {
    return;
  }
  size_t free_b = 0;
  size_t largest = 0;
  ha_mqtt_read_mem(&free_b, &largest);
  if (!ha_mqtt_start_ok(free_b, largest)) {
    s_mqtt_gate_since_ms = 0;
    ha_mqtt_log_hold(free_b, largest);
    return;
  }
  if (s_mqtt_gate_since_ms == 0) {
    s_mqtt_gate_since_ms = now_ms;
    return;
  }
  if ((now_ms - s_mqtt_gate_since_ms) < HA_MQTT_CALM_MS) {
    return;
  }
  if (ha_light_init_mqtt() != ESP_OK) {
    s_mqtt_gate_since_ms = 0;
  }
}

// ============================================================================
// LAMP NVS PERSISTENCE
// ============================================================================

static void ha_pattern_nvs_erase_once(void) {
  nvs_handle_t handle;
  if (nvs_open(LAMP_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
    return;
  }
  /* Old per-square drawing. Drop it once; do not load or write it back. */
  esp_err_t col = nvs_erase_key(handle, "pat_col");
  esp_err_t map = nvs_erase_key(handle, "pat_map");
  if (col == ESP_OK || map == ESP_OK) {
    nvs_commit(handle);
  }
  nvs_close(handle);
}

static void lamp_nvs_load(void) {
  nvs_handle_t handle;
  esp_err_t ret = nvs_open(LAMP_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (ret != ESP_OK) {
    return;
  }

  uint8_t state_u8 = 1, r = 255, g = 255, b = 255, brightness = 255;
  uint32_t auto_sec = HA_ACTIVITY_TIMEOUT_AUTO_DEFAULT_SEC;
  nvs_get_u8(handle, LAMP_NVS_KEY_STATE, &state_u8);
  nvs_get_u8(handle, LAMP_NVS_KEY_R, &r);
  nvs_get_u8(handle, LAMP_NVS_KEY_G, &g);
  nvs_get_u8(handle, LAMP_NVS_KEY_B, &b);
  nvs_get_u8(handle, LAMP_NVS_KEY_BRIGHTNESS, &brightness);
  if (nvs_get_u32(handle, LAMP_NVS_KEY_AUTO_TIMEOUT_SEC, &auto_sec) == ESP_OK &&
      auto_sec >= HA_ACTIVITY_TIMEOUT_AUTO_MIN_SEC &&
      auto_sec <= HA_ACTIVITY_TIMEOUT_AUTO_MAX_SEC) {
    activity_timeout_auto_sec = auto_sec;
  }
  uint8_t mode_u8 = 0;
  if (nvs_get_u8(handle, LAMP_NVS_KEY_MODE, &mode_u8) == ESP_OK &&
      (mode_u8 == HA_MODE_GAME || mode_u8 == HA_MODE_HA)) {
    s_nvs_mode = (ha_mode_t)mode_u8;
    s_nvs_mode_found = true;
  }
  nvs_close(handle);
  ha_pattern_nvs_erase_once();

  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ha_light_state.state = (state_u8 != 0);
    ha_light_state.r = r;
    ha_light_state.g = g;
    ha_light_state.b = b;
    ha_light_state.brightness = brightness;
    xSemaphoreGive(ha_light_state_mutex);
  }
  remember_lit_color(r, g, b, brightness);
}

static void lamp_nvs_save(void) {
  chess_task_wdt_reset_safe(TAG);
  bool st;
  uint8_t r, g, b, brightness;
  if (ha_light_state_mutex != NULL &&
      xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    st = ha_light_state.state;
    r = ha_light_state.r;
    g = ha_light_state.g;
    b = ha_light_state.b;
    brightness = ha_light_state.brightness;
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
  nvs_set_u8(handle, LAMP_NVS_KEY_BRIGHTNESS, brightness);
  chess_task_wdt_reset_safe(TAG);
  esp_err_t err = nvs_commit(handle);
  chess_task_wdt_reset_safe(TAG);
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

static void lamp_nvs_save_mode(void) {
  nvs_handle_t handle;
  if (nvs_open(LAMP_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
    ESP_LOGW(TAG, "lamp mode NVS open failed");
    return;
  }
  nvs_set_u8(handle, LAMP_NVS_KEY_MODE, (uint8_t)current_mode);
  esp_err_t err = nvs_commit(handle);
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "lamp mode NVS commit failed (%s)", esp_err_to_name(err));
  }
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * @brief Get the current regime
 */
ha_mode_t ha_light_get_mode(void) { return current_mode; }

bool ha_light_request_mode(ha_mode_t mode) {
  if (ha_light_cmd_queue == NULL) {
    return false;
  }
  ha_light_command_t cmd = {0};
  cmd.type = HA_CMD_SET_MODE;
  cmd.u.web_lamp.state = (uint8_t)mode;
  return xQueueSend(ha_light_cmd_queue, &cmd, 0) == pdTRUE;
}

void ha_light_persist_mode(void) { lamp_nvs_save_mode(); }

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
 * @brief Get the current state of the lamp (thread-safe for HTTP handlers)
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

void ha_light_last_lit_color(uint8_t *r, uint8_t *g, uint8_t *b,
                             uint8_t *brightness) {
  if (r)
    *r = s_lit_r;
  if (g)
    *g = s_lit_g;
  if (b)
    *b = s_lit_b;
  if (brightness)
    *brightness = s_lit_br;
}

/**
 * @brief Lamp setup request from web
 * @return true if the command is sent to the queue, false if the queue is NULL or
 * full
 */
bool ha_light_request_web_lamp(bool state, uint8_t r, uint8_t g, uint8_t b) {
  ha_light_command_t cmd = {0};
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
 * @brief Check if HA mode is available (WiFi STA connected)
 */
bool ha_light_is_available(void) { return ha_light_check_wifi_sta_connected(); }

/**
 * @brief Gets MQTT configuration (will load from NVS if not already loaded)
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
 * @brief Check if the MQTT client is connected
 */
bool ha_light_is_mqtt_connected(void) { return mqtt_connected; }

/**
 * @brief Disconnects and reinitializes the MQTT client with the current configuration from NVS
 *
 * @return ESP_OK on success, error code on failure
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
  s_mqtt_hold_logged = false;
  s_mqtt_gate_since_ms = 0;

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
 * @brief The main functions of the HA Light task
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
    // 🛑 BOOT ANIMATION PROTECTION - BLOCKS HA OPERATION DURING BOOT
    // =========================================================================
    // If the boot animation is running, the HA task must not send LED commands!
    // (e.g. "turn off the light" on a fast WiFi connection)
    if (led_is_booting()) {
      chess_task_wdt_reset_safe(TAG);
      vTaskDelay(pdMS_TO_TICKS(10)); // 10ms - quick wakeup after fade_out!
      continue;
    }
    if (!s_boot_mode_applied) {
      s_boot_mode_applied = true;
      if (s_nvs_mode_found && s_nvs_mode == HA_MODE_HA &&
          current_mode != HA_MODE_HA) {
        ESP_LOGI(TAG, "[STAGING] restore persisted lamp mode");
        ha_light_switch_to_ha_mode();
      }
    }
    // =========================================================================

    // Reset WDT
    chess_task_wdt_reset_safe(TAG);

    ha_discovery_pump();

    // Process queue with timeout (100ms) - replaces vTaskDelay
    ha_light_command_t cmd;
    if (ha_light_cmd_queue &&
        xQueueReceive(ha_light_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (cmd.type == HA_CMD_WEB_LAMP) {
        if (ha_light_state_mutex != NULL &&
            xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) ==
                pdTRUE) {
          remember_lit_color(ha_light_state.r, ha_light_state.g, ha_light_state.b,
                             ha_light_state.brightness);
          ha_light_state.state = (cmd.u.web_lamp.state != 0);
          ha_light_state.r = cmd.u.web_lamp.r;
          ha_light_state.g = cmd.u.web_lamp.g;
          ha_light_state.b = cmd.u.web_lamp.b;
          if (cmd.u.web_lamp.set_brightness) {
            ha_light_state.brightness = cmd.u.web_lamp.brightness;
          }
          remember_lit_color(ha_light_state.r, ha_light_state.g, ha_light_state.b,
                             ha_light_state.brightness);
          xSemaphoreGive(ha_light_state_mutex);
        }
        lamp_nvs_save();
        ha_light_switch_to_ha_mode();
      } else if (cmd.type == HA_CMD_MQTT_STATE) {
        bool was_on = ha_light_state.state;
        bool now_on_cmd = (cmd.u.web_lamp.state != 0);
        /* Capture the lit color before state becomes off, so the shrink wave
         * still has a disk to pull back into h1. */
        bool leaving_light =
            !now_on_cmd && was_on && (current_mode == HA_MODE_HA);
        if (leaving_light) {
          game_gesture_request_lamp_off_wave();
        }
        if (ha_light_state_mutex != NULL &&
            xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(100)) ==
                pdTRUE) {
          was_on = ha_light_state.state;
          remember_lit_color(ha_light_state.r, ha_light_state.g, ha_light_state.b,
                             ha_light_state.brightness);
          ha_light_state.state = (cmd.u.web_lamp.state != 0);
          ha_light_state.r = cmd.u.web_lamp.r;
          ha_light_state.g = cmd.u.web_lamp.g;
          ha_light_state.b = cmd.u.web_lamp.b;
          ha_light_state.brightness = cmd.u.web_lamp.brightness;
          remember_lit_color(ha_light_state.r, ha_light_state.g, ha_light_state.b,
                             ha_light_state.brightness);
          xSemaphoreGive(ha_light_state_mutex);
        }
        lamp_nvs_save();
        bool now_on = (cmd.u.web_lamp.state != 0);
        /* Turning the lamp on leaves whatever is on the board, game included. */
        bool entering_light =
            now_on && (current_mode != HA_MODE_HA || !was_on);
        bool apply = true;
        if (!entering_light && current_mode == HA_MODE_GAME) {
          uint32_t now_ms = esp_timer_get_time() / 1000;
          uint32_t idle_ms = now_ms - last_activity_time_ms;
          apply = (idle_ms >= HA_ACTIVITY_TIMEOUT_COMMAND_MS ||
                   game_get_move_count() == 0);
          if (!apply) {
            ESP_LOGI(TAG,
                     "MQTT state stored (game active, moves=%lu, idle=%lu ms)",
                     game_get_move_count(), idle_ms);
          }
        }
        if (apply && entering_light) {
          game_gesture_request_lamp_wave();
        } else if (leaving_light) {
          /* State is already off. The shrink wave owns the LEDs until it
           * commits HA mode, which then paints black. */
          ha_light_publish_state();
        } else if (apply) {
          ha_light_switch_to_ha_mode();
        } else {
          ha_light_publish_state();
        }
      } else if (cmd.type == HA_CMD_GAME_FACT) {
        ha_game_apply(cmd.u.web_lamp.state, cmd.u.web_lamp.r);
      } else if (cmd.type == HA_CMD_SET_MODE) {
        if (cmd.u.web_lamp.state == HA_MODE_HA) {
          /* Coming from the game, the lamp comes back in the last real color. */
          if (current_mode == HA_MODE_GAME && ha_light_state_mutex != NULL &&
              xSemaphoreTake(ha_light_state_mutex, pdMS_TO_TICKS(50)) ==
                  pdTRUE) {
            if (!ha_light_state.state ||
                (ha_light_state.r | ha_light_state.g | ha_light_state.b) == 0) {
              ha_light_state.r = s_lit_r;
              ha_light_state.g = s_lit_g;
              ha_light_state.b = s_lit_b;
              ha_light_state.brightness = s_lit_br;
              ha_light_state.state = true;
              xSemaphoreGive(ha_light_state_mutex);
              lamp_nvs_save();
            } else {
              xSemaphoreGive(ha_light_state_mutex);
            }
          }
          ha_light_switch_to_ha_mode();
        } else {
          ha_light_switch_to_game_mode();
        }
      } else if (cmd.type == HA_CMD_ACTIVITY) {
        const char *activity_name = (const char *)cmd.u.data;
        // Reset activity timer in HA task context
        last_activity_time_ms = esp_timer_get_time() / 1000;

        /* A bump that only renames a piece, or a piece put back on the same
         * square, is not a move. Only a real relocation leaves the lamp. */
        bool leave_lamp = false;
        if (activity_name != NULL &&
            (strcmp(activity_name, "move") == 0 ||
             strcmp(activity_name, "web_game_mode") == 0)) {
          leave_lamp = true;
        }
        if (current_mode == HA_MODE_HA && leave_lamp) {
          ESP_LOGI(TAG, "Switching from HA mode to GAME mode (activity: %s)",
                   activity_name);
          ha_light_switch_to_game_mode();
        }
        if (activity_name != NULL && strcmp(activity_name, "move") == 0) {
          ha_game_apply(HA_GAME_MOVE, HA_REASON_NONE);
        }

        // Check rate limit logic again here (in HA task context) to save MQTT
        // bandwidth
        static uint32_t last_mqtt_activity = 0;
        uint32_t now = esp_timer_get_time() / 1000;

        if (now - last_mqtt_activity > 500) { // 500ms limit
          last_mqtt_activity = now;

          if (mqtt_connected && mqtt_client != NULL &&
              ha_mqtt_set("{\"event\":\"%s\",\"timestamp_ms\":%lu}",
                          activity_name ? activity_name : "unknown",
                          (unsigned long)now)) {
            ha_mqtt_send(HA_TOPIC_GAME_ACTIVITY, 0);
          }
        }
      }
      chess_task_wdt_reset_safe(TAG);
    }

    // POLL: Check WiFi STA status periodically (every 5 seconds)
    static uint32_t last_wifi_check = 0;
    static uint32_t last_mqtt_retry_ms = 0;
    uint32_t current_time_ms = esp_timer_get_time() / 1000;
    /* Stop within one loop (~100 ms). Start only after the gate holds. */
    ha_mqtt_stop_if_low();
    if (sta_connected && mqtt_client == NULL) {
      ha_mqtt_consider_start(current_time_ms);
    }
    if (current_time_ms - last_wifi_check >= 5000) {
      static uint32_t last_heap_log_ms;
      if (current_time_ms - last_heap_log_ms >= 10000) {
        last_heap_log_ms = current_time_ms;
        size_t free_b = esp_get_free_heap_size();
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        size_t least = esp_get_minimum_free_heap_size();
        if (free_b < (size_t)CHESS_WIFI_HEAP_FLOOR_BYTES) {
          ESP_LOGW(TAG, "heap free=%u largest=%u min=%u", (unsigned)free_b,
                   (unsigned)largest, (unsigned)least);
        } else {
          ESP_LOGI(TAG, "heap free=%u largest=%u min=%u", (unsigned)free_b,
                   (unsigned)largest, (unsigned)least);
        }
      }
      bool wifi_connected = ha_light_check_wifi_sta_connected();
      if (wifi_connected && !sta_connected) {
        sta_connected = true;
        ESP_LOGI(TAG, "WiFi STA connected - MQTT waits for the heap gate");
        chess_task_wdt_reset_safe(TAG);
      } else if (wifi_connected && sta_connected && mqtt_client != NULL &&
                 s_mqtt_down && !mqtt_connected) {
        size_t free_b = 0;
        size_t largest = 0;
        ha_mqtt_read_mem(&free_b, &largest);
        /* Another TCP buffer on top of a running client. */
        if (ha_mqtt_start_ok(free_b, largest) &&
            current_time_ms - last_mqtt_retry_ms >= 15000) {
          last_mqtt_retry_ms = current_time_ms;
          esp_err_t rr = esp_mqtt_client_reconnect(mqtt_client);
          if (rr != ESP_OK) {
            ESP_LOGW(TAG, "MQTT reconnect: %s", esp_err_to_name(rr));
          }
        }
      } else if (!wifi_connected && sta_connected) {
        // WiFi disconnected
        sta_connected = false;
        mqtt_connected = false; // Just mark disconnected, don't destroy client
                                // to avoid complex cleanup
        ESP_LOGI(TAG, "WiFi STA disconnected - MQTT unavailable, lamp state kept");
      }
      last_wifi_check = current_time_ms;
    }

    // Monitor game activity (checks queues for activity)
    ha_light_monitor_game_activity();

    // Check activity timeout (5 minutes or value that is set in ap or web)
    ha_light_check_activity_timeout();

    if (mqtt_connected && s_armed_event != NULL) {
      const char *event = s_armed_event;
      s_armed_event = NULL;
      ha_game_publish_state();
      ha_game_publish_event(event);
    }

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
                                HA_MQTT_PAYLOAD_ONLINE, 0, 0, 1);
        last_avail_publish = current_time_ms;
      }
    }

    // Minimal yield not needed because xQueueReceive handles waiting
  }
}
