/**
 * @file uart_commands_extended.h
 * @brief ESP32-C6 Chess System - Header pro rozsirene UART prikazy
 * 
 * Deklarace funkci pro ovladani endgame animaci pres UART
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
// HLAVNI API PRO UART PRIKAZY
// ============================================================================

/**
 * @brief Registruje vsechny rozsirene UART prikazy
 * @return ESP_OK pri uspechu, jinak error kod
 */
esp_err_t register_extended_uart_commands(void);

// ============================================================================
// IMPLEMENTACE JEDNOTLIVYCH PRIKAZU
// ============================================================================

/**
 * @brief Obsluha prikazu "endgame animations"
 * Zobrazi seznam vsech dostupnych endgame animaci
 *
 * @param argc Pocet argumentu
 * @param argv Pole argumentu
 * @param response Buffer pro odpoved
 * @param response_size Velikost bufferu pro odpoved
 * @return ESP_OK pri uspechu
 */
esp_err_t cmd_endgame_animations(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Obsluha prikazu "endgame animation X [pozice]"
 * Spusti konkretni endgame animaci
 *
 * @param argc Pocet argumentu
 * @param argv Pole argumentu (argv[0] = cislo animace, argv[1] = pozice krale)
 * @param response Buffer pro odpoved
 * @param response_size Velikost bufferu pro odpoved
 * @return ESP_OK pri uspechu
 */
esp_err_t cmd_endgame_animation(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Obsluha prikazu "stop animations"
 * Zastavi vsechny bezici animace
 *
 * @param argc Pocet argumentu
 * @param argv Pole argumentu
 * @param response Buffer pro odpoved
 * @param response_size Velikost bufferu pro odpoved
 * @return ESP_OK pri uspechu
 */
esp_err_t cmd_stop_animations(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Obsluha prikazu "animation status"
 * Zobrazi stav animacniho systemu
 *
 * @param argc Pocet argumentu
 * @param argv Pole argumentu
 * @param response Buffer pro odpoved
 * @param response_size Velikost bufferu pro odpoved
 * @return ESP_OK pri uspechu
 */
esp_err_t cmd_animation_status(int argc, char **argv, char *response, size_t response_size);

// ============================================================================
// DISPATCHER FUNKCE PRO ESP CONSOLE
// ============================================================================

/**
 * @brief Dispatcher pro prikaz "endgame"
 * Smeruje na cmd_endgame_animations nebo cmd_endgame_animation
 */
int uart_endgame_command_dispatcher(int argc, char **argv);

/**
 * @brief Dispatcher pro prikaz "stop"
 * Smeruje na cmd_stop_animations
 */
int uart_stop_command_dispatcher(int argc, char **argv);

/**
 * @brief Dispatcher pro prikaz "animation"
 * Smeruje na cmd_animation_status
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