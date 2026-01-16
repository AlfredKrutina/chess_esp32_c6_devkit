/**
 * @file timer_system.c
 * @brief ESP32-C6 Chess System v2.4 - Timer System implementace
 * 
 * Tato komponenta implementuje casovy system pro sachovou hru:
 * - Presne mereni casu s ESP32 timer API
 * - Thread-safe operace s casem
 * - Ruzne typy casovych kontrol
 * - JSON API pro web rozhrani
 * - NVS persistence nastaveni
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-01-XX
 * 
 * @details
 * Timer system pouziva ESP32 timer API pro presne mereni casu.
 * Vsechny operace jsou thread-safe s pouzitim mutexu.
 * Podporuje standardni casove kontroly a vlastni nastaveni.
 */

#include "include/timer_system.h"
#include "../led_task/include/led_task.h"  // Pro timeout animace
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

static const char* TAG = "TIMER_SYSTEM";

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

// Mutex pro thread-safe operace
static SemaphoreHandle_t timer_mutex = NULL;

// Aktualni stav timeru
static chess_timer_t current_timer = {0};

// Předdefinované konfigurace časových kontrol
static const time_control_config_t TIME_CONTROLS[] = {
    {TIME_CONTROL_NONE, 0, 0, "Bez času", "Hra bez časové kontroly", false},
    {TIME_CONTROL_BULLET_1_0, 60000, 0, "Bullet 1+0", "1 minuta bez incrementu", true},
    {TIME_CONTROL_BULLET_1_1, 60000, 1000, "Bullet 1+1", "1 minuta + 1s increment", true},
    {TIME_CONTROL_BULLET_2_1, 120000, 1000, "Bullet 2+1", "2 minuty + 1s increment", true},
    {TIME_CONTROL_BLITZ_3_0, 180000, 0, "Blitz 3+0", "3 minuty bez incrementu", true},
    {TIME_CONTROL_BLITZ_3_2, 180000, 2000, "Blitz 3+2", "3 minuty + 2s increment", true},
    {TIME_CONTROL_BLITZ_5_0, 300000, 0, "Blitz 5+0", "5 minut bez incrementu", true},
    {TIME_CONTROL_BLITZ_5_3, 300000, 3000, "Blitz 5+3", "5 minut + 3s increment", true},
    {TIME_CONTROL_RAPID_10_0, 600000, 0, "Rapid 10+0", "10 minut bez incrementu", false},
    {TIME_CONTROL_RAPID_10_5, 600000, 5000, "Rapid 10+5", "10 minut + 5s increment", false},
    {TIME_CONTROL_RAPID_15_10, 900000, 10000, "Rapid 15+10", "15 minut + 10s increment", false},
    {TIME_CONTROL_RAPID_30_0, 1800000, 0, "Rapid 30+0", "30 minut bez incrementu", false},
    {TIME_CONTROL_CLASSICAL_60_0, 3600000, 0, "Classical 60+0", "1 hodina bez incrementu", false},
    {TIME_CONTROL_CLASSICAL_90_30, 5400000, 30000, "Classical 90+30", "90 minut + 30s increment", false},
    {TIME_CONTROL_CUSTOM, 0, 0, "Vlastní", "Vlastní nastavení času", false}
};

