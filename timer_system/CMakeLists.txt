idf_component_register(
    SRCS "timer_system.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_timer" "nvs_flash" "freertos"
)
