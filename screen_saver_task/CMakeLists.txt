# components/screen_saver_task/CMakeLists.txt
idf_component_register(
    SRCS "screen_saver_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_task
)
