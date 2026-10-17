#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32c0xx_ll_adc.h"
#include "stm32c0xx_ll_bus.h"
#include "stm32c0xx_ll_cortex.h"
#include "stm32c0xx_ll_gpio.h"
#include "stm32c0xx_ll_i2c.h"
#include "stm32c0xx_ll_pwr.h"
#include "stm32c0xx_ll_rcc.h"
#include "stm32c0xx_ll_system.h"
#include "stm32c0xx_ll_utils.h"

#include "hall_i2c_spec.h"

#ifndef HALL_I2C_OWNADDR7BIT
#define HALL_I2C_OWNADDR7BIT 0x30u
#endif

/**
 * Eight ASCII bytes in flash, then the 7-bit address. The ESP bootloader
 * finds "HALLADDR" in the image and writes 0x30+segment before programming
 * that chip. Identical boards stay one binary.
 */
typedef struct __attribute__((packed)) {
  char magic[8];
  uint8_t addr7;
  uint8_t reserved[3];
} hall_i2c_addr_slot_t;

extern const hall_i2c_addr_slot_t g_hall_i2c_addr_slot;

/** 7-bit slave address, read from the slot so a patched image is honored. */
uint8_t hall_i2c_own_addr7(void);

/** OA1[7:1] value in STM32 LL format (7bit << 1). */
#define HALL_I2C_ADDR_MATCH ((uint32_t)((uint32_t)hall_i2c_own_addr7() << 1))

void SystemClock_Config(void);
void Hall_RefreshPayload(void);

extern volatile uint8_t g_hall_pointer_reg;
extern volatile unsigned g_hall_tx_idx;
extern volatile uint8_t g_hall_payload_raw[HALL_I2C_PAYLOAD_BYTES];
extern volatile uint8_t g_hall_payload_ver[HALL_I2C_PAYLOAD_BYTES];
extern volatile const uint8_t *volatile g_hall_active_tx;

#ifdef __cplusplus
}
#endif
