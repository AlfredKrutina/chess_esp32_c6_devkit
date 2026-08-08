/**
 * @file uart_commands_extended.c
 * @brief ESP32-C6 Chess System - Extended UART commands for LED animations
 * 
 * Implementation of new commands for endgame animations and subtle effects.
 * Contains commands for controlling advanced LED animations and effects.
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-04
 * 
 * @details
 * This module extends basic UART commands with advanced functions
 * for controlling LED animation. Contains commands for endgame animations,
 * subtle effects and advanced patterns.
 */

#include "uart_commands_extended.h"
#include "game_led_animations.h"
#include "led_mapping.h"
#include "led_task.h"  // For LED control functions
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "UART_EXT";

// ============================================================================
// HELPFUL FUNCTIONS
// ============================================================================

/**
 * @brief Parses king position from string (eg "e4" -> 28)
 */
static uint8_t parse_king_position(const char* pos_str) {
    if (!pos_str || strlen(pos_str) != 2) {
        return 255; // Invalid
    }
    
    char file = pos_str[0];
    char rank = pos_str[1];
    
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return 255; // Invalid
    }
    
    uint8_t col = file - 'a';
    uint8_t row = rank - '1';
    
    return chess_pos_to_led_index(row, col);
}

/**
 * @brief Returns a string representation of the position (eg 28 -> "e4")
 */
static void position_to_string(uint8_t pos, char* out_str) {
    if (pos >= 64) {
        strcpy(out_str, "??");
        return;
    }
    
    uint8_t row, col;
    led_index_to_chess_pos(pos, &row, &col);
    char file = 'a' + col;
    char rank = '1' + row;
    
    out_str[0] = file;
    out_str[1] = rank;
    out_str[2] = '\0';
}

// ============================================================================
// IMPLEMENTATION OF COMMANDS
// ============================================================================

esp_err_t cmd_endgame_animations(int argc, char **argv, char *response, size_t response_size) {
    if (argc != 1) {
        snprintf(response, response_size,
                "📋 ENDGAME ANIMATIONS - Animations available:\n"
                "\n"
                "1. Victory Wave - Wave from the victorious king\n"
                "   Blue waves spreading from the king, red modulation for opponents\n"
                "   It continues until the reset is stopped by a button or a new game\n"
                "\n"
                "2. Victory Circles - Expanding circles\n"
                "   Three colored circles expanding from the center of the chessboard\n"
                "   Gold, orange and white color in rotation\n"
                "\n"
                "3. Victory Cascade - Cascade falling\n"
                "   A diagonal wave passing through a checkerboard\n"
                "   Effect of falling figures with colored shadows\n"
                "\n"
                "4. Victory Fireworks\n"
                "   Random fireworks in different colors\n"
                "   Expanding circles simulating explosions\n"
                "\n"
                "5. Victory Crown - Crown of the winner\n"
                "   Golden crown around the victorious king\n"
                "   Pulsating effect centered on the king\n"
                "\n"
                "🎮 Usage: 'endgame animation X [position]'\n"
                "   X = 1-5 (typ animace)\n"
                "   position = eg 'e1', 'e8' (king position, optional)\n"
                "\n"
                "Examples:\n"
                "• endgame animation 1 e1  - Victory Wave od e1\n"
                "• endgame animation 4 - Victory Fireworks (no king)\n"
                "• endgame animation 5 d8  - Victory Crown kolem d8");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "endgame animations command requires exactly 0 arguments, received: %d", argc);
    snprintf(response, response_size, "❌ Usage: 'endgame animations' (no arguments)");
    return ESP_ERR_INVALID_ARG;
}

