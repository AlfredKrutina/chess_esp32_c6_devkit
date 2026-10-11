/**
 * NUCLEO-C031C6 (MB1717): USER button B1 (PC13), green LD4 (PA5) and D9 (PC7).
 *
 * LD1 near micro-USB is the ST-LINK COM indicator — STM32 firmware cannot light it.
 * LD3 is the power LED — not a GPIO.
 * The green user LED by the MCU is LD4 on PA5 (active high).
 * D9 on Arduino header is PC7 (active high) — used as the second mock LED.
 */
#pragma once

#define BOARD_NUCLEO_LD4_GPIO_PORT GPIOA
#define BOARD_NUCLEO_LD4_PIN LL_GPIO_PIN_5

#define BOARD_NUCLEO_D9_GPIO_PORT GPIOC
#define BOARD_NUCLEO_D9_PIN LL_GPIO_PIN_7

#define BOARD_NUCLEO_USER_GPIO_PORT GPIOC
#define BOARD_NUCLEO_USER_PIN LL_GPIO_PIN_13
