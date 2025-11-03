/**
 * @file web_server_task.h
 * @brief ESP32-C6 Chess System v2.4 - Web Server Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani pro web server task:
 * - Funkce pro ovladani web serveru
 * - Handlery HTTP pozadavku
 * - WebSocket funkce
 * - Konfigurace a status funkce
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Web Server Task spravuje WiFi Access Point a HTTP server pro vzdalene
 * ovladani sachoveho systemu. Uzivatel se muze pripojit k WiFi hotspotu
 * "ESP32-CzechMate" a ovladat hru pres webovy prohlizec.
 * 
 * WiFi konfigurace:
 * - SSID: ESP32-CzechMate
 * - Heslo: 12345678
 * - IP: 192.168.4.1
 * - Kanal: 1
 * - Max pripojeni: 4
 * 
 * HTTP Endpointy:
 * - GET / - Hlavni stranka s webovym rozhranim
 * - GET /api/board - Aktualni stav sachovnice (JSON)
 * - GET /api/status - Stav hry (JSON)
 * - GET /api/history - Historie tahu (JSON)
 * - GET /api/captured - Sebrane figurky (JSON)
 * - GET /api/advantage - Material advantage graf (JSON)
 * - GET /api/timer - Stav casoveho systemu (JSON)
 * - POST /api/timer/config - Konfigurace casoveho systemu
 * - POST /api/timer/pause - Pozastaveni timeru
 * - POST /api/timer/resume - Obnoveni timeru
 * - POST /api/timer/reset - Reset timeru
 */

#ifndef WEB_SERVER_TASK_H
#define WEB_SERVER_TASK_H

#include "freertos_chess.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPY WEB SERVER PRIKAZU
// ============================================================================

/**
 * @brief Typy web server prikazu
 */
typedef enum {
    WEB_CMD_START_SERVER,   ///< Spust web server
    WEB_CMD_STOP_SERVER,    ///< Zastav web server
    WEB_CMD_GET_STATUS,     ///< Ziskej status serveru
    WEB_CMD_SET_CONFIG      ///< Nastav konfiguraci serveru
} web_command_type_t;

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Hlavni funkce Web Server tasku
 * 
 * Inicializuje WiFi AP, spusti HTTP server a zpracovava pozadavky.
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void web_server_task_start(void *pvParameters);

// Zpracovani prikazu

/**
 * @brief Zpracuj web server prikazy z fronty
 */
void web_server_process_commands(void);

/**
 * @brief Vykonaj web server prikaz
 * 
 * @param command Prikaz k vykonani (web_command_type_t)
 */
void web_server_execute_command(uint8_t command);

// Funkce pro ovladani web serveru

/**
 * @brief Spust web server
 * 
 * Inicializuje WiFi AP a spusti HTTP server.
 */
void web_server_start(void);

/**
 * @brief Zastav web server
 * 
 * Zastavi HTTP server a WiFi AP.
 */
void web_server_stop(void);

/**
 * @brief Ziskej status web serveru
 * 
 * Vypise stav serveru (aktivni, pocet klientu, uptime).
 */
void web_server_get_status(void);

/**
 * @brief Nastav konfiguraci web serveru
 * 
 * Zmeni konfiguraci serveru (port, max klientu, SSL).
 */
void web_server_set_config(void);

// Sprava stavu

/**
 * @brief Aktualizuj stav web serveru
 * 
 * Periodicky aktualizuje interní stav serveru.
 */
void web_server_update_state(void);

// HTTP request handlery

/**
 * @brief Zpracuj GET / (hlavni stranka)
 */
void web_server_handle_root(void);

/**
 * @brief Zpracuj GET /api/status (stav hry)
 */
void web_server_handle_api_status(void);

/**
 * @brief Zpracuj GET /api/board (sachovnice)
 */
void web_server_handle_api_board(void);

/**
 * @brief Zpracuj POST /api/move (tah)
 */
void web_server_handle_api_move(void);

// WebSocket funkce

/**
 * @brief Inicializuj WebSocket
 */
void web_server_websocket_init(void);

/**
 * @brief Posli aktualizaci pres WebSocket
 * 
 * @param data Data k poslani (JSON string)
 */
void web_server_websocket_send_update(const char* data);

// Utility funkce

/**
 * @brief Overi zda je web server aktivni
 * 
 * @return true pokud server bezi
 */
bool web_server_is_active(void);

/**
 * @brief Ziskej pocet pripojenych klientu
 * 
 * @return Pocet aktualne pripojenych klientu
 */
uint32_t web_server_get_client_count(void);

/**
 * @brief Ziskej uptime serveru
 * 
 * @return Uptime v milisekundach od spusteni
 */
uint32_t web_server_get_uptime(void);

/**
 * @brief Zaloguj HTTP pozadavek
 * 
 * @param method HTTP metoda (GET, POST, atd.)
 * @param path Cesta pozadavku (/api/board, atd.)
 */
void web_server_log_request(const char* method, const char* path);

/**
 * @brief Zaloguj chybu web serveru
 * 
 * @param error_message Chybova zprava
 */
void web_server_log_error(const char* error_message);

// Konfiguracni funkce

/**
 * @brief Nastav port serveru
 * 
 * @param port Cislo portu (80 = HTTP standard)
 */
void web_server_set_port(uint16_t port);

/**
 * @brief Nastav maximum klientu
 * 
 * @param max_clients Maximalni pocet soucastne pripojenych klientu
 */
void web_server_set_max_clients(uint32_t max_clients);

/**
 * @brief Zapni/vypni SSL
 * 
 * @param enable true pro zapnuti SSL (HTTPS)
 */
void web_server_enable_ssl(bool enable);

// Funkce pro status a ovladani

/**
 * @brief Overi zda web server task bezi
 * 
 * @return true pokud task bezi
 */
bool web_server_is_task_running(void);

/**
 * @brief Zastav web server task
 * 
 * Ukonci web server task a uvolni prostredky.
 */
void web_server_stop_task(void);

/**
 * @brief Resetuj web server
 * 
 * Provede kompletni reset web serveru.
 */
void web_server_reset(void);

// ============================================================================
// EXTERNI PROMENNE
// ============================================================================

/** @brief Fronta pro web server status */
extern QueueHandle_t web_server_status_queue;
/** @brief Fronta pro web server prikazy */
extern QueueHandle_t web_server_command_queue;

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_TASK_H
