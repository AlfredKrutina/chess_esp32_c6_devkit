# Deska a aplikace

Popis chování ve pracovním stromu na větvi `main` (nesloučené změny nad `e6b72964`). Fyzická šachovnice je ESP32-C6, klient je `flutter_czechmate`. Foundry v tomhle projektu je **Microsoft Azure AI Foundry** jako poskytovatel AI Coach v nastavení. Není to firmware a nemá vlastní obrazovku.

Hra běží ve firmwaru (`game_task`). Aplikace posílá příkazy a čte snapshot.

## Jak se to chová

### Párování a jeden velitel

Známá deska se připojuje sama, jakmile je na obrazovce hlavní shell (`tryResumeFromPrefs` v `main.dart`: uložené BLE, pak Wi‑Fi URL). První spuštění nejdřív ukáže onboarding; shell a tedy i připojení začnou až po jeho dokončení. Stejné obnovení běží, když se aplikace vrátí na popředí. Párovací okno se pro známé zařízení neotevírá.

Nové zařízení pustí jen párovací okno. Trojklik na tlačítko desky (`GESTURE_PAIR` v `game_gesture.c`) okno otevře na 60 s (`PAIR_WINDOW_US` v `ble_nimble_impl.c`). Druhý trojklik ho zruší. Po dobu okna deska v advertisingu hlásí párování a na LED běží párovací pulz (`led_gesture_start_pair_pulse`).

Scan response nese manufacturer data Espressif `0x02E5`, magic `0xC2` a stav: bit 0 = okno otevřené, bit 1 = už drží spojení velitel. Aplikace to čte v `board_pairing.dart`.

- Známé zařízení (shoda s `lastBleRemoteId`, na desce existující bond): připojení projde i při zavřeném okně.
- Neznámé zařízení mimo okno: firmware spojení ukončí. V párovacím okně aplikace ukáže „Deska není v párovacím režimu.“
- Deska už má velitele (bit 1, nebo druhé GATT spojení): druhé spojení firmware ukončí. Aplikace ukáže „Deska je připojená k jinému zařízení.“ a tah neodešle.
- Dokončené párování vydá token 32 B (64 hex znaků) v BLE `cmd_ack`: `{"code":"paired","api_token":"…"}`. Stejný token dá `POST /api/pair`, ale jen dokud je okno otevřené. První úspěch okno zavře, druhé zařízení další token nedostane.

Deska si v NVS (`pair_slots`) pamatuje až tři zařízení. Čtvrté přepíše nejstarší slot. Aplikace si pamatuje jednu poslední desku (`lastBleRemoteId`) a jeden příkazový token (`boardCommandToken`).

Wi‑Fi příkaz (`POST /api/move` a příkazový rámec na `/ws`) vyžaduje `Authorization: Bearer <token>` z párování, i když je webový zámek vypnutý. Cizí platný token, dokud sedadlo drží někdo jiný, dostane `board_busy` („Board is connected to another device.“). Bez tokenu je `api_token_required`. Čtení snapshotu (`GET /api/status`, `GET /api/game/snapshot`) token nevyžaduje. Aplikace režim diváka nemá.

### Partie a obrazovka

Čtyři záložky: Hra, Puzzle, Učení, Nastavení (`AppMainTab` v `app_navigation.dart`). Analýza je druhá vrstva hry, otevírá se z listu Více (`pushAnalysisRoute`). `AppMainTab.analysis` v kódu není. Klíče `reportExport*` v lokalizaci jsou.

Po startu se otevře poslední záložka. Když snapshot hlásí běžící partii (pauza, promoc, nebo běžící hodiny a už odehraný tah, a desku nedrží puzzle, zahájení ani průvodce stavění), shell jednou přepne na Hru (`boardHasRunningGame`).

Výchozí vzhled je tmavý. Světlý je druhá volba. Starší hodnoty `system` a `oled` se berou jako tmavé. Výchozí jazyk je čeština, angličtina je v nastavení.

Při partii jsou hodiny obou hráčů pořád vidět: na výšku nad a pod deskou, na šířku vedle desky (kompaktní). Když na obrazovce hry běží puzzle s ověřením tahu, místo hodin je pruh puzzle. Souřadnice jsou vždy: písmena dole, čísla vlevo. Po otočení (černý dole) je vlevo dole h8. Barva popisku se bere ze čtverce tak, aby kontrast vyšel. Figurky jsou stávající PNG v `assets/pieces/` (`chess_piece_art.dart`).

