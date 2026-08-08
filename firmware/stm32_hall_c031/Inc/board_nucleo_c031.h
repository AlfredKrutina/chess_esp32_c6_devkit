/**
 * NUCLEO-C031C6 (MB1717): USER button B1 and MCU-driven LED.
 *
 * LD1 near micro-USB is the ST-LINK COM indicator — STM32 firmware cannot light it.
 * The green user LED by the MCU is LD4 on PA5 (active high).
 */
#pragma once

#define BOARD_NUCLEO_LD4_GPIO_PORT GPIOA
#define BOARD_NUCLEO_LD4_PIN LL_GPIO_PIN_5

#define BOARD_NUCLEO_USER_GPIO_PORT GPIOC
#define BOARD_NUCLEO_USER_PIN LL_GPIO_PIN_13