esp_err_t cmd_endgame_animation(int argc, char **argv, char *response, size_t response_size) {
    if (argc < 1 || argc > 2) {
        snprintf(response, response_size,
                "❌ Incorrect number of arguments!\n"
                "\n"
                "🎮 Usage: 'endgame animation X [position]'\n"
                "   X = 1-5 (typ animace)\n"
                "   position = optional king position (eg 'e1')\n"
                "\n"
                "💡 To list all animations use: 'endgame animations'");
        return ESP_ERR_INVALID_ARG;
    }

    // Animation type parsing
    int animation_type = atoi(argv[0]);
    if (animation_type < 1 || animation_type >= ENDGAME_ANIM_MAX) {
        snprintf(response, response_size,
                "❌ Invalid animation type: %d\n"
                "\n"
                "✅ Available types: 1-5\n"
                "💡 For details use: 'endgame animations'", animation_type);
        return ESP_ERR_INVALID_ARG;
    }

    // Parsing the king's position (if given)
    uint8_t king_pos = 28; // Default e4 (center of the board)
    
    if (argc == 2) {
        king_pos = parse_king_position(argv[1]);
        if (king_pos == 255) {
            snprintf(response, response_size,
                    "❌ Invalid king position: '%s'\n"
                    "\n"
                    "✅ Format: letter a-h + numbers 1-8\n"
                    "💡 Examples: e1, e8, d4, h7", argv[1]);
            return ESP_ERR_INVALID_ARG;
        }
    }

    ESP_LOGI(TAG, "I am running an endgame animation of type %d at position %d", animation_type, king_pos);

    // We will stop the previous animation if it is running
    if (is_endgame_animation_running()) {
        ESP_LOGI(TAG, "Stopping the previous endgame animation");
        stop_endgame_animation();
        // A short break to clean up
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Let's start a new animation
    esp_err_t result = start_endgame_animation((endgame_animation_type_t)animation_type, king_pos);
    
    if (result == ESP_OK) {
        char pos_str[4];
        position_to_string(king_pos, pos_str);
        
        snprintf(response, response_size,
                "🎬 Endgame animation launched!\n"
                "\n"
                "📱 Typ: %s\n"
                "👑 King position: %s\n"
                "⏱️ The animation will run until stopped\n"
                "\n"
                "🛑 To stop: 'led clear' or 'new game'\n"
                "💡 For other animations: 'endgame animations'",
                get_endgame_animation_name((endgame_animation_type_t)animation_type),
                pos_str);
    } else {
        snprintf(response, response_size,
                "❌ Failed to start endgame animation!\n"
                "\n"
                "🔧 Possible causes:\n"
                "• The animation system is not initialized\n"
                "• Out of memory for timer\n"
                "• System error\n"
                "\n"
                "💡 Try rebooting the system: 'reboot'");
    }

    return result;
}

esp_err_t cmd_stop_animations(int argc, char **argv, char *response, size_t response_size) {
    ESP_LOGI(TAG, "I stop all animations");

    bool was_running = is_endgame_animation_running();
    
    // We will stop the endgame animations
    stop_endgame_animation();
    
    // Let's stop the soft animations
    stop_all_subtle_animations();

    if (was_running) {
        snprintf(response, response_size,
                "🛑 All animations stopped!\n"
                "\n"
                "✅ Endgame animace: zastavena\n"
                "✅ Smooth animations: stopped\n"
                "✅ Chessboard: cleaned\n"
                "\n"
                "💡 For new animations use: 'endgame animations'");
    } else {
        snprintf(response, response_size,
                "ℹ️ No animations were running\n"
                "\n"
                "✅ Fine animations: stopped (just in case)\n"
                "✅ Chessboard: cleaned\n"
                "\n"
                "💡 To run animations: 'endgame animations'");
    }

    return ESP_OK;
}

esp_err_t cmd_animation_status(int argc, char **argv, char *response, size_t response_size) {
    bool endgame_running = is_endgame_animation_running();
    
    snprintf(response, response_size,
            "📊 STATE OF THE ANIMATION SYSTEM\n"
            "\n"
            "🎬 Endgame animace: %s\n"
            "🎨 Subtle animations: active as needed\n"
            "⚡ Animation system: %s\n"
            "🔄 Refresh rate: 20 FPS (50ms frame)\n"
            "\n"
            "%s"
            "\n"
            "💡 Available commands:\n"
            "• endgame animations - list of animations\n"
            "• endgame animation X - start animation X\n"
            "• stop animations - stop everything\n"
            "• animation status - this overview",
            endgame_running ? "🟢 RUNNING" : "🔴 VYPNUTO",
            "🟢 INITIALIZED", // We assume that it is initialized when the command is executed
            endgame_running ? 
                "🎭 The animation runs in the background and refreshes automatically" :
                "😴 No endgame animation is running");

    return ESP_OK;
}

// ============================================================================
// REGISTRATION OF ORDERS
// ============================================================================

esp_err_t register_extended_uart_commands(void) {
    // Simple registration - commands are handled by uart_task.c directly
    ESP_LOGI(TAG, "✅ Extended UART commands prepared for uart_task.c");
    return ESP_OK;
}

// ============================================================================
// DISPATCHER FUNCTIONS FOR ESP CONSOLE
// ============================================================================

int uart_endgame_command_dispatcher(int argc, char **argv) {
    char response[1024];
    esp_err_t result;
    
    if (argc < 1) {
        printf("❌ Lack of arguments! Use: endgame animations or endgame animation X\n");
        return 1;
    }
    
    if (strcmp(argv[0], "animations") == 0) {
        result = cmd_endgame_animations(argc - 1, &argv[1], response, sizeof(response));
    } else if (strcmp(argv[0], "animation") == 0) {
        result = cmd_endgame_animation(argc - 1, &argv[1], response, sizeof(response));
    } else {
        printf("❌ Unknown subcommand: '%s'\n"
               "💡 Use: 'endgame animations' or 'endgame animation X'\n", argv[0]);
        return 1;
    }
    
    // We will write the answer
    printf("%s\n", response);
    
    return (result == ESP_OK) ? 0 : 1;
}

int uart_stop_command_dispatcher(int argc, char **argv) {
    char response[512];
    
    if (argc < 1 || strcmp(argv[0], "animations") != 0) {
        printf("❌ Use: 'stop animations'\n");
        return 1;
    }
    
    esp_err_t result = cmd_stop_animations(argc - 1, &argv[1], response, sizeof(response));
    
    printf("%s\n", response);
    
    return (result == ESP_OK) ? 0 : 1;
}

int uart_animation_command_dispatcher(int argc, char **argv) {
    char response[1024];
    
    if (argc < 1 || strcmp(argv[0], "status") != 0) {
        printf("❌ Use: 'animation status'\n");
        return 1;
    }
    
    esp_err_t result = cmd_animation_status(argc - 1, &argv[1], response, sizeof(response));
    
    printf("%s\n", response);
    
    return (result == ESP_OK) ? 0 : 1;
}

// ============================================================================
// SIMPLE WRAPPER FUNCTIONS FOR UART_TASK.C
// ============================================================================

/**
 * @brief Simple wrapper for LED test command
 */
void handle_led_test_command(char* argv[], int argc) {
    printf("LED Test Command - Testing LED strip...\r\n");
    
    // Test all LEDs with different colors
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 255, 0, 0);  // Red
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 255, 0);  // Green
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 255);  // Blue
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    
    led_clear_board_only();
    printf("LED Test completed!\r\n");
}

