/**
 * @file led_task.h
 * @brief ESP32-C6 Chess System v2.4 - LED Task Hlavicka
 *
 * Tato hlavicka definuje rozhrani LED tasku:
 * - WS2812B LED ovladani (73 LED: 64 sachovnice + 9 tlacitek)
 * - LED animace a vzory
 * - Tlacitkove LED feedback
 * - Time-multiplexed aktualizace
 *
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 *
 * @details
 * LED task je zodpovedny za vsechny LED operace v systemu:
 * - Ovladani 73 WS2812B LED (64 sachovnice + 9 tlacitek)
 * - Pokrocile animace a efekty
 * - Button LED feedback (dostupnost, press/release stavy)
 * - Error handling a visualizace
 * - Thread-safe operace s mutex ochranou
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include "esp_err.h"
#include "freertos_chess.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PROTOTYPY HLAVNICH FUNKCI
// ============================================================================

/**
 * @brief Spusti LED task
 *
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void led_task_start(void *pvParameters);

/**
 * @brief Vymaz vsechny LED
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t led_clear_all(void);

/**
 * @brief Zpracuj LED prikazy z fronty
 */
void led_process_commands(void);

/**
 * @brief Aktualizuj LED hardware
 *
 * Posle aktualni LED data do WS2812B LED pasku.
 */
void led_update_hardware(void);

/**
 * @brief Nastav globalni jas LED
 *
 * @param brightness Jas v procentech (0-100)
 */
void led_set_brightness_global(uint8_t brightness);

/**
 * @brief Vykonaj novy LED prikaz
 *
 * @param cmd Ukazatel na LED prikaz
 */
void led_execute_command_new(const led_command_t *cmd);

// ============================================================================
// INTERNI LED FUNKCE
// ============================================================================

/**
 * @brief Nastav LED pixel (interni funkce bez mutex)
 *
 * @param led_index Index LED (0-72)
 * @param red Cervena (0-255)
 * @param green Zelena (0-255)
 * @param blue Modra (0-255)
 */
void led_set_pixel_internal(uint8_t led_index, uint8_t red, uint8_t green,
                            uint8_t blue);

/**
 * @brief Nastav vsechny LED (interni funkce bez mutex)
 *
 * @param red Cervena (0-255)
 * @param green Zelena (0-255)
 * @param blue Modra (0-255)
 */