// NVS namespace pro timer nastaveni
#define TIMER_NVS_NAMESPACE "timer_settings"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecne ziska mutex
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
static esp_err_t timer_lock(void)
{
    if (timer_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take timer mutex");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Uvolni mutex
 */
static void timer_unlock(void)
{
    if (timer_mutex != NULL) {
        xSemaphoreGive(timer_mutex);
    }
}

/**
 * @brief Aktualizuje cas hrace na tahu
 * 
 * @param is_white_turn Je-li na tahu bily
 * @param elapsed_ms Ubehly cas v milisekundach
 */
static void timer_update_player_time(bool is_white_turn, uint32_t elapsed_ms)
{
    if (is_white_turn) {
        if (current_timer.white_time_ms > elapsed_ms) {
            current_timer.white_time_ms -= elapsed_ms;
        } else {
            current_timer.white_time_ms = 0;
            current_timer.time_expired = true;
        }
    } else {
        if (current_timer.black_time_ms > elapsed_ms) {
            current_timer.black_time_ms -= elapsed_ms;
        } else {
            current_timer.black_time_ms = 0;
            current_timer.time_expired = true;
        }
    }
}

/**
 * @brief Kontroluje upozorneni na nizky cas
 * 
 * @param time_ms Cas v milisekundach
 * @param is_white_turn Je-li na tahu bily
 */
static void timer_check_warnings(uint32_t time_ms, bool is_white_turn)
{
    if (time_ms < 5000 && !current_timer.warning_5s_shown) {
        current_timer.warning_5s_shown = true;
        ESP_LOGW(TAG, "⚠️ Critical time warning: %s has less than 5 seconds!", 
                 is_white_turn ? "White" : "Black");
    } else if (time_ms < 10000 && !current_timer.warning_10s_shown) {
        current_timer.warning_10s_shown = true;
        ESP_LOGW(TAG, "⚠️ Low time warning: %s has less than 10 seconds!", 
                 is_white_turn ? "White" : "Black");
    } else if (time_ms < 30000 && !current_timer.warning_30s_shown) {
        current_timer.warning_30s_shown = true;
        ESP_LOGW(TAG, "⚠️ Time warning: %s has less than 30 seconds!", 
                 is_white_turn ? "White" : "Black");
    }
}

/**
 * @brief Resetuje upozorneni
 */
static void timer_reset_warnings(void)
{
    current_timer.warning_30s_shown = false;
    current_timer.warning_10s_shown = false;
    current_timer.warning_5s_shown = false;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

esp_err_t timer_system_init(void)
{
    ESP_LOGI(TAG, "Initializing timer system...");
    
    // Vytvorit mutex
    timer_mutex = xSemaphoreCreateMutex();
    if (timer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create timer mutex");
        return ESP_FAIL;
    }
    
    // Inicializovat timer strukturu
    memset(&current_timer, 0, sizeof(chess_timer_t));
    current_timer.config = TIME_CONTROLS[TIME_CONTROL_NONE];
    current_timer.timer_running = false;
    current_timer.is_white_turn = true;
    current_timer.game_paused = false;
    current_timer.time_expired = false;
    
    // Nacist nastaveni z NVS
    esp_err_t ret = timer_load_settings();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load timer settings, using defaults");
    }
    
    ESP_LOGI(TAG, "Timer system initialized successfully");
    return ESP_OK;
}

esp_err_t timer_set_time_control(const time_control_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Kopirovat konfiguraci
    memcpy(&current_timer.config, config, sizeof(time_control_config_t));
    
    // Nastavit pocatecni casy
    current_timer.white_time_ms = config->initial_time_ms;
    current_timer.black_time_ms = config->initial_time_ms;
    
    // Resetovat stav
    current_timer.timer_running = false;
    current_timer.game_paused = false;
    current_timer.time_expired = false;
    current_timer.total_moves = 0;
    current_timer.avg_move_time_ms = 0;
    
    // Resetovat upozorneni
    timer_reset_warnings();
    
    timer_unlock();
    
    ESP_LOGI(TAG, "Time control set: %s (%lu ms + %lu ms increment)", 
             config->name, config->initial_time_ms, config->increment_ms);
    
    // Uložit nastavení do NVS
    esp_err_t save_ret = timer_save_settings();
    if (save_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save timer settings to NVS: %s", esp_err_to_name(save_ret));
        // Necháme to projít - uložení není kritické
    }
    
    return ESP_OK;
}

esp_err_t timer_start_move(bool is_white_turn)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Kontrola zda je casova kontrola aktivni
    if (current_timer.config.type == TIME_CONTROL_NONE) {
        timer_unlock();
        return ESP_OK; // Bez casove kontroly
    }
    
    // Kontrola zda hra neni pozastavena
    if (current_timer.game_paused) {
        timer_unlock();
        return ESP_OK; // Hra je pozastavena
    }
    
    // Kontrola zda cas nevyprsel
    if (current_timer.time_expired) {
        timer_unlock();
        return ESP_OK; // Cas uz vyprsel
    }
    
    // Nastavit aktualniho hrace
    current_timer.is_white_turn = is_white_turn;
    
    // Spustit timer
    current_timer.move_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    current_timer.timer_running = true;
    
    timer_unlock();
    
    ESP_LOGI(TAG, "⏱️ Timer started for %s (remaining: White=%lums, Black=%lums)", 
             is_white_turn ? "White" : "Black",
             current_timer.white_time_ms, current_timer.black_time_ms);
    
    return ESP_OK;
}

esp_err_t timer_end_move(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Kontrola zda je casova kontrola aktivni
    if (current_timer.config.type == TIME_CONTROL_NONE) {
        timer_unlock();
        return ESP_OK; // Bez casove kontroly
    }
    
    // Kontrola zda timer bezi
    if (!current_timer.timer_running) {
        timer_unlock();
        return ESP_OK; // Timer nebezi
    }
    
    // Vypocitat ubehly cas
    uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    uint32_t elapsed_ms = (uint32_t)(current_time - current_timer.move_start_time);
    
    // Ulozit ktereho hrace jsme odpoctavali (pred aktualizaci)
    bool was_white_turn = current_timer.is_white_turn;
    uint32_t time_before = was_white_turn ? current_timer.white_time_ms : current_timer.black_time_ms;
    
    // Aktualizovat cas hrace
    timer_update_player_time(was_white_turn, elapsed_ms);
    
    // Pridat increment pokud je nastaven
    if (current_timer.config.increment_ms > 0) {
        if (was_white_turn) {
            current_timer.white_time_ms += current_timer.config.increment_ms;
        } else {
            current_timer.black_time_ms += current_timer.config.increment_ms;
        }
    }
    
    uint32_t time_after = was_white_turn ? current_timer.white_time_ms : current_timer.black_time_ms;
    
    // Aktualizovat statistiky
    current_timer.total_moves++;
    if (current_timer.avg_move_time_ms == 0) {
        current_timer.avg_move_time_ms = elapsed_ms;
    } else {
        // Vypocitat prumerny cas
        current_timer.avg_move_time_ms = 
            (current_timer.avg_move_time_ms + elapsed_ms) / 2;
    }
    
    // Ukoncit timer
    current_timer.timer_running = false;
    current_timer.last_move_time = current_time;
    
    // Resetovat upozorneni pro dalsiho hrace
    timer_reset_warnings();
    
    timer_unlock();
    
    ESP_LOGI(TAG, "⏱️ Timer ended for %s: elapsed=%lums, time: %lum -> %lum (+%lum increment)", 
             was_white_turn ? "White" : "Black",
             elapsed_ms,
             time_before / 1000, time_after / 1000,
             current_timer.config.increment_ms / 1000);
    
    ESP_LOGD(TAG, "Move ended. Elapsed: %lu ms, %s time: %lu ms", 
             elapsed_ms, 
             current_timer.is_white_turn ? "White" : "Black",
             current_timer.is_white_turn ? current_timer.white_time_ms : current_timer.black_time_ms);
    
    return ESP_OK;
}

esp_err_t timer_pause(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Pokud timer bezi, ulozit elapsed time a odečíst ho
    if (current_timer.timer_running && current_timer.config.type != TIME_CONTROL_NONE) {
        uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        uint32_t elapsed_ms = (uint32_t)(current_time - current_timer.move_start_time);
        
        // Odečíst elapsed time od aktuálního hráče
        timer_update_player_time(current_timer.is_white_turn, elapsed_ms);
        
        ESP_LOGI(TAG, "⏸️ Timer paused: %s time saved (elapsed: %lums)", 
                 current_timer.is_white_turn ? "White" : "Black", elapsed_ms);
    }
    
    current_timer.game_paused = true;
    current_timer.timer_running = false;
    
    timer_unlock();
    
    ESP_LOGI(TAG, "Timer paused");
    
    return ESP_OK;
}

esp_err_t timer_resume(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Kontrola zda je casova kontrola aktivni
    if (current_timer.config.type == TIME_CONTROL_NONE) {
        timer_unlock();
        ESP_LOGW(TAG, "Cannot resume: no time control set");
        return ESP_OK;
    }
    
    // Kontrola zda cas nevyprsel
    if (current_timer.time_expired) {
        timer_unlock();
        ESP_LOGW(TAG, "Cannot resume: time has expired");
        return ESP_OK;
    }
    
    current_timer.game_paused = false;
    
    // Spustit timer znovu pro aktuálního hráče
    current_timer.move_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    current_timer.timer_running = true;
    
    timer_unlock();
    
    ESP_LOGI(TAG, "▶️ Timer resumed for %s", current_timer.is_white_turn ? "White" : "Black");
    
    return ESP_OK;
}

esp_err_t timer_reset(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Resetovat casy na pocatecni hodnoty
    current_timer.white_time_ms = current_timer.config.initial_time_ms;
    current_timer.black_time_ms = current_timer.config.initial_time_ms;
    
    // Resetovat stav
    current_timer.timer_running = false;
    current_timer.game_paused = false;
    current_timer.time_expired = false;
    current_timer.is_white_turn = true;
    current_timer.total_moves = 0;
    current_timer.avg_move_time_ms = 0;
    
    // Resetovat upozorneni
    timer_reset_warnings();
    
    timer_unlock();
    
    ESP_LOGI(TAG, "Timer reset");
    
    return ESP_OK;
}

bool timer_check_timeout(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return false;
    }
    
    // Kontrola zda je casova kontrola aktivni
    if (current_timer.config.type == TIME_CONTROL_NONE) {
        timer_unlock();
        return false; // Bez casove kontroly
    }
    
    // Kontrola zda hra neni pozastavena
    if (current_timer.game_paused) {
        timer_unlock();
        return false; // Hra je pozastavena
    }
    
    // Kontrola zda timer bezi
    if (!current_timer.timer_running) {
        timer_unlock();
        return current_timer.time_expired; // Vratit aktualni stav
    }
    
    // Vypocitat ubehly cas
    uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    uint32_t elapsed_ms = (uint32_t)(current_time - current_timer.move_start_time);
    
    // Aktualizovat cas hrace
    uint32_t current_player_time = current_timer.is_white_turn ? 
        current_timer.white_time_ms : current_timer.black_time_ms;
    
    if (current_player_time <= elapsed_ms) {
        // Cas vyprsel
        current_timer.time_expired = true;
        current_timer.timer_running = false;
        
        if (current_timer.is_white_turn) {
            current_timer.white_time_ms = 0;
        } else {
            current_timer.black_time_ms = 0;
        }
        
        timer_unlock();
        
        ESP_LOGW(TAG, "⏰ Time expired for %s!", 
                 current_timer.is_white_turn ? "White" : "Black");
        
        // Spustit timeout animaci - blikani cervenou barvou
        led_command_t timeout_cmd = {
            .type = LED_CMD_ANIM_CHECK,  // Pouzit check animaci pro timeout (blikani cervenou)
            .led_index = 0,
            .red = 255,
            .green = 0,
            .blue = 0,
            .duration_ms = 2000,  // 2 sekundy blikani
            .data = NULL,
            .response_queue = NULL
        };
        led_execute_command_new(&timeout_cmd);
        ESP_LOGI(TAG, "⏰ Timeout animation triggered");
        
        return true;
    }
    
    // Kontrola upozorneni
    uint32_t remaining_time = current_player_time - elapsed_ms;
    timer_check_warnings(remaining_time, current_timer.is_white_turn);
    
    timer_unlock();
    
    return false;
}

