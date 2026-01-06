# CzechMate Mastery Learning Plan

Tento plán převádí celý projekt **ESP32-C6 Chess System v2.4** do uceleného studijního materiálu. Cílem je, aby i člověk bez přístupu ke zdrojovým souborům dokázal porozumět každému řádku kódu a fungování celého systému.

---

## 1. Studijní strategie

- **Iterativní postup:** Každý modul studuj postupně – nejprve teorie, potom analýza kódu, nakonec praktické úkoly.
- **CzechMate konvence:** Dokumentace je záměrně bez diakritiky (požadavek projektu).
- **Nástroje:** Pro repete používej `idf.py build`. Příkaz `idf.py monitor` je zakázán (pravidla uživatele).
- **Poznámky:** Doporučuji vlastní soubor (např. `notes/learning_journal.md`) a stručně zapisovat poznatky či otázky.
- **Kontrola pochopení:** Každá lekce končí otázkami a praktickým cvičením. Upřímně si odpověz – plán slouží jako samostatný kurz.

---

## 2. Rychlá orientace v modulech

- ✅ **Modul 1 (Lekce 1–10)** – přeskoč, základy C/embedded znáš.
- ✅ **Modul 4 (Lekce 36–50)** – přeskoč, pokročilé datové struktury nepotřebuješ opakovat.
- ✅ **Modul 5 (Lekce 51–65)** – přeskoč, hardwarové basics máš splněné.
- ✅ **Modul 7 (Lekce 81–90)** – přeskoč, CI/test automation teoreticky zvládáš.
- ✅ **Modul G (Diagnostika)** – přeskoč.
- 🔵 **Modul 3 (Lekce 26–35)** – začni zde: ESP32-C6, build pipeline, NVS, WiFi, HTTP.
- 🟠 **Modul 6 (Lekce 66–80)** – nejdůležitější: architektura CzechMate po komponentách.
- ✅ **Modul 2 (Lekce 11–25)** – projdi hned po Modulu 3; FreeRTOS advanced.
- 🟢 **Modul 8 (Lekce 91–100)** – nech na závěr (prezentace, demo, troubleshoot).

---

## 3. Modul 3 – ESP32-C6 Specifika (Lekce 26–35)

### Lekce 26 – Architektura a pinout ESP32-C6

**Teorie:**
- ESP32-C6 používá jedno RISC-V jádro, podporuje WiFi 6 a BLE 5 LE.
- Boot strapping piny: `GPIO4`, `GPIO5`, `GPIO8`, `GPIO9`, `GPIO15` – nesmí být připojeny k periferiím, které by měnily jejich logickou úroveň při resetu.
- Flash/PSRAM piny (`GPIO24`–`GPIO26`) jsou rezervované.

**CzechMate konfigurace:**

```63:123:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/include/freertos_chess.h
/** @brief Pin pro WS2812B LED data (GPIO7) */
#define LED_DATA_PIN GPIO_NUM_7        // WS2812B data line
/** @brief Pin pro stavovy LED indikator (GPIO5 - bezpecny pin) */
#define STATUS_LED_PIN GPIO_NUM_5      // Status indicator (safe pin - GPIO8 is boot strapping pin!)
...
#define MATRIX_COL_5 GPIO_NUM_14       // Safe pin (changed from GPIO9 to avoid strapping pin)
#define BUTTON_RESET GPIO_NUM_27       // Safe pin (changed from GPIO4 to avoid strapping pin)
```

**Proč je to důležité:**
- LED data link je na `GPIO7`, protože `GPIO8` je strapping pin.
- Reset tlačítko je na `GPIO27`, mimo strapping oblast.
- Sloupce matice sdílí piny s tlačítky (time-multiplexing) – viz definice `BUTTON_*` makro hodnot.

**Poznámkové okruhy:**
- Nakresli si tabulku 8×8, kde pro každý sloupec uvedeš sdílené tlačítko.
- Zvaž, které piny zůstaly volné pro případné rozšíření (např. senzor prostředí).

**Self-check:**
1. Umíš vysvětlit, proč by použití `GPIO9` pro LED data mohlo zabránit bootu?
2. Víš, jak bys přemapoval `BUTTON_RESET`, kdyby `GPIO27` kolidovalo s rozšířením?

---

### Lekce 27 – ESP-IDF komponentový framework

**Teorie:**
- Každá komponenta má vlastní `CMakeLists.txt` a volitelně `idf_component.yml`.
- `REQUIRES` znamená veřejnou závislost (exponuje include), `PRIV_REQUIRES` jen interní.

**Ukázka pro `game_task`:**

```1:12:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/game_task/CMakeLists.txt
idf_component_register(
    SRCS "game_task.c" "game_task_json.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos_chess led_task timer_system config_manager
    PRIV_REQUIRES cJSON
)
```

**Analýza:**
- `game_task` potřebuje veřejná API z `freertos_chess`, `led_task`, `timer_system`, `config_manager`.
- Interně používá cJSON (`PRIV_REQUIRES`).

**Cvičení:**
1. Vytvoř diagram komponent se šipkami závislostí.
2. Navrhni, jak bys připojil novou komponentu `analysis_engine`, která potřebuje `game_task` i `timer_system`.

**Self-check:**
- Proč by `timer_system` v `PRIV_REQUIRES` nestačilo?
- Jaký je rozdíl mezi `INCLUDE_DIRS` a `PRIV_INCLUDE_DIRS`?

---

### Lekce 28 – Build a flash pipeline

**Teorie:**
- `cmake -S . -B build -G Ninja` generuje Ninja konfiguraci.
- `build/project_description.json` obsahuje seznam všech zdrojáků a závislostí.
- Výsledný firmware je `build/esp32_chess_v24.bin`.

