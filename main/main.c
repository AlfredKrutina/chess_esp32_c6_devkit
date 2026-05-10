/**
 * @file main.c
 * @brief ESP32-C6 Chess System: vstupni bod, fronty, tasky, boot a NVS
 * spoluprace.
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * Tento soubor spousti app_main(), vytvori FreeRTOS fronty a mutexy, nastavi
 * WDT a spusti systemove tasky (LED, matrix, button, game, UART, web, test).
 * Animation task je vypnuty (LED animace v led_task). Po kratsim cekani probiha
 * centralizovana boot animace; nasleduje initialize_chess_game(), ktere
 * respektuje obnovu ulozene hry z NVS v game_task.
 *
 * @subsection ss_queues Fronty
 * - game_command_queue: prikazy pro game_task (UART, matrix, web).
 * - button_event_queue: udalosti z tlacitek (ISR -> button_task).
 *
 * @subsection ss_priorities Priority (orientacne)
 * led_task (7) > matrix_task (6) > button_task (5) > game_task (4) >
 * uart / web (3) > test_task (1) > IDLE (0).
 *
 * @subsection ss_boot_nvs Boot a NVS
 * game_task_start() drive nez skonci boot animace: inicializuje desku, pripadne
 * nacte snapshot z NVS nebo spusti novou hru podle boot trackeru. Po fade-out
 * vola main initialize_chess_game(): pokud uz je stav nacteny z NVS nebo uz
 * bezela nucena nova hra, neposila se GAME_CMD_NEW_GAME (jinak by se prepisla
 * obnova). Pri obnove bez matrix guard se doplni LED pres game_refresh_leds().
 *
 * @warning Fronty musi existovat pred xTaskCreate. Kontroluj navratove hodnoty
 * xQueueCreate a xTaskCreate.
 */

// #include "animation_task.h"  // DISABLED - animation task not used (LED v led_task)
#include "button_task.h"
#include "chess_types.h"
#include "sdkconfig.h"
// #include "driver/uart.h" // UNUSED
// #include "driver/uart_vfs.h" // UNUSED
#include "esp_console.h"
#include "esp_log.h"
// #include "esp_random.h" // UNUSED
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
// #include "esp_vfs_dev.h" // UNUSED
// #include "freertos/FreeRTOS.h" // UNUSED
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos_chess.h"
#include "game_task.h"
#include "led_task.h"
#include "matrix_task.h"
#include "board_api_auth.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "stm32_i2c_bl.h"
#if CONFIG_CHESS_ENABLE_TEST_TASK
#include "test_task.h"
#endif
#include "uart_task.h"
// #include <inttypes.h> // UNUSED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include "matter_task.h"  // DISABLED - Matter neni podporovan touhle verzi
// FW
// #include "config_manager.h" // UNUSED
#include "ble_task.h"
#include "game_led_animations.h"
#include "ha_light_task.h"
#include "uart_commands_extended.h"
#include "web_server_task.h"

// ============================================================================
// RESET REASON DIAGNOSTICS (PRODUCTION STABILITY)
// ============================================================================

static const char *reset_reason_to_str(esp_reset_reason_t reason) {
  switch (reason) {
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_EXT:
    return "EXT (external reset pin)";
  case ESP_RST_SW:
    return "SW (esp_restart)";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INT_WDT";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT";
  case ESP_RST_WDT:
    return "WDT (other watchdog)";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "UNKNOWN";
  }
}

static const char *TAG = "MAIN";

static void main_mark_ota_app_valid_if_needed(void) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == NULL) {
    return;
  }
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(running, &st) != ESP_OK) {
    return;
  }
  if (st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "OTA: app marked valid (cancel rollback): %s",
             esp_err_to_name(e));
  }
}

// ============================================================================
// BOOT CYCLE COUNTER FOR DEEP SLEEP PROTECTION
// ============================================================================

/** @brief NVS namespace pro boot counter */
#define BOOT_COUNTER_NAMESPACE "boot_counter"
/** @brief NVS klíč pro počítadlo bootů */
#define BOOT_COUNTER_KEY "count"
/** @brief NVS klíč pro timestamp posledního bootu */
#define BOOT_COUNTER_TIME_KEY "last_time"
/** @brief Počet bootů před deep sleep (10) */
#define BOOT_CYCLES_LIMIT 10
/** @brief Časové okno pro počítání bootů (30 sekund) */
#define BOOT_CYCLES_WINDOW_MS 30000

/**
 * @brief Struktura pro uložení stavu boot counteru
 */
typedef struct {
  uint8_t count;          ///< Počítadlo bootů
  uint32_t last_boot_ms;  ///< Čas posledního bootu v ms
} boot_counter_state_t;

/**
 * @brief Načte stav boot counteru z NVS
 * @param[out] state Ukazatel na strukturu pro načtení stavu
 * @return ESP_OK při úspěchu
 */
static esp_err_t boot_counter_load(boot_counter_state_t *state) {
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(BOOT_COUNTER_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    state->count = 0;
    state->last_boot_ms = 0;
    return ESP_OK; // Vrátíme defaultní hodnoty
  }

  uint8_t count = 0;
  ret = nvs_get_u8(nvs_handle, BOOT_COUNTER_KEY, &count);
  if (ret != ESP_OK) {
    count = 0;
  }

  uint32_t last_time = 0;
  ret = nvs_get_u32(nvs_handle, BOOT_COUNTER_TIME_KEY, &last_time);
  if (ret != ESP_OK) {
    last_time = 0;
  }

  nvs_close(nvs_handle);

  state->count = count;
  state->last_boot_ms = last_time;
  return ESP_OK;
}

/**
 * @brief Uloží stav boot counteru do NVS
 * @param state Ukazatel na strukturu se stavem k uložení
 * @return ESP_OK při úspěchu
 */
