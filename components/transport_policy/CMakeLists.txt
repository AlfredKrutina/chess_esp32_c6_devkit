# transport_policy: V2 session policy — BLE as primary realtime path,
# WiFi STA for bulk/failover, lazy WiFi start on boot.

idf_component_register(
    SRCS "transport_policy.c"
    INCLUDE_DIRS "include"
    REQUIRES
        ble_task
        nvs_flash
        esp_wifi
        esp_coex
    PRIV_REQUIRES
        bt
)