void led_set_all_internal(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Vymaz vsechny LED (interni funkce bez mutex)
 */
void led_clear_all_internal(void);

// ============================================================================
// FUNKCE PRO ZOBRAZENI
// ============================================================================

/**
 * @brief Zobraz sachovnici na LED
 */
void led_show_chess_board(void);

/**
 * @brief Nastav button feedback LED
 *
 * @param button_id ID tlacitka (0-8)
 * @param available Je tlacitko dostupne?
 */
void led_set_button_feedback(uint8_t button_id, bool available);

/**
 * @brief Nastav tlacitko jako stisknute
 *
 * @param button_id ID tlacitka (0-8)
 */
void led_set_button_press(uint8_t button_id);

/**
 * @brief Nastav tlacitko jako uvolnene
 *
 * @param button_id ID tlacitka (0-8)
 */
void led_set_button_release(uint8_t button_id);

/**
 * @brief Ziskej barvu tlacitka
 *
 * @param button_id ID tlacitka (0-8)
 * @return Barva jako uint32_t RGB
 */
uint32_t led_get_button_color(uint8_t button_id);

// ============================================================================
// NOVA LOGIKA BUTTON LED
// ============================================================================

/**
 * @brief Nastav dostupnost promocniho tlacitka
 *
 * @param button_id ID tlacitka (0-8)
 * @param available Je tlacitko dostupne pro promoci?
 */
void led_set_button_promotion_available(uint8_t button_id, bool available);

/**
 * @brief Spust animaci
 *
 * @param duration_ms Doba trvani animace v milisekundach
 */
void led_start_animation(uint32_t duration_ms);

/**
 * @brief Zobraz testovaci vzor
 */
void led_test_pattern(void);

// ============================================================================
// SMART LED REPORTING FUNKCE
// ============================================================================

/**
 * @brief Vypis kompaktni status LED
 */
void led_print_compact_status(void);

/**
 * @brief Vypis detailni status LED
 */
void led_print_detailed_status(void);

/**
 * @brief Vypis pouze zmeny LED
 */
void led_print_changes_only(void);

/**
 * @brief Spust periodu ticheho vystupu
 *
 * @param duration_ms Doba trvani v milisekundach
 */
void led_start_quiet_period(uint32_t duration_ms);

// ============================================================================
// BEZPECNE LED FUNKCE PRO OSTATNI KOMPONENTY
// ============================================================================

/**
 * @brief Nastav LED pixel (thread-safe s mutex)
 *
 * @param led_index Index LED (0-72)
 * @param red Cervena (0-255)
 * @param green Zelena (0-255)
 * @param blue Modra (0-255)
 */
void led_set_pixel_safe(uint8_t led_index, uint8_t red, uint8_t green,
                        uint8_t blue);

/**
 * @brief Vymaz vsechny LED (thread-safe s mutex)
 */
void led_clear_all_safe(void);

/**
 * @brief Nastav vsechny LED (thread-safe s mutex)
 *
 * @param red Cervena (0-255)
 * @param green Zelena (0-255)
 * @param blue Modra (0-255)
 */
void led_set_all_safe(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Vynuceni okamzita LED aktualizace pro kriticke operace
 */
void led_force_immediate_update(void);

// ============================================================================
// FUNKCE PRO SPRAVU LED VRSTEV
// ============================================================================

/**
 * @brief Vymaz pouze sachovnicove LED (0-63), zachovej tlacitka
 */
void led_clear_board_only(void);

/**
 * @brief Vymaz pouze tlacitkove LED (64-72), zachovej sachovnici
 */
void led_clear_buttons_only(void);

/**
 * @brief Zachovej stavy tlacitek behem operaci
 */
void led_preserve_buttons(void);

/**
 * @brief Aktualizuj dostupnost tlacitek podle stavu hry
 */
void led_update_button_availability_from_game(void);

/**
 * @brief Nastav barvu vsech 64 LED desky pro HA mod
 *
 * @param r Cervena slozka (0-255)
 * @param g Zelena slozka (0-255)
 * @param b Modra slozka (0-255)
 * @param brightness Jas (0-255) - aplikuje se na RGB
 */
void led_set_ha_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

/**
 * @brief Obnovi sachovnici po HA modu
 *
 * Tato funkce obnovi normalni zobrazeni sachovnice (cerna/bila pole).
 */
void led_restore_chess_board(void);

/**
 * @brief Obnovi zobrazeni LED vsech tlacitek podle aktualniho stavu (dostupnost/stisk)
 * Volat po navratu z HA rezimu.
 */
void led_refresh_all_button_leds(void);

// ============================================================================
// ERROR HANDLING LED FUNKCE
// ============================================================================

/**
 * @brief LED chyba - neplatny tah
 *
 * @param cmd LED prikaz s informacemi o chybe
 */
void led_error_invalid_move(const led_command_t *cmd);

/**
 * @brief LED chyba - vrat figurku
 *
 * @param cmd LED prikaz s informacemi o chybe
 */
void led_error_return_piece(const led_command_t *cmd);

/**
 * @brief LED animace endgame
 *
 * @param cmd LED prikaz s parametry animace
 */
void led_anim_endgame(const led_command_t *cmd);

/**
 * @brief LED error recovery
 *
 * @param cmd LED prikaz s informacemi o recovery
 */
void led_error_recovery(const led_command_t *cmd);

/**
 * @brief Zobraz legalni tahy
 *
 * @param cmd LED prikaz s pozicemi legalnich tahu
 */
void led_show_legal_moves(const led_command_t *cmd);

// ============================================================================
// FUNKCE PRO INTEGRACI SE STAVEM HRY
// ============================================================================

/**
 * @brief Aktualizuj LED podle stavu hry
 */
void led_update_game_state(void);

/**
 * @brief Zvyrazni figurky ktere se mohou hybat
 */
void led_highlight_pieces_that_can_move(void);

/**
 * @brief Zvyrazni mozne tahy pro pole
 *
 * @param from_square Pole zdrojove figurky (0-63)
 */
void led_highlight_possible_moves(uint8_t from_square);

/**
 * @brief Vymaz vsechna zvyrazneni
 */
void led_clear_all_highlights(void);

/**
 * @brief Animace zmeny hrace
 */
void led_player_change_animation(void);

/**
 * @brief Overi zda ma LED vyznamne zmeny
 *
 * @return true pokud jsou vyznamne zmeny
 */
bool led_has_significant_changes(void);

// ============================================================================
// FUNKCE PRO PRISTUP K LED STAVU
// ============================================================================

/**
 * @brief Ziskej stav LED
 *
 * @param led_index Index LED (0-72)
 * @return Stav LED jako uint32_t RGB
 */
uint32_t led_get_led_state(uint8_t led_index);

/**
 * @brief Ziskej vsechny stavy LED
 *
 * @param[out] states Pole pro ulozeni stavu (min. 73 prvku)
 * @param max_count Maximalni pocet stavu k ziskani
 */
void led_get_all_states(uint32_t *states, size_t max_count);

/**
 * @brief Nastav animaci po endgame
 */
void led_setup_animation_after_endgame(void);

/**
 * @brief Zastav endgame animaci
 */
void led_stop_endgame_animation(void);

// BOOT ANIMATION LED FUNKCE
// ============================================================================

/**
 * @brief Spusti boot animaci (progresivni rozsviceni)
 */
void led_booting_animation(void);

/**
 * @brief Zjisti zda probiha boot animace
 * @return true pokud probiha boot animace
 */
bool led_is_booting(void);

/**
 * @brief LED boot animation step - rozsviti LED podle progress
 *
 * @param progress_percent Progress v procentech (0-100)
 * @details
 * Rozsviti LED podle progress boot procesu. Pouziva se pro zobrazeni
 * postupu inicializace systemu. Rozsviti LED svetle zelenou barvou.
 */
void led_boot_animation_step(uint8_t progress_percent);

/**
 * @brief LED boot animation fade out - postupne ztlumi vsechny LED na 0
 * @details
 * Postupne ztlumi vsechny board LED z brightness 128 na 0.
 * Pouziva se na konci boot procesu pro plynule ztlumeni.
 */
void led_boot_animation_fade_out(void);

#ifdef __cplusplus
}
#endif

#endif // LED_TASK_H