Vzdát se jde z listu Více na obrazovce hry. Dialog pojmenuje barvu strany na tahu a příkaz pošle až po potvrzení. Zrušení partii neukončí. Příkaz je BLE `{"cmd":"resign","side":"white"|"black"}` a `POST /api/game/resign` se stejným tělem. Deska skončí partii stejnou cestou jako zvednutí vlastního krále na 10 s (`GAME_CMD_RESIGN` → `game_resign_from_app` → `resignation_end_game`). Mimo partii, v puzzle, v tréninku zahájení a v průvodci stavění deska vrátí 409 (`not_running` nebo `mode_active`). Špatná barva je 409 `side_mismatch`. Zamčené webové rozhraní je 403.

Nabídka remízy v příkazech není. Report umí zobrazit remízu, když ji pravidla partie tak ukončí (pat a podobné stavy).

`GameControlPanel` pořád obsahuje přepínač Standard / Jen deska a přepínač souřadnic. Na obrazovku hry se nemontuje. Render desky `showCoordinates` neposlouchá (`FenBoardPreview` parametr má a ignoruje ho, `ChessBoardWidget` souřadnice kreslí vždy). V nastavení vypínač souřadnic není.

### Puzzle

Aplikace pošle FEN a linii tahů, ve které je první tah řešitele (`sendPuzzleToBoard`, akce `load`). Deska si linii uloží do RAM (nejvýš 12 půltahů, `PUZZLE_MAX_PLIES`) a rozsvítí stavění: zelená je hotové pole s figurou, bílá chybí, červená přebývá. Tah do partie puzzle nepustí, dokud obsazení nesedí s FEN. Když sedí, aplikace sama pošle `start` (nejvýš osm pokusů). `cancel` session zruší.

Špatný tah deska označí hned při položení (`PUZZLE_FEEDBACK_WRONG` v `game_physical.c`). Soupeřův půltah ukáže na LED a hráč ho na dřevě provede. Prázdná linie po srovnání figur spustí obyčejnou partii z toho FEN, ne puzzle. Session se do NVS nezapisuje. HTTP pořád přijímá i `prepare` a `hint`; cesta z aplikace je `load`, `start`, `cancel`.

`POST /api/game/puzzle` při startu na nesedící pozici vrátí 409 `physical_mismatch`.

### Průvodce

Onboarding, Nápověda a Nastavení otevírají stejných pět kroků (`board_guide.dart`): connect, move, hint, clock, resign. Každý krok má slot 16:9. Soubor tam ještě není, jen `.gitkeep`. Chybějící obrázek nechá prázdný panel s ikonou.

## Co se v kódu změnilo

Změny jsou v pracovním stromu, ne v commitu.

### Párování

- `components/ble_task/ble_nimble_impl.c`, `components/ble_task/include/ble_task.h` — okno 60 s, odmítnutí neznámého peeru, druhé spojení, manufacturer stav, vydání tokenu.
- `components/web_server_task/board_pair_policy.c`, `include/board_pair_policy.h`, `test_board_pair_policy.c` — až tři sloty, jeden velitel, `board_busy`.
- `components/web_server_task/board_api_auth.c`, `include/board_api_auth.h` — NVS sloty, Bearer na příkazech, `POST /api/pair`.
- `components/web_server_task/web_ws.c`, `web_routes.c`, `web_handlers_game.c` — token na WebSocketu a na herních POST.
- `components/game_task/game_gesture.c`, `components/led_task/led_gesture_anim.c` — trojklik a párovací pulz.
- `flutter_czechmate/lib/core/protocol/board_pairing.dart`
- `flutter_czechmate/lib/features/connection/board_discovery_screen.dart`
- `flutter_czechmate/lib/core/services/ble_czechmate_client.dart`, `board_api_client.dart`, `board_api_exception.dart`

### Puzzle

- `flutter_czechmate/lib/features/puzzle/puzzle_screen.dart`
- `flutter_czechmate/lib/features/connection/board_session_notifier.dart`
- `flutter_czechmate/lib/core/protocol/board_command_payload.dart`
- `flutter_czechmate/lib/core/utils/puzzle_uci.dart`, `flutter_czechmate/test/puzzle_line_test.dart`
- `components/game_task/game_puzzle.c`, `game_dispatch.c`
- `components/freertos_chess/include/chess_types.h` (`GAME_CMD_PUZZLE`)
- `components/web_server_task/web_handlers_game.c` (`POST /api/game/puzzle`)

### Vzdání

- `flutter_czechmate/lib/features/game/controls/resign_action.dart`, `game_more_sheet.dart`
- `flutter_czechmate/test/board_guide_resign_test.dart`
- `components/web_server_task/web_resign_dispatch.c`
- `components/game_task/game_resignation.c` (`game_resign_from_app`, `resignation_end_game`)
- `components/game_task/game_dispatch.c` (`GAME_CMD_RESIGN`)

