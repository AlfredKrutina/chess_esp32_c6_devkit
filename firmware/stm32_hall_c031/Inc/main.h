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

/** Hodnota OA1[7:1] ve formátu STM32 LL (7bit << 1). */
#define HALL_I2C_ADDR_MATCH ((uint32_t)((uint32_t)HALL_I2C_OWNADDR7BIT << 1))

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