**Praktický postup:**
1. V souboru `build/project_description.json` najdi sekci `components.matrix_task.sources` a ověř, že zahrnuje `matrix_task.c`.
2. Z dělaných kroků napiš popis pipeline: CMake → Ninja → GCC → Linker → `app.bin`.

**Self-check:**
- Jak zjistíš velikost aplikace (`idf.py size`)?
- Co bys udělal, abys zmenšil firmware, pokud by se nevešel do `app` partition?

---

### Lekce 29 – Flash a partition table

**Teorie:**
- Výchozí partition table ESP-IDF: NVS (16 kB), phy_init (8 kB), ota_data (32 kB), app0 (2 MB by default).
- CzechMate používá single OTA slot.

**Úkoly:**
1. Exportuj partition table: `idf.py partition-table` → vznikne CSV v `build/partition_table/`.
2. Zapiš, kolik místa zabírá `app0` a jakou maximální velikost může mít binárka.

**Self-check:**
- Jak bys přidal druhý OTA slot (app1)?
- Kam bys uložil statická webová data, kdyby byla příliš velká pro firmware?

---

### Lekce 30 – NVS a `config_manager`

**Struktury uložené v NVS:**

```632:639:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/include/chess_types.h
typedef struct {
    bool verbose_mode;
    bool quiet_mode;
    uint8_t log_level;
    uint32_t command_timeout_ms;
    bool echo_enabled;
} system_config_t;
```

- Dále `chess_config_t` (jas LED, výchozí časová kontrola, orientace desky, WiFi SSID...)
- `config_manager.c` obsahuje funkce `config_init()`, `config_load()`, `config_save()`.

**Cvičení:**
1. Popiš flow `config_set_brightness(200)` → `nvs_open` → `nvs_set_u8` → `nvs_commit`.
2. Připrav krizový scénář: `ESP_ERR_NVS_NOT_FOUND` – jak vrátíš defaultní hodnoty?

**Self-check:**
- Proč je `command_timeout_ms` uložen v NVS, ale front sizes ne?
- Jak bys přidal persistentní volbu pro `enable_screen_saver`?

---

### Lekce 31 – WiFi AP režim

**Start AP:**

```101:152:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/web_server_task/web_server_task.c
static esp_err_t wifi_init_softap(void)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_Chess",
            .ssid_len = 0,
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    return esp_wifi_start();
}
```

**Poznámky:**
- Otevřená síť (bez hesla). DHCP server automaticky přiděluje IP v 192.168.4.0/24.
- Přihlašovací portál (captive portal) je implementován (viz `/generate_204`).

**Cvičení:**
1. Navrhni změny pro WPA2 (nastav heslo, `authmode = WIFI_AUTH_WPA2_PSK`).
2. Jak uložíš credentials do NVS, aby šlo heslo měnit z webu?

**Self-check:**
- Jak zjistíš IP klientů (`esp_wifi_ap_get_sta_list`)?
- Co by se stalo, kdyby `max_connection` bylo 1?

---

### Lekce 32 – STA režim (příprava)

**Idea:**
- Přechod na `WIFI_MODE_APSTA` umožní CzechMate připojit se k domácímu routeru a zároveň poskytovat vlastní AP.
- Vyžaduje prefixované SSID, DHCP v STA režimu, synchronizaci času přes NTP.

**Úkol:**
- Sepiš pseudo-kód pro double-mode a zvaž, kde uložit router credentials (NVS `wifi_sta_ssid`, `wifi_sta_pass`).

---

### Lekce 33 – HTTP server a REST API

**Registrace endpointů:**

```300:486:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/web_server_task/web_server_task.c
static esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_uri_handlers = 16;
    ...
    httpd_start(&httpd_handle, &config);

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = http_get_status_handler,
    };
    httpd_register_uri_handler(httpd_handle, &status_uri);
    ...
    httpd_uri_t timer_reset_uri = {
        .uri = "/api/timer/reset",
        .method = HTTP_POST,
        .handler = http_post_timer_reset_handler,
    };
    httpd_register_uri_handler(httpd_handle, &timer_reset_uri);
    ...
}
```

**V praxi:**
- `/api/status` vrací souhrn hry (JSON).
- `/api/timer/*` ovládá chess clock.
- POST `/api/move` je z bezpečnostních důvodů zakázaný (web UI je read-only).

**Cvičení:**
1. Popiš, jak bys přidal `/api/new_game` (POST) – jaký handler, jaké JSON parametry.
2. Nastav CORS tak, aby povolil pouze GET/POST: `httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST");`.

**Self-check:**
- Co je `config.stack_size = 8192` a proč tak velký stack?
- Jak server řeší LRU purge (`config.lru_purge_enable = true`)?

---

### Lekce 34 – cJSON a serializace

**Vznik JSON objektu:**

```112:173:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/game_task/game_task_json.c
cJSON* game_get_status_json(void)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", game_state_to_string(current_game_state));
    cJSON_AddStringToObject(root, "player", (current_player == PLAYER_WHITE) ? "white" : "black");
    cJSON_AddNumberToObject(root, "move_count", move_count);
    ...
    return root;
}
```

**Poznámky:**
- Funkce vrací `cJSON*`, volající je zodpovědný za `cJSON_Delete(root)`.
- Velké výpisy používají `streaming_output` – omezuje RAM.

**Cvičení:**
1. Seznam všech klíčů ve `status` JSONu (state, player, move_count, fen, history...).
2. Popiš, jak bys serializoval LED konfiguraci (např. `led_manager_get_state_json`).

