# components/animation_task/CMakeLists.txt
idf_component_register(
    SRCS "animation_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_task
)
