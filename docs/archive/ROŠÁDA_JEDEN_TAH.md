# Rošáda = jeden tah v historii

## Chování

Rošáda se na fyzické desce provádí ve dvou krocích (král, pak věž), ale v logice hry i v historii tahů se počítá **jako jeden tah**.

- **Historie tahů** (`move_history`, `/api/history`): přidá se pouze tah **krále** (např. e1→g1 nebo e1→c1). Tah věže se do historie **nepřidává**.
- **Počet tahů** (`move_count`): po dokončení rošády se zvýší jen o 1 (při tahu krále), ne při tahu věže.
- **Statistiky**: při dokončení rošády se zvýší `white_castles` nebo `black_castles` (pro výukový přehled a API).

## Implementace (game_task.c)

1. Po úspěšném provedení tahu (`success`) se před přidáním do historie zkontroluje, zda jde o **dokončení rošády** (tah věže z očekávané pozice na očekávané pole při `castling_state.in_progress`).
2. Pokud ano (`is_castling_rook_completion`), **nepřidá se** tento tah do `move_history`, **nezvýší se** `move_count` a nevolá se `game_record_material_advantage()` pro tento krok. Aktualizuje se jen `last_move_time`.
3. Při dokončení rošády se dále inkrementuje `white_castles` nebo `black_castles`.

Díky tomu zůstává FEN (sestavený z desky a historie) konzistentní a Stockfish/API nedostávají „navíc“ tah věže, který by mohl vést k chybám typu `INVALID_FEN_VALIDATION_ERROR`.
