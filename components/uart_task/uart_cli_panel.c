/**
 * @file uart_cli_panel.c
 * @brief Jednotný CLI panel — flash, oddíly, web fronta, OTA, BLE příkazy, snapshot.
 */
#include "uart_cli_panel.h"

#include "ota_update.h"
#include "web_server_task.h"

#include "esp_app_format.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

extern void uart_send_colored_line(const char *color, const char *message);
extern void uart_send_error(const char *message);
extern void uart_send_success(const char *message);
extern void uart_send_formatted(const char *format, ...) __attribute__((format(printf, 1, 2)));
extern void uart_send_line(const char *str);

#define COLOR_INFO "\033[36m"

static const char *TAG = "cli_panel";

static const char *skip_leading_ws(const char *s) {
  if (!s) {
    return "";
  }
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}

static void skip_first_word(const char *line, char *rest, size_t rest_sz) {
  const char *s = skip_leading_ws(line);
  while (*s && *s != ' ' && *s != '\t') {
    s++;
  }
  s = skip_leading_ws(s);
  strncpy(rest, s, rest_sz - 1);
  rest[rest_sz - 1] = '\0';
}

static void cli_dump_partitions(void) {
  uart_send_colored_line(COLOR_INFO, "Partitions");
  esp_partition_iterator_t it =
      esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (it == NULL) {
    uart_send_line("(none)");
    return;
  }
  while (it != NULL) {
    const esp_partition_t *p = esp_partition_get(it);
    uart_send_formatted(
        "  %-12s  type=%u sub=%u  0x%08" PRIx32 "  %" PRIu32 " B",
        p->label, (unsigned)p->type, (unsigned)p->subtype,
        (uint32_t)p->address, (uint32_t)p->size);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

static void cli_flash_info(void) {
  uart_send_colored_line(COLOR_INFO, "Flash");
  uint32_t phy = 0;
  esp_err_t e = esp_flash_get_physical_size(esp_flash_default_chip, &phy);
  if (e != ESP_OK) {
    uart_send_formatted("  physical_size: (error %s)", esp_err_to_name(e));
  } else {
    uart_send_formatted("  detected_physical: %" PRIu32 " B (%" PRIu32 " MiB)",
                        phy, phy / (1024U * 1024U));
  }
}

static void cli_chip_info(void) {
  uart_send_colored_line(COLOR_INFO, "Chip / runtime");
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  uart_send_formatted("  model=%d cores=%d revision=%d", (int)ci.model,
                      ci.cores, ci.revision);
  uart_send_formatted("  reset_reason=%d", (int)esp_reset_reason());
  uart_send_formatted("  heap_free=%zu heap_min=%zu internal_free=%zu",
                      (size_t)esp_get_free_heap_size(),
                      (size_t)esp_get_minimum_free_heap_size(),
                      (size_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  uart_send_formatted("  ticks=%lu",
                      (unsigned long)xTaskGetTickCount());
}

static void cli_app_info(void) {
  uart_send_colored_line(COLOR_INFO, "Application");
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (run) {
    uart_send_formatted("  running: %s @ 0x%08" PRIx32 " size=%" PRIu32,
                        run->label, (uint32_t)run->address,
                        (uint32_t)run->size);
  } else {
    uart_send_line("  running: (unknown)");
  }
  const esp_app_desc_t *app = esp_app_get_description();
  uart_send_formatted("  version=%s project=%s idf=%s", app->version,
                      app->project_name, esp_get_idf_version());
}

static void cli_web_command(const char *sub_args) {
  char verb[16];
  if (sscanf(sub_args, "%15s", verb) != 1) {
    uart_send_error("CLI WEB START|STOP|STATUS");
    return;
  }

  if (!strcasecmp(verb, "START")) {
    esp_err_t e = web_server_enqueue_command(WEB_CMD_START_SERVER);
    if (e == ESP_OK) {
      uart_send_success("WEB START zařazeno (web_server task)");
    } else {
      uart_send_formatted("WEB START chyba: %s", esp_err_to_name(e));
    }
    return;
  }
  if (!strcasecmp(verb, "STOP")) {
    esp_err_t e = web_server_enqueue_command(WEB_CMD_STOP_SERVER);
    if (e == ESP_OK) {
      uart_send_success("WEB STOP zařazeno");
    } else {
      uart_send_formatted("WEB STOP chyba: %s", esp_err_to_name(e));
    }
    return;
  }
  if (!strcasecmp(verb, "STATUS")) {
    esp_err_t e = web_server_enqueue_command(WEB_CMD_GET_STATUS);
    if (e != ESP_OK) {
      uart_send_formatted("WEB STATUS fronta: %s", esp_err_to_name(e));
    }
    uart_send_formatted("  active=%d task_run=%d clients=%lu uptime_ms=%lu err=%s",
                        web_server_is_active() ? 1 : 0,
                        web_server_is_task_running() ? 1 : 0,
                        (unsigned long)web_server_get_client_count(),
                        (unsigned long)web_server_get_uptime(),
                        esp_err_to_name(web_server_get_last_http_error()));
    uart_send_formatted("  AP SSID=%s  web_locked=%d", web_server_get_ap_ssid(),
                        web_is_locked() ? 1 : 0);
    return;
  }
  uart_send_error("CLI WEB START|STOP|STATUS");
}

static void cli_ota_info(void) {
  uart_send_colored_line(COLOR_INFO, "OTA layout");
  const esp_partition_t *p0 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *p1 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  uart_send_formatted("  ota_0: %s", p0 ? p0->label : "(missing)");
  uart_send_formatted("  ota_1: %s", p1 ? p1->label : "(missing)");
  uart_send_formatted("  sta_wifi=%d",
                      wifi_is_sta_connected() ? 1 : 0);
}

static void cli_lock_command(const char *sub_args) {
  char tok[8];
  strncpy(tok, sub_args, sizeof(tok) - 1);
  tok[sizeof(tok) - 1] = '\0';
  for (char *p = tok; *p; p++) {
    *p = (char)toupper((unsigned char)*p);
  }
  if (!strcasecmp(tok, "ON") || !strcasecmp(tok, "1")) {
    esp_err_t e = web_lock_set(true);
    uart_send_formatted("LOCK ON → %s", esp_err_to_name(e));
    return;
  }
  if (!strcasecmp(tok, "OFF") || !strcasecmp(tok, "0")) {
    esp_err_t e = web_lock_set(false);
    uart_send_formatted("LOCK OFF → %s", esp_err_to_name(e));
    return;
  }
  uart_send_error("CLI LOCK ON|OFF");
}

static void cli_ble_json(const char *json_line) {
  const char *j = skip_leading_ws(json_line);
  if (*j == '\0') {
    uart_send_error("CLI BLE <json>  (stejné jako Zápis do GATT)");
    return;
  }
  esp_err_t e = web_server_ble_command_dispatch(j, strlen(j));
  if (e == ESP_OK) {
    uart_send_success("BLE příkaz vykonán");
  } else {
    uart_send_formatted("BLE dispatch: %s", esp_err_to_name(e));
  }
}

static void cli_snapshot(void) {
  char *json = NULL;
  size_t len = 0;
  esp_err_t e = web_server_build_game_snapshot_json_shared(&json, &len);
  if (e != ESP_OK || json == NULL || len == 0) {
    uart_send_formatted("SNAPSHOT chyba: %s", esp_err_to_name(e));
    return;
  }
  uart_send_colored_line(COLOR_INFO, "Snapshot JSON");
  const size_t chunk = 240;
  for (size_t i = 0; i < len; i += chunk) {
    size_t n = len - i;
    if (n > chunk) {
      n = chunk;
    }
    char line[256];
    if (n >= sizeof(line)) {
      n = sizeof(line) - 1;
    }
    memcpy(line, json + i, n);
    line[n] = '\0';
    uart_send_formatted("%s", line);
  }
}

void uart_cli_print_help(void) {
  uart_send_colored_line(COLOR_INFO, "CLI — jednotný panel");
  uart_send_line("  CLI HELP");
  uart_send_line("  CLI PARTITIONS | FLASH | CHIP | APP | HEAP");
  uart_send_line("  CLI WEB START | STOP | STATUS");
  uart_send_line("  CLI LOCK ON | OFF");
  uart_send_line("  CLI OTA INFO");
  uart_send_line("  CLI OTA <https://…>   (vyžaduje STA + ota_0/ota_1)");
  uart_send_line("  CLI BLE {\"cmd\":\"…\"}   (jako CZECHMATE GATT)");
  uart_send_line("  CLI SNAP              (game snapshot JSON)");
  uart_send_line("  CLI RESET             (esp_restart)");
}

command_result_t uart_cmd_cli(const char *args) {
  char line[512];
  if (args) {
    strncpy(line, args, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
  } else {
    line[0] = '\0';
  }

  char cmd[32];
  const char *s = skip_leading_ws(line);
  size_t ci = 0;
  while (*s && *s != ' ' && *s != '\t' && ci + 1 < sizeof(cmd)) {
    cmd[ci++] = (char)toupper((unsigned char)*s++);
  }
  cmd[ci] = '\0';

  if (cmd[0] == '\0' || !strcasecmp(cmd, "HELP") || !strcasecmp(cmd, "?")) {
    uart_cli_print_help();
    return CMD_SUCCESS;
  }

  char tail[480];
  skip_first_word(line, tail, sizeof(tail));

  if (!strcasecmp(cmd, "PARTITIONS") || !strcasecmp(cmd, "PART")) {
    cli_dump_partitions();
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "FLASH")) {
    cli_flash_info();
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "CHIP")) {
    cli_chip_info();
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "APP")) {
    cli_app_info();
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "HEAP")) {
    uart_send_formatted("heap_free=%zu heap_min=%zu internal_free=%zu",
                        (size_t)esp_get_free_heap_size(),
                        (size_t)esp_get_minimum_free_heap_size(),
                        (size_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "WEB")) {
    if (tail[0] == '\0') {
      uart_send_error("CLI WEB START|STOP|STATUS");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    cli_web_command(tail);
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "LOCK")) {
    char sub[16];
    sscanf(tail, "%15s", sub);
    if (sub[0] == '\0') {
      uart_send_error("CLI LOCK ON|OFF");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    cli_lock_command(sub);
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "OTA")) {
    const char *p = skip_leading_ws(tail);
    if (p[0] == '\0') {
      cli_ota_info();
      return CMD_SUCCESS;
    }
    if (!strncasecmp(p, "INFO", 4) &&
        (p[4] == '\0' || p[4] == ' ' || p[4] == '\t')) {
      cli_ota_info();
      return CMD_SUCCESS;
    }
    esp_err_t oe = ota_update_try_start_url(p);
    if (oe == ESP_OK) {
      uart_send_success("OTA stažení spuštěno na pozadí");
    } else {
      uart_send_formatted("OTA start: %s", esp_err_to_name(oe));
      ESP_LOGW(TAG, "OTA URL rejected: %s", esp_err_to_name(oe));
    }
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "BLE")) {
    cli_ble_json(tail);
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "SNAP") || !strcasecmp(cmd, "SNAPSHOT")) {
    cli_snapshot();
    return CMD_SUCCESS;
  }
  if (!strcasecmp(cmd, "RESET")) {
    uart_send_line("Restart…");
    esp_restart();
    return CMD_SUCCESS;
  }

  uart_send_error("Neznámý CLI podpříkaz — zadej CLI HELP");
  return CMD_ERROR_INVALID_SYNTAX;
}