esp_err_t timer_get_state(chess_timer_t* timer_data)
{
    if (timer_data == NULL) {
        ESP_LOGE(TAG, "Invalid timer_data parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Kopirovat aktualni stav
    memcpy(timer_data, &current_timer, sizeof(chess_timer_t));
    
    // Aktualizovat casy pokud timer bezi nebo pokud je casova kontrola nastavena
    if (current_timer.config.type != TIME_CONTROL_NONE && !current_timer.game_paused) {
        if (current_timer.timer_running) {
            // Timer bezi - vypocitat aktualni zbývající cas
            uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
            uint32_t elapsed_ms = (uint32_t)(current_time - current_timer.move_start_time);
            
            // Pouzit ORIGINALNI hodnoty z current_timer (ne z kopie)
            uint32_t original_white = current_timer.white_time_ms;
            uint32_t original_black = current_timer.black_time_ms;
            
            if (current_timer.is_white_turn) {
                if (original_white > elapsed_ms) {
                    timer_data->white_time_ms = original_white - elapsed_ms;
                } else {
                    timer_data->white_time_ms = 0;
                    timer_data->time_expired = true;
                }
                // Cerny cas zustava stejny (nebezi pro neho)
                timer_data->black_time_ms = original_black;
            } else {
                if (original_black > elapsed_ms) {
                    timer_data->black_time_ms = original_black - elapsed_ms;
                } else {
                    timer_data->black_time_ms = 0;
                    timer_data->time_expired = true;
                }
                // Bily cas zustava stejny (nebezi pro neho)
                timer_data->white_time_ms = original_white;
            }
        }
        // Pokud timer nebezi, casy zustavaji stejne (ulozene hodnoty)
        // To umozni zobrazení času i když timer ještě neběží
    }
    
    timer_unlock();
    
    return ESP_OK;
}

uint32_t timer_get_remaining_time(bool is_white_turn)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return 0;
    }
    
    uint32_t remaining_time = is_white_turn ? 
        current_timer.white_time_ms : current_timer.black_time_ms;
    
    // Aktualizovat cas pokud timer bezi
    if (current_timer.timer_running && !current_timer.game_paused && 
        current_timer.config.type != TIME_CONTROL_NONE &&
        current_timer.is_white_turn == is_white_turn) {
        
        uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        uint32_t elapsed_ms = (uint32_t)(current_time - current_timer.move_start_time);
        
        if (remaining_time > elapsed_ms) {
            remaining_time -= elapsed_ms;
        } else {
            remaining_time = 0;
        }
    }
    
    timer_unlock();
    
    return remaining_time;
}

