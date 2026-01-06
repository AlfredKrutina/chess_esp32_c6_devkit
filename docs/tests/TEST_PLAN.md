# Testovací plán pro CZECHMATE

## Přehled

Tento dokument popisuje komplexní testovací plán pro ověření správného fungování flow programu CZECHMATE, včetně identifikace potenciálních problémů, race conditions, edge cases a komplexních testovacích scénářů.

## Test Suite 1: Základní flow

### Test 1.1: Normální tah (PICKUP → DROP)

**Cíl:** Ověřit základní flow

**Kroky:**
1. Zvednout figurku z e2
2. Položit na e4
3. Ověřit: tah byl proveden, hráč se změnil, animace běží

**Ověření:**
- [ ] `piece_lifted` je true po PICKUP
- [ ] `piece_lifted` je false po DROP
- [ ] `current_player` se změnil
- [ ] `board[e4]` obsahuje figurku
- [ ] `board[e2]` je prázdné

### Test 1.2: Nevalidní tah

**Cíl:** Ověřit error handling

**Kroky:**
1. Zvednout pěšce z e2
2. Pokusit položit na e5 (nevalidní)
3. Ověřit: error recovery aktivován

**Ověření:**
- [ ] `error_recovery_state.waiting_for_move_correction` je true
- [ ] `error_recovery_state.invalid_row/col` je nastaveno
- [ ] `error_recovery_state.original_valid_row/col` je nastaveno
- [ ] LED bliká červeně

### Test 1.3: Oprava nevalidního tahu

**Cíl:** Ověřit error recovery

**Kroky:**
1. Po nevalidním tahu z e2→e5
2. Vrátit figurku na e2 (original position)
3. Ověřit: error recovery resetován

**Ověření:**
- [ ] `error_recovery_state` je resetován
- [ ] `piece_lifted` je false
- [ ] Board je v původním stavu

## Test Suite 2: Rychlé tahy

### Test 2.1: 10 rychlých tahů za sebou

**Cíl:** Ověřit stabilitu při rychlých tazích

**Kroky:**
1. Udělat 10 rychlých tahů (co nejrychleji)
2. Ověřit: všechny tahy byly zpracovány

**Ověření:**
- [ ] Všechny tahy byly provedeny
- [ ] Fronta nebyla přeplněná
- [ ] Žádné "queue full" warnings
- [ ] `current_player` je správně

### Test 2.2: Simultánní tahy z UART a Matrix

**Cíl:** Ověřit, že fronta funguje správně

**Kroky:**
1. Poslat tah z UART (e2e4)
2. Současně udělat fyzický tah (f2f4)
3. Ověřit: tahy se zpracují v pořadí

**Ověření:**
- [ ] Oba tahy byly zpracovány
- [ ] Tahy byly zpracovány v pořadí (FIFO)
- [ ] Žádné konflikty

## Test Suite 3: Edge Cases

### Test 3.1: DROP bez PICKUP

**Cíl:** Ověřit ochranu proti chybám

**Kroky:**
1. Poslat DROP příkaz bez předchozího PICKUP
2. Ověřit: chyba je správně ohlášena

**Ověření:**
- [ ] Chyba "No piece was lifted" (řádek 4507)
- [ ] `piece_lifted` zůstává false

### Test 3.2: Dva PICKUP za sebou

**Cíl:** Ověřit, že druhý PICKUP přepíše první

**Kroky:**
1. Zvednout figurku z e2 (PICKUP e2)
2. Zvednout jinou figurku z f2 (PICKUP f2)
3. Položit na e4 (DROP e4)
4. Ověřit: tah je z f2→e4 (ne e2→e4)

**Ověření:**
- [ ] `lifted_piece_row/col` je z f2
- [ ] Tah je f2→e4

### Test 3.3: Zrušení tahu (DROP na stejné pole)

**Cíl:** Ověřit zrušení tahu

**Kroky:**
1. Zvednout figurku z e2
2. Položit zpět na e2
3. Ověřit: tah byl zrušen

**Ověření:**
- [ ] `piece_lifted` je false
- [ ] Board zůstal nezměněn
- [ ] `current_player` se nezměnil

## Test Suite 4: Specialní tahy

### Test 4.1: Castling (kingside)

**Cíl:** Ověřit castling flow

**Kroky:**
1. Zvednout krále z e1
2. Položit na g1
3. Ověřit: castling state aktivní
4. Zvednout věž z h1
5. Položit na f1
6. Ověřit: castling dokončen, hráč se změnil

**Ověření:**
- [ ] `castling_state.in_progress` je true po kroku 2
- [ ] Hráč se NEMĚNÍ po kroku 2
- [ ] Hráč se MĚNÍ po kroku 5
- [ ] Board je správně aktualizován

### Test 4.2: En Passant

**Cíl:** Ověřit en passant flow

