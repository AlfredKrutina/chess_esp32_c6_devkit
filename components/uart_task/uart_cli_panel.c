/**
 * @file uart_cli_panel.c
 * @brief STM32 ROM bootloader command. The rest of the old panel is not in the CLI.
 */
#include "uart_cli_panel.h"

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_partition.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE
#include "stm32_i2c_bl.h"
#endif

extern void uart_send_error(const char *message);
extern void uart_send_success(const char *message);
extern void uart_send_formatted(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
extern void uart_send_line(const char *str);

static const char *skip_leading_ws(const char *s) {
  if (!s) {
    return "";
  }
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}

static size_t stm32_hex_to_bin(const char *hex, uint8_t *out, size_t out_max) {
  size_t n = strlen(hex);
  if ((n % 2u) != 0u || n / 2u > out_max) {
    return 0;
  }
  for (size_t i = 0; i < n / 2u; i++) {
    unsigned v = 0;
    if (sscanf(hex + 2 * i, "%2x", &v) != 1) {
      return 0;
    }
    out[i] = (uint8_t)v;
  }
  return n / 2u;
}

command_result_t uart_cmd_stm32(const char *args) {
#if !CONFIG_CHESS_STM32_I2C_BL_ENABLE
  (void)args;
  uart_send_error("STM32 bootloader is off in menuconfig");
  return CMD_ERROR_INVALID_SYNTAX;
#else
  const char *p = skip_leading_ws(args ? args : "");
  char verb[24];
  if (sscanf(p, "%23s", verb) != 1) {
    uart_send_error("STM32 HELP | PROBE | ERASE | WRITE | FLASH_PART");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  if (!strcasecmp(verb, "HELP") || !strcasecmp(verb, "?")) {
    uart_send_line("STM32 I2C ROM bootloader (AN4221)");
    uart_send_line("  STM32 PROBE <seg> [BOOT]");
    uart_send_line("  STM32 ERASE <seg> [BOOT]");
    uart_send_line("  STM32 WRITE <seg> <addr_hex> <data_hex>   (max 256 B)");
    uart_send_line("  STM32 FLASH_PART <seg> <partition_label> [BOOT]");
    return CMD_SUCCESS;
  }

  esp_err_t ie = stm32_i2c_bl_init();
  if (ie != ESP_OK) {
    uart_send_formatted("stm32_i2c_bl_init: %s", esp_err_to_name(ie));
    return CMD_ERROR_SYSTEM_ERROR;
  }

  if (!strcasecmp(verb, "PROBE")) {
    unsigned seg = 0;
    char bflag[8] = "";
    int n = sscanf(p, "%*s %u %7s", &seg, bflag);
    if (n < 1 || seg > 3) {
      uart_send_error("STM32 PROBE <seg> [BOOT]");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    bool boot = (n >= 2 && !strcasecmp(bflag, "BOOT"));
    uint16_t pid = 0;
    esp_err_t e = stm32_i2c_bl_cmd_get_id((uint8_t)seg, boot, &pid);
    if (e != ESP_OK) {
      uart_send_formatted("GET_ID: %s", esp_err_to_name(e));
      return CMD_ERROR_SYSTEM_ERROR;
    }
    uart_send_formatted("segment %u  PID=0x%04X", seg, (unsigned)pid);
    return CMD_SUCCESS;
  }

  if (!strcasecmp(verb, "ERASE")) {
    unsigned seg = 0;
    char bflag[8] = "";
    int n = sscanf(p, "%*s %u %7s", &seg, bflag);
    if (n < 1 || seg > 3) {
      uart_send_error("STM32 ERASE <seg> [BOOT]");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    bool boot = (n >= 2 && !strcasecmp(bflag, "BOOT"));
    esp_err_t e = stm32_i2c_bl_erase_all((uint8_t)seg, boot);
    if (e != ESP_OK) {
      uart_send_formatted("ERASE: %s", esp_err_to_name(e));
      return CMD_ERROR_SYSTEM_ERROR;
    }
    uart_send_success("Mass erase complete");
    return CMD_SUCCESS;
  }

  if (!strcasecmp(verb, "WRITE")) {
    unsigned seg = 0;
    uint32_t addr = 0;
    char hexbuf[520];
    if (sscanf(p, "%*s %u %" SCNx32 " %519s", &seg, &addr, hexbuf) != 3 ||
        seg > 3) {
      uart_send_error("STM32 WRITE <seg> <addr_hex> <data_hex>");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    uint8_t raw[256];
    size_t ln = stm32_hex_to_bin(hexbuf, raw, sizeof(raw));
    if (ln == 0) {
      uart_send_error("bad hex data (even length, max 512 hex characters)");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    esp_err_t e = stm32_i2c_bl_write_memory((uint8_t)seg, false, addr, raw, ln);
    if (e != ESP_OK) {
      uart_send_formatted("WRITE: %s", esp_err_to_name(e));
      return CMD_ERROR_SYSTEM_ERROR;
    }
    uart_send_success("WRITE OK");
    return CMD_SUCCESS;
  }

  if (!strcasecmp(verb, "FLASH_PART")) {
    unsigned seg = 0;
    char label[32];
    char bflag[8] = "";
    int n = sscanf(p, "%*s %u %31s %7s", &seg, label, bflag);
    bool boot = (n == 3 && !strcasecmp(bflag, "BOOT"));
    if (n < 2 || seg > 3) {
      uart_send_error("STM32 FLASH_PART <seg> <label> [BOOT]");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, label);
    if (!part) {
      uart_send_formatted("partition '%s' not found", label);
      return CMD_ERROR_INVALID_PARAMETER;
    }
    size_t psz = part->size;
    if (psz > 512 * 1024) {
      uart_send_error("partition too large (>512 KiB)");
      return CMD_ERROR_INVALID_PARAMETER;
    }
    uint8_t *buf = (uint8_t *)malloc(psz);
    if (!buf) {
      uart_send_error("malloc bin buffer");
      return CMD_ERROR_SYSTEM_ERROR;
    }
    esp_err_t le = esp_partition_read(part, 0, buf, part->size);
    if (le != ESP_OK) {
      free(buf);
      uart_send_formatted("read partition: %s", esp_err_to_name(le));
      return CMD_ERROR_SYSTEM_ERROR;
    }
    size_t eff = psz;
    while (eff > 0 && buf[eff - 1] == 0xFF) {
      eff--;
    }
    if (eff == 0) {
      free(buf);
      uart_send_error("STM32 image empty (partition is 0xFF)");
      return CMD_ERROR_INVALID_PARAMETER;
    }
    uart_send_formatted("FLASH_PART: %u B image", (unsigned)eff);
    esp_err_t fe =
        stm32_i2c_bl_flash_binary((uint8_t)seg, boot, 0x08000000, buf, eff);
    free(buf);
    if (fe != ESP_OK) {
      uart_send_formatted("FLASH_PART: %s", esp_err_to_name(fe));
      return CMD_ERROR_SYSTEM_ERROR;
    }
    uart_send_success("FLASH_PART done");
    return CMD_SUCCESS;
  }

  uart_send_error("unknown STM32 command — STM32 HELP");
  return CMD_ERROR_INVALID_SYNTAX;
#endif
}
