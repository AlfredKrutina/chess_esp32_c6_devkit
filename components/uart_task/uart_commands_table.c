/**
 * @file uart_commands_table.c
 * @brief UART command name/handler registry.
 *
 * This table is the whole USB Serial JTAG console.
 */

#include "uart_commands_table.h"
#include "uart_task.h"

const uart_command_t uart_commands[] = {
    {"HELP",
     uart_cmd_help,
     "Show command list",
     "",
     false,
     {"?", "H", "", "", ""}},
    {"LEDTEST",
     uart_cmd_ledtest,
     "Cycle all LEDs through colors",
     "",
     false,
     {"LED_TEST", "TEST_LED", "", "", ""}},
    {"STATUS",
     uart_cmd_status,
     "System status",
     "",
     false,
     {"S", "INFO", "", "", ""}},
    {"CLEAR",
     uart_cmd_clear,
     "Clear screen",
     "",
     false,
     {"CLS", "C", "", "", ""}},
    {"RESET",
     uart_cmd_reset,
     "System restart",
     "",
     false,
     {"RESTART", "R", "", "", ""}},
    {"HALL",
     uart_cmd_hall,
     "Print raw STM Hall values until STOP",
     "",
     false,
     {"HALLS", "", "", "", ""}},
    {"STOP",
     uart_cmd_hall_stop,
     "Stop the Hall print",
     "",
     false,
     {"HALLSTOP", "", "", "", ""}},
    {"STM32",
     uart_cmd_stm32,
     "ROM bootloader: HELP, PROBE, ERASE, WRITE, FLASH_PART",
     "STM32 HELP",
     false,
     {"", "", "", "", ""}},
    {"LOG",
     uart_cmd_log,
     "Log level: none error warn info debug verbose",
     "LOG debug",
     false,
     {"DEBUG", "LOGLEVEL", "", "", ""}},

    {NULL, NULL, "", "", false, {"", "", "", "", ""}}};
