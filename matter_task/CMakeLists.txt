# components/matter_task/CMakeLists.txt
idf_component_register(
    SRCS "matter_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver
)