static esp_err_t boot_counter_save(const boot_counter_state_t *state) {
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(BOOT_COUNTER_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = nvs_set_u8(nvs_handle, BOOT_COUNTER_KEY, state->count);
  if (ret != ESP_OK) {
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_u32(nvs_handle, BOOT_COUNTER_TIME_KEY, state->last_boot_ms);
  if (ret != ESP_OK) {
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
  return ret;
}

/**
 * @brief Aktualizuje boot counter a kontroluje zda by se mělo jít do deep sleep
 * @return true pokud by se mělo jít do deep sleep (překročen limit bootů)
 */
static bool boot_counter_check_and_update(void) {
  boot_counter_state_t state;
  boot_counter_load(&state);

  uint32_t current_time_ms = esp_timer_get_time() / 1000;

  // Kontrola zda jsme v časovém okně (30s od posledního bootu)
  if (current_time_ms - state.last_boot_ms > BOOT_CYCLES_WINDOW_MS) {
    // Jsme mimo okno - reset počítadla
    state.count = 1;
    state.last_boot_ms = current_time_ms;
    boot_counter_save(&state);
    ESP_LOGI(TAG, "Boot counter: reset (outside 30s window), count=1");
    return false;
  }

  // Jsme v okně - inkrementovat počítadlo
  state.count++;
  state.last_boot_ms = current_time_ms;
  boot_counter_save(&state);

  ESP_LOGI(TAG, "Boot counter: count=%d/%d (within 30s window)",
           state.count, BOOT_CYCLES_LIMIT);

  // Kontrola limitu
  if (state.count >= BOOT_CYCLES_LIMIT) {
    ESP_LOGW(TAG, "Boot counter: LIMIT REACHED (%d boots in 30s)",
             state.count);
    return true;
  }

  return false;
}

/**
 * @brief Resetuje boot counter (volat po úspěšném startu)
 */
static void boot_counter_reset(void) {
  boot_counter_state_t state = {0, 0};
  boot_counter_save(&state);
  ESP_LOGI(TAG, "Boot counter: reset after successful boot");
}

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void show_boot_animation_and_board(void);

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecny reset WDT s logovanim WARNING misto ERROR pro
 * ESP_ERR_NOT_FOUND
 *
 * Tato funkce bezpecne resetuje Task Watchdog Timer. Pokud task neni jeste
 * registrovany (coz je normalni behem startupu), loguje se WARNING misto ERROR.
 *
 * @return ESP_OK pokud uspesne, ESP_ERR_NOT_FOUND pokud task neni registrovany
 * (WARNING pouze)
 */
static esp_err_t main_task_wdt_reset_safe(void) {
  esp_err_t ret = esp_task_wdt_reset();

  if (ret == ESP_ERR_NOT_FOUND) {
    // Log as WARNING instead of ERROR - task not registered yet
    ESP_LOGW(
        TAG,
        "WDT reset: task not registered yet (this is normal during startup)");
    return ESP_OK; // Treat as success for our purposes
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}

/** @brief Globalni UART mutex pro cisty vystup */
SemaphoreHandle_t uart_mutex = NULL;

/** @brief Handle pro LED task */
TaskHandle_t led_task_handle = NULL;
/** @brief Handle pro Matrix task */
TaskHandle_t matrix_task_handle = NULL;
/** @brief Handle pro Button task */
TaskHandle_t button_task_handle = NULL;
/** @brief Handle pro UART task */
TaskHandle_t uart_task_handle = NULL;
/** @brief Handle pro Game task */
TaskHandle_t game_task_handle = NULL;
/** @brief Handle pro Animation task (NULL pokud je task vypnuty v create_system_tasks) */
TaskHandle_t animation_task_handle = NULL;
/** @brief Handle pro Test task */
TaskHandle_t test_task_handle = NULL;
// TaskHandle_t matter_task_handle = NULL;  // DISABLED - Matter not needed
/** @brief Handle pro Web Server task */
TaskHandle_t web_server_task_handle = NULL;
/** @brief Handle pro HA Light task */
TaskHandle_t ha_light_task_handle = NULL;
/** @brief Handle pro Reset Button task */
TaskHandle_t reset_button_task_handle = NULL;
/** @brief Handle pro Promotion Button task */
TaskHandle_t promotion_button_task_handle = NULL;

/** @brief Konfigurace demo modu - je demo mod zapnuty */
/** @brief Konfigurace demo modu - je demo mod zapnuty */
static volatile bool demo_mode_enabled = false;
/** @brief Zpozdeni mezi demo tahy v milisekundach - dynamicky meneno */
static uint32_t current_demo_delay_ms = 3000;

// Forward declaration
void demo_report_activity(void);

/** @brief Demo game 1: Scholar's Mate - White wins in 4 moves (fast checkmate)
 */
static const char *DEMO_GAME_WHITE_WIN[] = {
    "e2e4", "e7e5", // 1. e4 e5
    "f1c4", "b8c6", // 2. Bc4 Nc6
    "d1h5", "g8f6", // 3. Qh5 Nf6
    "h5f7"          // 4. Qxf7# - Checkmate! White wins
};

// Demo game definitions removed to save memory:
// - DEMO_GAME_BLACK_WIN (Fool's Mate)
// - DEMO_GAME_DRAW (Italian Game)

/** @brief Demo game 4: Opera Game - Morphy's Masterpiece (Paris 1858, White
 * wins) */
static const char *DEMO_GAME_OPERA[] = {
    "e2e4", "e7e5", // 1. e4 e5
    "g1f3", "d7d6", // 2. Nf3 d6
    "d2d4", "c8g4", // 3. d4 Bg4
    "d4e5", "g4f3", // 4. dxe5 Bxf3
    "d1f3", "d6e5", // 5. Qxf3 dxe5
    "f1c4", "g8f6", // 6. Bc4 Nf6
    "f3b3", "d8e7", // 7. Qb3 Qe7
    "b1c3", "c7c6", // 8. Nc3 c6
    "c1g5", "b7b5", // 9. Bg5 b5
    "c3b5", "c6b5", // 10. Nxb5 cxb5
    "c4b5", "b8d7", // 11. Bxb5+ Nbd7
    "e1c1", "a8d8", // 12. O-O-O Rd8
    "d1d7", "d8d7", // 13. Rxd7 Rxd7
    "h1d1", "e7e6", // 14. Rd1 Qe6
    "b5d7", "f6d7", // 15. Bxd7+ Nxd7
    "b3b8", "d7b8", // 16. Qb8+! Nxb8
    "d1d8"          // 17. Rd8# - Checkmate! Beautiful queen sacrifice
};

/** @brief Array of all demo games */
static const char **DEMO_GAMES[] = {DEMO_GAME_WHITE_WIN, DEMO_GAME_OPERA};

/** @brief Number of moves in each demo game */
static const int DEMO_GAME_LENGTHS[] = {
    sizeof(DEMO_GAME_WHITE_WIN) / sizeof(DEMO_GAME_WHITE_WIN[0]),
    sizeof(DEMO_GAME_OPERA) / sizeof(DEMO_GAME_OPERA[0])};

/** @brief Game type names for logging */
static const char *DEMO_GAME_NAMES[] = {"Scholar's Mate (White Wins)",
                                        "Opera Game (Morphy 1858)"};

/** @brief Total number of demo games */
static const int DEMO_GAMES_COUNT = sizeof(DEMO_GAMES) / sizeof(DEMO_GAMES[0]);

/** @brief Current demo game being played (0-2) */
static int current_demo_game = 0;

/** @brief Current move index within the current game */
static int demo_move_index = 0;

/** @brief Current game's move array (pointer to active game) */
static const char **current_demo_moves = DEMO_GAME_WHITE_WIN;

/** @brief Current game's move count */
static int demo_moves_count =
    sizeof(DEMO_GAME_WHITE_WIN) / sizeof(DEMO_GAME_WHITE_WIN[0]);

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Inicializuje hlavni systemove komponenty aplikace
 *
 * Tato funkce inicializuje vsechny systemove komponenty potrebne pro chod
 * sachoveho systemu. Vytvari mutexy, inicializuje FreeRTOS chess komponentu,
 * spousti timery a overuje dostupnost vsech front.
 *
 * @return ESP_OK pri uspechu, ESP_FAIL pri chybe
 */
esp_err_t main_system_init(void) {
  ESP_LOGI(TAG, "🔧 Initializing chess system components...");

  // Create UART mutex for clean output
  uart_mutex = xSemaphoreCreateMutex();
  if (uart_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create UART mutex");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "✅ UART mutex created");

  // Initialize FreeRTOS chess component (queues)
  ESP_LOGI(TAG, "🔄 Initializing FreeRTOS chess component...");
  esp_err_t ret = chess_system_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize FreeRTOS chess component: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ FreeRTOS chess component initialized");

  // Start FreeRTOS timers
  ESP_LOGI(TAG, "🔄 Starting FreeRTOS timers...");
  ret = chess_start_timers();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Timer start failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ FreeRTOS timers started successfully");

  // LED system will be initialized automatically when LED task starts

  // Verify all queues are available
  if (game_command_queue == NULL) {
    ESP_LOGE(TAG, "Game command queue not available");
    return ESP_FAIL;
  }
  // LED command queue removed - using direct LED calls
  ESP_LOGI(TAG, "LED system using direct calls (no queue)");
  if (matrix_command_queue == NULL) {
    ESP_LOGE(TAG, "Matrix command queue not available");
    return ESP_FAIL;
  }
  if (button_event_queue == NULL) {
    ESP_LOGE(TAG, "Button event queue not available");
    return ESP_FAIL;
  }

  // Verify UART queues
  if (uart_command_queue == NULL) {
    ESP_LOGE(TAG, "UART command queue not available");
    return ESP_FAIL;
  }
  if (uart_response_queue == NULL) {
    ESP_LOGE(TAG, "UART response queue not available");
    return ESP_FAIL;
  }

  // DISABLED: Verify Matter queues - Matter not needed
  /*
  if (matter_command_queue == NULL || matter_status_queue == NULL) {
      ESP_LOGE(TAG, "Matter queues not available");
      return ESP_FAIL;
  }
  */

  // Verify Web server queues
  if (web_command_queue == NULL || web_server_status_queue == NULL) {
    ESP_LOGE(TAG, "Web server queues not available");
    return ESP_FAIL;
  }

#if CONFIG_CHESS_ENABLE_TEST_TASK
  if (test_command_queue == NULL) {
    ESP_LOGE(TAG, "Test command queue not available");
    return ESP_FAIL;
  }
#endif

  // Verify Animation queues
  if (animation_command_queue == NULL || animation_status_queue == NULL) {
    ESP_LOGE(TAG, "Animation queues not available");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "✅ All system queues verified");

  // Initialize endgame animation system
  ESP_LOGI(TAG, "🔄 Initializing endgame animation system...");
  ret = init_endgame_animation_system();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize endgame animation system: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ Endgame animation system initialized");

  // Register extended UART commands
  ESP_LOGI(TAG, "🔄 Registering extended UART commands...");
  ret = register_extended_uart_commands();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register extended UART commands: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ Extended UART commands registered");

  ble_task_init();

  return ESP_OK;
}

// ============================================================================
// STARTUP SEQUENCE FUNCTIONS
// ============================================================================

/**
 * @brief Dokonci start hry po boot animaci: volitelny GAME_CMD_NEW_GAME, LED,
 * tlacitka.
 *
 * @details
 * Volat az po show_boot_animation_and_board() (flag led_is_booting() je uz
 * false). game_task_start() drive nastavil desku a pripadne nacetl NVS snapshot
 * nebo game_start_new_game() podle boot trackeru.
 *
 * - @c nvs_restored: neposilat GAME_CMD_NEW_GAME; stav pochazi z NVS. Po
 * fade-out zavolat game_refresh_leds() jen kdyz neni aktivni matrix guard
 * (ochrana LED pri nesouladu matice s ulozenou pozici).
 * - @c boot_already_new: boot tracker uz spustil novou hru v game_task;
 * neposilat duplicitni GAME_CMD_NEW_GAME.
 * - Jinak poslat GAME_CMD_NEW_GAME pro plny reset jako drive (zadny platny
 * snapshot).
 *
 * Vzdy vola led_update_button_availability_from_game().
 *
 * @note game_active po obnove NVS nastavuje game_load_snapshot_from_nvs();
 * drive to delal az prikaz NEW_GAME z teto funkce.
 *
 * @see game_was_snapshot_loaded_on_boot()
 * @see game_was_boot_new_game_triggered()
 * @see game_is_matrix_guard_active()
 * @see game_refresh_leds()
 */
void initialize_chess_game(void) {
  const bool nvs_restored = game_was_snapshot_loaded_on_boot();
  const bool boot_already_new = game_was_boot_new_game_triggered();

  if (nvs_restored) {
    ESP_LOGI(TAG, "[staging] Chess init: snapshot from NVS kept — skip "
                  "GAME_CMD_NEW_GAME");
  } else if (boot_already_new) {
    ESP_LOGI(TAG, "[staging] Chess init: boot rule already ran new game — skip "
                  "duplicate GAME_CMD_NEW_GAME");
  } else {
    ESP_LOGI(TAG, "🎯 Starting new chess game...");

    if (game_command_queue != NULL) {
      chess_move_command_t cmd = {0};
      cmd.type = GAME_CMD_NEW_GAME;

      if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to send GAME_CMD_NEW_GAME");
      } else {
        ESP_LOGI(TAG, "✅ New game command sent");
      }
    } else {
      ESP_LOGE(TAG, "❌ Game command queue not available");
    }
  }

  extern void led_update_button_availability_from_game(void);
  led_update_button_availability_from_game();

  /* Po fade-out je deska prazdna; drive NEW_GAME spustilo highlight v
   * game_task. */
  if (nvs_restored && !game_is_matrix_guard_active()) {
    game_refresh_leds();
  }

  ESP_LOGI(TAG, "🎯 Game ready! White to move.");
  ESP_LOGI(TAG, "💡 Type 'HELP' for available commands");
  ESP_LOGI(TAG, "💡 Type 'DEMO ON' to enable automatic play");
}

/**
 * @brief Prepina demo mod zapnuto/vypnuto
 *
 * Tato funkce umoznuje zapnout nebo vypnout demo mod, ktery automaticky
 * hraje preddefinovane tahy. Uzivatel muze zapnout demo mod prikazem
 * "DEMO ON" a vypnout prikazem "DEMO OFF".
 *
 * @param enabled true pro zapnuti, false pro vypnuti
 *
 * @details
 * Demo mod umoznuje automaticke hrani preddefinovanych tahu.
 * Kdyz je zapnuty, system automaticky hraje tahy z pole DEMO_MOVES.
 * Kdyz je vypnuty, uzivatel muze hrat manualne.
 */
void toggle_demo_mode(bool enabled) {
  demo_mode_enabled = enabled;

  if (enabled) {
    ESP_LOGI(TAG, "🤖 SCREENSAVER MODE ENABLED");
    ESP_LOGI(TAG, "Automatic play will start with variable speed (0.7s - 4s)");
    ESP_LOGI(TAG, "Touch the board to interrupt!");

    // Clear game error recovery so demo moves are accepted (e.g. after "return
    // piece to e7")
    game_reset_error_recovery_state();

    // Reset demo state
    demo_move_index = 0;
    current_demo_game = 0; // Start from first game
    current_demo_moves = DEMO_GAMES[current_demo_game];
    demo_moves_count = DEMO_GAME_LENGTHS[current_demo_game];

    ESP_LOGI(TAG, "🔍 DEMO INIT DEBUG: Game=%d, MovesCount=%d, FirstMove=%s",
             current_demo_game, demo_moves_count,
             current_demo_moves ? current_demo_moves[0] : "NULL");

    // Ensure logs are flushed
    vTaskDelay(pdMS_TO_TICKS(10));

  } else {
    ESP_LOGI(TAG, "🤖 SCREENSAVER MODE DISABLED");
    ESP_LOGI(TAG, " returned to manual control");
  }
}

/**
 * @brief Report activity on the board (interrupt demo mode)
 * Called from matrix_task when any change is detected
 */
void demo_report_activity(void) {
  if (demo_mode_enabled) {
    demo_mode_enabled = false;
    ESP_LOGW(TAG, "✋ DEMO INTERRUPTED by user activity!");
    // Optional: Send feedback to Web/UART if needed
  }
}

/**
 * @brief Get demo mode status
 * @return true if enabled, false otherwise
 */
bool is_demo_mode_enabled(void) { return demo_mode_enabled; }

/**
 * @brief Set demo mode speed
 * @param speed_ms Delay between moves in milliseconds
 */
void set_demo_speed_ms(uint32_t speed_ms) {
  if (speed_ms < 500)
    speed_ms = 500;
  if (speed_ms > 10000)
    speed_ms = 10000;
  current_demo_delay_ms = speed_ms;
  ESP_LOGI(TAG, "⏱️ Demo speed set to %lu ms", current_demo_delay_ms);
}

/**
 * @brief Vykona jeden demo tah
 *
 * Tato funkce vykona jeden tah z preddefinovane sekvence demo tahu.
 * Tahy jsou ulozeny v poli DEMO_MOVES a jsou hrany postupne.
 * Po dokonceni vsech tahu se sekvence resetuje.
 *
 * @details
 * Funkce vezme aktualni tah z pole DEMO_MOVES a posle ho do game tasku.
 * Tahy jsou ve formatu "e2e4" (z pozice e2 na pozici e4).
 * Po dokonceni vsech tahu se index resetuje na 0.
 */
void execute_demo_move(void) {
  // Reset watchdog at function entry.
  // This function can block for 10+ seconds (castling), causing main task
  // timeout
  main_task_wdt_reset_safe();

  if (!demo_mode_enabled || demo_move_index >= demo_moves_count) {
    return;
  }

  const char *move = current_demo_moves[demo_move_index];

  // DEBUG LOG - Critical to verify function entry
  ESP_LOGI(TAG, "🤖 EXECUTE DEMO MOVE: Game='%s', Ind=%d/%d, Move=%s",
           DEMO_GAME_NAMES[current_demo_game], demo_move_index + 1,
           demo_moves_count, move);

  // Use PICKUP/DROP commands for realistic demo (like up/dn)
  if (strlen(move) == 4 && game_command_queue != NULL) {
    chess_move_command_t cmd = {0};

    // Step 1: PICKUP command (lift piece from source square)
    cmd.type = GAME_CMD_PICKUP;
    cmd.is_demo_mode = true;             // Skip resignation timer in demo mode.
    strncpy(cmd.from_notation, move, 2); // e.g. "e2"

    ESP_LOGI(TAG, "  ⬆️  Lifting piece from %c%c...", move[0], move[1]);

    // Set response queue for feedback
    cmd.response_queue = uart_response_queue;

    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGE(TAG, "❌ Failed to send GAME_CMD_PICKUP");
      return;
    }

    // Longer delay for Pickup animation
    vTaskDelay(pdMS_TO_TICKS(1500)); // 1.5 seconds
    main_task_wdt_reset_safe();      // Reset after delay.

    // Step 2: DROP command (place piece on destination square)
    cmd.type = GAME_CMD_DROP;
    cmd.is_demo_mode = true;               // Skip error recovery in demo mode.
    strncpy(cmd.to_notation, move + 2, 2); // e.g. "e4"

    ESP_LOGI(TAG, "  ⬇️  Placing piece on %c%c...", move[2], move[3]);

    // Set response queue for feedback
    cmd.response_queue = uart_response_queue;

    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGE(TAG, "❌ Failed to send GAME_CMD_DROP");
      return;
    }

    //  Wait for DROP animation
    vTaskDelay(pdMS_TO_TICKS(800)); // 0.8 seconds
    main_task_wdt_reset_safe();     // Reset after delay.

    // Step 3: CASTLING Handling
    // If move is castling, we must also move the rook!
    char rook_from[3] = {0};
    char rook_to[3] = {0};
    bool is_castling = false;

    if (strcmp(move, "e1g1") == 0) { // White King-side
      strcpy(rook_from, "h1");
      strcpy(rook_to, "f1");
      is_castling = true;
    } else if (strcmp(move, "e1c1") == 0) { // White Queen-side
      strcpy(rook_from, "a1");
      strcpy(rook_to, "d1");
      is_castling = true;
    } else if (strcmp(move, "e8g8") == 0) { // Black King-side
      strcpy(rook_from, "h8");
      strcpy(rook_to, "f8");
      is_castling = true;
    } else if (strcmp(move, "e8c8") == 0) { // Black Queen-side
      strcpy(rook_from, "a8");
      strcpy(rook_to, "d8");
      is_castling = true;
    }

    if (is_castling) {
      ESP_LOGI(TAG, "  ♜ Castling detected! Moving rook %s -> %s", rook_from,
               rook_to);
      vTaskDelay(pdMS_TO_TICKS(500)); // Pause before moving rook
      main_task_wdt_reset_safe();     // Reset during castling.

      // Rook Pickup
      memset(&cmd, 0, sizeof(cmd));
      cmd.type = GAME_CMD_PICKUP;
      cmd.is_demo_mode = true; // Skip resignation timer for rook.
      strncpy(cmd.from_notation, rook_from, 2);
      cmd.response_queue = uart_response_queue; // Add feedback.
      xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
      vTaskDelay(pdMS_TO_TICKS(1000));
      main_task_wdt_reset_safe(); // Reset after rook pickup delay.

      // Rook Drop
      memset(&cmd, 0, sizeof(cmd));
      cmd.type = GAME_CMD_DROP;
      cmd.is_demo_mode = true; // Skip error recovery for rook.
      strncpy(cmd.to_notation, rook_to, 2);
      cmd.response_queue = uart_response_queue; // Add feedback.
      xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
      vTaskDelay(pdMS_TO_TICKS(800));
      main_task_wdt_reset_safe(); // Reset after rook drop delay.
    }
  }

  demo_move_index++;

  // Check if all moves were played (endgame reached).
  if (demo_move_index >= demo_moves_count) {
    ESP_LOGI(TAG, "🏁 Demo game '%s' complete! Endgame animations playing...",
             DEMO_GAME_NAMES[current_demo_game]);
    ESP_LOGI(TAG, "⏱️  Waiting 5 seconds for endgame animation to complete...");

    // Wait 5 seconds for endgame animation to play
    // Split into shorter delays with watchdog resets.
    for (int i = 0; i < 5; i++) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      main_task_wdt_reset_safe();
    }

    // Rotate to the next demo game.
    current_demo_game = (current_demo_game + 1) % DEMO_GAMES_COUNT;
    current_demo_moves = DEMO_GAMES[current_demo_game];
    demo_moves_count = DEMO_GAME_LENGTHS[current_demo_game];
    demo_move_index = 0;

    ESP_LOGI(TAG, "🔄 Starting new demo game: %s (%d moves)",
             DEMO_GAME_NAMES[current_demo_game], demo_moves_count);

    // Send reset command to start fresh game
    if (game_command_queue != NULL) {
      chess_move_command_t cmd = {.type = GAME_CMD_NEW_GAME};
      cmd.player = (demo_move_index % 2 == 0) ? PLAYER_WHITE : PLAYER_BLACK;

      // Set response queue so game_task sends confirmation (like UART
      // commands).
      cmd.response_queue = uart_response_queue;

      xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));

      // Wait for the game reset to complete before starting the first move.
      ESP_LOGI(TAG, "⏱️  Waiting 1 second for game reset to complete...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      main_task_wdt_reset_safe(); // Reset after game reset delay.
    }
  }
}

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Inicializace NVS (pro konfiguraci), konzole a UART / USB Serial JTAG.
 *
 * @details
 * NVS flash pro ulozeni konfigurace; esp_console pro prikazy; bez externiho
 * UART.
 */
