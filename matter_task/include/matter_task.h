/**
 * @file matter_task.h
 * @brief ESP32-C6 Chess System v2.4 - Matter Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani pro Matter task:
 * - Integrace Matter protokolu
 * - Inicializace a sprava zarizeni
 * - Protokolova komunikace
 * - Sitova konektivita
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Matter Task je zodpovedny za integraci s Matter protokolem.
 * Umoznuje pripojeni k Matter siti a komunikaci s ostatnimi
 * Matter zarizeními. V soucasne dobe je DISABLED (neni potreba).
 * 
 * @note Matter task je momentalne vypnuty a neni pouzivan
 */

#ifndef MATTER_TASK_H
#define MATTER_TASK_H

#include "freertos_chess.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPY MATTER PRIKAZU
// ============================================================================

/**
 * @brief Typy Matter prikazu
 */
typedef enum {
    MATTER_CMD_INIT,    ///< Inicializuj Matter
    MATTER_CMD_START,   ///< Spust Matter
    MATTER_CMD_STOP,    ///< Zastav Matter
    MATTER_CMD_STATUS   ///< Ziskej Matter status
} matter_command_type_t;

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Hlavni funkce Matter tasku
 * 
 * Bezi v nekonecne smycce a zpracovava Matter prikazy a aktualizace stavu.
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void matter_task_start(void *pvParameters);

// Zpracovani Matter prikazu

/**
 * @brief Zpracuj Matter prikazy z fronty
 * 
 * Cte prikazy z matter_command_queue a vykonava je.
 */
void matter_process_commands(void);

/**
 * @brief Vykonaj Matter prikaz
 * 
 * @param command Prikaz k vykonani (matter_command_type_t)
 */
void matter_execute_command(uint8_t command);

// Funkce pro ovladani Matter

/**
 * @brief Inicializuj Matter protokol
 * 
 * Inicializuje Matter protokol a zarizeni. Momentalne je to placeholder.
 */
void matter_initialize(void);

/**
 * @brief Spust Matter
 * 
 * Spusti Matter protokol a zahaji komunikaci se siti.
 */
void matter_start(void);

/**
 * @brief Zastav Matter
 * 
 * Zastavi Matter protokol a ukonci komunikaci.
 */
void matter_stop(void);

/**
 * @brief Ziskej Matter status
 * 
 * Vypise aktualni stav Matter protokolu (inicializovano, pripojeno, uptime).
 */
void matter_get_status(void);

// Aktualizace stavu Matter

/**
 * @brief Aktualizuj stav Matter
 * 
 * Periodicky aktualizuje stav Matter protokolu (placeholder pro budouci implementaci).
 */
void matter_update_state(void);

// Matter utility funkce

/**
 * @brief Overi zda je Matter inicializovan
 * 
 * @return true pokud je Matter inicializovan
 */
bool matter_is_initialized(void);

/**
 * @brief Overi zda je Matter pripojeno
 * 
 * @return true pokud je Matter pripojeno k siti
 */
bool matter_is_connected(void);

/**
 * @brief Ziskej Matter uptime
 * 
 * @return Uptime v milisekundach od inicializace
 */
uint32_t matter_get_uptime(void);

/**
 * @brief Posli data pres Matter
 * 
 * @param data Data k poslani
 * @param length Delka dat v bajtech
 */
void matter_send_data(uint8_t* data, size_t length);

/**
 * @brief Prijmi data pres Matter
 * 
 * @param buffer Buffer pro prijata data
 * @param max_length Maximalni delka bufferu
 */
void matter_receive_data(uint8_t* buffer, size_t max_length);

// ============================================================================
// EXTERNI PROMENNE
// ============================================================================

/** @brief Fronta pro Matter status odpovedi */
extern QueueHandle_t matter_status_queue;
/** @brief Fronta pro Matter prikazy */
extern QueueHandle_t matter_command_queue;

// ============================================================================
// FUNKCE PRO SPRAVU MATTER ZARIZENI
// ============================================================================

/**
 * @brief Registruj Matter zarizeni
 * 
 * Registruje sachovy system jako Matter zarizeni v siti.
 */
void matter_register_device(void);

/**
 * @brief Odregistruj Matter zarizeni
 * 
 * Odebere sachovy system z Matter site.
 */
void matter_unregister_device(void);

/**
 * @brief Aktualizuj stav Matter zarizeni
 * 
 * @param state Novy stav zarizeni
 */
void matter_update_device_state(uint8_t state);

// ============================================================================
// FUNKCE PRO STAV MATTER
// ============================================================================

/**
 * @brief Overi zda Matter task bezi
 * 
 * @return true pokud task bezi
 */
bool matter_is_task_running(void);

/**
 * @brief Zastav Matter task
 * 
 * Ukonci Matter task a uvolni prostredky.
 */
void matter_stop_task(void);

/**
 * @brief Resetuj Matter system
 * 
 * Provede kompletni reset Matter systemu.
 */
void matter_reset(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_TASK_H
