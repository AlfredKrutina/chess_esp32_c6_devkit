/* Archived from game_start_new_game() in components/game_task/game_init.c.
 * total_games is incremented just above this test, so the value is at least 1.
 * The else never ran. Highlight of movable pieces always ran.
 * Not part of the firmware build.
 */

if (total_games >= 1) {
  vTaskDelay(pdMS_TO_TICKS(100));
  game_task_wdt_reset_safe();
  chess_policy_highlight_movable_if_enabled();
  game_task_wdt_reset_safe();
} else {
  ESP_LOGI(TAG, "Boot sequence: Skipping movable pieces highlight (first game)");
}
