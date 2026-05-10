/**
 * @file uart_commands_extended.c
 * @brief ESP32-C6 Chess System - Rozsirene UART prikazy pro LED animace
 * 
 * Implementace novych prikazu pro endgame animace a jemne efekty.
 * Obsahuje prikazy pro ovladani pokrocilych LED animaci a efektu.
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-04
 * 
 * @details
 * Tento modul rozsiruje zakladni UART prikazy o pokrocile funkce
 * pro ovladani LED animaci. Obsahuje prikazy pro endgame animace,
 * jemne efekty a pokrocile vzory.
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
// POMOCNÉ FUNKCE
// ============================================================================

/**
 * @brief Parsuje pozici krále ze stringu (např. "e4" -> 28)
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
 * @brief Vrátí string reprezentaci pozice (např. 28 -> "e4")
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
// IMPLEMENTACE PŘÍKAZŮ
// ============================================================================

esp_err_t cmd_endgame_animations(int argc, char **argv, char *response, size_t response_size) {
    if (argc != 1) {
        snprintf(response, response_size,
                "📋 ENDGAME ANIMATIONS - Dostupné animace:\n"
                "\n"
                "1. Victory Wave - Vlna od vítězného krále\n"
                "   Modré vlny šířící se od krále, červená modulace pro protihráče\n"
                "   Pokračuje dokud se nezastaví reset tlačítkem nebo novou hrou\n"
                "\n"
                "2. Victory Circles - Expandující kruhy\n"
                "   Tři barevné kruhy expandující ze středu šachovnice\n"
                "   Zlatá, oranžová a bílá barva v rotaci\n"
                "\n"
                "3. Victory Cascade - Kaskádové padání\n"
                "   Diagonální vlna procházející šachovnicí\n"
                "   Efekt padajících figur s barevnými stíny\n"
                "\n"
                "4. Victory Fireworks - Ohňostroj\n"
                "   Náhodné ohňostroje v různých barvách\n"
                "   Expandující kruhy simulující výbuchy\n"
                "\n"
                "5. Victory Crown - Korunka vítěze\n"
                "   Zlatá korunka kolem vítězného krále\n"
                "   Pulsující efekt se středem na králi\n"
                "\n"
                "🎮 Použití: 'endgame animation X [pozice]'\n"
                "   X = 1-5 (typ animace)\n"
                "   pozice = např. 'e1', 'e8' (pozice krále, nepovinné)\n"
                "\n"
                "Příklady:\n"
                "• endgame animation 1 e1  - Victory Wave od e1\n"
                "• endgame animation 4     - Victory Fireworks (bez krále)\n"
                "• endgame animation 5 d8  - Victory Crown kolem d8");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "endgame animations příkaz vyžaduje přesně 0 argumentů, obdrženo: %d", argc);
    snprintf(response, response_size, "❌ Použití: 'endgame animations' (bez argumentů)");
    return ESP_ERR_INVALID_ARG;
}

esp_err_t cmd_endgame_animation(int argc, char **argv, char *response, size_t response_size) {
    if (argc < 1 || argc > 2) {
        snprintf(response, response_size,
                "❌ Nesprávný počet argumentů!\n"
                "\n"
                "🎮 Použití: 'endgame animation X [pozice]'\n"
                "   X = 1-5 (typ animace)\n"
                "   pozice = nepovinná pozice krále (např. 'e1')\n"
                "\n"
                "💡 Pro seznam všech animací použijte: 'endgame animations'");
        return ESP_ERR_INVALID_ARG;
    }

    // Parsování typu animace
    int animation_type = atoi(argv[0]);
    if (animation_type < 1 || animation_type >= ENDGAME_ANIM_MAX) {
        snprintf(response, response_size,
                "❌ Neplatný typ animace: %d\n"
                "\n"
                "✅ Dostupné typy: 1-5\n"
                "💡 Pro detaily použijte: 'endgame animations'", animation_type);
        return ESP_ERR_INVALID_ARG;
    }

    // Parsování pozice krále (pokud je zadána)
    uint8_t king_pos = 28; // Default e4 (střed šachovnice)
    
    if (argc == 2) {
        king_pos = parse_king_position(argv[1]);
        if (king_pos == 255) {
            snprintf(response, response_size,
                    "❌ Neplatná pozice krále: '%s'\n"
                    "\n"
                    "✅ Formát: písmeno a-h + číslice 1-8\n"
                    "💡 Příklady: e1, e8, d4, h7", argv[1]);
            return ESP_ERR_INVALID_ARG;
        }
    }

    ESP_LOGI(TAG, "Spouštím endgame animaci typu %d na pozici %d", animation_type, king_pos);

    // Zastavíme předchozí animaci pokud běží
    if (is_endgame_animation_running()) {
        ESP_LOGI(TAG, "Zastavuji předchozí endgame animaci");
        stop_endgame_animation();
        // Krátká pauza pro vyčištění
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Spustíme novou animaci
    esp_err_t result = start_endgame_animation((endgame_animation_type_t)animation_type, king_pos);
    
    if (result == ESP_OK) {
        char pos_str[4];
        position_to_string(king_pos, pos_str);
        
        snprintf(response, response_size,
                "🎬 Endgame animace spuštěna!\n"
                "\n"
                "📱 Typ: %s\n"
                "👑 Pozice krále: %s\n"
                "⏱️  Animace poběží dokud nebude zastavena\n"
                "\n"
                "🛑 Pro zastavení: 'led clear' nebo 'new game'\n"
                "💡 Pro jiné animace: 'endgame animations'",
                get_endgame_animation_name((endgame_animation_type_t)animation_type),
                pos_str);
    } else {
        snprintf(response, response_size,
                "❌ Nepodařilo se spustit endgame animaci!\n"
                "\n"
                "🔧 Možné příčiny:\n"
                "• Animační systém není inicializován\n"
                "• Nedostatek paměti pro timer\n"
                "• Systémová chyba\n"
                "\n"
                "💡 Zkuste restart systému: 'reboot'");
    }

    return result;
}

esp_err_t cmd_stop_animations(int argc, char **argv, char *response, size_t response_size) {
    ESP_LOGI(TAG, "Zastavuji všechny animace");

    bool was_running = is_endgame_animation_running();
    
    // Zastavíme endgame animace
    stop_endgame_animation();
    
    // Zastavíme jemné animace
    stop_all_subtle_animations();

    if (was_running) {
        snprintf(response, response_size,
                "🛑 Všechny animace zastaveny!\n"
                "\n"
                "✅ Endgame animace: zastavena\n"
                "✅ Jemné animace: zastaveny\n"
                "✅ Šachovnice: vyčištěna\n"
                "\n"
                "💡 Pro nové animace použijte: 'endgame animations'");
    } else {
        snprintf(response, response_size,
                "ℹ️  Žádné animace neběžely\n"
                "\n"
                "✅ Jemné animace: zastaveny (pro jistotu)\n"
                "✅ Šachovnice: vyčištěna\n"
                "\n"
                "💡 Pro spuštění animací: 'endgame animations'");
    }

    return ESP_OK;
}

esp_err_t cmd_animation_status(int argc, char **argv, char *response, size_t response_size) {
    bool endgame_running = is_endgame_animation_running();
    
    snprintf(response, response_size,
            "📊 STAV ANIMAČNÍHO SYSTÉMU\n"
            "\n"
            "🎬 Endgame animace: %s\n"
            "🎨 Jemné animace: aktivní podle potřeby\n"
            "⚡ Animační systém: %s\n"
            "🔄 Refresh rate: 20 FPS (50ms frame)\n"
            "\n"
            "%s"
            "\n"
            "💡 Dostupné příkazy:\n"
            "• endgame animations     - seznam animací\n"
            "• endgame animation X    - spustit animaci X\n"
            "• stop animations        - zastavit vše\n"
            "• animation status       - tento přehled",
            endgame_running ? "🟢 BĚŽÍ" : "🔴 VYPNUTO",
            "🟢 INICIALIZOVÁN", // Předpokládáme, že je inicializován pokud se příkaz spouští
            endgame_running ? 
                "🎭 Animace běží na pozadí a automaticky se obnovuje" :
                "😴 Žádná endgame animace neběží");

    return ESP_OK;
}

// ============================================================================
// REGISTRACE PŘÍKAZŮ
// ============================================================================

esp_err_t register_extended_uart_commands(void) {
    // Simple registration - commands are handled by uart_task.c directly
    ESP_LOGI(TAG, "✅ Rozšířené UART příkazy připraveny pro uart_task.c");
    return ESP_OK;
}

// ============================================================================
// DISPATCHER FUNKCE PRO ESP CONSOLE
// ============================================================================

int uart_endgame_command_dispatcher(int argc, char **argv) {
    char response[1024];
    esp_err_t result;
    
    if (argc < 1) {
        printf("❌ Nedostatek argumentů! Použijte: endgame animations nebo endgame animation X\n");
        return 1;
    }
    
    if (strcmp(argv[0], "animations") == 0) {
        result = cmd_endgame_animations(argc - 1, &argv[1], response, sizeof(response));
    } else if (strcmp(argv[0], "animation") == 0) {
        result = cmd_endgame_animation(argc - 1, &argv[1], response, sizeof(response));
    } else {
        printf("❌ Neznámý podpříkaz: '%s'\n"
               "💡 Použijte: 'endgame animations' nebo 'endgame animation X'\n", argv[0]);
        return 1;
    }
    
    // Vypíšeme odpověď
    printf("%s\n", response);
    
    return (result == ESP_OK) ? 0 : 1;
}

int uart_stop_command_dispatcher(int argc, char **argv) {
    char response[512];
    
    if (argc < 1 || strcmp(argv[0], "animations") != 0) {
        printf("❌ Použijte: 'stop animations'\n");
        return 1;
    }
    
    esp_err_t result = cmd_stop_animations(argc - 1, &argv[1], response, sizeof(response));
    
    printf("%s\n", response);
    
    return (result == ESP_OK) ? 0 : 1;
}

int uart_animation_command_dispatcher(int argc, char **argv) {
    char response[1024];
    
    if (argc < 1 || strcmp(argv[0], "status") != 0) {
        printf("❌ Použijte: 'animation status'\n");
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
            // Cervena: 0-60° plna, 60-120° klesa, jinak 0
            int r = (hue < 60) ? 255 : (hue < 120) ? 255 - ((hue - 60) * 255) / 60 : 0;
            // Zelena: 0-60° roste, 60-180° plna, 180-240° klesa, jinak 0
            int g = (hue < 60) ? (hue * 255) / 60 : (hue < 180) ? 255 : 255 - ((hue - 180) * 255) / 60;
            // Modra: 0-120° vypnuta, 120-240° roste, 240-360° plna
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