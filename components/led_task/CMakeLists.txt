# components/led_task/CMakeLists.txt
idf_component_register(
    SRCS "led_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_strip
)