/**
 * @file led_mapping.c  
 * @brief Správné mapování šachovnicové pozice na LED index (serpentine layout)
 */

#include "led_mapping.h"
#include "esp_log.h"
#include <ctype.h>

static const char *TAG = "LED_MAPPING";

/**
 * @brief Převod šachovnicové pozice na LED index (serpentine layout)
 * Layout: a1,b1,c1,d1,e1,f1,g1,h1, h2,g2,f2,e2,d2,c2,b2,a2, a3,b3,c3...
 * 
 * @param row Řádek (0-7, kde 0=rank 1, 7=rank 8)  
 * @param col Sloupec (0-7, kde 0=a, 7=h)
 * @return LED index (0-63)
 */
uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col)
{
    if (row >= 8 || col >= 8) {
        ESP_LOGE(TAG, "Invalid chess position: row=%d, col=%d", row, col);
        return 0;
    }
    
    // Oprava: Šachovnice je otočená po Y ose (a1 je na h1 pozici)
    // Takže col=0 (a) musí být mapováno na LED pozici 7 (h)
    uint8_t mapped_col = 7 - col;
    
    if (row % 2 == 0) {
        // Sudé řádky (0,2,4,6): normální pořadí a->h (ale s otočenými sloupci)
        return row * 8 + mapped_col;
    } else {
        // Liché řádky (1,3,5,7): obrácené pořadí h->a (ale s otočenými sloupci)
        return row * 8 + (7 - mapped_col);
    }
}

/**
 * @brief Převod LED indexu na šachovnicovou pozici (serpentine layout)
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
        // Sudé řádky: normální pořadí (ale s otočenými sloupci)
        *col = 7 - pos_in_row;
    } else {
        // Liché řádky: obrácené pořadí (ale s otočenými sloupci)
        *col = pos_in_row;
    }
}

/**
 * @brief Převod šachové notace na LED index
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
 * @brief Test LED mapování
 */
void test_led_mapping(void)
{
    ESP_LOGI(TAG, "=== LED MAPPING TEST ===");
    
    // Test známých pozic
    struct {
        const char* notation;
        uint8_t expected_row;
        uint8_t expected_col;
        uint8_t expected_led;
    } test_cases[] = {
        {"a1", 0, 0, 0},   // První LED
        {"h1", 0, 7, 7},   // Osmá LED
        {"h2", 1, 7, 8},   // Devátá LED (začátek druhého řádku)
        {"a2", 1, 0, 15},  // Šestnáctá LED (konec druhého řádku)
        {"a3", 2, 0, 16},  // Sedmnáctá LED
        {"h8", 7, 7, 56},  // První LED osmého řádku
        {"a8", 7, 0, 63}   // Poslední LED
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
