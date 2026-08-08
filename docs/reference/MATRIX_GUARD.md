# Matrix guard

[Reference index](README.md) · [Flow diagram](../diagrams/sources/chess_flow_matrix_guard.mmd)

Matrix guard pauses the game when the sensor matrix and logical `board[]` disagree — typically after lifting multiple pieces, a fast capture, or restoring a game from NVS.

## LED legend (on the board)

| Color | Meaning |
|-------|---------|
| Yellow | White piece in logic, physical mismatch |
| Blue | Black piece in logic, physical mismatch |
| Orange | Sensor reports a piece, logic says empty |
| White | Logic expects a piece, sensor is empty |

## How to clear the guard

1. Align **all** pieces per the LEDs (not just the last move).
2. Leave the board quiet for ~2 s (no more lifts).
3. Play resumes automatically — both layers (`matrix_task` + `game_task`) clear together.

## If it still fails

- **New game** / UART `GAME_RESET` / reset button (GPIO15).
- **Emergency guard clear** (only when the board is physically aligned):
  - UART: `GUARD_CLEAR` (alias `MATRIX_GUARD_CLEAR`)
  - HTTP: `POST /api/game/guard_clear`
- Check `/api/status`: `matrix_guard_active`, `matrix_guard_conflicts`.
- **Lamp** mode can override game LEDs — switch to Chessboard.

## API / UI state

| Field | Meaning |
|------|--------|
| `matrix_guard_active` | Game paused |
| `matrix_guard_conflicts` | Count of mismatched squares |
| `matrix_guard_*_mask_*` | Anomaly bitmasks (web/Flutter banner) |
| `restore_state.resync_required` | Guard after boot / NVS restore |

Web UI (`chess_app.js`) and Flutter (`MatrixGuardBanner`) show guidance from these fields.

## Implementation (from PR #5)

- Recovery target = logical `board[]` (`matrix_guard_apply_expected_occupancy`).
- `game_matrix_guard_restore_after_clear()` — unified LEDs + `state_version` bump for clients
- Second lift during capture tolerates race via `matrix_get_pending_lift_square()`.

## Menuconfig (from PR #20)

Matrix guard can be disabled or limited via `idf.py menuconfig` → **CzechMate firmware** → **Gameplay safety & LED hints** → **① Matrix guard**.

| Option | Default (FULL) | Purpose |
|-------|----------------|--------|
| `CHESS_MG_ENABLE` | y | Detect matrix vs logic mismatch |
| `CHESS_MG_FREEZE_MOVES` | y | Pause move flow |
| `CHESS_MG_AUTO_CLEAR` | y | Auto-clear after board aligned |
| `CHESS_MG_NVS_RESYNC` | y | Guard after NVS restore |
| `CHESS_MG_LED_*` | y | Color legend above |

### Presets

| Profile | Matrix guard | Typical use |
|--------|--------------|-------------|
| **FULL** | on | Production (default) |
| **DEV** | off | Opening HW dev without false guards |
| **LITE** | off | Factory / quiet mode |
| **FACTORY** | off | Log only, no LED or lock |

Build profiles: `sdkconfig.defaults.gameplay_dev`, `sdkconfig.defaults.gameplay_lite`.

When `CHESS_MG_ENABLE=n`:
- `matrix_guard_active` in JSON is always `false`
- `matrix_send_guard_command()` does not activate guard (ambiguous state is logged)
- Opening virtual / puzzle / setup still ignore guard via `mode_conflict_active()` even when MG is on

See [MENUCONFIG_FEATURES_PLAN.md](MENUCONFIG_FEATURES_PLAN.md) for the full MG / ER / MH taxonomy.
