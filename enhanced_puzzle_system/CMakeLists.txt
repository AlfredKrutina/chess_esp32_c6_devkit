idf_component_register(
    SRCS "enhanced_puzzle_system.c"
    INCLUDE_DIRS "include"
    REQUIRES led_state_manager unified_animation_manager visual_error_system freertos_chess
)
