/**
 * @file stm32_i2c_bl.h
 * @brief STM32 ROM I2C bootloader for the four Hall segments (AN4221).
 *
 * One segment is out of reset at a time. When the bus is shared with Hall,
 * hall_i2c_squares_init() must run first. Builds with the bootloader Kconfig
 * off return ESP_ERR_NOT_SUPPORTED from the esp_err_t entry points.
 */
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
 * @brief Configure NRST/BOOT0 and, if the bus is not shared, the I2C driver.
 * @return ESP_OK if already inited or GPIO/I2C setup succeeded,
 *         ESP_ERR_NOT_SUPPORTED when the bootloader is compiled out,
 *         otherwise the I2C error.
 */
esp_err_t stm32_i2c_bl_init(void);

/**
 * @brief Hold every STM in reset except one, then release that NRST.
 * @param segment_0_to_3 Segment 0…3.
 * @param boot0_enter_bootloader true: BOOT0 high before release (ROM bootloader).
 *        false: run the application.
 * @return ESP_OK, ESP_ERR_INVALID_ARG if the segment is not 0…3,
 *         ESP_ERR_NOT_SUPPORTED when compiled out, or an error from init.
 */
esp_err_t stm32_i2c_bl_select_target(uint8_t segment_0_to_3, bool boot0_enter_bootloader);

/**
 * @brief BOOT0 low (if connected) and NRST high so every segment runs its app.
 * @return ESP_OK, or ESP_ERR_NOT_SUPPORTED when the bootloader is compiled out.
 */
esp_err_t stm32_i2c_bl_release_all_run_app(void);

/**
 * @brief ROM GET_ID (command 0x02) on one segment.
 * @param segment_0_to_3 Segment 0…3.
 * @param boot0_enter true to enter the ROM bootloader before the command.
 * @param[out] pid_out Product ID. Required.
 * @return ESP_OK and writes *pid_out, ESP_ERR_INVALID_ARG if pid_out is NULL,
 *         ESP_ERR_NOT_SUPPORTED when compiled out, otherwise the transaction error.
 */
esp_err_t stm32_i2c_bl_cmd_get_id(uint8_t segment_0_to_3, bool boot0_enter, uint16_t *pid_out);

/**
 * @brief Bulk-erase flash (special erase 0xFFFF) when the MCU supports it.
 * @param segment_0_to_3 Segment 0…3.
 * @param boot0_enter true to enter the ROM bootloader first.
 * @return ESP_OK, a bootloader NACK/timeout, or ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t stm32_i2c_bl_erase_all(uint8_t segment_0_to_3, bool boot0_enter);

/**
 * @brief ROM WRITE MEMORY, at most 256 bytes per call.
 * @param segment_0_to_3 Segment 0…3.
 * @param boot0_enter true to enter the ROM bootloader first.
 * @param addr Destination address (flash start is typically 0x08000000).
 * @param data Bytes to write. Required.
 * @param len Length, 1…256.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_NOT_SUPPORTED, or the transaction error.
 */
esp_err_t stm32_i2c_bl_write_memory(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t addr, const uint8_t *data, size_t len);

/**
 * @brief ROM GO — jump to the user vector.
 * @param segment_0_to_3 Segment 0…3.
 * @param boot0_enter true to enter the ROM bootloader first.
 * @param addr Vector address, typically 0x08000000.
 * @return ESP_OK if the MCU accepted GO, otherwise the transaction error
 *         or ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t stm32_i2c_bl_go(uint8_t segment_0_to_3, bool boot0_enter, uint32_t addr);

/**
 * @brief Erase the chip (when possible) and write a RAM image at flash_base.
 *
 * Refuses an image larger than the STM32C031 32 KiB flash, or a tighter
 * CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES cap.
 * @param segment_0_to_3 Segment 0…3.
 * @param boot0_enter true to enter the ROM bootloader first.
 * @param flash_base Usually 0x08000000.
 * @param bin Image bytes. Required.
 * @param bin_len Image length in bytes. Must be non-zero and within the cap.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_SIZE, ESP_ERR_NOT_SUPPORTED,
 *         or the erase/write error.
 */
esp_err_t stm32_i2c_bl_flash_binary(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t flash_base, const uint8_t *bin,
                                    size_t bin_len);

/**
 * @brief If auto-flash-on-boot is enabled, program each STM from its partition.
 *
 * Call after shared I2C init (for example right after hall_i2c_squares_init).
 * No-op when that Kconfig is off. Does not return a status; result is logged.
 */
void stm32_i2c_bl_maybe_auto_flash_on_boot(void);

/**
 * @brief Create the binary semaphore main waits on before squares_task starts.
 *
 * No-op when auto-flash-on-boot is compiled out, or if the semaphore exists.
 */
void stm32_i2c_bl_boot_flash_sync_prepare(void);

/**
 * @brief Block until squares_task signals that auto-flash finished.
 * @param timeout_ticks FreeRTOS timeout. On expiry a warning is logged and
 *        the caller continues. No-op when auto-flash sync is compiled out.
 */
void stm32_i2c_bl_boot_flash_sync_wait(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
