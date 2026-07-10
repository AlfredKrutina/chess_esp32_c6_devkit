# components/web_server_task/CMakeLists.txt
idf_component_register(
    SRCS "board_api_auth.c" "ota_update.c" "web_server_task.c" "web_routes.c"
          "web_handlers_game.c" "web_handlers_wifi.c" "chess_piece_http.c"
    INCLUDE_DIRS "include"
    EMBED_FILES
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
    REQUIRES 
        freertos_chess 
        driver 
        esp_system 
        esp_timer 
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_server
        esp_event
        json
        timer_system
        config_manager
        game_task
        game_hooks
        ble_task
        led_task
        mdns
        esp_https_ota
        app_update
        esp_partition
        esp_http_client
        mbedtls
)
