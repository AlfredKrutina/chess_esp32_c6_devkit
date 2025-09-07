#ifndef LED_MAPPING_H
#define LED_MAPPING_H

#include <stdint.h>

uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col);
void led_index_to_chess_pos(uint8_t led_index, uint8_t* row, uint8_t* col);
uint8_t chess_notation_to_led_index(const char* notation);
void test_led_mapping(void);

#endif // LED_MAPPING_H