/**
 * @brief Simple wrapper for LED pattern command
 */
void handle_led_pattern_command(char* argv[], int argc) {
    if (argc < 2) {
        printf("Usage: led_pattern <pattern>\r\n");
        printf("Available patterns: checker, rainbow, spiral, cross\r\n");
        return;
    }
    
    led_clear_board_only();
    
    if (strcmp(argv[1], "checker") == 0) {
        // Checker pattern
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if ((row + col) % 2 == 0) {
                    uint8_t led = chess_pos_to_led_index(row, col);
                    led_set_pixel_safe(led, 255, 255, 255);  // White
                }
            }
        }
        printf("Checker pattern displayed\r\n");
    }
    else if (strcmp(argv[1], "rainbow") == 0) {
        // Rainbow pattern - HSV to RGB konverze pro duhovy efekt
        // Pro kazdy LED pixel vypocitame jinou barvu na duhove skale
        for (int i = 0; i < 64; i++) {
            // Rozdelime 360° duhy mezi 64 LED (0-360°)
            int hue = (i * 360) / 64;
            
            // HSV -> RGB konverze (zjednodusena verze)
            // Red: 0-60° full, 60-120° dim, otherwise 0
            int r = (hue < 60) ? 255 : (hue < 120) ? 255 - ((hue - 60) * 255) / 60 : 0;
            // Green: 0-60° rising, 60-180° full, 180-240° falling, otherwise 0
            int g = (hue < 60) ? (hue * 255) / 60 : (hue < 180) ? 255 : 255 - ((hue - 180) * 255) / 60;
            // Blue: 0-120° off, 120-240° rising, 240-360° full
            int b = (hue < 120) ? 0 : (hue < 240) ? ((hue - 120) * 255) / 120 : 255;
            
            led_set_pixel_safe(i, r, g, b);
        }
        printf("Rainbow pattern displayed\r\n");
    }
    else {
        printf("Unknown pattern: %s\r\n", argv[1]);
    }
}

