idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_system freertos_chess
    PRIV_INCLUDE_DIRS "../freertos_chess/include"
)
