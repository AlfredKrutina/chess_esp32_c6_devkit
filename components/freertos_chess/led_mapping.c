/**
 * @file led_mapping.c  
 * @brief Correct mapping of the box position to the LED index (serpentine layout)
 * 
 * This module contains functions for converting drawer positions to LED indexes.
 * Uses a serpentine layout for optimal LED distribution on the box.
 * 
 * @details
 * Serpentine layout LED signs are laid out in a serpentine pattern:
 * a1,b1,c1,d1,e1,f1,g1,h1,h2,g2,f2,e2,d2,c2,b2,a2,a3,b3,c3...
 * This layout enables easy conversion of positions to LED indexes.
 */

#include "led_mapping.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "LED_MAPPING";

/**
 * @brief Conversion of the box position to LED index (serpentine layout)
 * 
 * Layout: a1,b1,c1,d1,e1,f1,g1,h1,h2,g2,f2,e2,d2,c2,b2,a2,a3,b3,c3...
 * The box is rotated along the Y axis (a1 is at h1 position).
 * 
 * @param row Row (0-7, where 0=rank 1, 7=rank 8)  
 * @param col Column (0-7 where 0=a, 7=h)
 * @return LED index (0-63)
 */
uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col)
{
    if (row >= 8 || col >= 8) {
        ESP_LOGE(TAG, "Invalid chess position: row=%d, col=%d", row, col);
        return 0;
    }
    
    // The box is rotated along the Y axis (a1 is at h1 position)
    // So col=0 (a) must be mapped to LED position 7 (h)
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
 * @brief Conversion of the LED index to a box position (serpentine layout)
 * 
 * Reverse operation to chess_pos_to_led_index(). Calculation from the LED index
 * the position of the row and column on the drawer.
 * 
 * @param led_index LED index (0-63)
 * @param[out] row Pointer to row (0-7)
 * @param[out] col Pointer to column (0-7)
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
 * @brief Conversion of Sacha notation to LED index
 * 
 * Translates Sacha notation (e.g. "e2", "a1") to LED index.
 * Notation must be in [a-h][1-8] format.
 * 
 * @param notation Sach's notation (eg "e2")
 * @return LED index (0-63) or 0 on error
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
 * @brief LED mapping test
 * 
 * Tests the LED mapping functionality at known positions.
 * Checks notation->LED and LED->position conversions.
 */
void test_led_mapping(void)
{
    ESP_LOGI(TAG, "=== LED MAPPING TEST ===");
    
    // Test of known positions
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
