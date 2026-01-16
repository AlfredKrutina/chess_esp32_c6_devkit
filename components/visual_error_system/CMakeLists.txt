idf_component_register(
    SRCS "visual_error_system.c"
    INCLUDE_DIRS "include"
    REQUIRES led_state_manager unified_animation_manager freertos_chess
)
