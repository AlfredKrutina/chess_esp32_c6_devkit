# NimBLE GATT lives in ble_nimble_impl.c under #if CONFIG_BT_ENABLED — Kconfig must be
# visible to CMake: if(CONFIG_BT_ENABLED) in a subcomponent sometimes does not evaluate correctly,
# so bt is always in PRIV_REQUIRES (see ESP-IDF include checks from bt).
idf_component_register(
    SRCS "ble_task.c" "ble_nimble_impl.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bt esp_coex esp_timer game_hooks web_server_task
)
