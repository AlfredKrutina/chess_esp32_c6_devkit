# Matrix guard

[Rozcestník reference](README.md) · [Diagram flow](../diagrams/sources/chess_flow_matrix_guard.mmd)

Matrix guard pozastaví hru, když senzorová matice a logická `board[]` nejsou v souladu — typicky po zvednutí více figurek najednou, po rychlém capture, nebo po obnově hry z NVS.

## LED legenda (na desce)

| Barva | Význam |
|-------|--------|
| Žlutá | V logice je bílá figurka, fyzicky nesedí |
| Modrá | V logice je černá figurka, fyzicky nesedí |
| Oranžová | Senzor hlásí figurku, logika má prázdno |
| Bílá | Podle logiky má být figurka, senzor je prázdný |

## Jak se guard zruší

1. Srovnejte **všechny** figurky podle LED (ne jen poslední tah).
2. Nechte desku ~2 s v klidu (žádné další zvedání).
3. Hra automaticky pokračuje — obě vrstvy (`matrix_task` + `game_task`) se čistí společně.

## Když to pořád nejde

- **Nová hra** / UART `GAME_RESET` / tlačítko reset (GPIO15).
- **Nouzové vyčištění guardu** (jen když je deska fyzicky srovnaná):
  - UART: `GUARD_CLEAR` (alias `MATRIX_GUARD_CLEAR`)
  - HTTP: `POST /api/game/guard_clear`
- Zkontrolujte `/api/status`: `matrix_guard_active`, `matrix_guard_conflicts`.
- Režim **Lampa** může přebít herní LED — přepněte na Šachovnice.

## Stav v API / UI

| Pole | Význam |
|------|--------|
| `matrix_guard_active` | Hra pozastavena |
| `matrix_guard_conflicts` | Počet nesedících polí |
| `matrix_guard_*_mask_*` | Bitmasky anomálií (web/Flutter banner) |
| `restore_state.resync_required` | Guard po startu / NVS restore |

Web UI (`chess_app.js`) a Flutter (`MatrixGuardBanner`) zobrazují návod podle těchto polí.

## Implementace (od PR #5)

- Recovery cíl = logická `board[]` (`matrix_guard_apply_expected_occupancy`).
- `game_matrix_guard_restore_after_clear()` — sjednocené LED + `state_version` bump pro klienty
- Druhé zvednutí při capture toleruje race přes `matrix_get_pending_lift_square()`.

## Menuconfig (od PR #20)

Matrix guard lze vypnout nebo omezit přes `idf.py menuconfig` → **CzechMate firmware** → **Gameplay safety & LED** → **Matrix guard**.

| Volba | Výchozí (FULL) | Účel |
|-------|----------------|------|
| `CHESS_MG_ENABLE` | y | Detekce nesouladu matice vs. logika |
| `CHESS_MG_FREEZE_MOVES` | y | Pozastavení tahového flow |
| `CHESS_MG_AUTO_CLEAR` | y | Auto-clear po srovnání desky |
| `CHESS_MG_NVS_RESYNC` | y | Guard po obnově z NVS |
| `CHESS_MG_LED_*` | y | Barevná legenda výše |

### Presety

| Profil | Matrix guard | Typické použití |
|--------|--------------|-----------------|
| **FULL** | zapnuto | Produkce (default) |
| **DEV** | vypnuto | Opening HW vývoj bez falešných guardů |
| **LITE** | vypnuto | Factory / tichý režim |
| **FACTORY** | vypnuto | Jen log, bez LED a locku |

Build profily: `sdkconfig.defaults.gameplay_dev`, `sdkconfig.defaults.gameplay_lite`.

Při `CHESS_MG_ENABLE=n`:
- `matrix_guard_active` v JSON je vždy `false`
- `matrix_send_guard_command()` neaktivuje guard (ambiguous stav se loguje)
- Opening virtual / puzzle / setup stále ignorují guard přes `mode_conflict_active()` i když je MG zapnutý

Viz [MENUCONFIG_FEATURES_PLAN.md](MENUCONFIG_FEATURES_PLAN.md) pro celou taxonomii MG / ER / MH.
