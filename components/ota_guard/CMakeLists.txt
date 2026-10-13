# components/ota_guard/CMakeLists.txt
#
# OTA state machine + boot guard (confirm after stable start, confirm deadline,
# crash-loop fallback to the other slot, NVS journal of the last OTA attempt).
# Deliberately depends only on ESP-IDF — no game, LED, web or BLE components —
# so the transports (HTTP/HTTPS/BLE in web_server_task) and main share one truth.

idf_component_register(
    SRCS "ota_guard.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_partition
    PRIV_REQUIRES esp_system esp_timer nvs_flash log freertos
)