### Obrazovka, deska, průvodce

- `flutter_czechmate/lib/app_navigation.dart`, `main.dart`, `features/learn/learn_hub_screen.dart`
- `flutter_czechmate/lib/features/game/game_screen.dart`, `controls/player_clock.dart`, `state/running_game.dart`
- `flutter_czechmate/lib/features/game/board/board_coordinates.dart`, `chess_board_widget.dart`, `chess_piece_art.dart`
- `flutter_czechmate/lib/core/theme/app_theme.dart`
- `flutter_czechmate/lib/features/onboarding/board_guide.dart`, `onboarding_screen.dart`, `features/help/help_screen.dart`
- `flutter_czechmate/test/board_coordinates_test.dart`, `player_clock_test.dart`
- `flutter_czechmate/pubspec.yaml` — složky `assets/guide/{connect,move,hint,clock,resign}/`

`sdkconfig` v tom samém stromě nese hlavičku ESP-IDF 5.5.1 a přegenerované výchozí volby. Na párování, puzzle ani vzdání se v tom diffu nová produktová volba neobjevila. `dependencies.lock` má dvouřádkovou změnu; tenhle popis ji jako chování desky nebere.

## Co v produktu není

- Příkaz nebo tlačítko na nabídku remízy.
- Režim diváka v aplikaci. Čtení snapshotu po HTTP zůstává bez příkazového tokenu.
- Vypínač souřadnic na obrazovce, která se opravdu kreslí. Přepínač v `game_control_panel.dart` a v preferencích zůstal, panel se nikam nevkládá.
- Přepínač Standard / Jen deska na živé obrazovce hry. Stejný nemontovaný panel ho ještě obsahuje a `layoutMode` se ukládá, rozložení hodin a desky podle něj nevětví.
- Vlastní obrazovka Foundry. Azure AI Foundry je položka v Nastavení → Coach a AI.

Text průvodce v lokalizaci pořád říká, že vzdání je v ovládání hry nebo v menu ⋮. Horní menu ⋮ má vzhled desky, lampu a průvodce. Vzdání je v listu Více.

## Co ověřit na železe

1. Trojklik otevře okno, LED pulzuje, do 60 s se nové zařízení spáruje a dostane token. Druhý trojklik okno zavře. Po vypršení neznámý telefon spojení nedrží.
2. Už spárovaný telefon, po dokončení onboardingu (nebo při dalším otevření aplikace a při návratu na popředí), se připojí sám, bez trojkliku.
3. Druhý známý telefon, dokud první drží BLE, uvidí „Deska je připojená k jinému zařízení.“ a tah neprojde. Stejné u Wi‑Fi s jiným tokenem (`board_busy`). Příkaz bez tokenu deska odmítne.
4. Puzzle: po `load` svítí zelená / bílá / červená podle chybějících a přebývajících figur. `start` před srovnáním vrátí 409. Po srovnání tah projde a špatný tah deska označí hned. `cancel` světla stavění zhasne. Po restartu desky session puzzle není.
5. Vzdání z listu Více: dialog řekne barvu na tahu, Zrušit partii nechá být, potvrzení ukončí hodiny a zapíše výsledek. Stejný POST po Wi‑Fi i BLE JSON. Mimo partii a v puzzle je 409. Zvednutí krále na 10 s pořád funguje vedle příkazu z aplikace.
6. Po startu aplikace s rozjetou partií je záložka Hra a oboje hodiny jsou vidět. Souřadnice jsou vlevo a dole; s černým dole je vlevo dole h8.

## Blender

Jeden snímek nebo animace na krok, 16:9, 1280×720. Flutter bere nejdřív `.webp`, potom `.png`.

| Krok | Soubor |
|------|--------|
| Připojení | `flutter_czechmate/assets/guide/connect/connect.webp` nebo `connect.png` |
| Tah | `flutter_czechmate/assets/guide/move/move.webp` nebo `move.png` |
| Nápověda | `flutter_czechmate/assets/guide/hint/hint.webp` nebo `hint.png` |
| Hodiny | `flutter_czechmate/assets/guide/clock/clock.webp` nebo `clock.png` |
| Vzdání | `flutter_czechmate/assets/guide/resign/resign.webp` nebo `resign.png` |

Animovaný WebP z PNG sekvence (na Windows desktop není video plugin):

```bash
ffmpeg -framerate 30 -i frame_%04d.png -loop 0 -q:v 80 connect.webp
```

Složky jsou v `pubspec.yaml`. Dokud v nich je jen `.gitkeep`, průvodce ukáže ikonu a text.
