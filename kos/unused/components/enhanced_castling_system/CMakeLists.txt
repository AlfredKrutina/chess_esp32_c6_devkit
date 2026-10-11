idf_component_register(
    SRCS "enhanced_castling_system.c"
    INCLUDE_DIRS "include"
    REQUIRES "freertos" "esp_timer" "led_task" "freertos_chess" "game_led_animations"
)
