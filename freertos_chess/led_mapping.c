/**
 * @file led_mapping.c  
 * @brief Spravne mapovani sachovnicove pozice na LED index (serpentine layout)
 * 
 * Tento modul obsahuje funkce pro prevod sachovnicovych pozic na LED indexy.
 * Pouziva serpentine layout pro optimalni rozlozeni LED na sachovnici.
 * 
 * @details
 * Serpentine layout znamena ze LED jsou rozlozeny v hadovitem vzoru:
 * a1,b1,c1,d1,e1,f1,g1,h1, h2,g2,f2,e2,d2,c2,b2,a2, a3,b3,c3...
 * Toto rozlozeni umoznuje jednoduchy prevod pozic na LED indexy.
 */

#include "led_mapping.h"
#include "esp_log.h"
#include <ctype.h>

static const char *TAG = "LED_MAPPING";

/**
 * @brief Prevod sachovnicove pozice na LED index (serpentine layout)
 * 
 * Layout: a1,b1,c1,d1,e1,f1,g1,h1, h2,g2,f2,e2,d2,c2,b2,a2, a3,b3,c3...
 * Sachovnice je otocena po Y ose (a1 je na h1 pozici).
 * 
 * @param row Radek (0-7, kde 0=rank 1, 7=rank 8)  
 * @param col Sloupec (0-7, kde 0=a, 7=h)
 * @return LED index (0-63)
 */
uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col)
{
    if (row >= 8 || col >= 8) {
        ESP_LOGE(TAG, "Invalid chess position: row=%d, col=%d", row, col);
        return 0;
    }
    
    // Sachovnice je otocena po Y ose (a1 je na h1 pozici)
    // Takze col=0 (a) musi byt mapovano na LED pozici 7 (h)
    uint8_t mapped_col = 7 - col;
    
    if (row % 2 == 0) {
        // Sude radky (0,2,4,6): normalni poradi a->h (ale s otocenymi sloupci)
        return row * 8 + mapped_col;
    } else {
        // Liche radky (1,3,5,7): obracene poradi h->a (ale s otocenymi sloupci)
        return row * 8 + (7 - mapped_col);
    }
}

/**
 * @brief Prevod LED indexu na sachovnicovou pozici (serpentine layout)
 * 
 * Obracenou operaci k chess_pos_to_led_index(). Ze LED indexu vypocita
 * pozici radku a sloupce na sachovnici.
 * 
 * @param led_index LED index (0-63)
 * @param[out] row Ukazatel na radek (0-7)
 * @param[out] col Ukazatel na sloupec (0-7)
 */
void led_index_to_chess_pos(uint8_t led_index, uint8_t* row, uint8_t* col)
{
    if (led_index >= 64 || row == NULL || col == NULL) {
        ESP_LOGE(TAG, "Invalid LED index: %d", led_index);
        if (row) *row = 0;
        if (col) *col = 0;
        return;
    }
    
    *row = led_index / 8;
    uint8_t pos_in_row = led_index % 8;
    
    if (*row % 2 == 0) {
        // Sude radky: normalni poradi (ale s otocenymi sloupci)
        *col = 7 - pos_in_row;
    } else {
        // Liche radky: obracene poradi (ale s otocenymi sloupci)
        *col = pos_in_row;
    }
}

/**
 * @brief Prevod sachove notace na LED index
 * 
 * Preklada sachovou notaci (napr. "e2", "a1") na LED index.
 * Notace musi byt ve formatu [a-h][1-8].
 * 
 * @param notation Sachova notace (napr. "e2")
 * @return LED index (0-63) nebo 0 pri chybe
 */
uint8_t chess_notation_to_led_index(const char* notation)
{
    if (!notation || strlen(notation) < 2) {
        ESP_LOGE(TAG, "Invalid notation: %s", notation ? notation : "NULL");
        return 0;
    }
    
    char file = tolower(notation[0]);
    char rank = notation[1];
    
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        ESP_LOGE(TAG, "Invalid notation: %s", notation);
        return 0;
    }
    
    uint8_t col = file - 'a';  // a=0, b=1, ..., h=7
    uint8_t row = rank - '1';  // 1=0, 2=1, ..., 8=7
    
    return chess_pos_to_led_index(row, col);
}

/**
 * @brief Test LED mapovani
 * 
 * Testuje funkcnost LED mapovani na znamych pozicich.
 * Kontroluje prevody notace->LED a LED->pozice.
 */
void test_led_mapping(void)
{
    ESP_LOGI(TAG, "=== LED MAPPING TEST ===");
    
    // Test znamych pozic
    struct {
        const char* notation;
        uint8_t expected_row;
        uint8_t expected_col;
        uint8_t expected_led;
    } test_cases[] = {
        {"a1", 0, 0, 0},   // Prvni LED
        {"h1", 0, 7, 7},   // Osma LED
        {"h2", 1, 7, 8},   // Devata LED (zacatek druheho radku)
        {"a2", 1, 0, 15},  // Sestnacta LED (konec druheho radku)
        {"a3", 2, 0, 16},  // Sedmnacta LED
        {"h8", 7, 7, 56},  // Prvni LED osmeho radku
        {"a8", 7, 0, 63}   // Posledni LED
    };
    
    for (int i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        uint8_t led_idx = chess_notation_to_led_index(test_cases[i].notation);
        uint8_t row, col;
        led_index_to_chess_pos(led_idx, &row, &col);
        
        ESP_LOGI(TAG, "%s -> LED %d (row=%d,col=%d) %s", 
                test_cases[i].notation, led_idx, row, col,
                (led_idx == test_cases[i].expected_led && 
                 row == test_cases[i].expected_row && 
                 col == test_cases[i].expected_col) ? "✓" : "✗");
    }
}
