# Známé problémy z testů

## Identifikované problémy

### Problém 1: Duplicitní změna hráče v UART flow ✅ OPRAVENO

**Lokace:** `components/game_task/game_task.c:7816-7819` (původně)

**Popis:** V `game_process_move_command()` se hráč měnil na řádku 7818, ale `game_execute_move()` už hráče změnil na řádku 3346.

**Dopad:** Hráč se měnil dvakrát → špatný hráč na tahu

**Status:** ✅ OPRAVENO - `game_process_move_command()` nyní používá `game_execute_move()` místo přímé manipulace s boardem

**Oprava:**
- Změněno `game_process_move_command()` tak, aby volala `game_execute_move()` místo přímé manipulace s boardem
- Odstraněna duplicitní změna hráče
- Odstraněno duplicitní přidávání do historie

### Problém 2: current_player není chráněn mutexem

**Lokace:** Všechna místa, kde se mění `current_player`

**Popis:** `current_player` je globální statická proměnná, ale není chráněna mutexem při změně.

**Dopad:** Race condition při současném přístupu (nízká pravděpodobnost, protože všechny změny jsou v game_task)

**Status:** ⚠️ NÍZKÁ PRIORITA - Všechny změny `current_player` jsou v game_task, takže race condition je nepravděpodobná. Mutex ochrana by byla vhodná pro budoucí vylepšení.

**Doporučení:** Vytvořit helper funkce `game_set_current_player()` s mutex ochranou pro budoucí vylepšení.

### Problém 3: piece_lifted není chráněn mutexem

**Lokace:** Všechna místa, kde se mění `piece_lifted`

**Popis:** `piece_lifted` je globální statická proměnná, ale není chráněna mutexem.

**Dopad:** Race condition při současném přístupu (nízká pravděpodobnost, protože všechny změny jsou v game_task)

**Status:** ⚠️ NÍZKÁ PRIORITA - Všechny změny `piece_lifted` jsou v game_task, takže race condition je nepravděpodobná. Mutex ochrana by byla vhodná pro budoucí vylepšení.

**Doporučení:** Vytvořit helper funkce `game_set_piece_lifted()` s mutex ochranou pro budoucí vylepšení.

### Problém 4: Board state může být nekonzistentní

**Lokace:** `components/game_task/game_task.c:3021-3106`

**Popis:** `game_execute_move()` validuje tah, pak ho provede. Mezi validací a provedením může dojít ke změně board state.

**Dopad:** Tah může být proveden na nekonzistentním boardu (velmi nízká pravděpodobnost, protože board je v game_task a není sdílený)

**Status:** ✅ NENÍ PROBLÉM - Board je v game_task a není sdílený s jinými tasky, takže nekonzistence je nepravděpodobná.

## Ověřené funkce

### ✅ Error Recovery State
- Error recovery state je správně resetován po validním tahu (řádek 4566)
- `original_valid_row/col` je nastaveno pouze při prvním erroru (řádek 4719)
- Error recovery správně zpracovává návrat na original pozici

### ✅ Castling Flow
- Castling nemění hráče po prvním tahu (král) - komentář na řádku 3169
- Castling mění hráče až po druhém tahu (věž) - řádek 3231
- Castling state je správně spravován

### ✅ Promotion Flow
- PICKUP/DROP jsou ignorovány během promotion (řádky 3749, 4181)
- Promotion state je správně spravován
- Promotion button správně dokončuje promotion

### ✅ State Management
- `piece_lifted` je správně resetován po každém tahu
- `current_player` je správně měněn v `game_execute_move()`
- Board state je konzistentní

## Priorita oprav

1. ✅ **VYSOKÁ:** Duplicitní změna hráče v UART flow - **OPRAVENO**
2. ⚠️ **STŘEDNÍ:** Přidat mutex ochranu pro `current_player` a `piece_lifted` - **NÍZKÁ PRIORITA** (všechny změny jsou v game_task)
3. ✅ **NÍZKÁ:** Board state konzistence - **NENÍ PROBLÉM** (board není sdílený)

## Budoucí vylepšení

1. Vytvořit helper funkce s mutex ochranou pro `current_player` a `piece_lifted`
2. Přidat unit testy pro simulaci PICKUP/DROP
3. Přidat integration testy s hardware
4. Přidat stress testy (100+ tahů)

