# components/matrix_task/CMakeLists.txt
idf_component_register(
    SRCS "matrix_task.c" "hall_i2c_matrix.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver ha_light_task game_task stm32_i2c_bootloader
)
