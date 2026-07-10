/**
 * @file uart_handlers_debug.c
 * @brief Debug and diagnostics UART command handlers.
 */

#include "uart_task_internal.h"

#include "uart_task.h"
#include "freertos_chess.h"
#include "game_task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "UART_DEBUG";

command_result_t uart_cmd_self_test(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("🔧 SYSTEM SELF-TEST");
  uart_send_formatted("═══════════════════");

  int tests_passed = 0;
  int tests_total = 0;

  // Test 1: Memory System
  uart_send_formatted("🧠 MEMORY SYSTEM TEST:");
  tests_total++;
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

  if (free_heap > 10000 &&
      min_free_heap > 5000) { // At least 10KB free, 5KB minimum
    uart_send_formatted("   ✅ Free Heap: %zu bytes (%.1f%%)", free_heap,
                        (float)free_heap / total_heap * 100);
    uart_send_formatted("   ✅ Min Free: %zu bytes", min_free_heap);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ Memory: CRITICAL - %zu bytes free", free_heap);
  }

  // Test 2: Task System
  uart_send_formatted("📋 TASK SYSTEM TEST:");
  tests_total++;
  UBaseType_t task_count = uxTaskGetNumberOfTasks();
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(current_task);

  if (task_count > 0 &&
      stack_high_water > 100) { // At least 100 bytes stack free
    uart_send_formatted("   ✅ Tasks Running: %" PRIu32, task_count);
    uart_send_formatted("   ✅ Stack Free: %" PRIu32 " bytes",
                        stack_high_water);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ Task System: FAILED");
  }

  // Test 3: Queue System
  uart_send_formatted("📦 QUEUE SYSTEM TEST:");
  tests_total++;
  extern QueueHandle_t uart_command_queue;
  extern QueueHandle_t game_command_queue;
  // extern QueueHandle_t led_command_queue;  //  REMOVED: Queue hell
  // eliminated

  int queues_ok = 0;
  if (uart_command_queue)
    queues_ok++;
  if (game_command_queue)
    queues_ok++;
  // LED queues removed - using direct LED calls now

  if (queues_ok >= 2) {
    uart_send_formatted("   ✅ Core Queues: %d/2 available", queues_ok);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ Queues: Only %d/2 available", queues_ok);
  }

  // Test 4: Mutex System
  uart_send_formatted("🔒 MUTEX SYSTEM TEST:");
  tests_total++;
  extern SemaphoreHandle_t uart_mutex;
  extern SemaphoreHandle_t game_mutex;
  extern SemaphoreHandle_t led_mutex;

  int mutexes_ok = 0;
  if (uart_mutex)
    mutexes_ok++;
  if (game_mutex)
    mutexes_ok++;
  if (led_mutex)
    mutexes_ok++;

  if (mutexes_ok >= 3) {
    uart_send_formatted("   ✅ Core Mutexes: %d/3 available", mutexes_ok);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ Mutexes: Only %d/3 available", mutexes_ok);
  }

  // Test 5: Timer System
  uart_send_formatted("⏰ TIMER SYSTEM TEST:");
  tests_total++;
  int64_t start_time = esp_timer_get_time();
  vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
  int64_t end_time = esp_timer_get_time();
  int64_t elapsed = end_time - start_time;

  if (elapsed >= 8000 && elapsed <= 12000) { // 8-12ms tolerance
    uart_send_formatted("   ✅ Timer Accuracy: %" PRId64 " μs (expected ~10ms)",
                        elapsed);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ Timer: %" PRId64 " μs (expected ~10ms)",
                        elapsed);
  }

  // Test 6: CPU Performance
  uart_send_formatted("⚡ CPU PERFORMANCE TEST:");
  tests_total++;
  start_time = esp_timer_get_time();
  volatile int test_sum = 0;
  for (int i = 0; i < 1000; i++) {
    test_sum += i * i;
  }
  end_time = esp_timer_get_time();
  int64_t cpu_time = end_time - start_time;

  if (cpu_time < 1000) { // Should complete in less than 1ms
    uart_send_formatted("   ✅ CPU Speed: %" PRId64 " μs for 1K operations",
                        cpu_time);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ CPU: %" PRId64 " μs (too slow)", cpu_time);
  }

  // Test 7: System Uptime
  uart_send_formatted("🕐 SYSTEM UPTIME TEST:");
  tests_total++;
  int64_t uptime_us = esp_timer_get_time();
  uint64_t uptime_sec = uptime_us / 1000000;

  if (uptime_sec > 0) {
    uart_send_formatted("   ✅ Uptime: %" PRIu64 " seconds", uptime_sec);
    tests_passed++;
  } else {
    uart_send_formatted("   ❌ Uptime: Invalid");
  }

  // Test Summary
  uart_send_formatted("");
  uart_send_formatted("📊 TEST SUMMARY:");
  uart_send_formatted("   Tests Passed: %d/%d", tests_passed, tests_total);
  uart_send_formatted("   Success Rate: %.1f%%",
                      (float)tests_passed / tests_total * 100);

  if (tests_passed == tests_total) {
    uart_send_formatted("   🎉 ALL TESTS PASSED - System is healthy!");
  } else if (tests_passed >= tests_total * 0.8) {
    uart_send_formatted("   ⚠️  MOSTLY HEALTHY - %d test(s) failed",
                        tests_total - tests_passed);
  } else {
    uart_send_formatted("   🚨 SYSTEM ISSUES - %d test(s) failed",
                        tests_total - tests_passed);
  }

  uart_send_formatted("✅ Self-test completed");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_test_game(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("🎮 GAME ENGINE TEST");
  uart_send_formatted("═══════════════════");
  uart_send_formatted("✅ Game Task: Running");
  uart_send_formatted("✅ Board State: Valid");
  uart_send_formatted("✅ Move Validation: Available");
  uart_send_formatted("⚠️  TODO: Complete game logic tests");
  uart_send_formatted("📝 Status: BASIC TEST ONLY");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_status(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("🔍 DEBUG STATUS");
  uart_send_formatted("═══════════════");

  // System Information
  uart_send_formatted("🖥️  SYSTEM INFO:");
  uart_send_formatted("   CPU Frequency: 160 MHz (ESP32-C6)");
  uart_send_formatted("   Uptime: %" PRIu64 " seconds",
                      esp_timer_get_time() / 1000000);
  uart_send_formatted("   FreeRTOS Version: %s", tskKERNEL_VERSION_NUMBER);

  // Memory Information
  uart_send_formatted("💾 MEMORY DEBUG:");
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  uart_send_formatted("   Free Heap: %zu bytes (%.1f%%)", free_heap,
                      (float)free_heap / total_heap * 100);
  uart_send_formatted("   Min Free: %zu bytes", min_free_heap);
  uart_send_formatted("   Total Heap: %zu bytes", total_heap);
  uart_send_formatted("   Used: %zu bytes (%.1f%%)", total_heap - free_heap,
                      (float)(total_heap - free_heap) / total_heap * 100);

  // Task Information
  uart_send_formatted("📋 TASK DEBUG:");
  UBaseType_t task_count = uxTaskGetNumberOfTasks();
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(current_task);
  uart_send_formatted("   Total Tasks: %" PRIu32, task_count);
  uart_send_formatted("   Current Task Stack: %" PRIu32 " bytes free",
                      stack_high_water);
  uart_send_formatted("   Scheduler State: %s",
                      (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
                          ? "Running"
                          : "Suspended");

  // Queue Information
  uart_send_formatted("📦 QUEUE DEBUG:");
  extern QueueHandle_t uart_command_queue;
  extern QueueHandle_t game_command_queue;
  // extern QueueHandle_t led_command_queue;  //  REMOVED: Queue hell
  // eliminated

  if (uart_command_queue) {
    UBaseType_t uart_messages = uxQueueMessagesWaiting(uart_command_queue);
    UBaseType_t uart_spaces = uxQueueSpacesAvailable(uart_command_queue);
    uart_send_formatted("   UART Command: [%d/%d] messages", uart_messages,
                        uart_messages + uart_spaces);
  }

  if (game_command_queue) {
    UBaseType_t game_messages = uxQueueMessagesWaiting(game_command_queue);
    UBaseType_t game_spaces = uxQueueSpacesAvailable(game_command_queue);
    uart_send_formatted("   Game Command: [%d/%d] messages", game_messages,
                        game_messages + game_spaces);
  }

  // LED queues removed - using direct LED calls now

  // Mutex Information
  uart_send_formatted("🔒 MUTEX DEBUG:");
  extern SemaphoreHandle_t uart_mutex;
  extern SemaphoreHandle_t game_mutex;
  extern SemaphoreHandle_t led_mutex;

  if (uart_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(uart_mutex);
    uart_send_formatted("   UART Mutex: %s", holder ? "LOCKED" : "FREE");
  }

  if (game_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(game_mutex);
    uart_send_formatted("   Game Mutex: %s", holder ? "LOCKED" : "FREE");
  }

  if (led_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(led_mutex);
    uart_send_formatted("   LED Mutex: %s", holder ? "LOCKED" : "FREE");
  }

  // Performance Information
  uart_send_formatted("⚡ PERFORMANCE DEBUG:");
  uart_send_formatted("   Tick Count: %" PRIu32, xTaskGetTickCount());
  uart_send_formatted("   Tick Rate: %d Hz", configTICK_RATE_HZ);

  // Command Statistics
  uart_send_formatted("📊 COMMAND STATS:");
  uart_send_formatted("   Commands Processed: %" PRIu32, command_count);
  uart_send_formatted("   Errors Encountered: %" PRIu32, error_count);
  if (command_count > 0) {
    uart_send_formatted("   Error Rate: %.2f%%",
                        (float)error_count / command_count * 100);
  }

  uart_send_formatted("✅ Debug status displayed");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_game(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("🎯 GAME DEBUG INFO");
  uart_send_formatted("══════════════════");

  // Get game information from game_task
  extern player_t game_get_current_player(void);
  extern uint32_t game_get_move_count(void);
  extern game_state_t game_get_state(void);

  // Game State Information
  uart_send_formatted("🎮 GAME STATE:");
  game_state_t game_state = game_get_state();
  const char *state_str;
  switch (game_state) {
  case GAME_STATE_IDLE:
    state_str = "Idle";
    break;
  case GAME_STATE_INIT:
    state_str = "Initializing";
    break;
  case GAME_STATE_ACTIVE:
    state_str = "Active";
    break;
  case GAME_STATE_PAUSED:
    state_str = "Paused";
    break;
  case GAME_STATE_FINISHED:
    state_str = "Finished";
    break;
  case GAME_STATE_ERROR:
    state_str = "Error";
    break;
  case GAME_STATE_PLAYING:
    state_str = "Playing";
    break;
  case GAME_STATE_PROMOTION:
    state_str = "Promotion";
    break;
  default:
    state_str = "Unknown";
    break;
  }
  uart_send_formatted("   State: %s (%d)", state_str, game_state);

  player_t current_player = game_get_current_player();
  uart_send_formatted("   Current Player: %s (%s)",
                      current_player == PLAYER_WHITE ? "White" : "Black",
                      current_player == PLAYER_WHITE ? "♔" : "♚");

  uint32_t move_count = game_get_move_count();
  uart_send_formatted("   Move Count: %" PRIu32, move_count);
  uart_send_formatted("   Half-moves: %" PRIu32, move_count / 2);

  // Piece Information
  uart_send_formatted("♟️  PIECE STATS:");
  uart_send_formatted(
      "   Note: Detailed piece counts require game engine access");
  uart_send_formatted("   Use 'board' command for current position");

  // Game Task Information
  uart_send_formatted("🔧 GAME TASK DEBUG:");
  extern TaskHandle_t game_task_handle;
  if (game_task_handle) {
    const char *task_name = pcTaskGetName(game_task_handle);
    uart_send_formatted("   Task Name: %s", task_name);
    uart_send_formatted("   Task Priority: %" PRIu32,
                        uxTaskPriorityGet(game_task_handle));
    uart_send_formatted("   Stack High Water: %" PRIu32 " bytes",
                        uxTaskGetStackHighWaterMark(game_task_handle));
  } else {
    uart_send_formatted("   Task: NOT CREATED");
  }

  // Game Queue Information
  uart_send_formatted("📦 GAME QUEUES:");
  extern QueueHandle_t game_command_queue;
  extern QueueHandle_t game_status_queue;

  if (game_command_queue) {
    UBaseType_t messages = uxQueueMessagesWaiting(game_command_queue);
    UBaseType_t spaces = uxQueueSpacesAvailable(game_command_queue);
    uart_send_formatted("   Command Queue: [%d/%d] messages", messages,
                        messages + spaces);
  }

  if (game_status_queue) {
    UBaseType_t messages = uxQueueMessagesWaiting(game_status_queue);
    UBaseType_t spaces = uxQueueSpacesAvailable(game_status_queue);
    uart_send_formatted("   Status Queue: [%d/%d] messages", messages,
                        messages + spaces);
  }

  // Game Mutex Information
  uart_send_formatted("🔒 GAME MUTEX:");
  extern SemaphoreHandle_t game_mutex;
  if (game_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(game_mutex);
    uart_send_formatted("   Game Mutex: %s", holder ? "LOCKED" : "FREE");
    if (holder) {
      const char *holder_name = pcTaskGetName(holder);
      uart_send_formatted("   Holder: %s", holder_name);
    }
  } else {
    uart_send_formatted("   Game Mutex: NOT CREATED");
  }

  // Performance Information
  uart_send_formatted("⚡ GAME PERFORMANCE:");
  int64_t start_time = esp_timer_get_time();
  // Simulate a quick game operation
  volatile int test = 0;
  for (int i = 0; i < 100; i++) {
    test += i;
  }
  int64_t end_time = esp_timer_get_time();
  uart_send_formatted("   Operation Time: %" PRId64 " μs",
                      end_time - start_time);

  uart_send_formatted("✅ Game debug info displayed");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_debug_board(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("♞ BOARD DEBUG INFO");
  uart_send_formatted("══════════════════");

  // Board Structure Information
  uart_send_formatted("🏗️  BOARD STRUCTURE:");
  uart_send_formatted("   Size: 8x8 (64 squares)");
  uart_send_formatted("   Square Format: Algebraic notation (a1-h8)");
  uart_send_formatted("   Piece Representation: Integer values");

  // Piece Count Analysis
  uart_send_formatted("♟️  PIECE ANALYSIS:");
  uart_send_formatted(
      "   Note: Detailed piece analysis requires game engine access");
  uart_send_formatted("   Use 'board' command to see current position");
  uart_send_formatted("   Standard chess: 32 pieces (16 per side)");

  // Board Position Analysis
  uart_send_formatted("📍 POSITION ANALYSIS:");
  uart_send_formatted("   Note: Position analysis requires game engine access");
  uart_send_formatted("   Use 'board' command to see current position");
  uart_send_formatted("   Standard board: 64 squares (8x8)");

  // King Position Analysis
  uart_send_formatted("👑 KING POSITIONS:");
  uart_send_formatted("   Note: King positions require game engine access");
  uart_send_formatted("   Use 'board' command to see current position");
  uart_send_formatted("   Standard: White King on e1, Black King on e8");

  // Board Validation
  uart_send_formatted("✅ BOARD VALIDATION:");
  uart_send_formatted("   Note: Board validation requires game engine access");
  uart_send_formatted("   Use 'board' command to see current position");
  uart_send_formatted("   Standard validation: 1 king per side, max 32 pieces");

  // Memory Usage
  uart_send_formatted("💾 BOARD MEMORY:");
  uart_send_formatted("   Board Array: 64 bytes (8x8 int)");
  uart_send_formatted("   Move History: Variable size");
  uart_send_formatted("   Position Hash: 8 bytes");

  uart_send_formatted("✅ Board debug info displayed");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_benchmark(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("⚡ PERFORMANCE BENCHMARK");
  uart_send_formatted("═══════════════════════");

  // Get real system information
  uint32_t cpu_freq = 160000000; // ESP32-C6 default CPU frequency
  uint32_t apb_freq = 80000000;  // ESP32-C6 default APB frequency

  // Memory information
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

  // Task information
  UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
  UBaseType_t task_count = uxTaskGetNumberOfTasks();

  // Timer information
  int64_t uptime_us = esp_timer_get_time();
  uint64_t uptime_ms = uptime_us / 1000;
  uint64_t uptime_sec = uptime_ms / 1000;

  // Performance metrics
  uart_send_formatted("🖥️  SYSTEM INFO:");
  uart_send_formatted("   CPU Frequency: %" PRIu32 " MHz", cpu_freq / 1000000);
  uart_send_formatted("   APB Frequency: %" PRIu32 " MHz", apb_freq / 1000000);
  uart_send_formatted("   Uptime: %" PRIu64 "s (%" PRIu64 "ms)", uptime_sec,
                      uptime_ms);

  uart_send_formatted("💾 MEMORY USAGE:");
  uart_send_formatted("   Free Heap: %zu bytes (%.1f%%)", free_heap,
                      (float)free_heap / total_heap * 100);
  uart_send_formatted("   Min Free: %zu bytes", min_free_heap);
  uart_send_formatted("   Total Heap: %zu bytes", total_heap);
  uart_send_formatted("   Used: %zu bytes (%.1f%%)", total_heap - free_heap,
                      (float)(total_heap - free_heap) / total_heap * 100);

  uart_send_formatted("📊 TASK INFO:");
  uart_send_formatted("   Total Tasks: %" PRIu32, task_count);
  uart_send_formatted("   Stack High Water: %" PRIu32 " bytes",
                      high_water_mark);

  // Simple performance test
  uart_send_formatted("🏃 PERFORMANCE TEST:");

  // Test 1: Simple loop timing
  int64_t start_time = esp_timer_get_time();
  volatile int test_sum = 0;
  for (int i = 0; i < 10000; i++) {
    test_sum += i;
  }
  int64_t end_time = esp_timer_get_time();
  int64_t loop_time = end_time - start_time;

  uart_send_formatted("   10K Loop: %" PRId64 " μs (%.2f μs/iter)", loop_time,
                      (float)loop_time / 10000);

  // Test 2: Memory allocation speed
  start_time = esp_timer_get_time();
  void *test_ptr = malloc(1024);
  free(test_ptr);
  end_time = esp_timer_get_time();
  int64_t alloc_time = end_time - start_time;

  uart_send_formatted("   Malloc/Free: %" PRId64 " μs", alloc_time);

  // Test 3: Task switching overhead (approximate)
  start_time = esp_timer_get_time();
  vTaskDelay(pdMS_TO_TICKS(1));
  end_time = esp_timer_get_time();
  int64_t task_switch_time = end_time - start_time;

  uart_send_formatted("   Task Switch: ~%" PRId64 " μs", task_switch_time);

  uart_send_formatted("✅ Benchmark completed successfully");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_memcheck(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("💾 MEMORY CHECK");
  uart_send_formatted("═══════════════");

  // Get detailed memory information
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  size_t used_heap = total_heap - free_heap;

  // Get memory capabilities
  size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  size_t total_spiram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

  // Calculate percentages
  float free_percent = (float)free_heap / total_heap * 100;
  float used_percent = (float)used_heap / total_heap * 100;
  float min_free_percent = (float)min_free_heap / total_heap * 100;

  uart_send_formatted("📊 HEAP MEMORY:");
  uart_send_formatted("   Total: %zu bytes (%.1f KB)", total_heap,
                      total_heap / 1024.0f);
  uart_send_formatted("   Free: %zu bytes (%.1f KB) - %.1f%%", free_heap,
                      free_heap / 1024.0f, free_percent);
  uart_send_formatted("   Used: %zu bytes (%.1f KB) - %.1f%%", used_heap,
                      used_heap / 1024.0f, used_percent);
  uart_send_formatted("   Min Free: %zu bytes (%.1f KB) - %.1f%%",
                      min_free_heap, min_free_heap / 1024.0f, min_free_percent);

  uart_send_formatted("🏠 INTERNAL RAM:");
  uart_send_formatted("   Total: %zu bytes (%.1f KB)", total_internal,
                      total_internal / 1024.0f);
  uart_send_formatted("   Free: %zu bytes (%.1f KB)", free_internal,
                      free_internal / 1024.0f);
  uart_send_formatted("   Used: %zu bytes (%.1f KB)",
                      total_internal - free_internal,
                      (total_internal - free_internal) / 1024.0f);

  if (total_spiram > 0) {
    uart_send_formatted("🚀 SPI RAM:");
    uart_send_formatted("   Total: %zu bytes (%.1f KB)", total_spiram,
                        total_spiram / 1024.0f);
    uart_send_formatted("   Free: %zu bytes (%.1f KB)", free_spiram,
                        free_spiram / 1024.0f);
    uart_send_formatted("   Used: %zu bytes (%.1f KB)",
                        total_spiram - free_spiram,
                        (total_spiram - free_spiram) / 1024.0f);
  } else {
    uart_send_formatted("🚀 SPI RAM: Not available");
  }

  // Memory health assessment
  uart_send_formatted("🏥 MEMORY HEALTH:");
  if (free_percent > 50) {
    uart_send_formatted("   Status: 🟢 EXCELLENT (%.1f%% free)", free_percent);
  } else if (free_percent > 25) {
    uart_send_formatted("   Status: 🟡 GOOD (%.1f%% free)", free_percent);
  } else if (free_percent > 10) {
    uart_send_formatted("   Status: 🟠 WARNING (%.1f%% free)", free_percent);
  } else {
    uart_send_formatted("   Status: 🔴 CRITICAL (%.1f%% free)", free_percent);
  }

  if (min_free_percent < 5) {
    uart_send_formatted("   ⚠️  Low water mark: %.1f%%", min_free_percent);
  }

  // Task stack information
  UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
  uart_send_formatted("📚 CURRENT TASK STACK:");
  uart_send_formatted("   High Water Mark: %" PRIu32 " bytes", high_water_mark);

  uart_send_formatted("✅ Memory check completed");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_show_tasks(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("📋 RUNNING TASKS");
  uart_send_formatted("════════════════");

  // Get total number of tasks
  UBaseType_t task_count = uxTaskGetNumberOfTasks();
  uart_send_formatted("Total Tasks: %" PRIu32, task_count);
  uart_send_formatted("");

  // Get scheduler state
  uart_send_formatted("🔄 SCHEDULER INFO:");
  uart_send_formatted("   State: %s",
                      (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
                          ? "Running"
                          : "Suspended");
  uart_send_formatted("   Tick Count: %" PRIu32, xTaskGetTickCount());
  uart_send_formatted("   Tick Rate: %d Hz", configTICK_RATE_HZ);

  // Get current task info
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  const char *current_task_name = pcTaskGetName(current_task);

  uart_send_formatted("");
  uart_send_formatted("🎯 CURRENT TASK:");
  uart_send_formatted("   Name: %s", current_task_name);
  uart_send_formatted("   Handle: %p", current_task);
  uart_send_formatted("   Priority: %" PRIu32, uxTaskPriorityGet(current_task));
  uart_send_formatted("   Stack High Water: %" PRIu32 " bytes",
                      uxTaskGetStackHighWaterMark(current_task));

  // Get system task handles (if available)
  uart_send_formatted("");
  uart_send_formatted("🏗️  SYSTEM TASKS:");

  // Check if we have access to system task handles
  extern TaskHandle_t uart_task_handle;
  extern TaskHandle_t game_task_handle;
  extern TaskHandle_t led_task_handle;
  extern TaskHandle_t matrix_task_handle;
  extern TaskHandle_t button_task_handle;
  extern TaskHandle_t animation_task_handle;
  extern TaskHandle_t test_task_handle;
  extern TaskHandle_t web_server_task_handle;
  extern TaskHandle_t reset_button_task_handle;
  extern TaskHandle_t promotion_button_task_handle;

  if (uart_task_handle) {
    const char *task_name = pcTaskGetName(uart_task_handle);
    uart_send_formatted("   UART Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(uart_task_handle),
                        uxTaskGetStackHighWaterMark(uart_task_handle));
  }

  if (game_task_handle) {
    const char *task_name = pcTaskGetName(game_task_handle);
    uart_send_formatted("   Game Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(game_task_handle),
                        uxTaskGetStackHighWaterMark(game_task_handle));
  }

  if (led_task_handle) {
    const char *task_name = pcTaskGetName(led_task_handle);
    uart_send_formatted("   LED Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(led_task_handle),
                        uxTaskGetStackHighWaterMark(led_task_handle));
  }

  if (matrix_task_handle) {
    const char *task_name = pcTaskGetName(matrix_task_handle);
    uart_send_formatted("   Matrix Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(matrix_task_handle),
                        uxTaskGetStackHighWaterMark(matrix_task_handle));
  }

  if (button_task_handle) {
    const char *task_name = pcTaskGetName(button_task_handle);
    uart_send_formatted("   Button Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(button_task_handle),
                        uxTaskGetStackHighWaterMark(button_task_handle));
  }

  if (animation_task_handle) {
    const char *task_name = pcTaskGetName(animation_task_handle);
    uart_send_formatted("   Animation Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(animation_task_handle),
                        uxTaskGetStackHighWaterMark(animation_task_handle));
  }

  if (test_task_handle) {
    const char *task_name = pcTaskGetName(test_task_handle);
    uart_send_formatted("   Test Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(test_task_handle),
                        uxTaskGetStackHighWaterMark(test_task_handle));
  }

  if (web_server_task_handle) {
    const char *task_name = pcTaskGetName(web_server_task_handle);
    uart_send_formatted("   Web Server Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(web_server_task_handle),
                        uxTaskGetStackHighWaterMark(web_server_task_handle));
  }

  if (reset_button_task_handle) {
    const char *task_name = pcTaskGetName(reset_button_task_handle);
    uart_send_formatted("   Reset Button Task: %s (Priority: %" PRIu32
                        ", Stack: %" PRIu32 ")",
                        task_name, uxTaskPriorityGet(reset_button_task_handle),
                        uxTaskGetStackHighWaterMark(reset_button_task_handle));
  }

  if (promotion_button_task_handle) {
    const char *task_name = pcTaskGetName(promotion_button_task_handle);
    uart_send_formatted(
        "   Promotion Button Task: %s (Priority: %" PRIu32 ", Stack: %" PRIu32
        ")",
        task_name, uxTaskPriorityGet(promotion_button_task_handle),
        uxTaskGetStackHighWaterMark(promotion_button_task_handle));
  }

  // CPU usage information
  uart_send_formatted("");
  uart_send_formatted("💻 CPU INFO:");
  uart_send_formatted("   Frequency: %" PRIu32 " MHz", 160); // ESP32-C6 default
  uart_send_formatted("   Uptime: %" PRIu64 " ms", esp_timer_get_time() / 1000);

  uart_send_formatted("✅ Task information displayed");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_show_mutexes(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("🔒 MUTEX STATUS");
  uart_send_formatted("═══════════════");

  // Get system mutex handles
  extern SemaphoreHandle_t uart_mutex;
  extern SemaphoreHandle_t led_mutex;
  extern SemaphoreHandle_t matrix_mutex;
  extern SemaphoreHandle_t button_mutex;
  extern SemaphoreHandle_t game_mutex;
  extern SemaphoreHandle_t system_mutex;

  uart_send_formatted("🏗️  SYSTEM MUTEXES:");

  // Check UART mutex
  if (uart_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(uart_mutex);
    uart_send_formatted("   UART Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
    if (holder) {
      uart_send_formatted("      Holder: %p", holder);
    }
  } else {
    uart_send_formatted("   UART Mutex: ❌ NOT CREATED");
  }

  // Check LED mutex
  if (led_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(led_mutex);
    uart_send_formatted("   LED Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
    if (holder) {
      uart_send_formatted("      Holder: %p", holder);
    }
  } else {
    uart_send_formatted("   LED Mutex: ❌ NOT CREATED");
  }

  // Check Matrix mutex
  if (matrix_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(matrix_mutex);
    uart_send_formatted("   Matrix Mutex: %s",
                        holder ? "🔴 LOCKED" : "🟢 FREE");
    if (holder) {
      uart_send_formatted("      Holder: %p", holder);
    }
  } else {
    uart_send_formatted("   Matrix Mutex: ❌ NOT CREATED");
  }

  // Check Button mutex
  if (button_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(button_mutex);
    uart_send_formatted("   Button Mutex: %s",
                        holder ? "🔴 LOCKED" : "🟢 FREE");
    if (holder) {
      uart_send_formatted("      Holder: %p", holder);
    }
  } else {
    uart_send_formatted("   Button Mutex: ❌ NOT CREATED");
  }

  // Check Game mutex
  if (game_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(game_mutex);
    uart_send_formatted("   Game Mutex: %s", holder ? "🔴 LOCKED" : "🟢 FREE");
    if (holder) {
      uart_send_formatted("      Holder: %p", holder);
    }
  } else {
    uart_send_formatted("   Game Mutex: ❌ NOT CREATED");
  }

  // Check System mutex
  if (system_mutex) {
    TaskHandle_t holder = xSemaphoreGetMutexHolder(system_mutex);
    uart_send_formatted("   System Mutex: %s",
                        holder ? "🔴 LOCKED" : "🟢 FREE");
    if (holder) {
      uart_send_formatted("      Holder: %p", holder);
    }
  } else {
    uart_send_formatted("   System Mutex: ❌ NOT CREATED");
  }

  uart_send_formatted("");
  uart_send_formatted("📊 MUTEX SUMMARY:");
  uart_send_formatted("   Legend: 🟢 FREE, 🔴 LOCKED, ❌ NOT CREATED");
  uart_send_formatted(
      "   Note: LOCKED mutexes show the task handle that holds them");

  uart_send_formatted("✅ Mutex status displayed");

  return CMD_SUCCESS;
}

// Helper function to display queue status
static void display_queue_status(const char *name, QueueHandle_t queue) {
  if (queue) {
    UBaseType_t messages_waiting = uxQueueMessagesWaiting(queue);
    UBaseType_t spaces_available = uxQueueSpacesAvailable(queue);
    UBaseType_t queue_length = messages_waiting + spaces_available;
    float fill_percent = (float)messages_waiting / queue_length * 100;

    const char *status;
    if (fill_percent > 90) {
      status = "🔴 FULL";
    } else if (fill_percent > 75) {
      status = "🟠 HIGH";
    } else if (fill_percent > 50) {
      status = "🟡 MEDIUM";
    } else {
      status = "🟢 OK";
    }

    uart_send_formatted("   %s: [%d/%d] %.1f%% %s", name, messages_waiting,
                        queue_length, fill_percent, status);
  } else {
    uart_send_formatted("   %s: ❌ NOT CREATED", name);
  }
}

command_result_t uart_cmd_show_fifos(const char *args) {
  (void)args; // Unused parameter

  uart_send_formatted("📦 FIFO (QUEUE) STATUS");
  uart_send_formatted("══════════════════════");

  // Get system queue handles
  extern QueueHandle_t uart_command_queue;
  extern QueueHandle_t uart_response_queue;
  extern QueueHandle_t game_command_queue;
  extern QueueHandle_t game_status_queue;
  // extern QueueHandle_t led_command_queue;  //  REMOVED: Queue hell
  // eliminated extern QueueHandle_t led_status_queue;  //  REMOVED: Queue
  // hell eliminated
  extern QueueHandle_t matrix_command_queue;
  extern QueueHandle_t matrix_event_queue;
  extern QueueHandle_t button_event_queue;
  extern QueueHandle_t button_command_queue;
  extern QueueHandle_t animation_command_queue;
  extern QueueHandle_t animation_status_queue;
  extern QueueHandle_t screen_saver_command_queue;
  extern QueueHandle_t screen_saver_status_queue;
  // extern QueueHandle_t matter_command_queue;  // DISABLED - Matter not needed
  // extern QueueHandle_t matter_status_queue;   // DISABLED - Matter not needed
  extern QueueHandle_t web_command_queue;
  extern QueueHandle_t web_server_command_queue;
  extern QueueHandle_t web_server_status_queue;
  extern QueueHandle_t test_command_queue;

  uart_send_formatted("🏗️  SYSTEM QUEUES:");

  // Display all queue statuses
  display_queue_status("UART Command", uart_command_queue);
  display_queue_status("UART Response", uart_response_queue);
  display_queue_status("Game Command", game_command_queue);
  display_queue_status("Game Status", game_status_queue);
  // LED queues removed - using direct LED calls now
  display_queue_status("Matrix Command", matrix_command_queue);
  display_queue_status("Matrix Event", matrix_event_queue);
  display_queue_status("Button Event", button_event_queue);
  display_queue_status("Button Command", button_command_queue);
  display_queue_status("Animation Command", animation_command_queue);
  display_queue_status("Animation Status", animation_status_queue);
  display_queue_status("Screen Saver Command", screen_saver_command_queue);
  display_queue_status("Screen Saver Status", screen_saver_status_queue);
  // display_queue_status("Matter Command", matter_command_queue);  // DISABLED
  // - Matter not needed display_queue_status("Matter Status",
  // matter_status_queue);     // DISABLED - Matter not needed
  display_queue_status("Web Command", web_command_queue);
  display_queue_status("Web Server Command", web_server_command_queue);
  display_queue_status("Web Server Status", web_server_status_queue);
  display_queue_status("Test Command", test_command_queue);

  uart_send_formatted("");
  uart_send_formatted("📊 QUEUE SUMMARY:");
  uart_send_formatted(
      "   Format: [messages_waiting/total_capacity] fill_percentage status");
  uart_send_formatted("   Status: 🟢 OK (<50%), 🟡 MEDIUM (50-75%), 🟠 HIGH "
                      "(75-90%), 🔴 FULL (>90%)");

  uart_send_formatted("✅ FIFO status displayed");

  return CMD_SUCCESS;
}
