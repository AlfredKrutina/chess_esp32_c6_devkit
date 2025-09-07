# components/freertos_chess/CMakeLists.txt
idf_component_register(
    SRCS "freertos_chess.c" "shared_buffer_pool.c" "streaming_output.c" "led_mapping.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_system esp_timer nvs_flash button_task
)