/**
 * @brief Simple wrapper for LED animation command
 */
void handle_led_animation_command(char* argv[], int argc) {
    if (argc < 2) {
        printf("Usage: led_animation <animation>\r\n");
        printf("Available animations: cascade, fireworks, crown, wave, circles\r\n");
        return;
    }
    
    if (strcmp(argv[1], "cascade") == 0) {
        printf("Starting cascade animation...\r\n");
        start_endgame_animation(ENDGAME_ANIM_VICTORY_CASCADE, 32); // Center position
    }
    else if (strcmp(argv[1], "fireworks") == 0) {
        printf("Starting fireworks animation...\r\n");
        start_endgame_animation(ENDGAME_ANIM_VICTORY_FIREWORKS, 32); // Center position
    }
    else if (strcmp(argv[1], "crown") == 0) {
        printf("Starting crown animation...\r\n");
        start_endgame_animation(ENDGAME_ANIM_VICTORY_CROWN, 32); // Center position
    }
    else if (strcmp(argv[1], "wave") == 0) {
        printf("Starting wave animation...\r\n");
        start_endgame_animation(ENDGAME_ANIM_VICTORY_WAVE, 32); // Center position
    }
    else if (strcmp(argv[1], "circles") == 0) {
        printf("Starting circles animation...\r\n");
        start_endgame_animation(ENDGAME_ANIM_VICTORY_CIRCLES, 32); // Center position
    }
    else {
        printf("Unknown animation: %s\r\n", argv[1]);
    }
}

/**
 * @brief Simple wrapper for LED clear command
 */
void handle_led_clear_command(char* argv[], int argc) {
    led_clear_board_only();
    printf("All LEDs cleared\r\n");
}

/**
 * @brief Simple wrapper for LED brightness command
 */
void handle_led_brightness_command(char* argv[], int argc) {
    if (argc < 2) {
        printf("Usage: led_brightness <0-255>\r\n");
        return;
    }
    
    int brightness = atoi(argv[1]);
    if (brightness < 0 || brightness > 255) {
        printf("Brightness must be between 0 and 255\r\n");
        return;
    }
    
    // Set brightness for all LEDs
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, brightness, brightness, brightness);
    }
    printf("Brightness set to %d\r\n", brightness);
}

/**
 * @brief Simple wrapper for chess position command
 */
void handle_chess_pos_command(char* argv[], int argc) {
    if (argc < 2) {
        printf("Usage: chess_pos <position> (e.g., a1, h8)\r\n");
        return;
    }
    
    uint8_t led_index = chess_notation_to_led_index(argv[1]);
    uint8_t row, col;
    led_index_to_chess_pos(led_index, &row, &col);
    
    printf("Position %s -> LED index %d (row %d, col %d)\r\n", 
           argv[1], led_index, row, col);
    
    // Light up the position
    led_clear_board_only();
    led_set_pixel_safe(led_index, 255, 255, 0);  // Yellow
}

/**
 * @brief Simple wrapper for LED mapping test command
 */
void handle_led_mapping_test_command(char* argv[], int argc) {
    printf("Testing LED mapping (serpentine layout)...\r\n");
    test_led_mapping();
    
    // Visual test - show a pattern
    led_clear_board_only();
    for (int i = 0; i < 8; i++) {
        uint8_t led = chess_pos_to_led_index(i, i);  // Diagonal
        led_set_pixel_safe(led, 255, 0, 0);  // Red
    }
    printf("Diagonal pattern displayed for visual verification\r\n");
}