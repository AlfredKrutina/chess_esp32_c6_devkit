# main/CMakeLists.txt - ESP32-C6 Chess v2.4
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos_chess 
        led_task
        matrix_task
        button_task
        uart_task
        game_task
        animation_task
        screen_saver_task
        test_task
        matter_task
        web_server_task
        unified_animation_manager
        led_state_manager
        visual_error_system
        enhanced_puzzle_system
    PRIV_REQUIRES
        spi_flash
        console
        game_led_animations
        uart_commands_extended
)
