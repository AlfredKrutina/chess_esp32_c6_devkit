idf_component_register(SRCS "uart_commands_extended.c"
                    INCLUDE_DIRS "include" 
                    REQUIRES console freertos esp_common game_led_animations)