**Self-check:**
- Co se stane, pokud `cJSON_CreateObject()` vrátí NULL?
- Proč je vhodné `cJSON_AddItemToObject` pro pole (arrays)?

---

### Lekce 35 – Logging & diagnostika

**Použití logů při detekci tahu:**

```242:320:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/matrix_task/matrix_task.c
void matrix_detect_moves(void)
{
    uint8_t piece_lifted = 255;
    for (int i = 0; i < 64; i++) {
        if (matrix_previous[i] == 1 && matrix_state[i] == 0) {
            piece_lifted = i;
            break;
        }
    }
    ...
    if (piece_lifted != 255) {
        ESP_LOGI(TAG, "Piece lifted from square %d", piece_lifted);
        ...
    }
}
```

**Interpretace:**
- Kód loguje, kdy je figurka zvednuta a položena; timestampy v milisekundách.
- Debug (`ESP_LOGD`) se používá pro jemné ladění (např. `simulation_mode`).

**Self-check:**
- Jak bys nastavil log level na `WARN` pro `matrix_task` bez rekompilace? (Menuconfig → Component config → Log output → Default log level.)
- Kdy je vhodné použít `ESP_LOG_BUFFER_HEXDUMP`?

---

### Doplněk – Hardwarové zapojení a napájení

**Fyzické propojení:**

```1:38:/Users/alfred/Documents/my_local_projects/free_chess_v1 /ZAPOJENI.md
# ZAPOJENÍ ESP32-C6 Chess Board
WS2812B DIN → GPIO7, status LED na GPIO5 přes 220Ω, matrix rows na GPIO10–23, sloupce na GPIO0/1/2/3/6/14/16/17 s pull-up 10k.
```

- Reed switch každého pole spojuje příslušný sloupec se zvoleným řádkem (např. `A1: GPIO0 ↔ GPIO10`).
- Promotion tlačítka sdílejí sloupce matice a přes diody se připojují na všechny řádky — multiplexing bez zpětných proudů.
- Reset tlačítko je na `GPIO27` s interním pull-up; LED pásek má externí 5V/5A zdroj se společnou zemí.

**Napájecí bezpečnost:**
- Přidej velký elektrolytický kondenzátor (≥1000 µF) na vstup LED pásku pro pohlcení špiček.
- WS2812B akceptuje 3.3V datový signál při krátkém vedení; u delších kabelů zvaž level shifter.
- Všechna tlačítka mají diody 1N4148 (anoda k řádkům), aby při multiplexu neovlivňovala LED.

**Self-check:**
- Nakresli schéma propojení pro jedno promotion tlačítko včetně diod a pull-up rezistoru.
- Jak bys řešil galvanické oddělení, pokud by LED zdroj byl rušivý?

---

## 4. Modul 2 – FreeRTOS Pokročilé (Lekce 11–25)

### Lekce 12 – Vytváření tasků & watchdog

```7493:7561:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/game_task/game_task.c
void game_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Game task started successfully");
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    ...
    for (;;) {
        game_process_commands();
        game_process_matrix_events();
        game_update_timer_display();
        ...
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
    }
}
```

- Task registruje watchdog, aby se předešlo zablokování.
- Perioda 100 ms → 10 Hz zpracování herní logiky.

**Cvičení:**
1. Spočítej, kolik času zabere jedna iterace při 100 Hz, pokud bys periodu zkrátil na 10 ms.
2. Sepiš důsledky, když by `esp_task_wdt_add` selhal.

---

### Lekce 13 – Priority

- Podívej se do `freertos_chess.h` na `GAME_TASK_PRIORITY`, `LED_TASK_PRIORITY`, ...
- `game_task` má vyšší prioritu než LED, aby logika běžela deterministicky.
- `LED_TASK` má vysoký stack (`8 KB`) kvůli velkým bufferům.

**Self-check:**
- Co by se stalo, kdyby `LED_TASK_PRIORITY` byla vyšší než `GAME_TASK_PRIORITY`?
- Kdy používáš `vTaskPrioritySet`?

---

### Lekce 14 – Časování a `vTaskDelayUntil`

- `vTaskDelayUntil` drží stabilní periodu bez driftu.
- Sleduj, jak `last_wake_time` udržuje 100 ms cyklus.

**Cvičení:**
- Přepiš smyčku na `vTaskDelay(pdMS_TO_TICKS(100))` a popiš rozdíl.

---

### Lekce 16–17 – Fronty

**Deklarace typů:**

```482:504:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/include/chess_types.h
typedef enum {
    MATRIX_EVENT_PIECE_LIFTED = 0,
    MATRIX_EVENT_PIECE_PLACED = 1,
    MATRIX_EVENT_MOVE_DETECTED = 2,
    MATRIX_EVENT_ERROR = 3
} matrix_event_type_t;

typedef struct {
    matrix_event_type_t type;
    uint8_t from_square;
    uint8_t to_square;
    piece_t piece_type;
    uint32_t timestamp;
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
} matrix_event_t;
```

**Vytváření front:**

```519:583:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/freertos_chess.c
esp_err_t chess_create_queues(void)
{
    ...
    ESP_LOGI(TAG, "  - Matrix Event Queue: %d items × %zu bytes", MATRIX_QUEUE_SIZE, sizeof(matrix_event_t));
    SAFE_CREATE_QUEUE(matrix_event_queue, MATRIX_QUEUE_SIZE, sizeof(matrix_event_t), "Matrix Event Queue");
    ...
    ESP_LOGI(TAG, "  - UART Response Queue: %d items × %zu bytes", UART_QUEUE_SIZE, sizeof(game_response_t));
    SAFE_CREATE_QUEUE(uart_response_queue, UART_QUEUE_SIZE, sizeof(game_response_t), "UART Response Queue");
    ...
}
```