static void init_console(void) {
  ESP_LOGI(TAG, "Initializing console...");

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  esp_err_t tok_ret = board_api_auth_init();
  if (tok_ret != ESP_OK) {
    ESP_LOGW(TAG, "board_api_auth_init: %s", esp_err_to_name(tok_ret));
  }

  // Initialize console (USB Serial JTAG only - no UART needed)
  ESP_LOGI(TAG,
           "Using USB Serial JTAG console - no UART initialization needed");

  // Initialize console
  esp_console_config_t console_config = {
      .max_cmdline_args = 8,
      .max_cmdline_length = 256,
  };
  ESP_ERROR_CHECK(esp_console_init(&console_config));

  // Register help command
  esp_console_register_help_command();

  ESP_LOGI(TAG, "Console initialized successfully");
}

/**
 * @brief Vytvori vsechny systemove tasky
 *
 * Tato funkce vytvori vsechny FreeRTOS tasky potrebne pro chod systemu.
 * Kazdy task ma svoji prioritu a velikost stacku. Po vytvoreni tasku
 * se zobrazi boot animace a inicializuje se sachova hra.
 *
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Funkce vytvori hlavni tasky:
 * - LED task: ovladani LED pasku
 * - Matrix task: skenovani 8x8 matice
 * - Button task: ovladani tlacitek
 * - UART task: komunikace pres UART
 * - Game task: logika sachove hry
 * - Test task: testovani systemu (volitelne menuconfig)
 * - Web server task: web rozhrani
 * (Animation task vypnut — viz DISABLED blok v create_system_tasks.)
 *
 * Po vytvoreni tasku se u auto-flash STM32 synchronne pocka na dokonceni flash
 * v matrix_task (pred WiFi/web), pak boot animace a inicializace hry.
 */
