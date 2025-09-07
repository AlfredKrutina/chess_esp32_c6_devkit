idf_component_register(SRCS "game_led_animations.c"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos_chess driver esp_timer led_task esp_common)
