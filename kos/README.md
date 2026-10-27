# kos/ — staré a nepoužívané zdroje

Tato složka obsahuje zdroje, které nejsou potřeba pro Hall V2 build, ale zůstávají uloženy, aby se daly v případě potřeby vrátit zpět. **Toto je finální přehledná dokumentace, ne pracovní koncept.**

## kos/v1-reed/ — stará cesta s reed maticí (V1)

Tato část obsahuje zdroje, které patří pouze k prototypové desce V1 s 8×8 reed kontakty. V2 používá Hallovy čidla přes I²C, proto zdejší zdroje Hall build nepotřebuje.

- `hardware/v1-reed-matrix/README.md` — kompletní specifikace a zapojení V1 desky s reed maticí.

## Interleaved reed části, které zůstaly v live souborech

Protože výchozí build (`sdkconfig.defaults`) je stále `CONFIG_CHESS_MATRIX_INPUT_GPIO_REED`, nebylo možné vytrhnout reed kód z živých souborů, aniž by se rozbil základní build. Reed větve zůstávají na těchto místech:

- `components/matrix_task/matrix_task.c` — funkce `matrix_scan_row_internal()` a další GPIO multiplex logika jsou uvnitř `#else` větve pod `CONFIG_CHESS_MATRIX_INPUT_I2C_HALL`. Tento soubor je živý pro Hall, protože obsahuje i `hall_i2c_matrix_fill_state()` a hlavní smyčku detekce tahů.
- `components/freertos_chess/freertos_chess.c` — inicializace matrix row pinů a validace GPIO je pod `#ifndef CONFIG_CHESS_MATRIX_INPUT_I2C_HALL`. V Hall režimu se tyto piny přeskočí.
- `components/freertos_chess/include/freertos_chess.h` — definice `MATRIX_ROW_*` a `MATRIX_COL_*` zůstávají, protože je používá jak reed cesta, tak `button_task` pro sdílená tlačítka.
- `components/button_task/button_task.c` — čtení 4 promotion tlačítek je svázáno se sloupci matice (`BUTTON_QUEEN` až `BUTTON_KNIGHT`). Tato logika zůstává aktivní i při Hall buildu, protože tlačítka V2 stále používá.

## kos/unused/ — opravdu nepoužívané komponenty a skripty

Následující soubory byly v repozitáři označeny jako nepoužívané (orphan / legacy / not linked) a žádný živý build je nevolá. Přesunuty sem pro případnou pozdější archivaci nebo návrat.

- `components/animation_task/` — legacy animační task. Vytváření tasku je zakomentováno v `main/main.c`; animace se řídí z `led_task` a `unified_animation_manager`. Fronty `animation_command_queue` a `animation_status_queue` zůstávají vytvořeny v `freertos_chess.c`, protože na ně kontroluje `main.c` a `uart_handlers_debug.c`.
- `components/enhanced_castling_system/` — není linkováno z `game_task` ani z hlavního `CMakeLists.txt` (`EXTRA_COMPONENT_DIRS`).
- `components/promotion_button_task/` — orphan task, logika promotions je v `button_task`/`game_task`.
- `components/reset_button_task/` — orphan task, reset je obsloužen v `button_task`/`game_task`.
- `delete_dead_castling.sh` a `scripts/maintenance/delete_dead_castling.sh` — deprecated skripty, jejichž jediným účelem bylo smazání `enhanced_castling_system`.

## Co nebylo přesunuto

- `components/matrix_task/matrix_task.c`, `components/matrix_task/hall_i2c_matrix.c` a `components/matrix_task/include/hall_i2c_spec.h` zůstávají na místě — jsou to živé Hall cesty.
- `components/freertos_chess/freertos_chess.c` zůstává — je to jádro front, mutexů a inicializace používané oběma variantami.
- `components/game_task/`, `components/led_task/`, `components/web_server_task/`, `components/ble_task/`, `components/ha_light_task/`, `components/unified_animation_manager/`, `components/game_led_animations/`, `components/button_task/` a další sdílené komponenty zůstávají, protože je V2 stále používá (pravidla hry, LED, web, BLE, tlačítka, animace, OTA).
- `components/test_task/` zůstává — není reed-only jako celek, obsahuje obecné testy a je volitelně linkovaný přes menuconfig.

## Mrtvé herní handlery (2026-10-26)

Přesunuto z živého `game_task`, protože v celém stromu neměly volajícího. Firmware je nelinkuje.

- `unused/components/game_task/matrix_event_handlers.c` — `game_detect_new_game_setup`, `game_handle_piece_lifted`, `game_handle_piece_placed`, `game_handle_squares_move`, `game_highlight_opponent_pieces`. Tahy z matice jdou přes `GAME_CMD_PICKUP` / `GAME_CMD_DROP`.
- `unused/components/game_task/board_print_and_squares_events.c` — prázdné `game_process_squares_events` a `game_print_board_enhanced` bez volání.
- `unused/components/game_task/dead_highlight_skip.c` — větev `else` po `total_games++`, která se nespustila, protože čítač je v tu chvíli nejméně 1.

V souboru zůstalo a je živé: promoce, `game_execute_promotion`, `game_highlight_movable_pieces`, `game_is_castling_expected` (volá `game_physical.c`).

## Prohlížečové UI (2026-10-11)

Do `kos/unused/web_ui/` šla lidská šachová stránka: `web/` (JS moduly, `chess_app.js`, katalog zahájení, PNG figurek), `chess_piece_http.c` s hlavičkou a skripty, které JS a PNG balily do firmwaru. Živý `web_server_task` dál servíruje WiFi, REST `/api/*`, WebSocket `/ws` a OTA pro Flutter a desktop. `GET /` vrací JSON pro aplikaci, `GET /chess_app.js` vrací 404. Volba `CHESS_ENABLE_EMBEDDED_WEB_UI` v Kconfig už není.