- Makro `SAFE_CREATE_QUEUE` kontroluje chyby a loguje výsledek.

**Odesílání události:**

```242:320:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/matrix_task/matrix_task.c
if (matrix_event_queue != NULL) {
    matrix_event_t event = {
        .type = MATRIX_EVENT_PIECE_PLACED,
        .from_square = 255,
        .to_square = piece_placed,
        .piece_type = 1,
        .timestamp = esp_timer_get_time() / 1000
    };
    xQueueSend(matrix_event_queue, &event, pdMS_TO_TICKS(100));
}
```

**Zpracování ve hře:**

```7565:7601:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/game_task/game_task.c
while (xQueueReceive(matrix_event_queue, &event, 0) == pdTRUE) {
    switch (event.type) {
        case MATRIX_EVENT_PIECE_LIFTED:
            game_handle_piece_lifted(event.from_row, event.from_col);
            break;
        case MATRIX_EVENT_PIECE_PLACED:
            game_handle_piece_placed(event.to_row, event.to_col);
            break;
        case MATRIX_EVENT_MOVE_DETECTED:
            game_handle_matrix_move(event.from_row, event.from_col,
                                    event.to_row, event.to_col);
            break;
        ...
    }
}
```

**Self-check:**
- Jaký je rozdíl mezi blokujícím a neblokujícím `xQueueReceive`?
- Co se stane, když je fronta plná a `xQueueSend` má nulový timeout?

---

### Lekce 18.5 – Shared Buffer Pool a streamingový výstup

**Shared buffer pool:**

```1:107:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/shared_buffer_pool.c
BUFFER_POOL_SIZE = 4, BUFFER_SIZE = 2048; `get_shared_buffer_debug()` vrací předalokovaný blok a zaznamená vlastníka (task, soubor, řádek).
```

- Eliminuje opakované `malloc/free`, zabraňuje fragmentaci heapu a sleduje statistiky (`total_allocations`, `peak_usage`).
- Mutex `buffer_pool_mutex` chrání přístup; při vyčerpání poolu se loguje varování s údaji o tasku, který buffer drží.
- Makro `GET_SHARED_BUFFER()` (v hlavičce) doplní kontext `__FILE__`/`__LINE__` → snadné dohledání leaků.

**Streaming output:**

```1:205:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/streaming_output.c
`stream_printf()` formátuje do lokálního bufferu a volá `stream_write()`; backend může být UART, web nebo FreeRTOS fronta.
```

- `streaming_output_init()` vytváří mutex, nastaví výchozí mód `STREAM_UART` a resetuje statistiky (`stats.write_errors`, `stats.truncated_writes`).
- `stream_write()` rozlišuje backendy (`stream_write_uart`, `stream_write_web`, `stream_write_queue`) a inkrementuje počítadla chyb.
- `streaming_set_queue_output()` se používá pro odpovědi přes CLI (např. když `game_task` vrací velké JSON objekty do UART).

**Self-check:**
- Kdy zvolíš přímý přístup ke sdílenému bufferu místo `stream_printf` (např. budoucí generátor HTML šablon)?
- Co uděláš, když `stream_write` vrátí `ESP_ERR_TIMEOUT` (mutex nedostupný do 1 s)?

---

### Lekce 18–20 – Mutexy a kritické sekce

- Matice používá `matrix_mutex` při skenování (`matrix_scan_all`).
- LED systém používá `led_mutex` + krátké `portENTER_CRITICAL` na aktualizaci bufferu.

```180:207:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/matrix_task/matrix_task.c
if (xSemaphoreTake(matrix_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    for (int row = 0; row < 8; row++) {
        matrix_scan_row_internal(row);
    }
    ...
    xSemaphoreGive(matrix_mutex);
}
```

**Self-check:**
- Kdy bys místo mutexu zvažoval binární semafor?
- Proč `matrix_mutex` musí existovat i když se volá z timer callbacku?

---

### Lekce 21 – Software Timers

- `coordinated_multiplex_timer` zajišťuje střídání matrix/Button scan.
- Timer callback spouští `chess_multiplex_cycle()` (uvolnění pinů, sken, LED refresh).

**Úkol:**
- Sepiš timeline 0–25 ms: (0–20 ms matrix scan, 20–25 ms button scan, LED update).

---

### Lekce 24 – Streaming output

- `streaming_output.c` nahrazuje `malloc` při dlouhých výpisech.
- UART CLI posílá velký text po 128 bytech (`MAX_CHUNK_SIZE`).

**Self-check:**
- Proč se používá `vTaskDelay(pdMS_TO_TICKS(1))` mezi chunkem?
- Jak bys zachytil chybu, kdyby UART buffer nestíhal?

---

## 5. Modul 6 – Architektura CzechMate (Lekce 66–80)

### Lekce 66 – Globální přehled

**Hlavní datové toky:**

```
Matrix → matrix_event_queue → Game Task → (LED, Timer, UART, Web)
Buttons → button_event_queue → Game Task
UART/Web → game_command_queue → Game Task
Game Task → led_state_manager / unified_animation_manager
Timer System ↔ Game Task (čas hráče)
Visual Error System ↔ LED layers
```

Na papír si nakresli fronty, mutexy a časovače. Doporučený postup: barevně odlišené vrstvy (Hardware, Middleware, Aplikace).

---

### Lekce 67 – `freertos_chess`: infrastruktura

