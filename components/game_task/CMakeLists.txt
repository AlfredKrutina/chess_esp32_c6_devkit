# components/game_task/CMakeLists.txt
idf_component_register(
    SRCS "game_task.c" "game_led_direct.c" "game_matrix_guard.c" "game_snapshot.c" "game_board_core.c" "game_move_validate.c" "game_move_exec.c" "game_physical.c" "game_puzzle.c" "game_json_export.c" "game_timer.c" "game_dispatch.c" "game_cmd_handlers.c" "game_error_recovery.c" "game_init.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver led_task matrix_task game_led_animations timer_system game_hooks config_manager
    PRIV_INCLUDE_DIRS "../freertos_chess/include"
)
