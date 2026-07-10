/**
 * @file uart_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - UART Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani UART tasku:
 * - USB Serial JTAG konzolove rozhrani
 * - Znakovy vstup bez blokovani
 * - Parsovani a vykonavani prikazu
 * - Systemove testovani a diagnostika
 * - Formatovani a vystup odpovedi
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-01-27
 * 
 * @details
 * UART Task zpracovava vsechnu komunikaci s uzivatelem pres USB Serial JTAG.
 * Poskytuje radkovy terminal s histori, automatickym dokoncenim a barevnym vystupem.
 * 
 * Klicova vylepseni:
 * - I/O bez blokovani
 * - Lepsi zpracovani chyb
 * - Zjednodusene zpracovani vstupu
 */

#ifndef UART_TASK_LINENOISE_H
#define UART_TASK_LINENOISE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "chess_types.h"
#include "uart_queue_message.h"

#ifdef __cplusplus
extern "C" {
#endif

// Predni deklarace
// Poznamka: chess_move_command_t je definovan v chess_types.h

// ============================================================================
// TYPY VYSLEDKU PRIKAZU
// ============================================================================

/**
 * @brief Typy vysledku prikazu
 */
typedef enum {
    CMD_SUCCESS = 0,                  ///< Uspesne provedeno
    CMD_ERROR_INVALID_SYNTAX = -1,    ///< Neplatna syntaxe
    CMD_ERROR_INVALID_PARAMETER = -2, ///< Neplatny parameter
    CMD_ERROR_SYSTEM_ERROR = -3,      ///< Systemova chyba
    CMD_ERROR_NOT_FOUND = -4          ///< Prikaz nenalezen
} command_result_t;

/**
 * @brief Typ handleru prikazu (funkce pro zpracovani prikazu)
 */
typedef command_result_t (*command_handler_t)(const char* args);

// ============================================================================
// TYPY UART ZPRAV (uart_message_t v uart_queue_message.h)
// ============================================================================

/**
 * @brief Struktura UART prikazu
 */
typedef struct {
    const char* name;             ///< Nazev prikazu
    command_handler_t handler;    ///< Funkce pro zpracovani
    const char* description;      ///< Popis prikazu
    const char* usage;            ///< Pouziti prikazu
    bool requires_args;           ///< Vyzaduje argumenty?
    const char* aliases[5];       ///< Max 5 aliasu
} uart_command_t;

// ============================================================================
// EXTERNI PROMENNE
// ============================================================================

/** @brief Fronta pro UART vystup */
extern QueueHandle_t uart_output_queue;

// ============================================================================
// PROTOTYPY HLAVNICH FUNKCI
// ============================================================================

/**
 * @brief Hlavni funkce UART tasku
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void uart_task_start(void *pvParameters);

// ============================================================================
// FUNKCE PRO LINE-BASED VSTUP
// ============================================================================

/**
 * @brief Zapis retezec okamzite do UART
 * 
 * @param str Retezec k zapsani
 */
void uart_write_string_immediate(const char* str);

/**
 * @brief Zapis znak okamzite do UART
 * 
 * @param c Znak k zapsani
 */
void uart_write_char_immediate(char c);

// ============================================================================
// FUNKCE PRO PRIKAZY
// ============================================================================

/** @brief Prikaz help */
command_result_t uart_cmd_help(const char* args);
/** @brief Prikaz verbose */
command_result_t uart_cmd_verbose(const char* args);
/** @brief Prikaz quiet */
command_result_t uart_cmd_quiet(const char* args);
/** @brief Prikaz status */
command_result_t uart_cmd_status(const char* args);
/** @brief Prikaz version */
command_result_t uart_cmd_version(const char* args);
/** @brief Prikaz memory */
command_result_t uart_cmd_memory(const char* args);
/** @brief Prikaz history */
command_result_t uart_cmd_history(const char* args);
/** @brief Jednotný CLI panel — viz též `HELP CLI` */
command_result_t uart_cmd_cli(const char* args);
void uart_cli_print_help(void);
/** @brief Prikaz clear */
command_result_t uart_cmd_clear(const char* args);
/** @brief Prikaz reset */
command_result_t uart_cmd_reset(const char* args);
/** @brief Prikaz move */
command_result_t uart_cmd_move(const char* args);
/** @brief Prikaz up (zvedni figurku) */
command_result_t uart_cmd_up(const char* args);
/** @brief Prikaz dn (poloz figurku) */
command_result_t uart_cmd_dn(const char* args);
/** @brief Prikaz led_board */
command_result_t uart_cmd_led_board(const char* args);
/** @brief Prikaz board */
command_result_t uart_cmd_board(const char* args);
/** @brief Prikaz game_new */
command_result_t uart_cmd_game_new(const char* args);
/** @brief Prikaz game_reset */
command_result_t uart_cmd_game_reset(const char* args);
/** @brief Prikaz show_moves */
command_result_t uart_cmd_show_moves(const char* args);
/** @brief Prikaz undo */
command_result_t uart_cmd_undo(const char* args);
/** @brief Prikaz game_history */
command_result_t uart_cmd_game_history(const char* args);
/** @brief Prikaz benchmark */
command_result_t uart_cmd_benchmark(const char* args);
/** @brief Prikaz show_tasks */
command_result_t uart_cmd_show_tasks(const char* args);
/** @brief Prikaz self_test */
command_result_t uart_cmd_self_test(const char* args);
/** @brief Prikaz test_game */
command_result_t uart_cmd_test_game(const char* args);
/** @brief Prikaz debug_status */
command_result_t uart_cmd_debug_status(const char* args);
/** @brief Prikaz debug_game */
command_result_t uart_cmd_debug_game(const char* args);
/** @brief Prikaz debug_board */
command_result_t uart_cmd_debug_board(const char* args);
/** @brief Prikaz memcheck */
command_result_t uart_cmd_memcheck(const char* args);
/** @brief Prikaz show_mutexes */
command_result_t uart_cmd_show_mutexes(const char* args);
/** @brief Prikaz show_fifos */
command_result_t uart_cmd_show_fifos(const char* args);

/* Game / matrix handlers (uart_handlers_game.c) */
command_result_t uart_cmd_castle(const char* args);
command_result_t uart_cmd_promote(const char* args);
command_result_t uart_cmd_guard_clear(const char* args);
command_result_t uart_cmd_start_pos_check(const char* args);
command_result_t uart_cmd_matrixtest(const char* args);

/* System / demo handlers (uart_task.c) */
command_result_t uart_cmd_eval(const char* args);
command_result_t uart_cmd_ledtest(const char* args);
command_result_t uart_cmd_performance(const char* args);
command_result_t uart_cmd_config(const char* args);
command_result_t uart_cmd_demo(const char* args);
command_result_t uart_cmd_demo_status(const char* args);
command_result_t uart_cmd_component_off(const char* args);
command_result_t uart_cmd_component_on(const char* args);
command_result_t uart_cmd_endgame_white(const char* args);
command_result_t uart_cmd_endgame_black(const char* args);
command_result_t uart_cmd_endgame_wave(const char* args);
command_result_t uart_cmd_endgame_circles(const char* args);
command_result_t uart_cmd_endgame_cascade(const char* args);
command_result_t uart_cmd_endgame_fireworks(const char* args);
command_result_t uart_cmd_endgame_draw_spiral(const char* args);
command_result_t uart_cmd_endgame_draw_pulse(const char* args);
command_result_t uart_cmd_stop_endgame(const char* args);
command_result_t uart_cmd_test_move_anim(const char* args);
command_result_t uart_cmd_test_player_anim(const char* args);
command_result_t uart_cmd_test_castle_anim(const char* args);
command_result_t uart_cmd_test_promote_anim(const char* args);
command_result_t uart_cmd_test_endgame_anim(const char* args);

/* Timer / WiFi / web / MQTT handlers (uart_handlers_wifi.c) */
command_result_t uart_cmd_timer(const char* args);
command_result_t uart_cmd_timer_config(const char* args);
command_result_t uart_cmd_timer_pause(const char* args);
command_result_t uart_cmd_timer_resume(const char* args);
command_result_t uart_cmd_timer_reset(const char* args);
command_result_t uart_cmd_wifi(const char* args);
command_result_t uart_cmd_wifi_connect(const char* args);
command_result_t uart_cmd_wifi_disconnect(const char* args);
command_result_t uart_cmd_wifi_status(const char* args);
command_result_t uart_cmd_wifi_clear(const char* args);
command_result_t uart_cmd_web_lock(const char* args);
command_result_t uart_cmd_web_status(const char* args);
command_result_t uart_cmd_api_token(const char* args);
command_result_t uart_cmd_ble(const char* args);
command_result_t uart_cmd_mqtt_config(const char* args);
command_result_t uart_cmd_mqtt_status(const char* args);
command_result_t uart_cmd_mqtt_test(const char* args);

// ============================================================================
// POMOCNE FUNKCE
// ============================================================================

void uart_send_error(const char* message);
void uart_send_formatted(const char* format, ...);
void uart_send_line(const char* str);

/**
 * @brief Zobraz hlavni help
 */
void uart_display_main_help(void);

/**
 * @brief Help pro game prikazy
 */
void uart_cmd_help_game(void);

/**
 * @brief Help pro systemove prikazy
 */
void uart_cmd_help_system(void);

/**
 * @brief Help pro zacatecniky
 */
void uart_cmd_help_beginner(void);

/**
 * @brief Help pro debug prikazy
 */
void uart_cmd_help_debug(void);

// ============================================================================
// FUNKCE PRO ZOBRAZENI
// ============================================================================

/**
 * @brief Zobraz animaci tahu
 * 
 * @param from Zdrojova notace
 * @param to Cilova notace
 */
void uart_display_move_animation(const char* from, const char* to);

/**
 * @brief Zobraz rozsirenou sachovnici
 */
void uart_display_enhanced_board(void);

/**
 * @brief Zobraz LED sachovnici
 */
void uart_display_led_board(void);

// ============================================================================
// UTILITY FUNKCE
// ============================================================================

/**
 * @brief Overi platnost notace tahu
 * 
 * @param move Notace tahu (napr. "e2e4")
 * @return true pokud je notace platna
 */
bool is_valid_move_notation(const char* move);

/**
 * @brief Overi platnost notace pole
 * 
 * @param square Notace pole (napr. "e2")
 * @return true pokud je notace platna
 */
bool is_valid_square_notation(const char* square);

#ifdef __cplusplus
}
#endif

#endif // UART_TASK_LINENOISE_H