- Obsahuje globální handles, makra (`SAFE_CREATE_QUEUE`, `SAFE_CREATE_MUTEX`), inicializaci GPIO (`chess_gpio_init`), tvorbu front (`chess_create_queues`) a mutexů (`chess_create_mutexes`).
- `chess_system_init()` volá sekvence: GPIO → LED driver → Matrix → Button → Timer → Queues → Mutexes → Task start.

**Cvičení:**
- Sepiš pořadí inicializace a dopady, pokud některý krok selže (např. nedostatek heapu pro fronty).

---

### Lekce 68 – `chess_types.h`

- Centrální definice typů pro celou aplikaci.
- Obsahuje enumerace pro error codes, LED vrstvy, struktury pro game moves, matrix events, timer configs.

**Mini-úkol:**
- Zapiš si, co znamená `matrix_event_t.from_square = 255` (žádná hodnota / placeholder).

---

### Lekce 69 – Pipeline skenování a detekce tahu

**Kroky:**
1. `matrix_scan_all()` načte 64 reed switchů, porovná s předchozím stavem.
2. `matrix_detect_moves()` hledá přechody 1→0 (zvednutí) a 0→1 (položení).
3. `matrix_detect_complete_move()` vytváří event `MATRIX_EVENT_MOVE_DETECTED`.

```242:344:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/matrix_task/matrix_task.c
if (piece_lifted != 255) {
    last_piece_lifted = piece_lifted;
    move_detection_timeout = esp_timer_get_time() / 1000 + 5000;
    ESP_LOGI(TAG, "Piece lifted from square %d", piece_lifted);
    ...
}
if (piece_placed != 255) {
    matrix_event_t event = {
        .type = MATRIX_EVENT_PIECE_PLACED,
        .to_square = piece_placed,
        ...
    };
    xQueueSend(matrix_event_queue, &event, pdMS_TO_TICKS(100));
    if (last_piece_lifted != 255 && last_piece_lifted != piece_placed) {
        matrix_detect_complete_move(last_piece_lifted, piece_placed);
        last_piece_lifted = 255;
    }
}
```

- `matrix_square_to_notation` převádí index 0–63 na např. `e2`.

**Self-check:**
- Jak funguje timeout 5 sekund (`move_detection_timeout`)?
- Co se stane, když hráč figurku zvedne a položí na stejné pole?

---

### Lekce 70 – Button task a multiplexing

- Matice a tlačítka sdílí sloupce – během button scan se řádky nastaví na HIGH (`matrix_release_pins()`), po dokončení se obnoví (`matrix_acquire_pins()`).
- Button události posílají `button_event_queue`.

**Cvičení:**
- Popiš, jak bys debugoval situaci, kdy tlačítko neodpovídá (zkontrolovat multiplexing cyklus?).

---

### Lekce 71 – LED state manager a vrstvy

- 8 vrstev (background, pieces, moves, selection, animation, status, error, GUI).
- Každý pixel má `r,g,b,alpha,brightness,dirty,last_update`.

```180:366:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/led_state_manager/led_state_manager.c
esp_err_t led_composite_and_update(void) {
    if (dirty_count == 0) return ESP_OK;
    if (current_time - last_update_time < (1000 / current_config.update_frequency_hz)) return ESP_OK;
    for (int i = 0; i < 73; i++) {
        if (layers[0].pixels[i].dirty || ... || layers[4].pixels[i].dirty) {
            led_composite_pixel(i);
        }
    }
    last_update_time = current_time;
    dirty_count = 0;
    return ESP_OK;
}
```

- `blend_layer_pixel` aplikuje blend mode (REPLACE, ALPHA, ADDITIVE).
- `led_force_full_update()` značkuje všech 73 pixelů jako dirty.

**Self-check:**
- Proč se kontroluje `update_frequency_hz`?
- Jak docílíš, aby guidance vrstva měla poloprůhledné efekty?

---

### Lekce 72 – Unified Animation Manager

- Spravuje animace s prioritami (0–50) a typy (move path, check warning, endgame wave...).

```45:162:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/unified_animation_manager/include/unified_animation_manager.h
typedef enum {
    ANIM_PRIORITY_BACKGROUND = 0,
    ANIM_PRIORITY_LOW = 10,
    ...
    ANIM_PRIORITY_CRITICAL = 50,
} animation_priority_t;

struct animation_state_struct {
    uint32_t id;
    animation_type_t type;
    animation_priority_t priority;
    bool active;
    ...
    struct { uint8_t r, g, b; } color_start;
    struct { uint8_t r, g, b; } color_end;
    bool (*update_func)(animation_state_t* anim);
    void (*on_complete)(uint32_t id);
};
```

- Animace využívají LED layers: guidance, animation, error.
- `animation_start_endgame_wave()` spouští CRITICAL prioritu, zamezí přerušení.

**Cvičení:**
- Sepiš, jak bys přidal `ANIM_TYPE_SANDBOX_PREVIEW`.
- Jak zajistíš, aby nižší priorita obnovila stav po skončení vyšší?

---

### Lekce 73 – Game Task (hlavní orchestrátor)

- Udržuje stav desky `piece_t board[8][8]`, flags pro rošádu, en passant.

```160:222:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/game_task/game_task.c
static piece_t board[8][8] = {0};
static bool piece_moved[8][8] = {false};
static bool white_king_moved = false;
static bool white_rook_a_moved = false;
...
static chess_move_t move_history[MAX_MOVES_HISTORY];
```

- `game_handle_matrix_move()` validuje tah, aktualizuje board, spouští LED guidance a animace, kontroluje šach/mat.
- `game_save_move_history()` udržuje PGN/JSON export.

