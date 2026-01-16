# components/uart_task/CMakeLists.txt
idf_component_register(
    SRCS "uart_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess matrix_task led_task driver config_manager game_task uart_commands_extended
)
