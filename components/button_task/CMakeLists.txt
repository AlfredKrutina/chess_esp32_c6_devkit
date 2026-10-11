# components/button_task/CMakeLists.txt
idf_component_register(
    SRCS "button_task.c" "button_gesture.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_task
)
