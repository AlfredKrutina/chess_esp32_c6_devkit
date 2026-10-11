# components/squares_task/CMakeLists.txt
idf_component_register(
    SRCS "squares_task.c" "hall_i2c_squares.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver ha_light_task game_task stm32_i2c_bootloader nvs_flash led_task
)
