/**
 * @file led_mapping.c
 * @brief Square to SK6812 index for the quadrant chain.
 *
 * Four identical 4x4 boards. Chain starts at e1. Each next board is
 * rotated 90 degrees counter-clockwise from the previous one.
 *   0-15  e1-h4  0 deg    e1..e4, f4..f1, g1..g4, h4..h1
 *  16-31  e5-h8  90 CCW
 *  32-47  a5-d8  180
 *  48-63  a1-d4  270 CCW (quadrant not fitted on the bench)
 */

#include "led_mapping.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "LED_MAPPING";

/* pos_to_led[rank 0..7][file a..h] */
static const uint8_t pos_to_led[8][8] = {
    {63, 62, 61, 60, 0, 7, 8, 15},
    {56, 57, 58, 59, 1, 6, 9, 14},
    {55, 54, 53, 52, 2, 5, 10, 13},
    {48, 49, 50, 51, 3, 4, 11, 12},
    {44, 43, 36, 35, 19, 18, 17, 16},
    {45, 42, 37, 34, 20, 21, 22, 23},
    {46, 41, 38, 33, 27, 26, 25, 24},
    {47, 40, 39, 32, 28, 29, 30, 31},
};

static const uint8_t led_row[64] = {
    0, 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 0,
    4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
    7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7,
    3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0,
};

static const uint8_t led_col[64] = {
    4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
    7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7,
    3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 0,
};

/**
 * @brief Square (row, col) to LED index on the quadrant chain.
 *
 * @param row Rank index (0 = rank 1, 7 = rank 8)
 * @param col File index (0 = a, 7 = h)
 * @return LED index 0-63
 */
uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col)
{
    if (row >= 8 || col >= 8) {
        ESP_LOGE(TAG, "Invalid chess position: row=%d, col=%d", row, col);
        return 0;
    }

    return pos_to_led[row][col];
}

/**
 * @brief LED index to square (row, col).
 *
 * @param led_index LED index (0-63)
 * @param[out] row Rank index (0 = rank 1)
 * @param[out] col File index (0 = a)
 */
void led_index_to_chess_pos(uint8_t led_index, uint8_t* row, uint8_t* col)
{
    if (led_index >= 64 || row == NULL || col == NULL) {
        ESP_LOGE(TAG, "Invalid LED index: %d", led_index);
        if (row) *row = 0;
        if (col) *col = 0;
        return;
    }

    *row = led_row[led_index];
    *col = led_col[led_index];
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
        {"e1", 0, 4, 0},
        {"e4", 3, 4, 3},
        {"f4", 3, 5, 4},
        {"h1", 0, 7, 15},
        {"e5", 4, 4, 19},
        {"e8", 7, 4, 28},
        {"h8", 7, 7, 31},
        {"d5", 4, 3, 35},
        {"a5", 4, 0, 44},
        {"a8", 7, 0, 47},
        {"a1", 0, 0, 63},
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