**Self-check:**
- Jak se kontroluje, zda tah nezanechá krále v šachu (`game_simulate_move_check`)?
- Jak funguje `error_recovery_state` při nevalidních tazích?

---

### Lekce 74 – Timer System

- Ukládá předdefinované časové kontroly, spravuje mutex, varování.

```1:220:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/timer_system/timer_system.c
static const time_control_config_t TIME_CONTROLS[] = {
    {TIME_CONTROL_BULLET_1_0, 60000, 0, "Bullet 1+0", ...},
    ...
};

static esp_err_t timer_lock(void)
{
    if (timer_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take timer mutex");
        return ESP_FAIL;
    }
    return ESP_OK;
}
```

- `timer_system_start_move(player)` zahajuje tah, přidává increment tomu, kdo táhl.
- Varování při 30/10/5 s zbývajícího času (`timer_check_warnings`).

**Self-check:**
- Jak bys aktivoval zvukový signál při `warning_5s_shown`?
- Jak se timer resetuje při novém zápase?

---

### Lekce 75 – UART CLI

- CLI čte znaky, podporuje historii, auto-complete, registruje příkazy.

```1:220:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/uart_task/uart_task.c
static esp_err_t uart_task_wdt_reset_safe(void) {
    esp_err_t ret = esp_task_wdt_reset();
    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW("UART_TASK", "WDT reset: task not registered yet");
        return ESP_OK;
    }
    ...
}

#define CHUNKED_PRINTF(format, ...) do {
    printf(format, ##__VA_ARGS__);
    fflush(stdout);
    SAFE_WDT_RESET();
    vTaskDelay(pdMS_TO_TICKS(1));
} while(0)
```

- Příkaz `move e2e4` končí ve `game_command_queue` a `game_task` ho zpracuje.

**Cvičení:**
- Navrhni nový příkaz `led_saver on/off` využívající `config_manager`.

---

### Lekce 76 – Web API vs. Game Task

- Web server volá `game_get_status_json` → `cJSON_Print` → HTTP response.
- Při načtení stránky: `/` (HTML), `/chess_app.js`, `/api/status`, `/api/board`, `/api/timer`.

**Úkol:**
- Vytvoř sekvenci HTTP requestů, které se odešlou během 10 s se zapnutým automatickým refresh webu.

---

### Lekce 77 – Visual Error System

- Zobrazuje chyby (šach, invalid move, puzzle error) pomocí barev a LED pozic.

```1:198:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/visual_error_system/visual_error_system.c
esp_err_t error_show_visual(visual_error_type_t error_type, const chess_move_t* failed_move) {
    if (!system_initialized) return ESP_ERR_INVALID_STATE;
    if (error_type >= ERROR_VISUAL_COUNT) return ESP_ERR_INVALID_ARG;
    if (active_error_count >= current_config.max_concurrent_errors) return ESP_ERR_NO_MEM;
    error_clear_all_indicators();
    error_get_color_for_type(error_type, &r, &g, &b);
    ...
    error_flash_leds(led_positions, led_count, r, g, b);
    ...
}
```

- `error_get_led_positions_for_move` mapuje tah na LED indexy.
- `error_show_blocking_path` rozlišuje zdroj (červená) a cíl (oranžová).

**Self-check:**
- Jak bys zobrazil chybu pro špatnou promoci (nezvoleno tlačítko)?
- Jak se ukládá `last_error` pro opakovanou vizualizaci?

---

### Lekce 78 – Endgame Animace

- Funkce v `game_led_animations.c` (wave, circles, cascade, fireworks, draw spiral/pulse).
- Každá animace nastavuje `LED_LAYER_ANIMATION`, případně `on_complete` callback.

**Úkol:**
- Zapiš, jak se spustí `animation_start_endgame_fireworks()` (kdo ji volá, s jakými parametry).

---

### Lekce 79 – Promoce & tlačítka

- `promotion_button_task` čte 4 tlačítka + LED feedback (LED 64–67).
- Událost `promotion_event_t` se posílá do `game_task`.

**Self-check:**
- Jak je zajištěno, že hráč nezvolí promoční figuru dvakrát?
- Co se stane, když hráč nevybere nic (timeout)?

---

### Lekce 80 – Testovací režim

- `test_task` spouští diagnostiku matrixe, tlačítek, LED, timeru.
- Lze použít pro factory test.

**Úkol:**
- Připrav test instrukce: „po spuštění stiskni tlačítko Queen“ – popiš, co se stane.

---

### Lekce 81 – Matrix task – interní stavový automat

- `matrix_scan_all()` drží `matrix_mutex`, volá `matrix_scan_row_internal` a aktualizuje `matrix_state`, `matrix_previous`, `matrix_changes`.

```170:347:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/matrix_task/matrix_task.c
`matrix_detect_moves()` sleduje přechody 1→0 a 0→1, nastavuje `move_detection_timeout` a při kompletním tahu volá `matrix_detect_complete_move()`.
```

- `matrix_detect_complete_move()` převádí indexy na algebrickou notaci (`matrix_square_to_notation`) a odesílá `MATRIX_EVENT_MOVE_DETECTED`.
- `simulation_mode` umožňuje testování bez fyzické desky (simulované stisky každých 5 s).

**Self-check:**
- Jak bys rozšířil detekci o více zvednutých figurek (dva hráči najednou)?
- Co se stane, když hráč figurku zvedne a čeká déle než 5 s?

---

### Lekce 82 – Button task – debouncing a události

- Debouncing 50 ms, dlouhý stisk 1000 ms, double-press okno 300 ms.

