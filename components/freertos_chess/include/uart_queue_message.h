/**
 * @file uart_queue_message.h
 * @brief Spolecny typ zpravy pro UART vystupni frontu (jedna pravda pro sizeof).
 *
 * freertos_chess.c vytvari frontu s item size = sizeof(uart_message_t).
 * uart_task.c posila/prijima stejny typ — nesmi se rozchazet s magickymi cisly.
 */
#pragma once

#include <stdbool.h>

typedef enum {
  UART_MSG_NORMAL = 0,
  UART_MSG_ERROR = 1,
  UART_MSG_WARNING = 2,
  UART_MSG_SUCCESS = 3,
  UART_MSG_INFO = 4,
  UART_MSG_DEBUG = 5
} uart_msg_type_t;

/** Velikost textu ve fronte (kompatibilni s drivejsi optimalizaci ~128 B/polozka). */
#define UART_MESSAGE_TEXT_MAX 128

typedef struct {
  uart_msg_type_t type;
  bool add_newline;
  char message[UART_MESSAGE_TEXT_MAX];
} uart_message_t;