esp_err_t timer_get_config_by_type(time_control_type_t type, time_control_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (type >= TIME_CONTROL_MAX) {
        ESP_LOGE(TAG, "Invalid time control type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Kopirovat konfiguraci
    memcpy(config, &TIME_CONTROLS[type], sizeof(time_control_config_t));
    
    return ESP_OK;
}

esp_err_t timer_get_json(char* buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid buffer parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    chess_timer_t timer_data;
    esp_err_t ret = timer_get_state(&timer_data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Vytvorit JSON
    int written = snprintf(buffer, buffer_size,
        "{"
        "\"white_time_ms\":%" PRIu32 ","
        "\"black_time_ms\":%" PRIu32 ","
        "\"timer_running\":%s,"
        "\"is_white_turn\":%s,"
        "\"game_paused\":%s,"
        "\"time_expired\":%s,"
        "\"config\":{"
            "\"type\":%d,"
            "\"name\":\"%s\","
            "\"description\":\"%s\","
            "\"initial_time_ms\":%" PRIu32 ","
            "\"increment_ms\":%" PRIu32 ","
            "\"is_fast\":%s"
        "},"
        "\"total_moves\":%" PRIu32 ","
        "\"avg_move_time_ms\":%" PRIu32 ","
        "\"warning_30s_shown\":%s,"
        "\"warning_10s_shown\":%s,"
        "\"warning_5s_shown\":%s"
        "}",
        timer_data.white_time_ms,
        timer_data.black_time_ms,
        timer_data.timer_running ? "true" : "false",
        timer_data.is_white_turn ? "true" : "false",
        timer_data.game_paused ? "true" : "false",
        timer_data.time_expired ? "true" : "false",
        timer_data.config.type,
        timer_data.config.name,
        timer_data.config.description,
        timer_data.config.initial_time_ms,
        timer_data.config.increment_ms,
        timer_data.config.is_fast ? "true" : "false",
        timer_data.total_moves,
        timer_data.avg_move_time_ms,
        timer_data.warning_30s_shown ? "true" : "false",
        timer_data.warning_10s_shown ? "true" : "false",
        timer_data.warning_5s_shown ? "true" : "false"
    );
    
    if (written >= buffer_size) {
        ESP_LOGE(TAG, "JSON buffer too small");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

uint32_t timer_get_available_controls_count(void)
{
    return TIME_CONTROL_MAX;
}

uint32_t timer_get_available_controls(time_control_config_t* controls, uint32_t max_count)
{
    if (controls == NULL || max_count == 0) {
        return 0;
    }
    
    uint32_t count = (max_count < TIME_CONTROL_MAX) ? max_count : TIME_CONTROL_MAX;
    
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&controls[i], &TIME_CONTROLS[i], sizeof(time_control_config_t));
    }
    
    return count;
}

esp_err_t timer_save_settings(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(TIMER_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Ulozit aktualni konfiguraci (max 15 chars for NVS key)
    ret = nvs_set_u8(nvs_handle, "tc_type", (uint8_t)current_timer.config.type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save time control type: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Pokud je custom time control, ulozit take custom values
    if (current_timer.config.type == TIME_CONTROL_CUSTOM) {
        uint32_t custom_minutes = current_timer.config.initial_time_ms / 60000;
        uint32_t custom_increment = current_timer.config.increment_ms / 1000;
        
        ret = nvs_set_u32(nvs_handle, "tc_min", custom_minutes);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save custom minutes: %s", esp_err_to_name(ret));
        }
        
        ret = nvs_set_u32(nvs_handle, "tc_inc", custom_increment);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save custom increment: %s", esp_err_to_name(ret));
        }
    } else {
        // Pro non-custom, smazat custom hodnoty z NVS
        nvs_erase_key(nvs_handle, "tc_min");
        nvs_erase_key(nvs_handle, "tc_inc");
    }
    
    // Commit zmeny
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Timer settings saved");
    
    return ret;
}

esp_err_t timer_load_settings(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(TIMER_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Nacist typ casove kontroly (max 15 chars for NVS key)
    uint8_t time_control_type = TIME_CONTROL_NONE;
    
    ret = nvs_get_u8(nvs_handle, "tc_type", &time_control_type);
    if (ret == ESP_OK) {
        // Nastavit casovou kontrolu
        if (time_control_type == TIME_CONTROL_CUSTOM) {
            // Pro custom, nacist custom hodnoty
            uint32_t custom_minutes = 10;
            uint32_t custom_increment = 0;
            
            esp_err_t ret_mins = nvs_get_u32(nvs_handle, "tc_min", &custom_minutes);
            esp_err_t ret_inc = nvs_get_u32(nvs_handle, "tc_inc", &custom_increment);
            
            if (ret_mins == ESP_OK && ret_inc == ESP_OK) {
                // Nastavit custom time control
                ret = timer_set_custom_time_control(custom_minutes, custom_increment);
                ESP_LOGI(TAG, "Timer settings loaded: Custom %" PRIu32 "+%" PRIu32, custom_minutes, custom_increment);
            } else {
                ESP_LOGW(TAG, "Custom time control type found but values missing, using defaults");
                // Pouzit default custom
                ret = timer_set_custom_time_control(10, 0);
            }
        } else {
            // Předdefinovaná časová kontrola
            time_control_config_t config;
            ret = timer_get_config_by_type((time_control_type_t)time_control_type, &config);
            if (ret == ESP_OK) {
                timer_set_time_control(&config);
                ESP_LOGI(TAG, "Timer settings loaded: %s", config.name);
            }
        }
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No timer settings found, using defaults");
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to load time control type: %s", esp_err_to_name(ret));
    }
    
    nvs_close(nvs_handle);
    
    return ret;
}

uint32_t timer_get_average_move_time(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return 0;
    }
    
    uint32_t avg_time = current_timer.avg_move_time_ms;
    
    timer_unlock();
    
    return avg_time;
}

uint32_t timer_get_total_moves(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return 0;
    }
    
    uint32_t total_moves = current_timer.total_moves;
    
    timer_unlock();
    
    return total_moves;
}

bool timer_is_active(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return false;
    }
    
    bool is_active = (current_timer.config.type != TIME_CONTROL_NONE);
    
    timer_unlock();
    
    return is_active;
}

time_control_type_t timer_get_current_type(void)
{
    esp_err_t ret = timer_lock();
    if (ret != ESP_OK) {
        return TIME_CONTROL_NONE;
    }
    
    time_control_type_t type = current_timer.config.type;
    
    timer_unlock();
    
    return type;
}

esp_err_t timer_set_custom_time_control(uint32_t minutes, uint32_t increment_seconds)
{
    if (minutes == 0 || minutes > 180) {
        ESP_LOGE(TAG, "Invalid minutes: %lu (must be 1-180)", minutes);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (increment_seconds > 60) {
        ESP_LOGE(TAG, "Invalid increment: %lu (must be 0-60)", increment_seconds);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Vytvorit vlastni konfiguraci
    time_control_config_t custom_config = TIME_CONTROLS[TIME_CONTROL_CUSTOM];
    custom_config.initial_time_ms = minutes * 60 * 1000; // Convert to milliseconds
    custom_config.increment_ms = increment_seconds * 1000; // Convert to milliseconds
    
    snprintf(custom_config.name, sizeof(custom_config.name), "Custom %" PRIu32 "+%" PRIu32, minutes, increment_seconds);
    snprintf(custom_config.description, sizeof(custom_config.description), 
             "Vlastní nastavení: %" PRIu32 " minut + %" PRIu32 " sekund increment", minutes, increment_seconds);
    custom_config.is_fast = (minutes < 10);
    
    ESP_LOGI(TAG, "Setting custom time control: %" PRIu32 " minutes + %" PRIu32 " seconds increment", 
             minutes, increment_seconds);
    
    return timer_set_time_control(&custom_config);
}

esp_err_t timer_get_config_by_index(uint32_t index, time_control_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (index >= TIME_CONTROL_MAX) {
        ESP_LOGE(TAG, "Invalid index: %lu (must be 0-%d)", index, TIME_CONTROL_MAX - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Kopirovat konfiguraci
    memcpy(config, &TIME_CONTROLS[index], sizeof(time_control_config_t));
    
    return ESP_OK;
}

esp_err_t timer_system_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing timer system...");
    
    // Ulozit nastaveni
    timer_save_settings();
    
    // Uvolnit mutex
    if (timer_mutex != NULL) {
        vSemaphoreDelete(timer_mutex);
        timer_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "Timer system deinitialized");
    
    return ESP_OK;
}
