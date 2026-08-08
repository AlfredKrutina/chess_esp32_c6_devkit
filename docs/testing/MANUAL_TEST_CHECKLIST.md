# Opening Trainer — manual HW checklist (v1.0 release gate)

**Purpose:** Verify §21 release gate in [OPENING_TRAINING_PLAN.md](../reference/OPENING_TRAINING_PLAN.md) on a real board.  
**HW:** ESP32-C6 CzechMate, physical chessboard, LED, Flutter client (BLE or Wi‑Fi).  
**Time:** ~45–60 min for a full run; minimum gate = sections **A + B + C**.

---

## Before testing

| # | Steps | Expected result |
|---|--------|-----------|
| 0.1 | Flash firmware from `main` (`idf.py flash`) | Boot without WDT reset |
| 0.2 | Board in **standard starting position** | Matrix = 32 pieces |
| 0.3 | Flutter connected (BLE or HTTP) | Snapshot `/api/status` or GATT OK |
| 0.4 | (Optional) HTTP smoke: `./scripts/test_opening_api.sh http://<board-ip>` | `action: start` returns 200, status contains `opening_training` |

**Note:** With a virtual opponent, expect a **checkpoint** — the physical board must match the logical position.

---

## A — Gate G2: 3 lines × Learn + Drill

Each line: **Physical opponent** (default), first **Learn**, then **Drill** (after Learn completes).

### A1 — Italian Game for White (`italian_giuoco_white`)

| Step | Action | Expected result |
|------|------|-----------|
| 1 | Catalog → Italian Game → Physical + **Learn** | Lesson starts; rationale only on move 1 |
| 2 | Move `e2→e4` on the board | LED + text “e2 → e4”; move comment |
| 3 | After `e7e5` LED shows opponent move | Move the black pawn |
| 4 | Complete all 4 player moves | `complete` + endgame LED; ★1 saved |
| 5 | Reopen line → **Drill** | No move comments; mistake counter |
| 6 | Complete Drill with ≤2 mistakes | ★2 |

### A2 — Sicilian ODB for Black (`sicilian_odb_black`)

| Step | Action | Expected result |
|------|------|-----------|
| 1 | Catalog → Sicilian → Physical + **Learn** | White `e4` plays automatically before the first black move |
| 2 | First black player move | UCI validation on the board |
| 3 | Complete Learn | ★1 |
| 4 | Drill with ≤2 mistakes | ★2 |

### A3 — Spanish Berlin for White (`spanish_berlin_white`)

| Step | Action | Expected result |
|------|------|-----------|
| 1 | Learn screen L12 or catalog → **Learn** | Mode picker (not a hidden default) |
| 2 | Walk through the line with a physical opponent | Comments + idea visible in Learn |
| 3 | Complete Learn + Drill | ★1 → ★2 |

**Gate G2 ✅** if all three lines pass Learn and Drill without getting stuck.

---

## B — Gate G3/G4: Modes, checkpoint, cancel

### B1 — Virtual opponent + checkpoint

| Step | Action | Expected result |
|------|------|-----------|
| 1 | `italian_giuoco_white`, **Virtual** + Learn | Opponent moves without moving pieces manually |
| 2 | After ~4th ply (checkpoint) | UI “Align board”; miniboard highlights differences |
| 3 | Leave board misaligned → **Continue** | Button disabled / 409 via API |
| 4 | Align board → **Board aligned — continue** | Lesson continues |

### B2 — Timed

| Step | Action | Expected result |
|------|------|-----------|
| 1 | Line with ★≥2 → **Timed** 90 s | Countdown in app bar |
| 2 | Finish within limit | ★3 |
| 3 | (Optional) Let time expire | Lesson ends, no ★3 |

### B3 — Cancel + matrix guard

| Step | Action | Expected result |
|------|------|-----------|
| 1 | Start any lesson | Opening active |
| 2 | During lesson, lift a piece off-turn (ghost) with **virtual** opponent | Matrix guard must **not** freeze the game |
| 3 | Close lesson (×) | Return to normal game |
| 4 | Play a normal move / puzzle | Matrix guard works again |

**Gate G3/G4 ✅** if checkpoint, timed, and cancel all pass.

---

## C — Gate P6: 2 mirror pairs (e4 + d4)

Requires ★★ on the main line before Mirror unlocks.

### C1 — e4 pair: Italian White ↔ Petrov Black

| Step | Action | Expected result |
|------|------|-----------|
| 1 | `italian_giuoco_white` has ★≥2 | Mirror unlocked |
| 2 | Choose **Mirror — opposite side** | Loads `petrov_black` |
| 3 | Complete with ≤2 mistakes | ★4 on `petrov_black` |

### C2 — d4 pair: London System ↔ Slav Black

| Step | Action | Expected result |
|------|------|-----------|
| 1 | `london_system_white` ★≥2 | Mirror unlocked |
| 2 | Mirror → `slav_defence_black` | Opposite side of d4 repertoire |
| 3 | Complete | ★4 |

**Gate P6 ✅** if both pairs work and lead to the correct opposite side.

---

## D — Pedagogy and UX (P1–P5)