```1:220:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/button_task/button_task.c
`button_scan_all()` čte promotion tlačítka (MATRIX_COL_0-3) v multiplexním okně a reset tlačítko na GPIO27; simulation mode generuje testovací sekvenci.
```

- `button_send_event()` naplní `button_event_queue` strukturou `button_event_t` (typ, ID, délka stisku, timestamp).
- LED feedback pro tlačítka spravuje `game_task`, button task sleduje stav `button_blinking`, `button_available`.

**Self-check:**
- Jak detekuješ factory reset (dlouhý stisk reset tlačítka)?
- Co bys změnil, aby černá promoční tlačítka měla fyzické vstupy místo virtuálních?

---

### Lekce 83 – LED task a WS2812B pipeline

- Používá `led_strip` driver, double-buffer (`LED_FRAME_BUFFER_SIZE`), duration systém (`led_duration_state_t`) a health statistiky.

```1:218:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/led_task/led_task.c
Obsahuje batch commit (`led_commit_pending_changes`), health monitoring (`commands_processed`, `frame_drops`) a ochranu přes mutex `led_unified_mutex`.
```

- `led_force_immediate_update()` provádí synchronní refresh; `led_changes_pending` brání zbytečným commitům při více změnách v jednom cyklu.
- Duration systém umožňuje dočasné zvýraznění polí a automatické obnovení původní barvy.

**Self-check:**
- Jak bys exportoval LED health statistiky na web?
- Proč je double-buffering nutný pro WS2812B při složitých animacích?

---

### Lekce 84 – Streaming output v praxi

- `game_task` a `uart_task` používají `stream_printf` při dlouhých výstupech (ASCII board, historie, diagnostika).
- Statistiky (`stats.truncated_writes`, `stats.mutex_timeouts`) odhalí úzká hrdla; CLI může zobrazit stav.
- `streaming_set_web_output` připravuje chunkovaný HTTP přenos (budoucí rozšíření historie tahů).

**Cvičení:**
- Navrhni funkci `web_stream_history()` vracející PGN po částech.
- Sleduj, kdy se objeví truncation při 500 tazích.

---

### Lekce 85 – Web server task – handshake s UI

- `start_http_server()` konfiguruje httpd (port, `max_uri_handlers`, `max_open_sockets`, LRU purge, stack 8192 B).

