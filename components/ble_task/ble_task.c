/**
 * @file ble_task.c
 * @brief BLE vrstva — NimBLE při CONFIG_BT_ENABLED (viz ble_nimble_impl.c).
 *
 * Samostatný FreeRTOS task „BLE“ v tomto souboru není — host běží v tasku
 * vytvořeném z nimble_port_freertos_init(ble_host_task) v ble_nimble_stack_init().
 */
#include "ble_task.h"
#include "esp_log.h"

#if CONFIG_BT_ENABLED
extern void ble_nimble_stack_init(void);
#endif

static const char *TAG = "BLE_TASK";

void ble_task_init(void) {
#if CONFIG_BT_ENABLED
  ESP_LOGI(TAG, "Starting NimBLE stack (CZECHMATE GATT; host task = nimble_port_freertos_init)");
  ble_nimble_stack_init();
  ESP_LOGD(TAG, "[STAGING] ble_task_init: nimble_port_init + host task queued");
#else
  ESP_LOGD(TAG, "BLE vypnuto — nastav CONFIG_BT_ENABLED=y pro GATT");
#endif
}
