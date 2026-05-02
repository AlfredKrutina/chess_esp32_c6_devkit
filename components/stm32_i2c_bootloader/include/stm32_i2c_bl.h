#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializace: GPIO NRST/BOOT0, případně I2C driver (pokud nesdílíš sběrnici s Hall).
 * Při sdílení s Hall musí být dříve zavolán hall_i2c_matrix_init().
 */
esp_err_t stm32_i2c_bl_init(void);

/** Jen jeden STM má NRST uvolněný (HIGH), ostatní drženy v resetu (LOW). boot0_enter=1 nastaví BOOT0 HIGH před uvolněním. */
esp_err_t stm32_i2c_bl_select_target(uint8_t segment_0_to_3, bool boot0_enter_bootloader);

/** BOOT0 LOW (pokud zapojeno), všechny NRST HIGH — běh aplikace (základní stav). */
esp_err_t stm32_i2c_bl_release_all_run_app(void);

esp_err_t stm32_i2c_bl_cmd_get_id(uint8_t segment_0_to_3, bool boot0_enter, uint16_t *pid_out);

/** Hromadné smazání flash (special erase 0xFFFF), pokud MCU podporuje — viz AN4221. */
esp_err_t stm32_i2c_bl_erase_all(uint8_t segment_0_to_3, bool boot0_enter);

/**
 * Zápis do flash/RAM podle ROM bootloaderu (max 256 B na volání).
 * addr: např. 0x08000000 pro začátek flash.
 */
esp_err_t stm32_i2c_bl_write_memory(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t addr, const uint8_t *data, size_t len);

/** GO příkaz — skok na uživatelský vektor (addr typicky 0x08000000). */
esp_err_t stm32_i2c_bl_go(uint8_t segment_0_to_3, bool boot0_enter, uint32_t addr);

/**
 * Pomocná rutina: smaže celý čip (pokud lze) a nahraje binárku od flash_base.
 * flash_base obvykle 0x08000000.
 */
esp_err_t stm32_i2c_bl_flash_binary(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t flash_base, const uint8_t *bin,
                                    size_t bin_len);

/**
 * Jednorázově po bootu: pokud je CHESS_STM32_BL_AUTO_FLASH_ON_BOOT, nahraje STM z oddílu.
 * Volat po inicializaci sdíleného I2C (např. hned po hall_i2c_matrix_init).
 */
void stm32_i2c_bl_maybe_auto_flash_on_boot(void);

#ifdef __cplusplus
}
#endif