esp_err_t create_system_tasks(void) {
  ESP_LOGI(TAG, "Creating system tasks...");

  // Create LED task
  BaseType_t result = xTaskCreate((TaskFunction_t)led_task_start, "led_task",
                                  LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY,
                                  &led_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create LED task");
    return ESP_FAIL;
  }

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ LED task created successfully (%dKB stack) - will self-register "
           "with TWDT",
           LED_TASK_STACK_SIZE / 1024);

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT
  /* Semafor před matrix_task — main počká hned po matrix_task (viz níže), ne až po WiFi,
   * jinak web_server_task zaplaví sériovku a sdílené I²C zbytečně soupeří s bootloaderem. */
  stm32_i2c_bl_boot_flash_sync_prepare();
#endif

  // Create Matrix task
  result = xTaskCreate((TaskFunction_t)matrix_task_start, "matrix_task",
                       MATRIX_TASK_STACK_SIZE, NULL, MATRIX_TASK_PRIORITY,
                       &matrix_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Matrix task");
    return ESP_FAIL;
  }

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ Matrix task created successfully (%dKB stack) - will "
           "self-register with TWDT",
           MATRIX_TASK_STACK_SIZE / 1024);

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT
  ESP_LOGI(TAG,
           "[staging] Čekám na dokončení STM32 auto-flash (hned po matrix_task, "
           "před ostatní tasky / WiFi)…");
  stm32_i2c_bl_boot_flash_sync_wait(pdMS_TO_TICKS(120000));
  ESP_LOGI(TAG,
           "[staging] STM32 auto-flash pokus dokončen (sync s matrix_task) — "
           "úspěch/chyba viz STM32_AUTO / STM32_I2C_BL výše; pokračuji tasky a boot "
           "animace");
