idf_component_register(
    SRCS "reset_button_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver
)
