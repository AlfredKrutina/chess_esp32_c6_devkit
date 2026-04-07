# NimBLE GATT je v ble_nimble_impl.c pod #if CONFIG_BT_ENABLED — Kconfig musí být
# v CMake viditelný: if(CONFIG_BT_ENABLED) v podkomponentě někdy nevyhodnotí správně,
# proto je bt vždy v PRIV_REQUIRES (viz ESP-IDF kontrola include z bt).
idf_component_register(
    SRCS "ble_task.c" "ble_nimble_impl.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bt esp_coex web_server_task
)
