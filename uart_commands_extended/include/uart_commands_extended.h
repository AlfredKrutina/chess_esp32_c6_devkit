/**
 * @file uart_commands_extended.h
 * @brief ESP32-C6 Chess System - Header pro rozšířené UART příkazy
 * 
 * Deklarace funkcí pro ovládání endgame animací přes UART
 * 
 * Author: Alfred Krutina
 * Version: 2.5 - COMPLETE ANIMATIONS  
 * Date: 2025-09-04
 */

#ifndef UART_COMMANDS_EXTENDED_H
#define UART_COMMANDS_EXTENDED_H

#include "esp_err.h"
#include "game_led_animations.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// HLAVNÍ API PRO UART PŘÍKAZY
// ============================================================================

/**
 * @brief Registruje všechny rozšířené UART příkazy
 * @return ESP_OK při úspěchu, jinak error kód
 */
esp_err_t register_extended_uart_commands(void);

// ============================================================================
// IMPLEMENTACE JEDNOTLIVÝCH PŘÍKAZŮ
// ============================================================================

/**
 * @brief Obsluha příkazu "endgame animations"
 * Zobrazí seznam všech dostupných endgame animací
 * 
 * @param argc Počet argumentů
 * @param argv Pole argumentů
 * @param response Buffer pro odpověď
 * @param response_size Velikost bufferu pro odpověď
 * @return ESP_OK při úspěchu
 */
esp_err_t cmd_endgame_animations(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Obsluha příkazu "endgame animation X [pozice]"
 * Spustí konkrétní endgame animaci
 * 
 * @param argc Počet argumentů
 * @param argv Pole argumentů (argv[0] = číslo animace, argv[1] = pozice krále)
 * @param response Buffer pro odpověď
 * @param response_size Velikost bufferu pro odpověď
 * @return ESP_OK při úspěchu
 */
esp_err_t cmd_endgame_animation(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Obsluha příkazu "stop animations"
 * Zastaví všechny běžící animace
 * 
 * @param argc Počet argumentů
 * @param argv Pole argumentů
 * @param response Buffer pro odpověď
 * @param response_size Velikost bufferu pro odpověď
 * @return ESP_OK při úspěchu
 */
esp_err_t cmd_stop_animations(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Obsluha příkazu "animation status"
 * Zobrazí stav animačního systému
 * 
 * @param argc Počet argumentů
 * @param argv Pole argumentů
 * @param response Buffer pro odpověď
 * @param response_size Velikost bufferu pro odpověď
 * @return ESP_OK při úspěchu
 */
esp_err_t cmd_animation_status(int argc, char **argv, char *response, size_t response_size);

// ============================================================================
// DISPATCHER FUNKCE PRO ESP CONSOLE
// ============================================================================

/**
 * @brief Dispatcher pro příkaz "endgame"
 * Směruje na cmd_endgame_animations nebo cmd_endgame_animation
 */
int uart_endgame_command_dispatcher(int argc, char **argv);

/**
 * @brief Dispatcher pro příkaz "stop" 
 * Směruje na cmd_stop_animations
 */
int uart_stop_command_dispatcher(int argc, char **argv);

/**
 * @brief Dispatcher pro příkaz "animation"
 * Směruje na cmd_animation_status
 */
int uart_animation_command_dispatcher(int argc, char **argv);

// ============================================================================
// SIMPLE WRAPPER FUNCTIONS FOR UART_TASK.C
// ============================================================================

/**
 * @brief Simple wrapper for LED test command
 */
void handle_led_test_command(char* argv[], int argc);

/**
 * @brief Simple wrapper for LED pattern command
 */
void handle_led_pattern_command(char* argv[], int argc);

/**
 * @brief Simple wrapper for LED animation command
 */
void handle_led_animation_command(char* argv[], int argc);

/**
 * @brief Simple wrapper for LED clear command
 */
void handle_led_clear_command(char* argv[], int argc);

/**
 * @brief Simple wrapper for LED brightness command
 */
void handle_led_brightness_command(char* argv[], int argc);

/**
 * @brief Simple wrapper for chess position command
 */
void handle_chess_pos_command(char* argv[], int argc);

/**
 * @brief Simple wrapper for LED mapping test command
 */
void handle_led_mapping_test_command(char* argv[], int argc);

#ifdef __cplusplus
}
#endif

#endif // UART_COMMANDS_EXTENDED_H