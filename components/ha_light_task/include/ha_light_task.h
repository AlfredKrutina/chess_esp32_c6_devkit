/**
 * @file ha_light_task.h
 * @brief HA Light Task - Home Assistant RGB Light Integration
 *
 * @details
 * =============================================================================
 * CO TENTO KOMPONENT DELA?
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
 * @version 1.0
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

// 10 minut necinosti = automaticky přepne do HA módu
#define HA_ACTIVITY_TIMEOUT_AUTO_MS (600000)

// 2 minuty necinosti = umožní HA command přepnout do HA módu
#define HA_ACTIVITY_TIMEOUT_COMMAND_MS (120000)

// MQTT topics
#define HA_TOPIC_LIGHT_COMMAND "esp32-chess/light/command"
#define HA_TOPIC_LIGHT_STATE "esp32-chess/light/state"
#define HA_TOPIC_LIGHT_AVAILABILITY "esp32-chess/light/availability"
#define HA_TOPIC_GAME_ACTIVITY "esp32-chess/game/activity"

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
  HA_MODE_GAME = 0, ///< Herni rezim - LED zobrazuji sachovnici
  HA_MODE_HA = 1    ///< HA rezim - LED jako RGB svetlo
} ha_mode_t;

/**
 * @brief HA Light prikaz pro vnitrni komunikaci
 */
typedef struct {
  uint8_t type; ///< Typ prikazu
  void *data;   ///< Data prikazu (volitelne)
} ha_light_command_t;

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Spusti HA Light task
 *
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void ha_light_task_start(void *pvParameters);

/**
 * @brief Ziskej aktualni rezim
 *
 * @return ha_mode_t Aktualni rezim (GAME nebo HA)
 */
ha_mode_t ha_light_get_mode(void);

/**
 * @brief Zjisti zda je HA rezim dostupny (WiFi STA pripojeno)
 *
 * @return true pokud je WiFi STA pripojeno, false jinak
 */
bool ha_light_is_available(void);

/**
 * @brief Zprava o aktivity hry (pro resetovani 5min timeru)
 *
 * Tuto funkci volaji ostatni tasky (game_task, matrix_task) kdyz
 * detekuji aktivitu (pohyb figurky, herni prikaz).
 *
 * @param activity_type Typ aktivity ("move", "pickup", "drop", "command")
 */
void ha_light_report_activity(const char *activity_type);

/**
 * @brief Uloží MQTT konfiguraci do NVS
 *
 * @param host Broker hostname/IP (max 127 znaků)
 * @param port Broker port (1-65535)
 * @param username MQTT username (NULL nebo prázdné = bez auth)
 * @param password MQTT password (NULL nebo prázdné = bez auth)
 * @return ESP_OK při úspěchu, chybový kód při chybě
 */
esp_err_t mqtt_save_config_to_nvs(const char *host, uint16_t port,
                                  const char *username, const char *password);

/**
 * @brief Získá MQTT konfiguraci (načte z NVS pokud ještě nebyla načtena)
 *
 * @param host Buffer pro broker host (min 128 bytes)
 * @param host_len Velikost host bufferu
 * @param port Ukazatel na port
 * @param username Buffer pro username (min 64 bytes)
 * @param username_len Velikost username bufferu
 * @param password Buffer pro password (min 64 bytes)
 * @param password_len Velikost password bufferu
 * @return ESP_OK při úspěchu
 */
esp_err_t mqtt_get_config(char *host, size_t host_len, uint16_t *port,
                          char *username, size_t username_len, char *password,
                          size_t password_len);

/**
 * @brief Zjistí zda je MQTT klient připojen
 *
 * @return true pokud je MQTT připojen, false jinak
 */
bool ha_light_is_mqtt_connected(void);

/**
 * @brief Odpojí a reinicializuje MQTT klienta s aktuální konfigurací z NVS
 *
 * @return ESP_OK při úspěchu, chybový kód při chybě
 *
 * @details
 * Tato funkce se používá při změně MQTT konfigurace za běhu.
 * Zastaví stávajícího klienta, znovu načte konfiguraci z NVS a inicializuje
 * nového klienta. Pokud není WiFi STA připojeno, vrátí chybu.
 */
esp_err_t ha_light_reinit_mqtt(void);

#ifdef __cplusplus
}
#endif

#endif // HA_LIGHT_TASK_H
