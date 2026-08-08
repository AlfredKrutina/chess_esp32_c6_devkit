#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialization: GPIO NRST/BOOT0, or I2C driver (if you do not share the bus with Hall).
 * When sharing with Hall, hall_i2c_matrix_init() must be called before.
 */
esp_err_t stm32_i2c_bl_init(void);

/** Only one STM has NRST released (HIGH), others held in reset (LOW). boot0_enter=1 sets BOOT0 HIGH before releasing. */
esp_err_t stm32_i2c_bl_select_target(uint8_t segment_0_to_3, bool boot0_enter_bootloader);

/** BOOT0 LOW (if connected), NRST HIGH (+ pulse) — application running from flash. */
esp_err_t stm32_i2c_bl_release_all_run_app(void);

esp_err_t stm32_i2c_bl_cmd_get_id(uint8_t segment_0_to_3, bool boot0_enter, uint16_t *pid_out);

/** Bulk erase flash (special erase 0xFFFF) if MCU supports — see AN4221. */
esp_err_t stm32_i2c_bl_erase_all(uint8_t segment_0_to_3, bool boot0_enter);

/**
 * Write to flash/RAM according to ROM bootloader (max 256 B per call).
 * addr: eg 0x08000000 for start flash.
 */
esp_err_t stm32_i2c_bl_write_memory(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t addr, const uint8_t *data, size_t len);

/** GO command — jump to user vector (addr typically 0x08000000). */
esp_err_t stm32_i2c_bl_go(uint8_t segment_0_to_3, bool boot0_enter, uint32_t addr);

/**
 * Helper routine: erases the entire chip (if possible) and loads the binary from flash_base.
 * flash_base usually 0x08000000.
 */
esp_err_t stm32_i2c_bl_flash_binary(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t flash_base, const uint8_t *bin,
                                    size_t bin_len);

/**
 * One time after boot: if CHESS_STM32_BL_AUTO_FLASH_ON_BOOT, flash STM from partition.
 * Call after shared I2C initialization (e.g. right after hall_i2c_matrix_init).
 */
void stm32_i2c_bl_maybe_auto_flash_on_boot(void);

/**
 * Main thread: call before xTaskCreate(matrix_task) if auto-flash is enabled on boot.
 * Makes boot_flash_sync_wait() wait for maybe_auto_flash in matrix_task to complete.
 */
void stm32_i2c_bl_boot_flash_sync_prepare(void);

/** Main thread: wait for signal_done from matrix_task (timeout in FreeRTOS ticks). */
void stm32_i2c_bl_boot_flash_sync_wait(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
