/**
 * @file ha_light_task.h
 * @brief HA Light Task - Home Assistant RGB Light Integration
 *
 * @details
 * =============================================================================
 * WHAT DID THIS COMPONENT DO?
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

#ifndef HA_LIGHT_TASK_H
#define HA_LIGHT_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KONSTANTY
// ============================================================================

#include "freertos_chess.h"

// ============================================================================
// KONSTANTY
// ============================================================================

// Default idle time for automatic switching to HA mode (lamp) - can be changed from the website, saved in NVS
#define HA_ACTIVITY_TIMEOUT_AUTO_MS (300000)  // 5 min default (realna hodnota se cte z NVS)

// 2 minutes of inactivity = enable HA command to switch to HA mode
#define HA_ACTIVITY_TIMEOUT_COMMAND_MS (120000)

// MQTT topics
#define HA_TOPIC_LIGHT_COMMAND "esp32-chess/light/command"
#define HA_TOPIC_LIGHT_STATE "esp32-chess/light/state"
#define HA_TOPIC_LIGHT_AVAILABILITY "esp32-chess/light/availability"
#define HA_TOPIC_GAME_ACTIVITY "esp32-chess/game/activity"
#define HA_TOPIC_GAME_STATE "esp32-chess/game/state"
#define HA_TOPIC_GAME_EVENT "esp32-chess/game/event"

/* Facts the board reports. HA keeps the sensor state and fires one trigger. */
#define HA_GAME_MOVE 1
#define HA_GAME_WHITE_WIN 2
#define HA_GAME_BLACK_WIN 3
#define HA_GAME_DRAW 4
#define HA_GAME_LAMP 5
#define HA_GAME_BOARD 6
#define HA_GAME_NEW 7

#define HA_REASON_NONE 0
#define HA_REASON_CHECKMATE 1
#define HA_REASON_STALEMATE 2
#define HA_REASON_FIFTY 3
#define HA_REASON_REPETITION 4
#define HA_REASON_INSUFFICIENT 5
#define HA_REASON_TIMEOUT 6
#define HA_REASON_RESIGNATION 7
#define HA_REASON_AGREEMENT 8

// MQTT Payloads
#define HA_MQTT_PAYLOAD_ONLINE "online"
#define HA_MQTT_PAYLOAD_OFFLINE "offline"

// HA Auto-Discovery Constants
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_COMPONENT_LIGHT "light"
#define HA_DEVICE_MANUFACTURER "Alfred Krutina"
#define HA_DEVICE_MODEL "ESP32-Chess-System"
#define HA_DEVICE_SW_VERSION "2.4.1"

// MQTT client config
#define HA_MQTT_CLIENT_ID "esp32-chess-light"
#define HA_MQTT_BROKER_PORT 1883

// ============================================================================
// TYPY A STRUKTURY
// ============================================================================

/**
 * @brief Rezimy provozu
 */
typedef enum {
  HA_MODE_GAME = 0, ///< Game mode - LED displays chest
  HA_MODE_HA = 1    ///< HA rezim - LED jako RGB svetlo
} ha_mode_t;

/** Command type: Activity report (switching from HA to GAME) */
#define HA_CMD_ACTIVITY 1
/** Type of command: Web lamps (set colors/turn on from the web) */
#define HA_CMD_WEB_LAMP 2
/** Commit game or lamp after the gesture wave has covered the board. */
#define HA_CMD_SET_MODE 3
/** MQTT JSON light state (on, RGB, brightness). Applied in the HA task. */
#define HA_CMD_MQTT_STATE 4
/** Game fact for the HA sensor and device triggers. state = fact, r = reason. */
#define HA_CMD_GAME_FACT 5
/** One stored drawing. action: off, on, stamp, clear, colors. */
#define HA_CMD_PATTERN 6
#define HA_PAT_OFF 0
#define HA_PAT_ON 1
#define HA_PAT_STAMP 2
#define HA_PAT_CLEAR 3
#define HA_PAT_COLORS 4

/**
 * @brief HA Light command for internal communication
 */
typedef struct {
  uint8_t type; ///< Command type (HA_CMD_ACTIVITY, HA_CMD_WEB_LAMP)
  union {
    void *data; ///< Pro HA_CMD_ACTIVITY: (const char*) activity_type
    struct {
      uint8_t state; ///< 0=off, 1=on (web lamp, MQTT, or mode)
      uint8_t r, g, b;
      uint8_t brightness;     ///< MQTT brightness 0-255
      uint8_t set_brightness; ///< 1 = write brightness, 0 = keep the last one
    } web_lamp;
    struct {
      uint8_t action;
      uint8_t square;
      uint8_t pen;
      uint8_t ar, ag, ab;
      uint8_t br, bg, bb;
      uint8_t write_colors;
      uint8_t clear_map;
    } pattern;
  } u;
} ha_light_command_t;

