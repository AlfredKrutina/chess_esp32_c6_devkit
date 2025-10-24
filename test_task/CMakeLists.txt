# components/test_task/CMakeLists.txt
idf_component_register(
    SRCS "test_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_task
)
