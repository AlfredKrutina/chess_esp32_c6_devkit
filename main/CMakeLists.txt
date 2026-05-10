# main/CMakeLists.txt - ESP32-C6 Chess v1.8.0
# test_task jen pokud CONFIG_CHESS_ENABLE_TEST_TASK=y (jinak se komponenta vůbec nenalinkuje).
set(MAIN_REQUIRES
    freertos_chess
    led_task
    matrix_task
    button_task
    uart_task
    game_task
    web_server_task
    led_state_manager
    visual_error_system
    timer_system
    unified_animation_manager
)
if(CONFIG_CHESS_ENABLE_TEST_TASK)
  list(APPEND MAIN_REQUIRES test_task)
endif()

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ${MAIN_REQUIRES}
    PRIV_REQUIRES
        spi_flash
        console
        game_led_animations
        uart_commands_extended
        ble_task
)
