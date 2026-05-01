# components/uart_task/CMakeLists.txt
idf_component_register(
    SRCS "uart_task.c" "uart_cli_panel.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess matrix_task led_task driver config_manager game_task uart_commands_extended ble_task web_server_task spi_flash esp_partition app_update
)