| # | Test | Expected result |
|---|------|-----------|
| D1 | Wrong move in Learn (e.g. `Bb5` instead of `Bc4` in Italian) | Text “Not Bb5…” (`common_mistakes`), not `Stav: wrong` |
| D2 | 3× wrong move on the same ply | `mistake_hint` + LED shows correct move |
| D3 | EN locale in the app | English `idea`, `steps`, feedback |
| D4 | Miniboard during lesson | Position + hint from→to; checkpoint purple square |
| D5 | Progress after app restart | ★ persist (SharedPreferences) |

---

## E — BLE-only build (optional HW profile)

For a board with firmware `CONFIG_CHESS_ENABLE_WEB_SERVER=n`:

| # | Steps | Expected result |
|---|--------|-----------|
| E1 | Build: see CI job `ble-only` in `firmware-build.yml` | Link without errors |
| E2 | Flash BLE-only firmware | UART `WEB` reports HTTP disabled |
| E3 | Flutter over BLE: start Learn `italian_giuoco_white` | Same lesson as HTTP build |
| E4 | Hint / cancel / complete over GATT | Parity with HTTP |

---

## F — Web parity (G5, if HTTP build)

| # | Steps | Expected result |
|---|--------|-----------|
| F1 | Browser → board IP → Opening trainer | Catalog of 41 lines |
| F2 | Learn + Drill of one line | Same feedback text as Flutter |
| F3 | `localStorage` `opening_progress_v1` | Stars after completion |

---

## G — Gameplay policy profiles (menuconfig, PR #20)

Verification after Kconfig changes. Requires flash with the corresponding `SDKCONFIG_DEFAULTS`.

### G1 — FULL (production, default)

| # | Steps | Expected result |
|---|--------|-----------|
| G1.1 | Boot monitor | `GAMEPLAY_POLICY: profile=FULL MG=1 ER=1 MH=1` |
| G1.2 | Lift 2 pieces at once | Matrix guard active, yellow/blue LED |
| G1.3 | Illegal move on board | Red square + lock (`error_state.active`) |
| G1.4 | After valid move | Blue/yellow hints per LED guidance |

### G2 — LITE (`sdkconfig.defaults.gameplay_lite`)

| # | Steps | Expected result |
|---|--------|-----------|
| G2.1 | Boot monitor | `profile=LITE`, `MG=0` |
| G2.2 | Lift 2 pieces | **No** guard, game continues (UART warning) |
| G2.3 | Illegal move | UART error, **no** red LED, **no** lock |
| G2.4 | `/api/status` | `matrix_guard_active=false`, `error_state.active=false`, `gameplay_profile":"LITE"` |

### G3 — DEV (`sdkconfig.defaults.gameplay_dev`)

| # | Steps | Expected result |
|---|--------|-----------|
| G3.1 | Opening Learn physical opponent | Guard does **not** activate on normal moves |
| G3.2 | Illegal move | ER lock + LED like FULL |
| G3.3 | Ghost piece off-turn | No guard pause (like LITE for MG) |

**Gate gameplay ✅** if G1–G3 match the table in [MENUCONFIG_FEATURES_PLAN.md](../reference/MENUCONFIG_FEATURES_PLAN.md) §10.

---

## H — Hall V2 + STM32 auto-flash (optional, HW V2)

Build: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build flash`  
Wiring: [WIRING_ESP_STM4.md](../reference/WIRING_ESP_STM4.md)

| # | Steps | Expected result |
|---|--------|-----------|
| H1 | First boot virgin STM (or after ESP `chip erase`) | `STM32_AUTO` → flash from `stm32_fw` partition |
| H2 | After flash | `STM32_I2C_BL: [hall_probe] seg0 addr 0x30 OK` |
| H3 | Second boot (same STM) | `auto-flash skipped` + hall_probe OK |
| H4 | STM swap / blank chip, NVS unchanged | `NVS … but Hall not responding — forcing auto-flash` |
| H5 | Matrix scan | `HALL_I2C` without WARN on seg0; squares respond to magnet |
| H6 | UART | `CLI HALL PROBE 0` → `Hall seg0 probe OK` |

---

## Gate summary (check after test)

| ID | Criterion | HW | Note / date |
|----|-----------|-----|----------------|
| G2 | 3× Learn + Drill | ☐ | |
| G3 | Matrix guard without regression | ☐ | |
| G4 | 4 modes on FW | ☐ | |
| G5 | Flutter ≈ web | ☐ | |
| G6 | Curriculum unlock | ☐ | auto test CI |
| G7 | Progress after restart | ☐ | |
| P1 | No `Stav:` in opening UI | ☐ | |
| P2 | Rationale only ply 0 | ☐ | |
| P3 | EN locale steps | ☐ | |
| P4 | common_mistakes ≥10 lines | ☐ | auto test CI |
| P5 | Miniboard | ☐ | |
| P6 | 2 mirror pairs e4+d4 | ☐ | |
| P7 | L10/L12 mode picker | ☐ | |
| GP | Gameplay FULL / LITE / DEV (§G) | ☐ | menuconfig PR #20 |

**v1.0 release:** all G* and P* rows ✅ (G6/G7/P4 partially covered by CI — see `opening_release_gate_test.dart`).

---

## Automated supplements (CI)

- `openings-catalog.yml` — 41 lines, UCI, mirror-symmetric, sync
- `flutter-test.yml` — catalog, progress, curriculum, UX, release gate
- `firmware-build.yml` — full HTTP + BLE-only + gameplay-lite + gameplay-dev + hall-v2 profiles
- `scripts/test_opening_api.sh` — HTTP smoke on the board