/**
 * @brief Get the current state of the lamp (color, on)
 *
 * Call from HTTP handler for GET /api/status. Thread-safe.
 */
void ha_light_get_state(uint8_t *r, uint8_t *g, uint8_t *b,
                        uint8_t *brightness, bool *state);

/** Last lamp color that was not black, including after the light was turned off. */
void ha_light_last_lit_color(uint8_t *r, uint8_t *g, uint8_t *b,
                             uint8_t *brightness);

/**
 * @brief Lamp settings request from the web (mode Lamp, color, on)
 *
 * Does not need WiFi or MQTT. The command is queued and processed in the HA task.
 * @return true if the command was sent to the queue, false if the queue does not exist
 * or it is full (the caller can return 503).
 */
bool ha_light_request_web_lamp(bool state, uint8_t r, uint8_t g, uint8_t b);

/** Doba (v sekundach) po ktere se pri necinosti prepne do rezimu lampa (5..7200). */
uint32_t ha_light_get_activity_timeout_sec(void);
/** Set and save to NVS; sec will be limited to 5..7200. */
esp_err_t ha_light_set_activity_timeout_sec(uint32_t sec);

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Start the HA Light task
 *
 * @param pvParameters Task parameters (not used)
 */
void ha_light_task_start(void *pvParameters);

/**
 * @brief Get the current regime
 *
 * @return ha_mode_t Current mode (GAME or HA)
 */
ha_mode_t ha_light_get_mode(void);

/**
 * Queue a real mode switch (game or lamp). The HA task applies it and
 * writes the mode byte into the existing lamp_cfg NVS namespace.
 * @return false if the command queue is missing or full.
 */
bool ha_light_request_mode(ha_mode_t mode);

/** Write the current game/lamp mode into lamp_cfg. Safe to call before sleep. */
void ha_light_persist_mode(void);

/**
 * @brief Check if HA mode is available (WiFi STA connected)
 *
 * @return true if WiFi STA is connected, false otherwise
 */
bool ha_light_is_available(void);

/**
 * @brief Game activity report (for resetting the 5min timer)
 *
 * I call this function last tasks (game_task, squares_task) when
 * detect activity (movement of figures, game command).
 *
 * @param activity_type Activity type ("move", "pickup", "drop", "command")
 */
void ha_light_report_activity(const char *activity_type);

/**
 * Report a game fact to Home Assistant.
 * Safe from the game task. If MQTT is down, the sensor state is remembered
 * and published on the next connect. Move events are not replayed.
 */
void ha_game_report(uint8_t fact, uint8_t reason);

/** True while the single piece-drawing pattern owns the lamp LEDs. */
bool ha_pattern_is_active(void);

/**
 * Queue a pattern change. Stamp uses square only. Colors are written when
 * write_colors is 1. clear_map wipes the drawing.
 */
bool ha_pattern_request(uint8_t action, uint8_t square, uint8_t pen,
                        uint8_t ar, uint8_t ag, uint8_t ab, uint8_t br,
                        uint8_t bg, uint8_t bb, uint8_t write_colors,
                        uint8_t clear_map);

/**
 * @brief Save MQTT configuration to NVS
 *
 * @param host Broker hostname/IP (max 127 characters)
 * @param port Broker port (1-65535)
 * @param username MQTT username (NULL or empty = no auth)
 * @param password MQTT password (NULL or empty = no auth)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_save_config_to_nvs(const char *host, uint16_t port,
                                  const char *username, const char *password);

/**
 * @brief Gain MQTT configuration (load from NVS if it hasn't been loaded yet)
 *
 * @param host Buffer for broker host (min 128 bytes)
 * @param host_len Host buffer size
 * @param port A pointer to a port
 * @param username Buffer for username (min 64 bytes)
 * @param username_len The size of the username buffer
 * @param password Buffer for password (min 64 bytes)
 * @param password_len Password buffer size
 * @return ESP_OK on success
 */
esp_err_t mqtt_get_config(char *host, size_t host_len, uint16_t *port,
                          char *username, size_t username_len, char *password,
                          size_t password_len);

/**
 * @brief Check if the MQTT client is connected
 *
 * @return true if MQTT is connected, false otherwise
 */
bool ha_light_is_mqtt_connected(void);

/**
 * @brief Disconnects and reinitializes the MQTT client with the current configuration from NVS
 *
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * This function is used when changing the MQTT configuration on the fly.
 * Stops the build client, reloads the configuration from NVS and initializes
 * a new client. If the WiFi STA is not connected, return an error.
 */
esp_err_t ha_light_reinit_mqtt(void);

#ifdef __cplusplus
}
#endif

#endif // HA_LIGHT_TASK_H