```300:447:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/web_server_task/web_server_task.c
Registruje endpointy `/api/board`, `/api/status`, `/api/history`, `/api/captured`, `/api/timer/*` a captive portal (`/generate_204`, `/connecttest.txt`).
```

- `/api/status` → JSON se stavem hry, `/api/board` → FEN + mapping, `/api/timer` → chess clock.
- POST `/api/move` je zakomentován (web read-only); tahy jdou přes CLI nebo fyzickou desku.

**Self-check:**
- Jak bys přidal CORS pro multi-client (mobil + tablet)?
- Co znamená `config.backlog_conn = 5` při více připojeních?

---

### Lekce 86 – Timer system – race conditions a varování

- `timer_lock()` chrání `current_timer`; `timer_set_time_control()` kopíruje konfiguraci a resetuje příznaky.

```1:188:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/timer_system/timer_system.c
`timer_check_warnings()` loguje varování při 30/10/5 s, `timer_update_player_time()` odečítá čas a nastavuje `time_expired`.
```

- API vystavuje JSON i CLI odpovědi; increment se přidává v `timer_system_end_move()`.
- Varování lze napojit na LED status layer nebo audio.

**Self-check:**
- Kde bys implementoval LED flash při `warning_5s_shown`?
- Jak využiješ `avg_move_time_ms` při analýze výkonu hráče?

---

### Lekce 87 – Visual error system – barvy a hinty

- `error_system_init()` nastavuje parametry (`flash_count`, `guidance_duration_ms`, `enable_recovery_hints`).

```1:198:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/visual_error_system/visual_error_system.c
`error_show_visual()` vybírá barvu, načítá LED pozice (`error_get_led_positions_for_move`) a ukládá `user_message` i `recovery_hint`.
```

- Typy chyb: invalid move, wrong turn, piece blocking, check violation, puzzle chyby, syntaktické chyby.
- `max_concurrent_errors` zabraňuje zahlcení, `error_clear_all_indicators()` resetuje stav.

**Self-check:**
- Jak bys přidal audio signalizaci pro `ERROR_VISUAL_CHECK_VIOLATION`?
- Co se stane, pokud dojde k překročení `max_concurrent_errors`?

---

### Lekce 88 – UART CLI – command dispatcher

- CLI využívá tabulku handlerů, chunked výstup (`CHUNKED_PRINTF`) a historii.
- Příkazy `move`, `board`, `led_test`, `timer_set` komunikují s ostatními tasky přes fronty a streaming output.
- `uart_task_wdt_reset_safe()` brání WDT panic při dlouhých výstupech.

**Cvičení:**
- Navrhni příkaz `led_profile <low|medium|high>` zapisující jas do NVS.
- Implementuj `config_dump` pomocí shared buffer pool (alokace 2 KB, JSON export).

---

### Lekce 89 – Test task a diagnostika (rozšíření)

- `test_run_all()` spouští sérii testů a loguje úspěšnost v procentech.
- Matrix test využívá simulované události, LED test přehrává animace, UART test ověřuje komunikaci.
- Výsledky můžeš uložit do NVS nebo poslat přes UART pro servisní režim.

**Self-check:**
- Jak přidáš test WiFi (ping klienta) do portfolia?
- Kde bys uchovával timestamp posledního kompletního testu?

---

### Lekce 90 – Energetika a multiplexing v praxi

- Multiplex cyklus: 0–20 ms matrix scan, 20–25 ms button scan; LED task respektuje příznak `matrix_scanning_enabled`.
- `TOTAL_CYCLE_TIME_MS` je 25 ms (původně 30 ms) pro rychlejší odezvu; screen saver snižuje jas na 25 % a zpomaluje skenování.
- Pro power-saving můžeš vypnout WiFi AP, snížit jas, prodloužit matrix scan periodu.

**Self-check:**
- Jak bys implementoval battery mód (zmenšit jas, vypnout WiFi po neaktivitě)?
- Co se stane, když screen saver běží a hráč provede tah (který task budí ostatní)?

---

## 6. Modul 8 – Finální syntéza (Lekce 91–100)

### Lekce 91–92 – Prezentace architektury

- Připrav slidedeck s hlavními bloky: Hardware → Tasks → API → UX.
- Zahrň security (open WiFi, read-only web, UART CLI pro admina).

### Lekce 93–95 – Troubleshooting playbook

- Scénáře:
    - Reed switch nefunguje → zkontroluj multiplexing cyklus, kabeláž, logy `matrix_task`.
    - LED neblikají → ověř `led_state_manager`, proud, `dirty_count`.
    - WiFi se nespustí → zkontroluj strapping piny, log `wifi_init_softap`.

### Lekce 96–98 – Demo skript

1. Zapni board, sleduj log `Chess System Initializing`.
2. Připoj mobil k WiFi `ESP32_Chess`.
3. Otevři `http://192.168.4.1`, ukaž board, timer, historii.
4. Proveď tah, sleduj LED guidance a záznam v historii.
5. Vyvolej endgame animaci (např. `test_endgame` přes UART) – ukaž vlna/ohňostroj.

### Lekce 99 – Roadmapa

- Budoucí cíle uživatele: historie tahů s preview, sandbox mód, multi-client, UI rozšíření.
- Zapiš, jak by ovlivnily stávající architekturu (např. rozšíření web API, další fronty).

### Lekce 100 – Audit

- Využij checklist (viz níže). Pokud něco neumíš vysvětlit, vrať se k příslušné lekci.

---

## 7. Checklist mistrovství

- [ ] Vysvětlím kompletní pinout a multiplexing.
- [ ] Popsal jsem pipeline detekce tahu od reed switch po `game_handle_matrix_move`.
- [ ] Rozumím skladbě LED vrstev a animací.
- [ ] Umím debugovat invalid move přes UART (`move`, `history`, `debug_matrix`).
- [ ] Nastavím a uložím konfiguraci v NVS (např. jas LED).
- [ ] Umím prezentovat systém (demo skript) a řešit standardní chyby.

---

## 8. Příloha – Důležité výtažky

### 8.1 Makro pro bezpečné vytvoření fronty

```500:525:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/freertos_chess/include/freertos_chess.h
#define SAFE_CREATE_QUEUE(handle, size, item_size, name) \
    do { \
        handle = xQueueCreate(size, item_size); \
        if (handle == NULL) { \
            ESP_LOGE(TAG, "Failed to create queue: %s", name); \
            return ESP_ERR_NO_MEM; \
        } \
        ESP_LOGI(TAG, "✓ Queue created: %s", name); \
    } while (0)
```

### 8.2 Struktura chess boardu a historie

```160:208:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/game_task/game_task.c
static piece_t board[8][8] = {0};
static bool piece_moved[8][8] = {false};
...
static chess_move_t move_history[MAX_MOVES_HISTORY];
static uint32_t history_index = 0;
```

### 8.3 Handler HTTP serveru

```300:402:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/web_server_task/web_server_task.c
httpd_uri_t board_uri = {
    .uri = "/api/board",
    .method = HTTP_GET,
    .handler = http_get_board_handler,
};
httpd_register_uri_handler(httpd_handle, &board_uri);
```

### 8.4 Timer varování

```107:147:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/timer_system/timer_system.c
static void timer_check_warnings(uint32_t time_ms, bool is_white_turn)
{
    if (time_ms < 5000 && !current_timer.warning_5s_shown) {
        current_timer.warning_5s_shown = true;
        ESP_LOGW(TAG, "⚠️ Critical time warning: %s has less than 5 seconds!",
                 is_white_turn ? "White" : "Black");
    } else if (time_ms < 10000 && !current_timer.warning_10s_shown) {
        ...
    }
}
```

### 8.5 Převod polí na notaci

```356:364:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/matrix_task/matrix_task.c
void matrix_square_to_notation(uint8_t square, char* notation)
{
    int row = square / 8;
    int col = square % 8;
    notation[0] = 'a' + col;
    notation[1] = '1' + row;
    notation[2] = '\0';
}
```

### 8.6 Vizuální error flash

```179:190:/Users/alfred/Documents/my_local_projects/free_chess_v1 /components/visual_error_system/visual_error_system.c
error_flash_leds(from_led_positions, from_count, 255, 0, 0); // Red for source
error_flash_leds(to_led_positions, to_count, 255, 165, 0);   // Orange for destination
```

---

## 9. Závěr

Po absolvování plánu budeš schopen:
- Číst a vysvětlovat každý subsystém projektu bez potřeby nahlížet do originálního kódu.
- Debugovat běžné i komplexní chyby.
- Navrhovat rozšíření (sandbox mód, multi-client, historie s preview) na základě stávající architektury.
- Prezentovat CzechMate odbornému publiku včetně důvodů pro designové volby.

Pokud si nejsi jistý některou oblastí, vrať se k příslušné lekci a zopakuj praktické cvičení. Tento dokument je navržen jako kompletní referenční příručka – klidně do něj přidávej vlastní poznámky a aktualizace při dalším vývoji projektu.

