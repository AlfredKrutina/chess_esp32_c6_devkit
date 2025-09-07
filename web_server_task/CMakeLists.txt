# components/web_server_task/CMakeLists.txt
idf_component_register(
    SRCS "web_server_task.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver esp_system esp_timer nvs_flash
)