**Kroky:**
1. Bílý pěšec e2→e4
2. Černý pěšec d7→d5
3. Bílý pěšec e4→d5 (en passant)
4. Ověřit: černý pěšec z d5 je odstraněn

**Ověření:**
- [ ] `en_passant_available` je true po kroku 1
- [ ] En passant je detekován v kroku 3
- [ ] Černý pěšec z d5 je odstraněn

### Test 4.3: Promotion

**Cíl:** Ověřit promotion flow

**Kroky:**
1. Bílý pěšec a7→a8
2. Ověřit: promotion state aktivní
3. PICKUP/DROP jsou ignorovány
4. Stisknout promotion button (QUEEN)
5. Ověřit: pěšec je promován na damu

**Ověření:**
- [ ] `promotion_state.pending` je true po kroku 1
- [ ] PICKUP/DROP jsou ignorovány (řádky 3749, 4181)
- [ ] Promotion je dokončena po kroku 4

## Test Suite 5: Error Recovery

### Test 5.1: Nevalidní tah → oprava

**Cíl:** Ověřit error recovery

**Kroky:**
1. Zvednout pěšce z e2
2. Položit na e5 (nevalidní)
3. Ověřit: error state aktivní
4. Vrátit na e2
5. Ověřit: error state resetován

**Ověření:**
- [ ] `error_recovery_state.waiting_for_move_correction` je true
- [ ] `error_recovery_state.original_valid_row/col` je e2
- [ ] Error state je resetován po kroku 4

### Test 5.2: Nevalidní tah → validní oprava

**Cíl:** Ověřit, že lze opravit nevalidní tah validním

**Kroky:**
1. Zvednout pěšce z e2
2. Položit na e5 (nevalidní)
3. Zvednout z e5
4. Položit na e4 (validní)
5. Ověřit: tah je proveden

**Ověření:**
- [ ] Error state je resetován
- [ ] Tah e5→e4 je proveden
- [ ] Hráč se změnil

## Test Suite 6: State Consistency

### Test 6.1: Board state consistency

**Cíl:** Ověřit, že board je vždy konzistentní

**Kroky:**
1. Provedeme několik tahů
2. Po každém tahu ověříme board state

**Ověření:**
- [ ] Board obsahuje správné figurky
- [ ] Žádné duplicitní figurky
- [ ] Žádné "ghost" figurky

### Test 6.2: current_player consistency

**Cíl:** Ověřit, že current_player je vždy správný

**Kroky:**
1. Provedeme několik tahů
2. Po každém tahu ověříme current_player

**Ověření:**
- [ ] current_player se střídá správně
- [ ] current_player odpovídá board state
- [ ] Žádné duplicitní změny hráče

## Test Suite 7: Concurrent Operations

### Test 7.1: Tah během animace

**Cíl:** Ověřit, že animace neblokují tahy

**Kroky:**
1. Provedeme tah (spustí animaci)
2. Okamžitě provedeme další tah
3. Ověřit: oba tahy byly zpracovány

**Ověření:**
- [ ] Oba tahy byly provedeny
- [ ] Animace běží asynchronně
- [ ] Žádné blokování

### Test 7.2: UART příkaz během fyzického tahu

**Cíl:** Ověřit, že UART a Matrix příkazy nekonfliktují

**Kroky:**
1. Zvednout figurku fyzicky (PICKUP z Matrix)
2. Poslat tah z UART
3. Ověřit: oba příkazy se zpracují správně

**Ověření:**
- [ ] Oba příkazy byly zpracovány
- [ ] Žádné konflikty
- [ ] Fronta funguje správně

## Test Suite 8: Endgame

### Test 8.1: Checkmate detection

**Cíl:** Ověřit detekci matu

**Kroky:**
1. Nastavit pozici s matem
2. Provedeme tah, který způsobí mat
3. Ověřit: endgame je detekován

**Ověření:**
- [ ] `current_game_state` je GAME_STATE_FINISHED
- [ ] `game_active` je false
- [ ] Endgame animace běží

### Test 8.2: Stalemate detection

**Cíl:** Ověřit detekci patu

**Kroky:**
1. Nastavit pozici s patem
2. Provedeme tah, který způsobí pat
3. Ověřit: endgame je detekován

**Ověření:**
- [ ] `current_game_state` je GAME_STATE_FINISHED
- [ ] `game_active` je false

## Nástroje pro testování

1. **UART logger** - logování všech příkazů a odpovědí
2. **State checker** - periodická kontrola konzistence stavu
3. **Queue monitor** - sledování velikosti front
4. **Performance profiler** - měření času zpracování

## Implementace testů

### Unit testy (simulace)
- Test helper funkce pro simulaci PICKUP/DROP
- Test validace bez hardware
- Test error recovery bez hardware

### Integration testy (s hardware)
- Fyzické testy na skutečné šachovnici
- Test rychlých tahů
- Test edge cases

### Stress testy
- 100 tahů za sebou
- Simultánní operace
- Dlouhodobý běh (1+ hodina)

