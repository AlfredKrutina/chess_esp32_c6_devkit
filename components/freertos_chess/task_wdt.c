/**
 * @file task_wdt.c
 * @brief Shared Task Watchdog reset helpers.
 */

#include "task_wdt.h"

#include "esp_log.h"
#include "esp_task_wdt.h"

esp_err_t chess_task_wdt_reset_safe(const char *log_tag) {
  const char *tag = (log_tag != NULL) ? log_tag : "task_wdt";
  esp_err_t ret = esp_task_wdt_reset();

  if (ret == ESP_ERR_NOT_FOUND) {
    ESP_LOGW(tag,
             "WDT reset: task not registered yet (this is normal during startup)");
    return ESP_OK;
  }
  if (ret != ESP_OK) {
    ESP_LOGE(tag, "WDT reset failed: %s", esp_err_to_name(ret));
    return ret;
  }
  return ESP_OK;
}

esp_err_t chess_task_wdt_reset_if_owner(const char *log_tag,
                                        TaskHandle_t owner) {
  if (owner != NULL && xTaskGetCurrentTaskHandle() != owner) {
    return ESP_OK;
  }
  return chess_task_wdt_reset_safe(log_tag);
}
