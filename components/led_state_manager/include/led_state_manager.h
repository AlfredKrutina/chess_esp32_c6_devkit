/**
 * @file led_state_manager.h
 * @brief LED State Manager - Pokrocila sprava LED stavu a vrstev
 * 
 * Tento modul poskytuje komplexni spravu LED stavu:
 * - Vrstvovy system pro LED efekty
 * - Prioritni spravce LED
 * - Perzistentni LED stav
 * - Optimalizovane aktualizace
 * - Podpora pro kompozitni efekty
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * LED State Manager poskytuje pokrocily system pro spravu LED stavu.
 * Umoznuje kombinovat vice efektu najednou pomoci vrstvoveho systemu,
 * kde kazda vrstva ma svou prioritu a blending mode.
 * 
 * Vyhody:
 * - Vrstvovy kompoziting (sachovnice + efekty + GUI)
 * - Prioritni system s alpha blendingem
 * - Optimalizovane dirty-pixel updates
 * - Thread-safe pristup ke stavu
 * - Perzistence mezi aktualizacemi
 */

#ifndef LED_STATE_MANAGER_H
#define LED_STATE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LED VRSTVY
// ============================================================================

/**
 * @brief LED vrstvy (layers)
 * 
 * Nizsi cislo = nizsi vrstva (pozadi), vyssi cislo = vyssi vrstva (popredi)
 */
typedef enum {
    LED_LAYER_BACKGROUND = 0,     ///< Pozadi (board base color)
    LED_LAYER_PIECES = 1,         ///< Figurky
    LED_LAYER_MOVES = 2,          ///< Legalni tahy
    LED_LAYER_SELECTION = 3,      ///< Vyber figurky
    LED_LAYER_ANIMATION = 4,      ///< Animace (tah, capture, atd.)
    LED_LAYER_STATUS = 5,         ///< Status (check, checkmate)
    LED_LAYER_ERROR = 6,          ///< Chybove indikace
    LED_LAYER_GUI = 7,            ///< GUI overlay (buttons, atd.)
    LED_LAYER_COUNT               ///< Pocet vrstev
} led_layer_t;

/**
 * @brief Blending modes pro LED vrstvy
 */
typedef enum {
    BLEND_REPLACE = 0,    ///< Nahrad spodni vrstvy (alpha = 1.0)
    BLEND_ALPHA,          ///< Alpha blending (mix s alpha)
    BLEND_ADDITIVE,       ///< Aditivni (scitani barev)
    BLEND_MULTIPLY,       ///< Nasobeni
    BLEND_OVERLAY,        ///< Overlay efekt
    BLEND_MODE_COUNT
} blend_mode_t;

// ============================================================================
// KONFIGURACNI STRUKTURY
// ============================================================================

/**
 * @brief Konfigurace LED manageru
 */
typedef struct {
    uint8_t max_brightness;             ///< Maximalni jas (0-255)
    uint8_t default_brightness;         ///< Vychozi jas (0-255)
    bool enable_smooth_transitions;     ///< Povolit plynule prechody
    bool enable_layer_compositing;      ///< Povolit vrstvove slozeni
    uint8_t update_frequency_hz;        ///< Frekvence aktualizaci (Hz)
    uint32_t transition_duration_ms;    ///< Delka prechodu (ms)
} led_manager_config_t;

// ============================================================================
// LED STRUKTURY
// ============================================================================

/**
 * @brief RGB pixel s rozsir enymi atributy
 */
typedef struct {
    uint8_t r, g, b;      ///< RGB barva
    uint8_t alpha;        ///< Alpha kanal (0-255)
    uint8_t brightness;   ///< Jas pixelu (0-255)
    bool dirty;           ///< Pixel potrebuje update?
    uint32_t last_update; ///< Cas posledniho update (ms)
} led_pixel_t;

/**
 * @brief LED vrstva
 */
typedef struct {
    led_pixel_t pixels[73];  ///< 73 pixelu (64 sachovnice + 9 tlacitek)
    blend_mode_t blend_mode; ///< Blending mode
    bool enabled;            ///< Je vrstva povolena? (legacy)
    bool layer_enabled;      ///< Je vrstva povolena?
    uint8_t master_alpha;    ///< Master alpha pro celou vrstvu (legacy)
    uint8_t layer_opacity;   ///< Opacity vrstvy (0-255)
    bool dirty;              ///< Je vrstva dirty (potrebuje update)?
    bool needs_composite;    ///< Potrebuje prekompozici?
} led_layer_state_t;

/**
 * @brief Kompletni LED stav (vsechny vrstvy)
 */
typedef struct {
    led_layer_state_t layers[LED_LAYER_COUNT]; ///< Vsechny vrstvy
    led_pixel_t composite[64];                 ///< Finalni slozeny obraz
    bool dirty_pixels[64];                     ///< Dirty pixel mapa
    bool needs_update;                         ///< Potrebuje update?
    uint32_t last_update_time;                 ///< Cas posledniho update
    SemaphoreHandle_t mutex;                   ///< Mutex pro thread-safety
} led_state_t;

// ============================================================================
// INICIALIZACE A ZAKLADNI FUNKCE
// ============================================================================

/**
 * @brief Inicializuj LED state manager
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_init(void);

/**
 * @brief Inicializuj LED manager s konfiguraci
 * 
 * @param config Konfigurace manageru
 * @return ESP_OK pri uspechu
 */
esp_err_t led_manager_init(const led_manager_config_t* config);