#endif

  // Create Button task
  result = xTaskCreate((TaskFunction_t)button_task_start, "button_task",
                       BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY,
                       &button_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Button task");
    return ESP_FAIL;
  }

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ Button task created successfully (%dKB stack) - will "
           "self-register with TWDT",
           BUTTON_TASK_STACK_SIZE / 1024);

  // Create UART task (but suspend it until after boot animation)
  result = xTaskCreate((TaskFunction_t)uart_task_start, "uart_task",
                       UART_TASK_STACK_SIZE, NULL, UART_TASK_PRIORITY,
                       &uart_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create UART task");
    return ESP_FAIL;
  }

  // Suspend UART task until after boot animation
  vTaskSuspend(uart_task_handle);

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ UART task created successfully (%dKB stack) - suspended until "
           "after boot animation, will self-register with TWDT",
           UART_TASK_STACK_SIZE / 1024);

  // Create Game task
  result = xTaskCreate((TaskFunction_t)game_task_start, "game_task",
                       GAME_TASK_STACK_SIZE, NULL, GAME_TASK_PRIORITY,
                       &game_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Game task");
    return ESP_FAIL;
  }

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ Game task created successfully (%dKB stack) - will self-register "
           "with TWDT",
           GAME_TASK_STACK_SIZE / 1024);

  // DISABLED: Create Animation task — fronty zustavaji, animace LED v led_task
  /*
  result = xTaskCreate((TaskFunction_t)animation_task_start, "animation_task",
                       ANIMATION_TASK_STACK_SIZE, NULL, ANIMATION_TASK_PRIORITY,
                       &animation_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Animation task");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG,
           "✓ Animation task created successfully (%dKB stack) - will "
           "self-register with TWDT",
           ANIMATION_TASK_STACK_SIZE / 1024);
  */
  ESP_LOGI(TAG,
           "⏭️ Animation task disabled (LED animace: led_task / "
           "unified_animation_manager)");

