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
- `game_matrix_guard_clear_both_layers()` synchronizuje game i matrix vrstvu.
- Druhé zvednutí při capture toleruje race přes `matrix_get_pending_lift_square()`.
