/**
 * Zapojení podle KiCad „matrix“ (STM32C031Fx TSSOP20, Czechmate).
 *
 * Ověřeno proti DS STM32C031 — Figure 3 TSSOP20: pin 1 = PB7 (I2C1_SDA),
 * pin 20 = PB6 (I2C1_SCL). ROM bootloader I2C1 na PB6/PB7 — AN2606 Table 12 (C031): 7-bit 0x63.
 *
 * CD74HC4067: E = HIGH vypne všechny spínače; E = LOW povolí adresu na S0–S3.
 */
#pragma once

#include "stm32c0xx_ll_gpio.h"

/* I2C1 → ESP (slave) */
#define MATRIX_I2C_GPIO_PORT GPIOB
#define MATRIX_I2C_SCL_PIN LL_GPIO_PIN_6
#define MATRIX_I2C_SDA_PIN LL_GPIO_PIN_7
#define MATRIX_I2C_AF LL_GPIO_AF_6

/* ADC ← výstup COM multiplexoru */
#define MATRIX_ADC_COM1_PORT GPIOA
#define MATRIX_ADC_COM1_PIN LL_GPIO_PIN_0 /* U37 → ADC_IN0 */
#define MATRIX_ADC_COM2_PIN LL_GPIO_PIN_1 /* U38 → ADC_IN1 */

/* Adresní sběrnice muxů (společná) */
#define MATRIX_MUX_S0_PIN LL_GPIO_PIN_2
#define MATRIX_MUX_S1_PIN LL_GPIO_PIN_3
#define MATRIX_MUX_S2_PIN LL_GPIO_PIN_4
#define MATRIX_MUX_S3_PIN LL_GPIO_PIN_5
#define MATRIX_MUX_ADDR_PINS                                                         \
  (MATRIX_MUX_S0_PIN | MATRIX_MUX_S1_PIN | MATRIX_MUX_S2_PIN | MATRIX_MUX_S3_PIN)

/* Enable jednotlivých HC4067 (každý vlastní E) */
#define MATRIX_MUX_E_U37_PIN LL_GPIO_PIN_6
#define MATRIX_MUX_E_U38_PIN LL_GPIO_PIN_7
