# components/game_task/CMakeLists.txt
idf_component_register(
    SRCS "game_required_step.c" "demo_mode_helpers.c" "game_board_transition.c" "game_board_machine.c" "game_hall_hand.c" "game_hall_position.c" "game_hall_led.c" "game_task.c" "game_policy.c" "game_led_direct.c" "game_squares_guard.c" "game_hall_match.c" "game_hall_restore_guide.c" "game_snapshot.c" "game_board_core.c" "game_move_validate.c" "game_move_exec.c" "game_physical.c" "game_physical_undo.c" "game_puzzle.c" "game_opening_trainer.c" "game_json_export.c" "game_timer.c" "game_dispatch.c" "game_error_recovery.c" "game_init.c" "game_matrix_workflow.c" "game_endgame_report.c" "game_endgame_detect.c" "game_promotion.c" "game_resignation.c" "game_move_gen.c" "game_castling.c" "game_gesture.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess driver squares_task game_led_animations timer_system game_hooks config_manager esp_wifi chess_core
    PRIV_INCLUDE_DIRS "../freertos_chess/include"
)
