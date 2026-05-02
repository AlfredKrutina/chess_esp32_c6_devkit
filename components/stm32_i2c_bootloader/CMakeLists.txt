idf_component_register(
    SRCS "stm32_i2c_bl.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_partition nvs_flash
)
