/**
 * NUCLEO-C031C6 (MB1717): USER tlačítko B1 a LED řízená z MCU.
 *
 * LD1 u micro-USB je COM indikátor ST-LINK — z firmware STM32 nejde rozsvítit.
 * Zelená uživatelská LED u MCU je LD4 na PA5 (active high).
 */
#pragma once

#define BOARD_NUCLEO_LD4_GPIO_PORT GPIOA
#define BOARD_NUCLEO_LD4_PIN LL_GPIO_PIN_5

#define BOARD_NUCLEO_USER_GPIO_PORT GPIOC
#define BOARD_NUCLEO_USER_PIN LL_GPIO_PIN_13
