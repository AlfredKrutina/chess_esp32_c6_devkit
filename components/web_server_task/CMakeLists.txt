# components/web_server_task/CMakeLists.txt
#
# CONFIG_CHESS_ENABLE_WEB_SERVER=y (default): WiFi + HTTP machine API + WebSocket.
# CONFIG_CHESS_ENABLE_WEB_SERVER=n: BLE/remote bridge only (no HTTP handlers).
# Browser chess page and piece PNGs live in kos/unused/web_ui and are not linked.

set(WS_SRCS
    "board_api_auth.c"
    "board_pair_policy.c"
    "ota_update.c"
    "web_opening_dispatch.c"
    "web_resign_dispatch.c"
    "web_handlers_game.c"
    "web_server_task.c"
)

set(WS_REQUIRES
    freertos_chess
    driver
    esp_system
    esp_timer
    nvs_flash
    json
    timer_system
    config_manager
    game_task
    game_hooks
    ble_task
    led_task
    ota_guard
    esp_https_ota
    app_update
    esp_partition
    esp_http_client
    esp_http_server
    mbedtls
    esp_wifi
    esp_netif
    esp_event
)

if(CONFIG_CHESS_ENABLE_WEB_SERVER)
    list(APPEND WS_SRCS
        "web_routes.c"
        "web_handlers_wifi.c"
        "web_handlers_system.c"
        "web_ws.c"
    )
    list(APPEND WS_REQUIRES
        espressif__mdns
    )
    message(STATUS "web_server_task: HTTP API ENABLED (no browser UI)")
else()
    message(STATUS "web_server_task: HTTP web server DISABLED (BLE bridge only)")
endif()

idf_component_register(
    SRCS ${WS_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES ${WS_REQUIRES}
    PRIV_REQUIRES transport_policy
)
