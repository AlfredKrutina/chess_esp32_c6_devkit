# components/uart_task/CMakeLists.txt
idf_component_register(
    SRCS "uart_task.c" "uart_cli_panel.c" "uart_commands_table.c" "uart_parse.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess squares_task led_task driver config_manager game_task ble_task web_server_task ha_light_task spi_flash esp_partition app_update stm32_i2c_bootloader
)
