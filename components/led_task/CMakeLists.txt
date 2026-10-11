# components/led_task/CMakeLists.txt
idf_component_register(
    SRCS "led_task.c" "led_board_restore_guide.c" "led_gesture_anim.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_strip
)