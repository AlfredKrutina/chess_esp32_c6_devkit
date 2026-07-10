# Plán v2: Interaktivní trénink zahájení (Opening Trainer)

**Verze:** 2.2 (implementační review 2026-07-10)  
**Stav:** **v1.0 v implementaci** — FW + Flutter + web parita hotové; katalog 41 linií; rationale sidecar; fyzický/virtuální soupeř  
**Aktivní PR:** [#9](https://github.com/AlfredKrutina/chess_esp32_c6_devkit/pull/9) (`cursor/opening-opponent-physical-8fdd`)  
**Cíl:** Krok-za-krokem výuka slavných zahájení pro **bílé i černé**, s fyzickou deskou, LED nápovědou a jednotným UX na webu + Flutter.  
**Vstupní dokumentace:** [docs/README.md](../README.md) · [MATRIX_GUARD.md](MATRIX_GUARD.md) · [WEB_UI_DEPLOY.md](WEB_UI_DEPLOY.md) · [CZECHMATE_INTEGRATION_CHECKLIST.md](CZECHMATE_INTEGRATION_CHECKLIST.md)

---

## 0. Co je nového oproti v1

| Oblast | v1 | v2 upgrade |
|--------|----|------------|
| Rozhodnutí | 4 otevřené otázky | **Uzavřená architektura** (§3) s odůvodněním |
| Deska | „virtuální soupeř“ zmíněn | **Model synchronizace fyzické vs logické desky** (§6) |
| LED | statické cyan/orange | **Reuse `LED_CMD_ANIM_MOVE_PATH`** + nový pulzní hint (§9) |
| API | jeden endpoint | **Plný kontrakt** včetně chyb, BLE, snapshot (§10) |
| Obsah | 24 zahájení tabulka | **41 linií + učební cesty + rationale sidecar** (§8, §12) |
| Soupeř | jen virtuální model | **Volitelný fyzický soupeř** (`opponent_mode`) — parita s botem (§3 D10, §6.6) |
| Pedagogika | `idea` + `steps` | **`rationale`** — proč varianta, místo čeho, kdy hrát (§8.6) |
| Fáze | 6 hrubých fází | **9 fází + stav implementace** s checklistem (§14) |
| Inventář kódu | obecný | **Konkrétní soubory, funkce, mezery** (§5) |
| Progress | localStorage zmínka | **Hvězdičky, spaced repetition, curriculum unlock** (§11) |
| Sanity | — | **§20 — ověření proti kódu + HW** (matrix guard, setup, captures) |

---

## 0.1 Changelog v2 → v2.1 (sanity review)

| Oprava | Proč |
|--------|------|
| Matrix guard **musí** být vypnutý v opening režimu | Bez toho virtuální tahy okamžitě spustí guard (logika ≠ matrix) |
| Setup **není** puzzle-style prázdná deska pro standardní FEN | `prepare` = empty board jen pro mid-line FEN (v1.1); v1 = startovní pozice |
| Checkpoint = **fyzický resync**, ne jen UI tlačítko | `checkpoint_ack` až po shodě matrix vs logika na checkpoint FEN |
| `game_execute_move_uci` → `game_opening_apply_uci()` | V kódu neexistuje UCI helper; reuse `convert_notation_to_coords` + `game_execute_move` |
| Mirror režim přejmenován / upřesněn | Není „hrát soupeřovy tahy v jedné linii“, ale **párová linie** `mirror_line_id` |
| Katalog v1: omezení capture/rosady | Hráčův capture na virtuálně přesunutou figuru bez resyncu je nemožný |
| Web fáze 2: Flutter-first nebo obnovit embed | `/chess_app.js` dnes vrací 404 (`browser_ui_removed`) |

---

## 0.2 Changelog v2.1 → v2.2 (implementační review)

| Změna | Proč |
|--------|------|
| Katalog **41 linií** (9+9+12+11) | Původní cíl 30 splněn a rozšířen; §12 synchronizováno s `openings_master.json` |
| **`opponent_mode`**: `physical` \| `virtual` | Hráč může přesouvat figurky soupeře sám (LED jako u bota); default `physical` |
| **`openings_rationale.json`** sidecar | Pedagogika oddělená od tahů; merge při `sync_catalog.py --copy` |
| Setup phase **nesmí** zrušit opening config | `game_enter_board_setup_tutorial()` zachová aktivní linii |
| Status JSON: `setup_phase`, `physical_match`, `mistake_hint`, `wrong_move_count` | Parita Flutter + web setup wizard a retry |
| Web embed **obnoven** | `concat_web_js.py` → `chess_app.js`; opening moduly v `web/js/` |
| Learn screen **naviguje** do katalogu / lekce | L10/L12 → `OpeningTrainerScreen` |

---

## 0.3 Stav implementace (živý přehled)

| Oblast | Stav | Důkaz / poznámka |
|--------|------|------------------|
| Schema + validátor | ✅ | `sync_catalog.py --validate --physical-rules` |
| FW `game_opening_trainer.c` | ✅ | start/cancel/hint/checkpoint, auto-reply |
| Matrix guard OFF v opening | ✅ | D8 v `game_task_matrix_guard_mode_conflict_active` |
| `opponent_mode` physical/virtual | ✅ | `game_physical.c` validace tahu soupeře |
| HTTP `POST /api/game/opening` | ✅ | `web_opening_dispatch.c` |
| Status `opening_training` | ✅ | `game_json_export.c` + Flutter modely |
| Katalog 41 linií | ✅ | `data/openings_master.json` |
| Rationale 41 záznamů CS/EN | ✅ | `data/openings_rationale.json` |
| Flutter katalog + lekce | ✅ | `opening_catalog_screen.dart`, `opening_trainer_screen.dart` |
| Web opening moduly | ✅ | `opening_catalog.js`, `opening_trainer.js` |
| Režimy Learn/Drill/Timed/Mirror | ✅ | Hvězdičky + curriculum unlock |
| Setup wizard + retry | ✅ | `setup_phase` flow obě platformy |
| PGN import | ✅ | `tools/openings/pgn_to_catalog.py` |
| `common_mistakes` v UI | ⬜ | Schema připraveno, obsah/UI backlog |
| Miniboard sync v lekci | ⬜ | Text + progress bar; plný miniboard backlog |
| Stockfish „proč tento tah“ | ⬜ | Fáze 5 backlog |
| Spaced repetition notifikace | ⬜ | Fronta due lines hotová; push notif backlog |
| Větvení `branches[]` | ⬜ | v1.1 |
| FW-native LED pulz | ⬜ | Fáze 6 backlog |

**Zbývá pro v1.0 polish:** §14 Fáze 5 (common_mistakes, miniboard, manuální UX test 5 uživatelů).

---

## 1. Shrnutí a design principy

CZECHMATE už umí **skládat pozici po krocích** (setup tutorial, puzzle prepare) a **ukázat tah na LED** (`hint_highlight`). Opening Trainer spojí tyto bloky do režimu **`opening_trainer`**:

1. Katalog **slavných linií** (JSON na klientu).
2. **Setup** výchozí FEN (reuse wizardu).
3. **Řetězec tahů** s validací UCI na fyzické desce.
4. **Soupeř** — volitelně virtuální (logická deska) **nebo fyzický** (`opponent_mode: physical`, LED ukáže tah).
5. **Postupné režimy** Learn → Drill → Timed → Mirror (obě strany).

### Principy kvality (nezpochybnitelné)

| Princip | Implementace |
|---------|--------------|
| Fyzická deska = primární vstup | UI jen řídí a vysvětluje; tah bez matrix pickup/drop neplatí |
| LED čitelná při živé lekci | FW posílá hint/trace při `start` a auto-reply; klient **refreshuje** hint každých 600 ms |
| Parita Web = Flutter = BLE | Jeden JSON kontrakt; žádné „jen web“ API |
| Offline-first | Katalog v assets; FW nepotřebuje internet |
| Malé PR, zelená CI | Každá fáze = samostatná větev `cursor/opening-trainer-phaseN-8fdd` |
| Žádné nové monolity | `game_opening_trainer.c` max ~400 ř.; logika v existujících modulech |

---

## 2. Cíle a ne-cíle

### Cíle (v1.0 produkt)

| ID | Cíl | Metrika |
|----|-----|---------|
| G1 | **41 linií** (21 bílých + 20 černých v 4 kurikulech) | 100 % legální UCI + rationale v CI |
| G2 | Krok za krokem: setup → linie → dokončení | E2E HW test 3 linie |
| G3 | Fyzická deska + LED | 0 regressí matrix guard |
| G4 | Learn / Drill / Timed / Mirror | Každý režim má HW checklist |
| G5 | Web + Flutter parita | Stejná lekce dokončitelná z obou |
| G6 | Curriculum s odemykáním | 4 učební cesty (§8.3) |
| G7 | Progress + hvězdičky | Uloženo lokálně; sync volitelně v2 |

### Ne-cíle (v1.0)

- Opening book engine / neomezená Stockfish hloubka během drillu
- Rozpoznávání typu figury na matrix (jen 0/1 obsazenost)
- Cloud účty a sync mezi zařízeními
- Lichess API za běhu
- Větvení variant (více odpovědí soupeře) — **architektura připravena**, obsah v1.1
- Mazání / přejmenování existujících souborů bez schválení

---

## 3. Uzavřená architektura (dříve „rozhodnutí k potvrzení“)

| # | Rozhodnutí | Volba v2 | Proč |
|---|------------|----------|------|
| D1 | Kde žije katalog | **Klient** (`openings_catalog.json`) | Flash ESP ušetříme; obsah updatuje se s app/web bez OTA |
| D2 | Formát linie | **Plná `line_uci[]`** + `player_ply_indices[]` | FW jednoduše iteruje ply; soupeř i hráč ve stejném poli |
| D3 | Soupeř | **`opponent_mode`**: `virtual` (logika) nebo `physical` (hráč přesune figurku) | Default `physical` = stejné UX jako bot; `virtual` = rychlejší, vyžaduje checkpoint |
| D4 | Režim FW | **Samostatný `opening_trainer`** | Puzzle = 1 tah; opening = stavový stroj s auto-reply — jiná semantika |
| D5 | Setup startovní pozice | **Standardní FEN: přeskočit empty prepare** | Pokud `game_is_physical_board_starting_occupancy()` → rovnou `start`; jinak setup wizard (32 kroků). Empty-board `prepare` jen pro mid-line FEN (v1.1) |
| D6 | Hint LED | **Klient refresh 600 ms** + FW internal hint po auto-reply | Reuse `SETUP_TUTORIAL_REFRESH_MS`; FW-native pulz = Fáze 6 |
| D7 | `GAME_CMD` slot | **`GAME_CMD_OPENING_TRAINER`** v `chess_types.h` | Konzistentní s puzzle/setup pattern |
| D8 | Matrix guard | **Vypnutý po celou aktivní lekci** (`virtual` soupeř) | Při `physical` soupeři deska zůstává synchronní — guard stále off kvůli konzistenci režimu |
| D9 | Validace tahu | **Nejdřív expected UCI, pak `game_is_valid_move`** | Špatná destinace = opening feedback; legalita z logické desky |
| D10 | Volba soupeře | **`opponent_mode` v `start` requestu** | `physical`: FW čeká pickup/drop soupeře, feedback `opponent_turn`; `virtual`: auto-reply na logice |
| D11 | Pedagogika variant | **Sidecar `openings_rationale.json`** | Merge do exportu; master drží tahy, rationale editovatelné bez dotyku UCI |

---

## 4. Uživatelské režimy a pedagogika

### 4.1 Režimy tréninku

```mermaid
stateDiagram-v2
  [*] --> Browse: Otevřít katalog
  Browse --> Setup: prepare
  Setup --> Learn: start (learn)
  Setup --> Drill: start (drill)
  Learn --> Drill: Dokončeno 1×
  Drill --> Timed: 3× bez chyby
  Drill --> Mirror: Odemčeno curriculum
  Timed --> [*]: Čas OK
  Learn --> [*]: Linie hotová
  Drill --> [*]: Linie hotová
  Mirror --> [*]: Protistrana hotová
  Browse --> [*]: cancel
  Setup --> [*]: cancel
```

| Režim | Chování | LED | UI |
|-------|---------|-----|-----|
| **Learn** | Komentář ke každému hráčovu ply; rationale na úvodu; soupeř dle `opponent_mode` | Zlatý pulz `to`; cyan `from` po pickup; u physical i LED tah soupeře | Velký text + rationale panel + progress |
| **Drill** | Jen „Tah N/M“; bez komentářů | Jen `to`; po 3 chybách `mistake_hint` | Minimální panel |
| **Timed** | Drill + časovač (celá linie nebo/tah) | Stejné + červený pulz pod 5 s | Timer ring |
| **Mirror** | Samostatná **párová linie** (`mirror_line_id`, typicky opačná barva) | Stejné LED | Badge „Trénuješ černou proti 1.e4“ |
| **Review** | Prohlížení dokončené linie bez HW | Animace na miniboardu | Pouze app (bez FW) |

### 4.2 Hvězdičky a mastery

| Hvězda | Podmínka |
|--------|----------|
| ★ | Dokončeno v Learn |
| ★★ | Dokončeno v Drill ≤ 2 chyby |
| ★★★ | Dokončeno v Timed v limitu |
| ★★★★ | Dokončena **párová** linie `mirror_line_id` s ★★ |

Progress klíč: `opening_progress_v1` → `{ "line_id": { "stars": 3, "best_drill_errors": 1, "last_completed_at": "ISO" } }`.

### 4.4 Režim soupeře (`opponent_mode`)

| Režim | Kdo hýbe figurkou soupeře | Logická deska | Matrix | Typické použití |
|-------|---------------------------|---------------|--------|-----------------|
| **`physical`** (default) | Hráč — LED ukáže `from`→`to` | Tah se provede po fyzickém dropu | **Synchronní** s logikou | Klubová praxe, začátečníci, parita s botem |
| **`virtual`** | FW auto-reply | Soupeř táhne virtuálně | **Může divergovat** (ghost) | Rychlejší drill; vyžaduje checkpoint resync |

Volba v **mode pickeru** (Flutter SegmentedButton; web stejný kontrakt v `openingStartPayload`).

---

### 4.5 Zdůvodnění variant (`rationale`)

Každá linie = jedna varianta. Kromě `idea` (strategický motiv) má exportovaný katalog blok **`rationale`**:

| Pole | Účel v UI |
|------|-----------|
| `summary` | Subtitle v seznamu katalogu (1 věta) |
| `why_this_line` | Proč učíme právě tuto linii |
| `instead_of` | Hlavní alternativa a proč ne |
| `when_to_play` | Praktický kontext (turnaj, klub, styl) |
| `related_line_ids` | Klikatelné související linie v mode pickeru |

Zdroj: `data/openings_rationale.json` (sidecar). Detail §8.6.

---

### 4.6 Spaced repetition (Fáze 7)

- Linie s ★★ se řadí do fronty „opakovat za 3 dny“ (`OpeningCurriculumUnlock.linesDueForReview`).
- Notifikace ve Flutteru (lokální) — **backlog**; web = banner při otevření katalogu.

---

## 5. Inventář existujícího kódu (audit)

### 5.1 Co přímo reuseovat

| Potřeba | Soubor | Symbol / detail |
|---------|--------|-----------------|
| Puzzle lifecycle vzor | `game_puzzle.c` | `enter_setup` → `start` → `cancel`; feedback enum |
| Setup tutorial FW | `game_init.c` | `game_enter_board_setup_tutorial()`, `game_finish_board_setup_tutorial_from_web()` |
| Fyzická validace | `game_physical.c` | `game_process_drop_command` — error recovery, blink |
| Matrix guard | `game_task.c` | `game_task_matrix_guard_mode_conflict_active()` — **rozšířit** |
| Status JSON | `game_json_export.c` | `game_get_status_json()` — přidat `opening_training` |
| HTTP handlery | `web_handlers_game.c` | Vzor `http_post_game_puzzle_handler`, `setup_tutorial` |
| Routes | `web_routes.c` | Registrace `POST /api/game/opening` |
| LED hint | `web_handlers_game.c` | `web_server_apply_hint_highlight_json_body` |
| LED anim tahu | `led_task.c` | `LED_CMD_ANIM_MOVE_PATH` → `led_anim_move_path()` (~ř. 2207) |
| LED barvy hint | `led_task.c` | cyan from / orange to (`LED_CMD_HIGHLIGHT_HINT`) |
| Web setup UX | `web/js/app_main.js` | `SETUP_TUTORIAL_*`, `buildPuzzleSetupStepsFromFen()` |
| Web prefs/API | `web/js/api.js`, `prefs.js` | `apiPostJson`, auth headers |
| Flutter setup | `board_setup_wizard_screen.dart` | Matrix poll 400 ms, LED refresh 900 ms |
| Flutter FEN kroky | `board_setup_fen_steps.dart` | `BoardSetupFenSteps.build(fen)` |
| Flutter API | `board_api_client.dart` | `postSetupTutorial`, `postHintHighlight*` |
| Flutter session | `board_session_notifier.dart` | `postSetupTutorialAction`, `postHintDestination` |
| BLE parita | `ble_czechmate_client.dart` | `postSetupTutorial`, `postHintHighlightDestinationOnly` |
| ECO popisky | `opening_eco.dart` | Rozšířit mapování pro katalog karty |

### 5.2 Kritické mezery — původní (Fáze 1) → stav v2.2

| Mezera (původně) | Stav v2.2 |
|-------------------|-----------|
| Žádný `game_opening_trainer.c` | ✅ Implementováno |
| Žádný `POST /api/game/opening` | ✅ `web_opening_dispatch.c` |
| Puzzle = 1 tah | ✅ Větev v `game_physical.c` |
| Flutter nevolá opening API | ✅ `postOpeningAction` |
| Status JSON bez `opening_training` | ✅ Export + parita |
| `openings_catalog.json` neexistuje | ✅ 41 linií + rationale v assets |

**Zbývající mezery (polish):** `common_mistakes` ve FW/klientu, plný miniboard v lekci, FW-native LED pulz.

### 5.3 Vzácná vyloučení režimů a matrix guard

Rozšířit `game_task_matrix_guard_mode_conflict_active()` (`game_task.c` ~1801) a hint gating (`app_main.js` ~1158):

```c
|| game_is_opening_trainer_active()
|| game_is_opening_trainer_setup_active()
```

**Proč je to povinné (ne jen „blokovat jiné režimy“):**  
`game_matrix_guard.c` při `mode_conflict_active()` **ignoruje guard** a nechává UP/DN pokračovat. Virtuální tahy soupeře záměrně vytváří rozdíl logika ↔ matrix (§6). Bez D8 by guard po prvním auto-reply pozastavil hru.

Stejně rozšířit:

- `game_matrix_guard_check_resync_after_restore()` — přeskočit při aktivním opening traineru
- `game_physical.c` — pickup/drop povolen i když matrix ≠ logika (guard inactive)

Blokovat současně: normální hra, puzzle, setup tutorial, bot tah, Stockfish hint.

---

## 6. Model synchronizace fyzické vs logické desky

**Nejdůležitější designový problém.** Matrix vidí jen obsazenost; soupeř hraje virtuálně. Tento model **dává smysl jen s D8** (guard off) a **fyzickým checkpoint resync**.

### 6.0 Verdikt: dává to smysl?

| Otázka | Odpověď |
|--------|---------|
| Lze validovat tahy hráče? | **Ano** — porovnání `(from,to)` s UCI + `game_is_valid_move` na logické desce |
| Spustí matrix guard? | **Ano, bez D8** — po 1. virtuálním tahu. **Řešení:** opening v `mode_conflict_active` |
| Lze hrát capture v linii? | **Jen s opatrností** — viz §6.5 a §8.5 |
| Je lepší fyzický soupeř? | Pro v1 **ne** — 2× délka lekce, vyšší chybovost u začátečníků |
| Alternativa v2.0 HW? | Hall senzory s typem figury — mimo scope V1 |

**Závěr:** Plán je realizovatelný na V1 reed desce, pokud dodržíme D8 + checkpoint resync + katalogová pravidla.

### 6.1 Pravidla

| Fáze | Logická deska | Fyzická deska (matrix) |
|------|---------------|------------------------|
| **Setup** | `game_load_position_from_fen(start_fen)` po potvrzení fyzické shody | Hráč má figurky na startovní pozici (wizard jen pokud nesedí) |
| **Hráčův tah** | Očekává se UCI tah hráče | Pickup/drop musí sedět s `expected_from/to` |
| **Soupeřův tah (`virtual`)** | `game_opening_apply_uci()` na logice | **Žádná změna matrix** |
| **Soupeřův tah (`physical`)** | Po fyzickém dropu hráče | **Matrix i logika se mění** — feedback `opponent_turn` |
| **Checkpoint** | FEN po `ply_index` | **Hráč fyzicky srovná** podle LED diff wizardu |
| **Po checkpointu** | Shoda logika = matrix (0/1) | Pokračuje další hráčův tah |

### 6.2 Důsledek: „ghost“ figurky (mezi checkpointy)

Mezi checkpointy **fyzická deska ≠ logická pozice** — záměr, ne bug.

**Mitigace v1:**

1. **Checkpoint = fyzický resync** (ne jen „OK“ tlačítko): klient porovná `matrix_occupied` vs occupancy z logické FEN; `checkpoint_ack` FW přijme jen při shodě (nebo tolerance 0 diff).
2. **Resync wizard:** reuse `BoardSetupFenSteps` — jen pole kde se liší occupancy (max ~8 figur po 4 tazích).
3. **Learn mode:** po auto-reply pauza 1,5 s + `led_anim_move_path` + text „Soupeř hrál … (virtuálně)“.
4. **Drill/Timed:** checkpoint **povinný** před dalším hráčovým tahem.

```mermaid
sequenceDiagram
  participant P as Hráč (fyzicky)
  participant M as Matrix
  participant L as Logická deska
  participant LED as LED

  P->>M: Tah e2→e4
  M->>L: expected UCI OK → execute
  L->>LED: Green flash e4
  L->>L: Auto e7→e5 (virtuální)
  L->>LED: ANIM_MOVE_PATH e7→e5
  Note over P,M: Mezi checkpointy: matrix ≠ logika<br/>Guard je OFF (D8)
  P->>M: Tah g1→f3
  M->>L: Validace OK (g1,f3 fyzicky sedí)
  L->>L: Checkpoint po ply 4
  P->>M: Resync figur podle LED
  M->>L: checkpoint_ack (shoda)
```

### 6.3 Proč dva režimy soupeře

| Režim | Výhoda | Nevýhoda |
|-------|--------|----------|
| **`physical`** | Stejné UX jako hra proti botovi; deska = logika; žádné ghost figurky | Více fyzických pohybů |
| **`virtual`** | Méně pohybů; rychlejší průchod linií | Ghost figurky mezi checkpointy; nutný D8 |

### 6.4 Proč ne vynutit jen fyzického soupeře

V1 ponechává **obě volby** — začátečníci preferují `physical`, pokročilí drill mohou zvolit `virtual`.

### 6.5 Matrix guard — povinná integrace

```c
// game_matrix_guard.c — existující chování (ř. ~151):
if (game_task_matrix_guard_mode_conflict_active()) {
  // guard se IGNORUJE, UP/DN pokračuje
}
```

Opening trainer **musí** být v tomto seznamu. Bez toho je virtuální model nefunkční.

Doplňkově v `game_matrix_guard_check_resync_after_restore()`:

```c
if (game_is_opening_trainer_active() || game_is_opening_trainer_setup_active()) {
  return;  // neaktivovat guard po NVS restore během lekce
}
```

### 6.6 Omezení tahů v katalogu (v1)

| Typ tahu | Povoleno v1? | Podmínka |
|----------|--------------|----------|
| Hráč: quiet move (e4, Nf3) | ✅ | Vždy |
| Hráč: capture fyzicky přítomné figury | ✅ | Po checkpointu, když je figura na poli fyzicky |
| Hráč: capture jen na logické desce | ❌ | Zakázáno v1 — validator fail |
| Soupeř: capture (virtuální) | ✅ | Vždy; checkpoint za 1–2 ply |
| Rosada | ⚠️ | Jen pokud fyzický resync zvládne wizard — **v1 spíš vynechat** |
| Promoce | ⚠️ | UCI 5 znaků (`e7e8q`) — Fáze 4+, ne v počátečním katalogu |

---

## 7. Architektura systému

```mermaid
flowchart TB
  subgraph content [Content layer — repo]
    Master[data/openings_master.json]
    Rationale[data/openings_rationale.json]
    Script[tools/openings/sync_catalog.py]
    Schema[openings_catalog.schema.json]
  end
  subgraph clients [Clients]
    WebMod[web/js/opening_trainer.js]
    WebUI[app_main.js Learn panel]
    FlRepo[opening_catalog_repository.dart]
    FlUI[OpeningTrainerScreen]
  end
  subgraph esp [ESP32 firmware]
    Routes[web_routes.c]
    Handler[web_handlers_game.c]
    Dispatch[game_dispatch.c]
    OT[game_opening_trainer.c]
    Phys[game_physical.c]
    JSON[game_json_export.c]
    LED[led_task.c]
  end
  Master --> Script
  Rationale --> Script
  Script --> WebData[web/data/openings_catalog.json]
  Script --> FlAssets[flutter assets]
  Schema --> CI[CI validator]
  WebMod --> WebData
  FlRepo --> FlAssets
  WebMod --> Handler
  FlUI --> Handler
  Handler --> Dispatch
  Dispatch --> OT
  OT --> Phys
  OT --> LED
  OT --> JSON
```

### Rozdělení odpovědností

| Vrstva | Odpovědnost |
|--------|-------------|
| **Master JSON** | Jediný zdroj pravdy v `data/openings_master.json` |
| **Sync skript** | Kopie do web + Flutter assets; validace python-chess |
| **Firmware** | Stav stroje, UCI validace, auto-reply, LED příkazy, feedback |
| **Klient** | Katalog UI, texty, progress, hint refresh, checkpoint UX |

Firmware **nezná** názvy zahájení — dostane `line_uci[]`, `player_ply_indices[]`, `mode`, `start_fen`.

---

## 8. Datový model obsahu

### 8.1 Umístění souborů

```
data/openings_master.json              # zdroj pravdy — tahy, kurikula (git)
data/openings_rationale.json           # sidecar — pedagogika variant (git)
data/openings_rationale.schema.json    # schema rationale
data/openings_catalog.schema.json      # JSON Schema draft-07 (export + master)
tools/openings/sync_catalog.py         # validace + merge rationale + kopie
tools/openings/pgn_to_catalog.py       # import z PGN
components/web_server_task/web/data/openings_catalog.json
flutter_czechmate/assets/data/openings_catalog.json
```

### 8.2 Schéma katalogu v2

```json
{
  "$schema": "../openings_catalog.schema.json",
  "version": 2,
  "locale_default": "cs",
  "curricula": [
    {
      "id": "white_classics",
      "name": { "cs": "Klasická bílá", "en": "White classics" },
      "line_ids": ["italian_giuoco_white", "spanish_berlin_white"]
    }
  ],
  "openings": [
    {
      "id": "italian_giuoco_white",
      "eco": "C50",
      "family": "italian",
      "name": { "cs": "Italská hra — Giuoco Piano", "en": "Italian Game — Giuoco Piano" },
      "side": "white",
      "difficulty": 2,
      "tags": ["classical", "e4", "development"],
      "idea": { "cs": "Rychlý rozvoj a tlak na f7.", "en": "Rapid development and pressure on f7." },
      "start_fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "line_uci": ["e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5", "c2c3", "g8f6"],
      "player_ply_indices": [0, 2, 4, 6],
      "checkpoint_ply_indices": [4],
      "steps": [
        {
          "ply_index": 0,
          "comment": { "cs": "Ovládni centrum e4.", "en": "Control the center with e4." },
          "arrow": { "from": "e2", "to": "e4" }
        },
        {
          "ply_index": 2,
          "comment": { "cs": "Rozviň jezdce na f3.", "en": "Develop the knight to f3." }
        }
      ],
      "opponent_annotations": [
        {
          "ply_index": 1,
          "comment": { "cs": "Černý odpovídá symetricky v centru.", "en": "Black mirrors in the center." }
        }
      ],
      "common_mistakes": [
        {
          "wrong_uci": "f1b5",
          "at_ply_index": 4,
          "hint": { "cs": "Nejdřív c4 — španělská je jiná lekce.", "en": "Play c4 first — Spanish is another lesson." }
        }
      ],
      "mirror_line_id": "italian_giuoco_black",
      "prerequisites": []
    }
  ]
}
```

**Pravidla:**

- `line_uci` = **kompletní** sekvence včetně soupeře.
- `player_ply_indices` = indexy, kde **očekáváme fyzický tah hráče**.
- `steps[]` odkazuje na `ply_index` (ne duplicitní UCI).
- `checkpoint_ply_indices` = po tomto ply **povinný fyzický resync** před dalším hráčovým tahem.
- `common_mistakes` = **jen klient** v1 (porovnání před odesláním tahu); FW v1.1.
- Pro `side: "black"`: první bílé ply v `line_uci` se auto-playne virtuálně; hráč začíná na `player_ply_indices[0]`.

### 8.5 Validace obsahu (CI + python-chess)

Kromě §8.4 pipeline přidat kontroly:

1. **Legální UCI** — každý tah z `start_fen`.
2. **Player capture rule** — u každého `player_ply_indices[i]` s capture: buď `requires_checkpoint_before: true`, nebo capture cíl byl fyzicky na desce po předchozím checkpointu (simulace).
3. **Žádná rosada** v `line_uci` v1 (`e1g1`, `e8c8`, …) — fail CI.
4. **Max 12 plies**, max 6 hráčových tahů.
5. **`checkpoint_ply_indices`** musí být ≤ poslední virtuální soupeřův tah před dalším hráčovým capture (pokud existuje).

Skript: `tools/openings/sync_catalog.py --validate --physical-rules`

### 8.3 Učební cesty (curricula) — 41 linií

| ID | Název | Počet linií | Odemčení |
|----|-------|-------------|----------|
| `basics_white` | Bílé základy | 9 | Vždy |
| `basics_black` | Černé základy | 9 | Po 2× `basics_white` ★ |
| `classical_deep` | Klasika hlouběji | 12 | Po `basics_*` ★★ |
| `systems` | Systémová hra | 11 | Po 5 liniích ★★ celkem |

### 8.6 Sidecar rationale (pedagogika variant)

**Proč sidecar:** autorské texty se mění často; UCI linie zůstávají stabilní. CI validuje pokrytí 1:1.

```json
{
  "version": 1,
  "entries": {
    "italian_giuoco_white": {
      "summary": { "cs": "Nejklidnější italská…", "en": "The calmest Italian…" },
      "why_this_line": { "cs": "…", "en": "…" },
      "instead_of": { "cs": "Místo Two Knights…", "en": "Instead of Two Knights…" },
      "when_to_play": { "cs": "…", "en": "…" },
      "related_line_ids": ["italian_two_knights_white", "spanish_berlin_white"]
    }
  }
}
```

**Pipeline:**

```bash
python3 tools/openings/sync_catalog.py --validate --physical-rules  # kontrola rationale coverage
python3 tools/openings/sync_catalog.py --copy                       # merge → web + Flutter
python3 components/web_server_task/tools/concat_web_js.py           # web bundle
```

**UI mapování:** viz §4.5. Soubory: `opening_rationale.dart`, `OpeningRationalePanel`.

### 8.4 Pipeline obsahu

```bash
# Lokální validace (včetně rationale sidecar)
python3 tools/openings/sync_catalog.py --validate --physical-rules
python3 tools/openings/sync_catalog.py --copy   # → web + flutter assets (+ rationale merge)

# Import z PGN studie
python3 tools/openings/pgn_to_catalog.py --pgn studies/italian.pgn --id italian_giuoco_white
```

**CI job `openings-catalog`:**

1. JSON Schema validate `data/openings_master.json`
2. Rationale coverage: každý `opening.id` ∈ `openings_rationale.json`
3. python-chess: každý UCI tah legální od `start_fen`
4. `--physical-rules`: capture/rosada/ghost pravidla
5. Duplicitní `id` = fail
6. Soubory web/flutter **shodné hash** s výstupem skriptu

---

## 9. LED a animace — specifikace

### 9.1 Existující building blocks

| Efekt | Mechanismus | Soubor |
|-------|-------------|--------|
| Hint from/to | `POST /api/game/hint_highlight` | `web_handlers_game.c` |
| Clear | `POST /api/game/hint_clear` | `web_handlers_game.c` |
| Tahová stopa | `LED_CMD_ANIM_MOVE_PATH` | `led_task.c` `led_anim_move_path` |
| Chyba | `game_show_invalid_move_error_with_blink` | error recovery |
| Konec | `LED_CMD_ANIM_ENDGAME` | `led_task.c` |

### 9.2 Opening-specific chování

| Událost | FW akce | Barva / animace |
|---------|---------|-----------------|
| Čeká se na hráče | `hint_highlight` (klient refresh 600 ms) | Orange `to`; cyan `from` po pickup |
| Správný tah | `led_anim_move_path` 300 ms + clear | Zelená trace |
| Špatný tah | error blink | Červená na `to` |
| Soupeř auto-reply | `led_anim_move_path(from,to)` | Modrá trace 1 s |
| Checkpoint | `hint_highlight` na diff polích | Fialová (nová volitelná barva v1.1) |
| Dokončení linie | `LED_CMD_ANIM_ENDGAME` zkrácená | Victory wave 2 s |

### 9.3 Fáze 1b — FW-native pulz (volitelné vylepšení)

Nový `LED_CMD_HIGHLIGHT_HINT_PULSE` — firmware sám pulzuje `to` bez HTTP refresh. Snižuje traffic při Learn. **Backlog** pokud Fáze 1 stačí s klient refresh.

### 9.4 `led_guidance_level` integrace

Respektovat `GET /api/status.led_guidance_level`:

| Level | Learn | Drill |
|-------|-------|-------|
| 0 (off) | Jen UI text | Jen UI |
| 1 | `to` only | `to` only |
| 2 | `from` + `to` | `to` + chyba blink |
| 3 | + opponent trace | + opponent trace |

---

## 10. API kontrakt

### 10.1 Tok lekce (opravený setup flow)

```mermaid
sequenceDiagram
  participant C as Client
  participant F as Firmware

  C->>F: GET /api/status (matrix / board)
  alt Deska = start_fen occupancy
    C->>F: POST opening start + line_uci
  else Deska nesedí
    C->>F: POST setup_tutorial start NEBO opening prepare (v1.1 mid-FEN)
    loop Setup kroky
      C->>F: hint + poll matrix
    end
    C->>F: POST opening start
  end
  F->>F: game_load_position_from_fen(start_fen)
  loop Hráčovy tahy
    C->>F: hint_highlight (refresh)
    Note over F: Hráč fyzický tah
    F->>F: virtual opponent plies + checkpoint?
  end
```

**Důležité:** Pro standardní zahájení (`start_fen` = počátek hry) **nepoužívat** `puzzle`-style `prepare` s prázdnou deskou. To by donutilo hráče zbytečně skládat 32 figur. `prepare` s `setup_phase` vyhradit pro v1.1 mid-line FEN.

### 10.2 `POST /api/game/opening`

**Request:**

```json
{
  "action": "start",
  "line_id": "italian_giuoco_white",
  "mode": "learn",
  "opponent_mode": "physical",
  "start_fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "line_uci": ["e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5", "c2c3", "g8f6"],
  "player_ply_indices": [0, 2, 4, 6],
  "checkpoint_ply_indices": [4]
}
```

| `action` | Chování | HTTP |
|----------|---------|------|
| `prepare` | **v1.1 mid-FEN only:** prázdná logika + `setup_phase`. **v1 standard:** klient nepoužívá — viz §10.1 | 200 |
| `start` | Načte FEN, auto-play bílé úvodní ply pro black lines, `active=true` | 200 |
| `cancel` | Reset; `hint_clear` | 200 |
| `hint` | Zopakuje LED pro aktuální expected tah | 200 |
| `checkpoint_ack` | Klient potvrdil **fyzický** resync (FW ověří matrix vs logika) | 200 nebo 409 `resync_incomplete` |
| `pause` / `resume` | Timed mode | 200 |

**Response 200:**

```json
{
  "ok": true,
  "opening_training": {
    "active": true,
    "setup_phase": false,
    "mode": "learn",
    "opponent_mode": "physical",
    "line_id": "italian_giuoco_white",
    "ply_index": 2,
    "ply_total": 8,
    "player_ply_index": 1,
    "player_ply_total": 4,
    "player_side": "white",
    "feedback": "none",
    "message_key": "opening.await_move",
    "expected_from": "g1",
    "expected_to": "f3",
    "checkpoint_required": false,
    "awaiting_checkpoint_ack": false,
    "physical_synced": true,
    "wrong_move_count": 0,
    "mistake_hint": false
  }
}
```

**Feedback hodnoty:** `none` | `correct` | `wrong` | `mistake_hint` | `complete` | `illegal` | `checkpoint` | `opponent_turn`

Při `opponent_mode: physical` a tahu soupeře: `feedback: "opponent_turn"`, `awaiting_opponent_physical: true`, `expected_from` / `expected_to` pro LED.

**Chyby:**

| HTTP | `error` | Kdy |
|------|---------|-----|
| 400 | `invalid_action` | Neznámá action |
| 400 | `invalid_line` | Prázdné `line_uci` |
| 409 | `mode_conflict` | Aktivní hra / puzzle |
| 409 | `setup_incomplete` | `start` bez fyzické startovní pozice |
| 409 | `resync_incomplete` | `checkpoint_ack` ale matrix ≠ logika |
| 503 | `queue_full` | Game command queue |

### 10.3 Status JSON (`GET /api/status` / snapshot)

Nový blok (vedle `puzzle`, `board_setup_tutorial`):

```json
"opening_training": {
  "active": true,
  "setup_phase": false,
  "mode": "drill",
  "line_id": "sicilian_odb_black",
  "ply_index": 5,
  "ply_total": 10,
  "player_side": "black",
  "feedback": "none",
  "message_key": "opening.await_move",
  "expected_from": "g8",
  "expected_to": "f6",
  "last_opponent_uci": "f1c4",
  "checkpoint_required": true,
  "physical_setup_match": true
}
```

`matrix_occupied[64]` exportovat když `setup_phase || checkpoint_required`.

### 10.4 BLE parita

```json
{ "cmd": "opening", "action": "start", "line_id": "...", "opponent_mode": "physical", "line_uci": ["..."], "player_ply_indices": [0,2,4] }
```

Implementace: `ble_nimble_impl.c` → stejný parser jako HTTP (`web_server_apply_opening_json_body` — nová shared funkce vedle hint).

### 10.5 Flutter API

```dart
// board_api_client.dart
Future<Map<String, dynamic>> postOpening(String baseUrl, Map<String, dynamic> body);

// board_session_notifier.dart
Future<void> postOpeningAction({required String action, required OpeningLine line});
```

### 10.6 Staging logy

```c
STAGING_LOGI(TAG, "opening action=%s line=%s ply=%u/%u feedback=%d",
             action, line_id, ply_index, ply_total, feedback);
```

---

## 11. Klienti — UX specifikace

### 11.1 Informační architektura

```
Learn (home)
├── Curriculum cards (4 cesty)
├── Opening catalog (filtr: side / difficulty / tag)
│   └── Opening detail card
│       ├── Rationale panel („Proč tato varianta?“)
│       ├── Volba opponent_mode (Fyzicky / Virtuálně)
│       ├── Start Learn
│       ├── Start Drill (locked until ★)
│       └── Párová linie (mirror_line_id) — locked until ★★
└── Progress summary (hvězdy, streak)

Opening lesson (fullscreen)
├── Phase: Setup → Play → Complete
├── Miniboard (sync /api/board)
├── Instruction panel
├── Progress bar (player plies)
└── Actions: Hint | Pause | Cancel | Checkpoint OK
```

### 11.2 Web moduly

> **Stav repa (2026-07-10):** `chess_app.js` generován `concat_web_js.py`; opening moduly v `web/js/`.

| Soubor | Úkol |
|--------|------|
| `web/js/opening_trainer.js` | Stav lekce, API, progress, checkpoint, rationale (ply 0) |
| `web/js/opening_catalog.js` | Katalog + rationale v exportu (generovaný) |
| `web/js/app_main.js` | Hint gating, setup tutorial hook |
| `web/data/openings_catalog.json` | Generovaný asset (s rationale) |

Reuse konstant: `SETUP_TUTORIAL_REFRESH_MS = 600`, `SETUP_TUTORIAL_FAST_POLL_MS = 400`, `SETUP_TUTORIAL_OCC_STABLE_TICKS = 2`.

### 11.3 Flutter soubory

| Soubor | Úkol |
|--------|------|
| `lib/features/opening/opening_trainer_screen.dart` | Hlavní lekce |
| `lib/features/opening/opening_catalog_screen.dart` | Katalog + filtry |
| `lib/features/opening/opening_catalog_repository.dart` | Parse JSON + rationale |
| `lib/features/opening/opening_rationale.dart` | Model + `OpeningRationalePanel` |
| `lib/features/learn/learn_screen.dart` | Curriculum → katalog / přímá lekce L10/L12 |
| `board_setup_wizard_screen.dart` | `BoardSetupWizardKind.openingStart` |
| `board_session_notifier.dart` | `postOpeningAction` |

### 11.4 Learn screen mapování (L10–L12)

| Lekce | Opening ID | Curriculum |
|-------|------------|------------|
| L10 Control the center | `italian_giuoco_white` | `basics_white` |
| L11 Opening principles | `meta_principles` (text-only, bez FW) | — |
| L12 Ruy Lopez intro | `spanish_berlin_white` | `classical_deep` |

### 11.5 Přístupnost (a11y)

- Každý krok: `Semantics(label: instructionText)` na Flutteru.
- Klávesnice: Enter = Hint, Esc = Cancel.
- Kontrast miniboardu WCAG AA.
- Screen reader oznamuje „Správný tah“, „Chyba“, „Soupeř hrál e5“.

---

## 12. Katalog v1 — 41 linií

> **Zdroj pravdy:** `data/openings_master.json` · pedagogika: `data/openings_rationale.json`  
> Každá linie: **6–12 plných tahů**, checkpoint dle `checkpoint_ply_indices`, bez rosady v1.

### 12.1 `basics_white` (9)

| ID | ECO | Plies |
|----|-----|-------|
| `italian_giuoco_white` | C50 | 8 |
| `london_system_white` | D02 | 8 |
| `four_knights_white` | C47 | 8 |
| `vienna_white` | C25 | 8 |
| `scotch_game_white` | C45 | 8 |
| `colle_system_white` | D05 | 10 |
| `italian_two_knights_white` | C55 | 10 |
| `reti_opening_white` | A06 | 10 |
| `english_four_knights_white` | A28 | 10 |

### 12.2 `basics_black` (9)

| ID | ECO | Plies |
|----|-----|-------|
| `sicilian_odb_black` | B50 | 8 |
| `caro_kann_classical_black` | B12 | 8 |
| `petrov_black` | C42 | 8 |
| `pirc_classical_black` | B07 | 8 |
| `alekhine_defence_black` | B03 | 10 |
| `slav_defence_black` | D10 | 10 |
| `dutch_defence_black` | A80 | 10 |
| `philidor_black` | C41 | 10 |
| `modern_defence_black` | B06 | 10 |

### 12.3 `classical_deep` (12)

| ID | ECO | Plies |
|----|-----|-------|
| `spanish_berlin_white` | C60 | 6 |
| `queens_gambit_white` | D06 | 8 |
| `queens_gambit_declined_black` | D30 | 10 |
| `french_classical_black` | C11 | 10 |
| `nimzo_indian_black` | E20 | 10 |
| `torre_attack_white` | A46 | 10 |
| `ruy_lopez_classical_white` | C78 | 10 |
| `queens_indian_black` | E12 | 10 |
| `kings_indian_black` | E60 | 10 |
| `kings_gambit_declined_white` | C30 | 10 |
| `sicilian_closed_black` | B23 | 10 |
| `semi_slav_black` | D45 | 10 |

### 12.4 `systems` (11)

| ID | ECO | Plies |
|----|-----|-------|
| `english_reversed_white` | A13 | 9 |
| `catalan_white` | E00 | 9 |
| `trompowsky_white` | A45 | 8 |
| `english_symmetrical_white` | A35 | 10 |
| `london_vs_kid_white` | A47 | 10 |
| `bogo_indian_black` | E11 | 10 |
| `grunfeld_black` | D70 | 10 |
| `catalan_vs_slav_white` | E00 | 10 |
| `sicilian_alapin_white` | B22 | 10 |
| `chigorin_black` | D07 | 10 |
| `owen_defence_black` | B00 | 10 |

---

## 13. Firmware — `game_opening_trainer.c`

### 13.1 Stav stroje

```c
typedef enum {
  OPENING_FEEDBACK_NONE = 0,
  OPENING_FEEDBACK_CORRECT,
  OPENING_FEEDBACK_WRONG,
  OPENING_FEEDBACK_MISTAKE_HINT,
  OPENING_FEEDBACK_ILLEGAL,
  OPENING_FEEDBACK_COMPLETE,
  OPENING_FEEDBACK_CHECKPOINT
} opening_feedback_t;

typedef enum {
  OPENING_MODE_LEARN = 0,
  OPENING_MODE_DRILL,
  OPENING_MODE_TIMED,
  OPENING_MODE_MIRROR
} opening_mode_t;

typedef struct {
  bool active;
  bool setup_phase;
  opening_mode_t mode;
  char line_id[48];
  char start_fen[120];
  char line_uci[OPENING_MAX_PLIES][5];  /* OPENING_MAX_PLIES = 16 */
  uint8_t line_uci_count;
  uint8_t player_ply_indices[OPENING_MAX_PLIES];
  uint8_t player_ply_count;
  uint8_t checkpoint_ply_indices[4];
  uint8_t checkpoint_count;
  uint8_t ply_index;
  uint8_t player_ply_index;  /* který hráčův tah v řadě (0..player_ply_count-1) */
  char expected_from[3];
  char expected_to[3];
  opening_feedback_t feedback;
  player_t player_side;
  bool awaiting_checkpoint_ack;
  uint64_t timed_deadline_ms;
} opening_trainer_state_t;
```

### 13.2 Integrační body (soubor po souboru)

| Soubor | Změna |
|--------|-------|
| `chess_types.h` | `GAME_CMD_OPENING_TRAINER`, payload struct |
| `game_opening_trainer.c` | **NOVÝ** — celý stavový stroj |
| `game_task_internal.h` | Export state, hooks pro physical |
| `game_physical.c` | Větev `opening_trainer_active` před/po puzzle |
| `game_dispatch.c` | Handler `GAME_CMD_OPENING_TRAINER` |
| `game_init.c` | Vzájemné vyloučení s puzzle/tutorial |
| `game_json_export.c` | Sekce `opening_training` |
| `game_task.c` | Matrix guard rozšíření |
| `web_handlers_game.c` | `http_post_game_opening_handler` |
| `web_routes.c` | Route registrace |
| `ble_nimble_impl.c` | BLE `opening` cmd |
| `game_task/CMakeLists.txt` | Přidat `game_opening_trainer.c` |

### 13.3 Hlavní funkce

```c
bool game_opening_apply_uci(const char *uci);  /* parse 4–5 znaků → game_execute_move */
esp_err_t game_opening_prepare(const opening_load_request_t *req);
esp_err_t game_opening_start(void);
esp_err_t game_opening_cancel(void);
bool game_opening_validate_checkpoint_physical(void);  /* matrix vs logika */
bool game_opening_on_physical_move(uint8_t from_row, uint8_t from_col,
                                   uint8_t to_row, uint8_t to_col);
void game_opening_advance_after_correct(void);  /* auto-reply loop */
bool game_is_opening_trainer_active(void);
bool game_is_opening_trainer_setup_active(void);
```

`game_opening_apply_uci`: rozparsovat `"e2e4"` / `"e7e8q"` přes `convert_notation_to_coords` (existuje v `game_task.c`) + sestavit `chess_move_t` + `game_execute_move()` — **neexistující** `game_execute_move_uci` nepřidávat.

### 13.4 Validace tahu (flow) — integrace `game_physical.c`

V `game_process_drop_command`, **před** puzzle větví (~ř. 1609):

1. Pokud `game_is_opening_trainer_active()` a `ply_index` je hráčův:
2. Porovnat `(from,to)` s `expected_from/to` z `line_uci[ply_index]`.
3. **Neshoda:** `OPENING_FEEDBACK_WRONG` + error blink (reuse puzzle pattern — dočasně mutovat board pro recovery).
4. **Shoda:** `game_execute_move(&move)` na logice.
5. Volat `game_opening_advance_after_correct()`:
   - while další ply ∉ `player_ply_indices`: `game_opening_apply_uci()` + `led_anim_move_path`
   - pokud další ply ∈ `checkpoint_ply_indices`: `CHECKPOINT`, čekat `checkpoint_ack` + `game_opening_validate_checkpoint_physical()`
6. **Konec linie:** `OPENING_FEEDBACK_COMPLETE` + `LED_CMD_ANIM_ENDGAME` (zkrácená).

**Poznámka:** `game_is_valid_move` se volá až po shodě s expected UCI — jinak by legální ale „špatná varianta“ prošla.

---

## 14. Implementační roadmap

> **Legenda:** ✅ hotovo · 🟡 částečně · ⬜ backlog

### Fáze 0a — Schema a validátor ✅

- [x] `data/openings_catalog.schema.json`
- [x] `data/openings_master.json` — 41 linií
- [x] `tools/openings/sync_catalog.py` včetně `--physical-rules` + rationale merge
- [x] CI job `openings-catalog`

### Fáze 0b — PGN import ✅

- [x] `tools/openings/pgn_to_catalog.py`
- [x] Dokumentace v `tools/openings/README.md`

### Fáze 1a — Firmware core ✅

- [x] `game_opening_trainer.c` — prepare/start/cancel/hint
- [x] `game_physical.c` — opening větev
- [x] `GAME_CMD_OPENING_TRAINER` + dispatch
- [x] `opening_training` v status JSON
- [x] Matrix guard rozšíření

### Fáze 1b — Auto-reply + LED ✅

- [x] `game_opening_advance_after_correct` + `led_anim_move_path`
- [x] Checkpoint stav + `checkpoint_ack`
- [x] BLE `opening` cmd

### Fáze 1c — Fyzický soupeř ✅ (PR #9)

- [x] `opponent_mode` v API a FW
- [x] `opponent_turn` feedback + validace v `game_physical.c`
- [x] Flutter + web volba Fyzicky/Virtuálně (default physical)
- [x] Setup phase preserve opening config

### Fáze 2 — Web parita ✅

- [x] `web/js/opening_trainer.js`, `opening_catalog.js`
- [x] `concat_web_js.py` → `chess_app.js`
- [x] `localStorage` progress

### Fáze 3 — Flutter parita ✅

- [x] `OpeningTrainerScreen`, repository, rationale model
- [x] `postOpeningAction` v notifier + BLE
- [x] Learn screen navigace L10, L12 + katalog
- [x] SharedPreferences progress

### Fáze 4 — Plný katalog + režimy ✅

- [x] 41 linií v `openings_master.json`
- [x] Drill + Timed + Mirror
- [x] Curriculum unlock logika
- [x] Hvězdičky

### Fáze 4b — Rationale pedagogika ✅ (PR #9)

- [x] `data/openings_rationale.json` — 41 CS/EN záznamů
- [x] Merge do exportu; Flutter + web UI
- [x] `opening_rationale_test.dart`

### Fáze 5 — Pedagogika a polish 🟡

- [x] Rationale CS/EN kompletní (sidecar)
- [ ] `steps[]` komentáře u všech hráčových ply (částečně)
- [ ] `common_mistakes` u 10+ linií + UI
- [ ] Miniboard sync v lekci
- [ ] Checkpoint „srovnej desku“ UI — základ hotový, polish
- [ ] Stockfish „proč tento tah“ (read-only, Learn only)
- [ ] Rozšířit `MANUAL_TEST_CHECKLIST.md`

**Acceptance:** 5 uživatelů dokončí lekci bez nápovědy dokumentace.

### Fáze 6 — Obsah v1.1 ⬜

- Mid-line FEN start (kratší setup)
- Větvení variant (`branches[]` v JSON)
- SPIFFS mirror katalogu na ESP
- `LED_CMD_HIGHLIGHT_HINT_PULSE` ve FW

### Fáze 7 — Spaced repetition 🟡

- [x] Opakování fronta (`linesDueForReview`)
- [ ] Lokální notifikace Flutter

---

## 15. Testování

| Vrstva | Nástroj | Co testuje |
|--------|---------|------------|
| JSON | `sync_catalog.py`, CI | Schema, legální UCI, hash sync |
| Firmware unit | Host test / Unity stub | UCI → from/to, ply advance |
| API | `scripts/test_opening_api.sh` | HTTP prepare/start/cancel |
| Web | Playwright (mock + live) | Catalog load, lesson state machine |
| Flutter | `flutter test` | Parse catalog, ply index, progress |
| HW | `MANUAL_TEST_CHECKLIST.md` | 3 linie × 4 režimy |
| Regrese | Existující CI | `idf.py build`, matrix guard, puzzle beze změny |

### Kritické HW scénáře

1. Learn Italian white — 4 hráčovy tahy, 3 virtual opponent replies.
2. Sicilian black — auto `e2e4` před prvním černým tahem.
3. Checkpoint po 4. ply — **fyzický resync** + `checkpoint_ack` (409 pokud matrix ≠ logika).
4. Cancel během lekce — návrat do normální hry, guard znovu aktivní.
5. Matrix guard během opening — **nesmí** aktivovat (D8); po `cancel` guard funguje normálně.

---

## 16. Rizika a mitigace

| Riziko | Dopad | Mitigace |
|--------|-------|----------|
| Ghost figurky (fyz ≠ logika) | Zmatení hráče | Checkpoint **fyzický resync** + text v Learn; §6 |
| Matrix guard při virtuálním tahu | Hra zamrzne | **D8** — opening v `mode_conflict_active`; §6.4 |
| Matrix nezná typ figury | Špatná figura při setupu | Snapshot `/api/board` + text „dáma na d1“ |
| Hráčův capture na ghost figuru | Nemožný tah | §8.5 CI `--physical-rules`; checkpoint před capture |
| LED přepsány jiným režimem | Ztráta hintu | 600 ms refresh; mode conflict guard |
| Web UI 404 | Žádný browser klient | ✅ Obnoveno `concat_web_js.py` |
| Příliš dlouhé linie | Únava | Max 12 plies; checkpoint |
| Flutter/web drift | Jiné chování | §10 API kontrakt; shared JSON |
| Flash overflow | Build fail | Katalog jen klient; FW max 16 plies buffer |
| Queue overflow | API 503 | Opening příkazy priorita jako puzzle |
| Špatný PGN import | Nelegální linie | CI python-chess + physical-rules |
| Timed na BLE | Latence | Timer běží na klientu; FW jen stav |

---

## 17. Výkon a paměť (ESP32 budget)

| Položka | Odhad | Limit |
|---------|-------|-------|
| `opening_trainer_state_t` | ~400 B RAM | OK |
| `line_uci[16][5]` v RAM | 80 B | Načteno při start z HTTP body |
| Status JSON rozšíření | +~300 B | Jednorázový buffer |
| LED animace | Reuse existující | Žádná nová task |
| HTTP body max | ~2 KB | Validovat délku v handleru |

**Pravidlo:** FW **neukládá** celý katalog — jen aktivní linii z posledního `start` requestu.

---

## 18. Odkazy na kód

| Oblast | Soubor |
|--------|--------|
| Puzzle vzor | `components/game_task/game_puzzle.c` |
| Setup tutorial | `components/game_task/game_init.c` |
| Fyzická validace | `components/game_task/game_physical.c` |
| Dispatch | `components/game_task/game_dispatch.c` |
| Status JSON | `components/game_task/game_json_export.c` |
| HTTP | `components/web_server_task/web_handlers_game.c` |
| Routes | `components/web_server_task/web_routes.c` |
| LED anim | `components/led_task/led_task.c` |
| Web setup | `components/web_server_task/web/js/app_main.js` |
| Flutter wizard | `flutter_czechmate/lib/features/setup/board_setup_wizard_screen.dart` |
| Flutter learn | `flutter_czechmate/lib/features/learn/learn_screen.dart` |
| ECO | `flutter_czechmate/lib/core/utils/opening_eco.dart` |
| GAME_CMD enum | `components/freertos_chess/include/chess_types.h` |

---

## 19. Referenční inspirace (UX benchmark)

| Produkt | Co převzít | Co nedělat |
|---------|------------|------------|
| **Lichess Opening Trainer** | Krátké linie, okamžitá zpětná vazba | Čistě online bez fyzické desky |
| **Chess.com Lessons** | Komentáře po tazích, hvězdičky | Video-heavy obsah |
| **CZECHMATE Setup Tutorial** | LED + matrix poll | Hardcoded 32 kroků |
| **CZECHMATE Puzzle** | Jednotahová validace + feedback | Jen 1 tah |

**CZECHMATE diferenciátor:** jediný opening trainer s **fyzickou deskou + LED vedením** — UI je doplněk, ne náhrada dotyku figurek.

---

## 20. Sanity review — checklist (ověřeno 2026-07-10)

| Tvrzení v plánu | Ověření v repu | Verdikt |
|-----------------|----------------|---------|
| Setup tutorial + matrix poll funguje | `app_main.js`, `board_setup_wizard_screen.dart` | ✅ |
| Puzzle = 1 tah, opening rozšíření | `game_puzzle.c`, `game_physical.c` | ✅ |
| `game_opening_trainer.c` existuje | `components/game_task/` | ✅ |
| Matrix guard ignoruje opening | `game_task_matrix_guard_mode_conflict_active` | ✅ |
| `opponent_mode` physical/virtual | `game_opening_trainer.c`, `web_opening_dispatch.c` | ✅ |
| Rationale sidecar 41/41 | `openings_rationale.json` + sync merge | ✅ |
| Flutter opening API | `board_session_notifier.dart` | ✅ |
| Web `chess_app.js` | `concat_web_js.py` | ✅ |
| Learn screen → opening | `learn_screen.dart` | ✅ |
| Katalog 41 linií CI | `opening_catalog_test.dart`, `opening_rationale_test.dart` | ✅ |
| `common_mistakes` v UI | grep klient | ⬜ Backlog Fáze 5 |
| Miniboard v lekci | `opening_trainer_screen.dart` | ⬜ Backlog Fáze 5 |

### Doporučené pořadí (další práce)

1. **Fáze 5** — `steps[]` doplnit, `common_mistakes`, miniboard, manuální HW checklist  
2. **Fáze 7** — push notifikace pro spaced repetition  
3. **Fáze 6** — větvení variant (`branches[]`) až po UX feedbacku z v1.0

---

*Plán v2.2 — živý dokument. Implementace: PR [#9](https://github.com/AlfredKrutina/chess_esp32_c6_devkit/pull/9) · větev `cursor/opening-opponent-physical-8fdd`.*
