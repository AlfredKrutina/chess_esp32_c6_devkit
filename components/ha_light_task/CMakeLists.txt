# components/ha_light_task/CMakeLists.txt
idf_component_register(
    SRCS "ha_light_task.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos_chess 
        esp_system 
        esp_timer 
        esp_wifi
        esp_netif
        esp_event
        mqtt
        led_task
        game_task
        nvs_flash
        json
)
