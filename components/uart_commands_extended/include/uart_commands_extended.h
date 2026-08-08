/**
 * @file uart_commands_extended.h
 * @brief ESP32-C6 Chess System - Header for extended UART commands
 * 
 * Function declaration for controlling endgame animation via UART
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
// MAIN API FOR UART COMMANDS
// ============================================================================

/**
 * @brief Registers all extended UART commands
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t register_extended_uart_commands(void);

// ============================================================================
// IMPLEMENTATION OF INDIVIDUAL COMMANDS
// ============================================================================

/**
 * @brief "endgame animations" command handler
 * Displays a list of all available endgame animations
 *
 * @param argc Argument count
 * @param argv Argument array
 * @param response Buffer for the response
 * @param response_size Response buffer size
 * @return ESP_OK on success
 */
esp_err_t cmd_endgame_animations(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Command handler "endgame animation X [position]"
 * Run a specific endgame animation
 *
 * @param argc Argument count
 * @param argv Argument array (argv[0] = animation number, argv[1] = king position)
 * @param response Buffer for the response
 * @param response_size Response buffer size
 * @return ESP_OK on success
 */
esp_err_t cmd_endgame_animation(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Handling the "stop animations" command
 * Stop all running animations
 *
 * @param argc Argument count
 * @param argv Argument array
 * @param response Buffer for the response
 * @param response_size Response buffer size
 * @return ESP_OK on success
 */
esp_err_t cmd_stop_animations(int argc, char **argv, char *response, size_t response_size);

/**
 * @brief Handling the "animation status" command
 * Displays the state of the animation system
 *
 * @param argc Argument count
 * @param argv Argument array
 * @param response Buffer for the response
 * @param response_size Response buffer size
 * @return ESP_OK on success
 */
esp_err_t cmd_animation_status(int argc, char **argv, char *response, size_t response_size);

// ============================================================================
// DISPATCHER FUNCTIONS FOR ESP CONSOLE
// ============================================================================

/**
 * @brief Dispatcher to display "endgame"
 * Points to cmd_endgame_animations or cmd_endgame_animation
 */
int uart_endgame_command_dispatcher(int argc, char **argv);

/**
 * @brief Dispatcher for "stop" command
 * Redirects to cmd_stop_animations
 */
int uart_stop_command_dispatcher(int argc, char **argv);

/**
 * @brief Dispatcher to display "animation"
 * Points to cmd_animation_status
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