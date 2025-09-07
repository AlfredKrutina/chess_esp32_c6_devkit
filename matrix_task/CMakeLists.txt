# components/matrix_task/CMakeLists.txt
idf_component_register(
    SRCS "matrix_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver
)