#if CONFIG_CHESS_ENABLE_TEST_TASK
  result = xTaskCreate((TaskFunction_t)test_task_start, "test_task",
                       TEST_TASK_STACK_SIZE, NULL, TEST_TASK_PRIORITY,
                       &test_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Test task");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG,
           "✓ Test task created successfully (%dKB stack) - will self-register "
           "with TWDT",
           TEST_TASK_STACK_SIZE / 1024);
#else
  ESP_LOGI(
      TAG,
      "⏭️ Test task disabled (menuconfig: CzechMate → Enable automated test "
      "task)");
#endif

  // DISABLED: Create Matter task - Matter not needed
  /*
  result = xTaskCreate(
      (TaskFunction_t)matter_task_start, "matter_task", MATTER_TASK_STACK_SIZE,
  NULL, MATTER_TASK_PRIORITY, &matter_task_handle
  );

  if (result != pdPASS) {
      ESP_LOGE(TAG, "Failed to create Matter task");
      return ESP_FAIL;
  }

  ret = esp_task_wdt_add(matter_task_handle);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
      ESP_LOGW(TAG, "Warning: Matter task WDT registration failed: %s",
  esp_err_to_name(ret));
  }
  ESP_LOGI(TAG, "✓ Matter task created successfully (%dKB stack) and registered
  with TWDT", MATTER_TASK_STACK_SIZE / 1024);
  */

  // Create Web Server task
  result = xTaskCreate((TaskFunction_t)web_server_task_start, "web_server_task",
                       WEB_SERVER_TASK_STACK_SIZE, NULL,
                       WEB_SERVER_TASK_PRIORITY, &web_server_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Web Server task");
    return ESP_FAIL;
  }

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ Web Server task created successfully (%dKB stack) - will "
           "self-register with TWDT",
           WEB_SERVER_TASK_STACK_SIZE / 1024);

  // Create HA Light task (starts after WiFi STA is connected)
  result = xTaskCreate((TaskFunction_t)ha_light_task_start, "ha_light_task",
                       HA_LIGHT_TASK_STACK_SIZE, NULL, HA_LIGHT_TASK_PRIORITY,
                       &ha_light_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create HA Light task");
    return ESP_FAIL;
  }

  // Task will register itself with TWDT internally
  ESP_LOGI(TAG,
           "✓ HA Light task created successfully (%dKB stack) - will "
           "self-register with TWDT",
           HA_LIGHT_TASK_STACK_SIZE / 1024);

  ESP_LOGI(TAG, "All system tasks created successfully");

  // Cas pro start tasku pred boot animaci.
  vTaskDelay(pdMS_TO_TICKS(1000));

  // ASCII banner, progress bar, led_boot_animation_step, fade-out.
  show_boot_animation_and_board();

  // Sladeni s NVS: initialize_chess_game() nemusi poslat GAME_CMD_NEW_GAME.
  initialize_chess_game();

  // UART bezel suspended az do konce boot animace (vystup neprerusuje logo).
  vTaskResume(uart_task_handle);
  ESP_LOGI(TAG, "✅ UART task resumed after boot animation");

  // led_is_booting() cisti az po fade_out (viz led_boot_animation_fade_out v
  // led_task).

  return ESP_OK;
}

// ============================================================================
// CENTRALIZED BOOT ANIMATION AND BOARD DISPLAY
// ============================================================================

/**
 * @brief Centralizovana boot animace: ASCII logo, progress bar, LED krok,
 * fade-out.
 *
 * @details
 * Volat az po vytvoreni tasku. Behem smycky vola led_boot_animation_step() a
 * WDT reset v main; na konci led_boot_animation_fade_out() vycisti desku a
 * nastavi led_booting_active = false. Nasleduje initialize_chess_game().
 *
 * @note game_task behem led_is_booting() nezpracovava tahy (viz
 * game_task_start).
 */
