/**
 * @file shared_buffer_pool.h
 * @brief ESP32-C6 Chess System - Hlavicka Shared Buffer Poolu
 * 
 * Centralizovany buffer pool pro nahrazeni malloc/free volani a eliminaci
 * problemuS fragmentace heapu v prikazech jako led_board a endgame_white.
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-01-27
 * 
 * @details
 * Shared Buffer Pool poskytuje predalokavane buffery misto dynamicke alokace.
 * Eliminuje fragmentaci heapu a zlepsuje performance alokace pameti.
 * Obsahuje 4 buffery o velikosti 2KB kazdy (celkem 8KB).
 * 
 * Vyh ody:
 * - Eliminace fragmentace heapu
 * - Rychlejsi alokace (bez malloc overhead)
 * - Prevence memory leaku (automaticke sledovani)
 * - Detekce buffer leaku
 * 
 * Priklad pouziti:
 * @code
 * char* buffer = get_shared_buffer(1536);
 * if (buffer != NULL) {
 *     sprintf(buffer, "Hello World");
 *     printf("%s\n", buffer);
 *     release_shared_buffer(buffer);
 * }
 * @endcode
 */

#ifndef SHARED_BUFFER_POOL_H
#define SHARED_BUFFER_POOL_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MAKRA PRO SNADNE POUZITI
// ============================================================================

/**
 * @brief Ziskej sdileny buffer s automatickym sledovanim souboru/radky
 * 
 * @param size Minimalni pozadovana velikost bufferu v bajtech
 * @return Ukazatel na buffer nebo NULL pri selhani
 * 
 * @note Pouziva makra __FILE__ a __LINE__ pro debug sledovani
 */
#define get_shared_buffer(size) get_shared_buffer_debug(size, __FILE__, __LINE__)

// ============================================================================
// STRUKTURY
// ============================================================================

/**
 * @brief Struktura statistik buffer poolu
 * 
 * Obsahuje informace o pouziti buffer poolu (velikost, pouziti, selhani).
 */
typedef struct {
    uint32_t pool_size;          ///< Celkovy pocet bufferu v poolu
    uint32_t buffer_size;        ///< Velikost jednoho bufferu v bajtech
    uint32_t current_usage;      ///< Aktualne alokovane buffery
    uint32_t peak_usage;         ///< Maximalni dosazen e pouziti
    uint32_t total_allocations;  ///< Celkovy pocet alokaci
    uint32_t total_releases;     ///< Celkovy pocet uvolneni
    uint32_t allocation_failures;///< Pocet selhanych alokaci
} buffer_pool_stats_t;

// ============================================================================
// INICIALIZACNI FUNKCE
// ============================================================================

/**
 * @brief Inicializuje shared buffer pool
 * 
 * Vytvori mutex pro thread-safe pristup a inicializuje vsechny
 * buffery jako volne. Resetuje statistiky.
 * 
 * @return ESP_OK pri uspechu, ESP_ERR_NO_MEM pri selhani vytvoreni mutexu
 */
esp_err_t buffer_pool_init(void);

/**
 * @brief Deinicializuje buffer pool a uvolni prostredky
 * 
 * Kontroluje zda nejsou neuvolnene buffery (leak detection)
 * a uvolni mutex.
 */
void buffer_pool_deinit(void);

// ============================================================================
// FUNKCE PRO ALOKACI/UVOLNENI BUFFERU
// ============================================================================

/**
 * @brief Ziskej sdileny buffer z poolu (interni funkce)
 * 
 * Hleda volny buffer v poolu a oznaci ho jako pouzivany.
 * Loguje informace o alokaci pro debug ucely.
 * 
 * @param min_size Minimalni pozadovana velikost bufferu v bajtech
 * @param file Nazev zdrojoveho souboru (pro debug)
 * @param line Cislo radky (pro debug)
 * @return Ukazatel na buffer nebo NULL pokud alokace selhala
 * 
 * @note Nepouzivejte primo - pouzijte makro get_shared_buffer()
 */
char* get_shared_buffer_debug(size_t min_size, const char* file, int line);

/**
 * @brief Uvolni sdileny buffer zpet do poolu
 * 
 * Oznaci buffer jako volny a muze byt pouzit jinymi tasky.
 * 
 * @param buffer Ukazatel na buffer k uvolneni
 * @return ESP_OK pri uspechu, ESP_ERR_INVALID_ARG pokud buffer je neplatny
 */
esp_err_t release_shared_buffer(char* buffer);

// ============================================================================
// UTILITY A STATUS FUNKCE
// ============================================================================

/**
 * @brief Vypis detailni status buffer poolu do konzole
 * 
 * Zobrazuje vsechny buffery, jejich stav (volny/pouzivany),
 * vlastnika a statistiky.
 */
void buffer_pool_print_status(void);

/**
 * @brief Ziskej statistiky buffer poolu
 * 
 * @return Struktura s aktualnimi statistikami
 */
buffer_pool_stats_t buffer_pool_get_stats(void);

/**
 * @brief Overi zda je buffer pool ve zdravem stavu
 * 
 * Kontroluje zda nejsou preteceni, leaky nebo jine problemy.
 * 
 * @return true pokud je pool zdravy, false pokud jsou detekovany problemy
 */
bool buffer_pool_is_healthy(void);

/**
 * @brief Detekuj potencialni buffer leaky
 * 
 * Loguje varovani pro buffery drzene dele nez je ocekavano.
 * Pomaha identifikovat zapomnute uvolneni bufferu.
 */
void buffer_pool_detect_leaks(void);

// ============================================================================
// POMOCNA MAKRA
// ============================================================================

/**
 * @brief Bezpecna alokace bufferu s kontrolou velikosti
 * 
 * Pouziti: SAFE_GET_BUFFER(ptr, size, cleanup_label)
 * 
 * @par Priklad:
 * @code
 * char* buffer;
 * SAFE_GET_BUFFER(buffer, 1536, cleanup);
 * // ... pouzij buffer ...
 * cleanup:
 *     release_shared_buffer(buffer);
 * @endcode
 */
#define SAFE_GET_BUFFER(ptr, size, cleanup_label) do { \
    ptr = get_shared_buffer(size); \
    if (ptr == NULL) { \
        ESP_LOGE("BUFFER", "Failed to allocate buffer of size %zu", size); \
        goto cleanup_label; \
    } \
} while(0)

/**
 * @brief Bezpecne uvolneni bufferu s kontrolou NULL
 * 
 * Pouziti: SAFE_RELEASE_BUFFER(ptr)
 */
#define SAFE_RELEASE_BUFFER(ptr) do { \
    if (ptr != NULL) { \
        release_shared_buffer(ptr); \
        ptr = NULL; \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // SHARED_BUFFER_POOL_H
