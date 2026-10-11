/**
 * @file task_wdt.h
 * @brief Shared Task Watchdog reset helpers (ESP_ERR_NOT_FOUND → WARNING).
 */
#ifndef TASK_WDT_H
#define TASK_WDT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reset TWDT for the current task; log WARNING (not ERROR) if not subscribed yet.
 */
esp_err_t chess_task_wdt_reset_safe(const char *log_tag);

/**
 * Like chess_task_wdt_reset_safe(), but no-op when @p owner is non-NULL and the
 * caller is not that task (HTTP/MQTT workers calling into LED/web code).
 */
esp_err_t chess_task_wdt_reset_if_owner(const char *log_tag,
                                        TaskHandle_t owner);

#ifdef __cplusplus
}
#endif

#endif /* TASK_WDT_H */