/**
 * @brief Deinicializuj LED state manager
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_deinit(void);

/**
 * @brief Deinicializuj LED manager
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_manager_deinit(void);

/**
 * @brief Nastav konfiguraci LED manageru
 * 
 * @param config Nova konfigurace
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set_config(const led_manager_config_t* config);

/**
 * @brief Nastav frekvenci aktualizace
 * 
 * @param frequency_hz Frekvence v Hz (1-60)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set_update_frequency(uint8_t frequency_hz);

/**
 * @brief Nastav delku prechodu
 * 
 * @param duration_ms Delka v ms
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set_transition_duration(uint32_t duration_ms);

/**
 * @brief HSV to RGB konverze
 * 
 * @param h Hue (0.0-1.0)
 * @param s Saturation (0.0-1.0)
 * @param v Value (0.0-1.0)
 * @param r Vystupni Red (0-255)
 * @param g Vystupni Green (0-255)
 * @param b Vystupni Blue (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_hsv_to_rgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b);

// ============================================================================
// FUNKCE PRO SPRAVU VRSTEV
// ============================================================================

/**
 * @brief Nastav pixel ve vrstve
 * 
 * @param layer Vrstva
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @param r Cervena (0-255)
 * @param g Zelena (0-255)
 * @param b Modra (0-255)
 * @param alpha Alpha (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_set_pixel(led_layer_t layer, uint8_t row, uint8_t col,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t alpha);

/**
 * @brief Nastav pixel ve vrstve primo podle LED indexu
 * 
 * @param layer Vrstva
 * @param led_index LED index (0-72)
 * @param r Cervena (0-255)
 * @param g Zelena (0-255)
 * @param b Modra (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set_pixel_layer(led_layer_t layer, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Ziskej pixel z vrstvy
 * 
 * @param layer Vrstva
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @param[out] pixel Output pixel
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_get_pixel(led_layer_t layer, uint8_t row, uint8_t col,
                               led_pixel_t* pixel);

/**
 * @brief Vymaz vrstvu (nastav vsechny pixely na transparent)
 * 
 * @param layer Vrstva
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_clear_layer(led_layer_t layer);

/**
 * @brief Vymaz vrstvu (alias pro led_state_clear_layer)
 * 
 * @param layer Vrstva
 * @return ESP_OK pri uspechu
 */
esp_err_t led_clear_layer(led_layer_t layer);

/**
 * @brief Vymaz vsechny vrstvy
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_clear_all_layers(void);

/**
 * @brief Povol/zakaz vrstvu
 * 
 * @param layer Vrstva
 * @param enabled Povolit?
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_set_layer_enabled(led_layer_t layer, bool enabled);

/**
 * @brief Nastav blending mode vrstvy
 * 
 * @param layer Vrstva
 * @param mode Blending mode
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_set_blend_mode(led_layer_t layer, blend_mode_t mode);

/**
 * @brief Nastav master alpha vrstvy
 * 
 * @param layer Vrstva
 * @param alpha Master alpha (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_set_layer_alpha(led_layer_t layer, uint8_t alpha);

// ============================================================================
// KOMPOZITNI RENDEROVANI
// ============================================================================

/**
 * @brief Zkomponuj vsechny vrstvy do finalniho obrazu
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_compose(void);

/**
 * @brief Aplikuj blending mezi dvema pixely
 * 
 * @param bottom Spodni pixel
 * @param top Vrchni pixel
 * @param mode Blending mode
 * @param[out] result Vysledny pixel
 */
void led_state_blend_pixel(const led_pixel_t* bottom, 
                           const led_pixel_t* top,
                           blend_mode_t mode, 
                           led_pixel_t* result);

// ============================================================================
// OPTIMALIZOVANE AKTUALIZACE
// ============================================================================

/**
 * @brief Aktualizuj pouze dirty pixely na hardware
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_update_dirty(void);

/**
 * @brief Aktualizuj cely obraz na hardware
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_update_all(void);

/**
 * @brief Vynuc okamzity full update vsech LED
 * @return ESP_OK pri uspechu
 */
esp_err_t led_force_full_update(void);

/**
 * @brief Oznac pixel jako dirty
 * 
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 */
void led_state_mark_dirty(uint8_t row, uint8_t col);

/**
 * @brief Oznac vsechny pixely jako dirty
 */
void led_state_mark_all_dirty(void);

/**
 * @brief Vymazat dirty flagy
 */
void led_state_clear_dirty_flags(void);

// ============================================================================
// POKROCILE FUNKCE
// ============================================================================

/**
 * @brief Nastav celou vrstvu najednou
 * 
 * @param layer Vrstva
 * @param pixels Pole 64 pixelu
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_set_layer_bulk(led_layer_t layer, const led_pixel_t* pixels);

/**
 * @brief Fade vrstvu in/out
 * 
 * @param layer Vrstva
 * @param target_alpha Cilove alpha
 * @param duration_ms Delka fade v ms
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_fade_layer(led_layer_t layer, uint8_t target_alpha, 
                               uint32_t duration_ms);

/**
 * @brief Aplikuj gamma korekci na vrstvu
 * 
 * @param layer Vrstva
 * @param gamma Gamma hodnota (1.0 = zadna korekce, 2.2 = typicke)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_apply_gamma(led_layer_t layer, float gamma);

/**
 * @brief Aplikuj jas na vrstvu
 * 
 * @param layer Vrstva
 * @param brightness Jas (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_state_apply_brightness(led_layer_t layer, uint8_t brightness);

// ============================================================================
// STATUS A DIAGNOSTIKA
// ============================================================================

/**
 * @brief Vypis stav LED manageru
 */
void led_state_print_status(void);

/**
 * @brief Vypis stav vrstvy
 * 
 * @param layer Vrstva
 */
void led_state_print_layer(led_layer_t layer);

/**
 * @brief Ziskej globalni LED stav (pro read-only pristup)
 * 
 * @return Ukazatel na globalni stav
 */
const led_state_t* led_state_get_global(void);

#ifdef __cplusplus
}
#endif

#endif // LED_STATE_MANAGER_H
