# components/game_task/CMakeLists.txt
idf_component_register(
    SRCS "game_task.c" "game_led_direct.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_task game_led_animations
    PRIV_INCLUDE_DIRS "../freertos_chess/include"
)