void show_boot_animation_and_board(void) {
  ESP_LOGI(TAG, "🎬 Starting centralized boot animation...");

  // Clear screen and show welcome logo
  printf("\033[2J\033[H");

  // Greek-inspired CZECHMAT banner with ANSI colors
  // New ASCII art logo - using fputs to avoid printf formatting issues
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs("\x1b[0m............................................................"
        "\x1b[34m:=*+-\x1b[0m.................................................."
        ".............\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................................................\x1b[34m:="
        "#%@@%*=-=+#@@@%*=:\x1b[0m............................................."
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m..............................................\x1b[34m-=*%@@%*="
        "-=*%@%@=*@%@%*=-+#%@@%*=-\x1b[0m......................................"
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m......................................\x1b[34m:-+#@@@%+--+#%@%+"
        "@+#@@%@%%@%@@-*@=@@%#=-=*%@@@#+-:\x1b[0m.............................."
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m...............................\x1b[34m:-+%@@@#+--*%@@*@=*@*@@@"
        "#=\x1b[0m...........\x1b[34m:+%@@%+@:#@*@@%+--+%@@@%+-:\x1b[0m........"
        ".......................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m........................\x1b[34m:-*@@@@#-:=#@@*@*+@+@@@%+:\x1b["
        "0m.........................\x1b[34m-*@@@%+@:@@#@@#-:=#@@@@#-:\x1b[0m.."
        "......................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m....................\x1b[34m%@@@@**#@@@@@@@@@@@@@@@@@@@@@@@@@@@"
        "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%@@@@#\x1b[0m........"
        "............\x1b[0m\n",
        stdout);
  fputs("\x1b[0m....................\x1b[34m%@#################################"
        "################################################%@#\x1b[0m............"
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[34m:%@=@+#@+@##@=@#%@+@*#@+@#%@=@*#@+"
        "@#*@+@#*@+@%*@+@%=@=%@+@**@=@%+@+#@=@%=@+#@+%%=@+:\x1b[0m............."
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m......................\x1b[34m#@==============================="
        "===============================================@+\x1b[0m.............."
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.......................\x1b[34m##==========@\x1b[0m::::::::::::"
        ":::::::::::::::::::::::::::::::::::::::::\x1b[34m*@==========@+\x1b["
        "0m........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m........................\x1b[34m:@*******%@:\x1b[0m.\x1b[34m:%%"
        "%%%%%%%%%%%%%%%%%%%--#@@#.+%%%%%%%%%%%%%%%%%%%%*\x1b[0m..\x1b[34m-@#**"
        "****%%\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m-@#+%:%.@\x1b[0m...\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%%%=:+@@=\x1b[0m..:::::::::::::::::::\x1b[37m@%\x1b["
        "0m....\x1b[34m@+#+*%*@:\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m=@#=%:%.@\x1b[0m...\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%#--:*@@@@+-*-\x1b[0m.................\x1b[37m@%\x1b["
        "0m....\x1b[34m@+#+*%*@-\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m=%#=%:%.@\x1b[0m...\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%#.%@@@@@@@@%:\x1b[0m.................\x1b[37m@%\x1b["
        "0m...\x1b[34m:%**+*%+@-\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m=%#-%:%.@\x1b[0m...\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%#-@@@@@@@@:\x1b[0m..................\x1b[37m@%\x1b["
        "0m...\x1b[34m-%**+*#+@-\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m+#%-%:%:@\x1b[0m...\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%#-########-\x1b[0m..................\x1b[37m@%\x1b["
        "0m...\x1b[34m=%**+*#+@=\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m**%-%:%:@\x1b[0m...\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%:#%%%##%%%*\x1b[0m..................\x1b[37m@%\x1b["
        "0m...\x1b[34m+#**+*#*%=\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m#+%:%:%-%:\x1b[0m..\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%*::@@@@@%\x1b[0m....................\x1b[37m@%\x1b["
        "0m...\x1b[34m*##*+***%+\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m#=%:%:#-%:\x1b[0m..\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%%%.%@@@@*\x1b[0m....................\x1b[37m@%\x1b["
        "0m...\x1b[34m#*#++***#+\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m%:%:%:#=%=\x1b[0m..\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%%#:@@@@@%\x1b[0m....................\x1b[37m@%\x1b["
        "0m...\x1b[34m%*#++*+***\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m%:%:%:#=#+\x1b[0m..\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%%%-*@@@@@@-\x1b[0m...................\x1b[37m@%\x1b["
        "0m...\x1b[34m%+%++*+#+#\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m@:%:%:#+#*\x1b[0m..\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%#:=%%%%%%%%:\x1b[0m..................\x1b[37m@%\x1b["
        "0m...\x1b[34m@+%++*+#=#\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.........................\x1b[34m@:%:%:#+*#\x1b[0m..\x1b[34m-@@"
        "%%%%%%%%%%%%%%%%-=%@%%%%%%%%-\x1b[0m.................\x1b[37m@%\x1b["
        "0m...\x1b[34m@=%=+*=#-%\x1b[0m.........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.......................\x1b[34m:@*++++++++%#.-@@%%%%%%%%%%%%%%%"
        ".%@@@@@@@@@@@@#\x1b[0m................\x1b[37m@%\x1b[0m..\x1b[34m@*+++"
        "+++++%%\x1b[0m........................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m......................\x1b[34m=@=----------*@-@@@@@@@@@@@@@@@@@"
        ":*############=:@@@@@@@@@@@@@@@@%-@=----------=@:\x1b[0m.............."
        ".........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m....................\x1b[34m*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
        "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@=\x1b[0m............"
        ".........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m........................."
        ".......................................................\x1b[37m@+\x1b["
        "0m.....................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m=@@@@@:+@@@@@@"
        "..@@@@@+..%@@@@@.-@%...+@%..@@#...=@@:...=@@-.=@@@@@@%-@@@@@-\x1b[0m.."
        "\x1b[37m@+\x1b[0m.....................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m%@+....:...:@@:"
        "..@@....-@@:...:::@#...=@#..@@@#.*@@@:..:%@@@:...@@:..:@@\x1b[0m......"
        "\x1b[37m@+\x1b[0m.....................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m@@:.......=@%.."
        "..@@%%%.+@#......:@@%%%%@#.:@*+@@@:%@-..+@.*@#...@@:..:@@#@*\x1b[0m..."
        "\x1b[37m@+\x1b[0m.....................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m..\x1b[34m+@%:..-*.=@%..:"
        "=.@@...*:@@=...+-:@#...=@#.=@=.+@:.#@=.=@#**%@+..@@:..:@@...=\x1b[0m.."
        "\x1b[37m@+\x1b[0m.....................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#*\x1b[0m...\x1b[34m:*%@@#.=%%%%%%"
        ":-%%%%%*..-#@@%+.#%%:..#%#:#%=.....#%*:%%-..*%%=-%%+..=%%%%%=\x1b[0m.."
        "\x1b[37m@+\x1b[0m.....................\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m##--------------------------------"
        "------------------------------------------------@+\x1b[0m............."
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m#%================================"
        "================================================@+\x1b[0m............."
        "........\x1b[0m\n",
        stdout);
  fputs("\x1b[0m.....................\x1b[37m+#################################"
        "#################################################-\x1b[0m............."
        "........\x1b[0m\n",
        stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);
  fputs(
      "\x1b[0m................................................................."
      "............................................................\x1b[0m\n",
      stdout);

  // Show boot progress with smooth animation and status messages
  printf("\033[1;32m"); // bold green
  printf("Initializing Chess Engine...\n");

  const int bar_width = 50;    // Longer bar for better visibility
  const int total_steps = 200; // More steps for smoother animation
  const int step_delay = 25;   // 25ms per step for ultra-smooth animation

  // Status messages with progress thresholds
  const char *status_messages[] = {
      "Starting system...",   "Creating tasks...",   "Initializing GPIO...",
      "Setting up matrix...", "Configuring LEDs...", "Loading chess engine...",
      "Preparing board...",   "System ready!"};
  const int num_messages = sizeof(status_messages) / sizeof(status_messages[0]);
  // int current_message = 0; // Unused variable removed

  for (int i = 0; i <= total_steps; i++) {
    int progress = (i * 100) / total_steps;
    int filled = (i * bar_width) / total_steps;

    // Update status message based on progress
    int message_index = (progress * num_messages) / 100;
    if (message_index >= num_messages)
      message_index = num_messages - 1;

    // Clear current line and redraw
    printf("\rBooting: [");
    for (int j = 0; j < bar_width; j++) {
      if (j < filled) {
        printf("\033[1;32m█\033[0m"); // Bright green
      } else {
        printf("\033[2;37m░\033[0m"); // Dim gray
      }
    }
    printf("] %3d%% - %s", progress, status_messages[message_index]);
    fflush(stdout);

    // Spustit LED boot animaci podle progress
    led_boot_animation_step((uint8_t)progress);

    // CRITICAL: Reset watchdog timer during loading (only if registered)
    esp_err_t wdt_ret = main_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    if (i < total_steps) {
      vTaskDelay(pdMS_TO_TICKS(step_delay));
    }
  }

  // Fade out after the boot animation completes.
  led_boot_animation_fade_out();

  printf("\n\033[1;32m✓ Chess Engine Ready!\033[0m\n\n"); // Success message

  // Chess board will be displayed by game task after initialization
  ESP_LOGI(TAG, "🎯 Chess board will be displayed by game task...");

  // Show game guide after board
  printf("\n");
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("📋 CHESS GAME GUIDE - Type commands to play:\n");
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("📋 Basic Commands:\n");
  printf("  • move e2e4    - Move piece from e2 to e4\n");
  printf("  • help         - Show all available commands\n");
  printf("  • board        - Display current board\n");
  printf("  • status       - Show game status\n");
  printf("  • reset        - Start new game\n");
  printf("\n");
  printf("🎯 Quick Start: Type 'move e2e4' to make your first move!\n");
  printf("═══════════════════════════════════════════════════════════\n");

  ESP_LOGI(TAG, "✅ Boot animation and board display completed");
}

// ============================================================================
// MAIN APPLICATION FUNCTION
// ============================================================================

/**
 * @brief Hlavni funkce aplikace
 *
 * Tato funkce je hlavnim vstupnim bodem aplikace. Inicializuje system,
 * vytvori tasky a spusti hlavni smycku aplikace. Obsahuje error handling
 * a safe mode pro pripady chyb.
 *
 * @details
 * Funkce je volana jako prvni po spusteni ESP32. Inicializuje Task Watchdog
 * Timer, vytvori vsechny systemove tasky a spusti hlavni smycku aplikace.
 * Obsahuje error handling a safe mode pro pripady chyb pri inicializaci.
 *
 * Hlavni smycka:
 * - Resetuje watchdog timer
 * - Loguje system status kazdych 60 sekund
 * - Zpracovava demo mod pokud je zapnuty
 * - Ceka 1 sekundu mezi iteracemi
 */
void app_main(void) {
  ESP_LOGI(TAG, "🎯 ESP32-C6 Chess System v1.8.0 starting...");
  ESP_LOGI(TAG, "📅 Build Timestamp: %s %s", __DATE__, __TIME__);
  esp_reset_reason_t rr = esp_reset_reason();
  ESP_LOGI(TAG, "🔁 Reset reason: %d (%s)", (int)rr, reset_reason_to_str(rr));
  ESP_LOGI(TAG,
           "===============================================================");

  // Kontrola boot counteru - pokud je překročen limit, jít do deep sleep
  if (boot_counter_check_and_update()) {
    ESP_LOGW(TAG, "⚠️ Too many reboots detected! Entering deep sleep for 10 seconds...");
    ESP_LOGW(TAG, "   (This protects against boot loops)");
    // Deep sleep pro 10 sekund
    esp_sleep_enable_timer_wakeup(10 * 1000000); // 10 sekund v mikrosekundách
    esp_deep_sleep_start();
    // Tento kód se nikdy neprovede - probudíme se z deep sleep jako nový boot
  }

  // Zvyseni WDT timeout pro inicializaci
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 10000, // 10 sekund pro init - optimalizovano pro web server
      .idle_core_mask = 0,
      .trigger_panic = true};
  // Use reconfigure instead of init to avoid "TWDT already
  // initialized" error
  esp_err_t ret = esp_task_wdt_reconfigure(&twdt_config);
  if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "TWDT already initialized, skipping reconfiguration");
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure TWDT: %s", esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "TWDT configured with %lu ms timeout",
             (unsigned long)twdt_config.timeout_ms);
  }

  // Add main task to Task Watchdog Timer BEFORE any initialization
  ret = esp_task_wdt_add(NULL);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGE(TAG, "Failed to add main task to TWDT: %s", esp_err_to_name(ret));
    return;
  } else {
    ESP_LOGI(TAG, "✅ Main task registered with Task Watchdog Timer");
  }

  // Initialize console and UART FIRST - before any system initialization
  // This ensures that safe mode can output error messages
  ESP_LOGI(TAG, "🔄 Initializing console and UART...");
  init_console();
  ESP_LOGI(TAG, "✅ Console and UART initialized successfully");

  // System initialization with error recovery
  ret = main_system_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ System init failed: %s", esp_err_to_name(ret));
    ESP_LOGE(TAG, "🔄 Entering safe mode - basic UART only");

    // Safe mode - jen UART pro debugging
    while (1) {
      ESP_LOGI(TAG, "💔 Safe mode: Init failed, system halted");
      // Note: No watchdog reset in safe mode - task not registered
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  // Task Watchdog Timer is automatically initialized by ESP-IDF
  // (CONFIG_ESP_TASK_WDT_INIT=y in sdkconfig)
  ESP_LOGI(TAG, "Task Watchdog Timer initialized automatically by ESP-IDF");

  // Create system tasks
  ret = create_system_tasks();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Task creation failed: %s", esp_err_to_name(ret));
    ESP_LOGE(TAG, "🔄 Entering safe mode - basic UART only");

    // Safe mode - jen UART pro debugging
    while (1) {
      ESP_LOGI(TAG, "💔 Safe mode: Task creation failed, system halted");
      // Note: No watchdog reset in safe mode - task not registered
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  // Reset watchdog AFTER task creation
  main_task_wdt_reset_safe();

  // Navrat normalniho WDT timeoutu po inicializaci - optimalizovano pro
  // web server
  twdt_config.timeout_ms = 8000;          // Zvyseno na 8 sekund pro web server
  esp_task_wdt_reconfigure(&twdt_config); // Pouzit reconfigure misto init

  // Main task is already registered with TWDT at the beginning of app_main()
  ESP_LOGI(TAG, "✓ Main task already registered with Task Watchdog Timer");

  // Wait for tasks to initialize (startup banner now handled by UART task)
  vTaskDelay(pdMS_TO_TICKS(200));

  // Main application loop
  // uint32_t loop_count = 0; // Unused variable removed
  uint32_t last_status_time = 0;
  uint32_t last_demo_move_time =
      0; // Track time in ms instead of seconds for finer granularity

  ESP_LOGI(TAG, "🎯 Main application loop started");

  // Reset boot counter - systém úspěšně nastartoval
  boot_counter_reset();

  main_mark_ota_app_valid_if_needed();

  for (;;) {
    // CRITICAL: Reset watchdog for main task in every iteration
    main_task_wdt_reset_safe();

    // Get current time in ms
    uint32_t current_time_ms = esp_timer_get_time() / 1000;
    uint32_t current_time_sec = current_time_ms / 1000;

    // Periodic system status logging (respects quiet_mode and verbose_mode)
    if (current_time_sec - last_status_time >= 60) { // Every 60 seconds
      ESP_LOGI(
          TAG, "🔄 System Status: Uptime=%lu s, FreeHeap=%zu bytes, Tasks=%d",
          current_time_sec, esp_get_free_heap_size(), uxTaskGetNumberOfTasks());

      // LOGS REMOVED FOR BREVITY
      last_status_time = current_time_sec;
    }

    // Demo mode processing
    if (demo_mode_enabled) {
      if (current_time_ms - last_demo_move_time >= current_demo_delay_ms) {
        ESP_LOGI(
            TAG,
            "⏱️ DEMO TICK: Time=%lu, Last=%lu, Delay=%lu -> EXECUTING PROBE",
            current_time_ms, last_demo_move_time, current_demo_delay_ms);

        execute_demo_move();

        // Update time AFTER execution to ensure delay is counted from end
        // of move Otherwise long execution time (>delay) causes immediate
        // re-trigger
        last_demo_move_time = esp_timer_get_time() / 1000;

        // ⏳ Delay is now controlled by user configuration via
        // set_demo_speed_ms current_demo_delay_ms is NOT modified here,
        // preserving user setting
      }
    }

    // Short delay to allow frequent checks (interruption, etc.) but not starve
    // WDT
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}