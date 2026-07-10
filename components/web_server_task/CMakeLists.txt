# components/web_server_task/CMakeLists.txt
#
# CONFIG_CHESS_ENABLE_WEB_SERVER=y (default): full HTTP + WiFi UI stack.
# CONFIG_CHESS_ENABLE_WEB_SERVER=n: BLE/remote bridge only (no HTTP handlers/embeds).

set(WS_SRCS
    "board_api_auth.c"
    "ota_update.c"
    "web_opening_dispatch.c"
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
    esp_https_ota
    app_update
    esp_partition
    esp_http_client
    mbedtls
    esp_wifi
    esp_netif
    esp_event
)

set(WS_EMBED_FILES)

if(CONFIG_CHESS_ENABLE_WEB_SERVER)
    list(APPEND WS_SRCS
        "web_routes.c"
        "web_handlers_wifi.c"
        "web_handlers_system.c"
        "web_ws.c"
        "chess_piece_http.c"
    )
    list(APPEND WS_REQUIRES
        esp_http_server
        mdns
    )
    set(WS_EMBED_FILES
        "web/piece_assets/PieceWhiteKing.png"
        "web/piece_assets/PieceWhiteQueen.png"
        "web/piece_assets/PieceWhiteRook.png"
        "web/piece_assets/PieceWhiteBishop.png"
        "web/piece_assets/PieceWhiteKnight.png"
        "web/piece_assets/PieceWhitePawn.png"
        "web/piece_assets/PieceBlackKing.png"
        "web/piece_assets/PieceBlackQueen.png"
        "web/piece_assets/PieceBlackRook.png"
        "web/piece_assets/PieceBlackBishop.png"
        "web/piece_assets/PieceBlackKnight.png"
        "web/piece_assets/PieceBlackPawn.png"
    )
    message(STATUS "web_server_task: HTTP web server ENABLED")
else()
    message(STATUS "web_server_task: HTTP web server DISABLED (BLE bridge only)")
endif()

idf_component_register(
    SRCS ${WS_SRCS}
    INCLUDE_DIRS "include"
    EMBED_FILES ${WS_EMBED_FILES}
    REQUIRES ${WS_REQUIRES}
)
