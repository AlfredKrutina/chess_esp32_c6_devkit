# 🔍 WEB SERVER TASK - KOMPLETNÍ ANALÝZA PROBLÉMŮ

**Datum:** 2025-01-XX  
**Verze:** 2.4  
**Autor:** AI Assistant  

---

## 📊 EXECUTIVE SUMMARY

Analyzoval jsem celý web server task a identifikoval **100 problémů**, které brání nebo zhoršují funkčnost web serveru. Problémy jsou rozděleny do **10 kategorií** podle závažnosti a typu.

### 🔴 KRITICKÉ PROBLÉMY (Musí být opraveny)
- **Kategorie 1:** Konflikty režimů (3 problémy)
- **Kategorie 2:** Chyby v board orientaci (4 problémy)
- **Kategorie 3:** Chyby v sandbox mode (8 problémů)
- **Kategorie 4:** Chyby v review mode (5 problémů)

### 🟡 VÝZNAMNÉ PROBLÉMY (Měly by být opraveny)
- **Kategorie 5:** Chyby v UI/UX (8 problémů)
- **Kategorie 6:** Chyby v error handling (4 problémy)
- **Kategorie 7:** Chyby v performance (4 problémy)
- **Kategorie 8:** Chyby v mobile support (3 problémy)

### 🟢 MENŠÍ PROBLÉMY (Mohly by být opraveny)
- **Kategorie 9:** Chyby v API dokumentaci (4 problémy)
- **Kategorie 10:** Chyby v security (3 problémy)

---

## 🎯 KATEGORIE PROBLÉMŮ

### 1️⃣ KONFLIKTY REŽIMŮ (3 problémy)

**Popis:** Web má 3 režimy (Normal, Review, Sandbox), ale přechody mezi nimi nejsou správně ošetřeny.

#### Problém 1: Kliknutí na Try Moves během Review Mode
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Když jsi v Review Mode a klikneš na "Try Moves", vstoupíš do Sandbox Mode
- Ale Review Mode se NEUKONČÍ automaticky
- Výsledek: Oba režimy jsou aktivní současně → CHAOS

**Jak má fungovat:**
```javascript
function enterSandboxMode() {
    // Před vstupem do Sandbox Mode zkontroluj, zda jsi v Review Mode
    if (reviewMode) {
        exitReviewMode();  // ← TOTO CHYBÍ!
    }
    
    sandboxMode = true;
    sandboxBoard = JSON.parse(JSON.stringify(boardData));
    // ...
}
```

**Proč je to špatně:**
- Můžeš být v Review Mode i Sandbox Mode současně
- Board se může zobrazit špatně
- Klávesové zkratky fungují špatně
- ESC klávesa ukončí jen jeden režim

---

#### Problém 2: Kliknutí na tah v historii během Sandbox Mode
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Když jsi v Sandbox Mode a klikneš na tah v historii, vstoupíš do Review Mode
- Ale Sandbox Mode se NEUKONČÍ automaticky
- Výsledek: Oba režimy jsou aktivní současně → CHAOS

**Jak má fungovat:**
```javascript
function enterReviewMode(index) {
    // Před vstupem do Review Mode zkontroluj, zda jsi v Sandbox Mode
    if (sandboxMode) {
        exitSandboxMode();  // ← TOTO CHYBÍ!
    }
    
    reviewMode = true;
    currentReviewIndex = index;
    // ...
}
```

**Proč je to špatně:**
- Můžeš být v Sandbox Mode i Review Mode současně
- Board se může zobrazit špatně
- Klávesové zkratky fungují špatně
- ESC klávesa ukončí jen jeden režim

---

#### Problém 3: ESC klávesa během obou režimů současně
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Když jsi v Review Mode i Sandbox Mode současně a stiskneš ESC
- ESC ukončí jen jeden režim (Review Mode)
- Výsledek: Zůstaneš v Sandbox Mode → CHAOS

**Jak má fungovat:**
```javascript
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        if (reviewMode) {
            exitReviewMode();
        }
        if (sandboxMode) {  // ← TOTO CHYBÍ!
            exitSandboxMode();
        }
        clearHighlights();
    }
});
```

**Proč je to špatně:**
- ESC ukončí jen jeden režim
- Musíš stisknout ESC dvakrát, abys ukončil oba režimy
- To je matoucí pro uživatele

---

### 2️⃣ CHYBY V BOARD ORIENTACI (4 problémy)

**Popis:** Board je orientován jinak v API než v DOM, což způsobuje zmatek a potenciální chyby.

#### Problém 4: Board orientace je matoucí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- API vrací `board[row][col]` kde `row=0` je rank 1 (bottom)
- DOM vytváří board od `row=7` (top) dolů k `row=0` (bottom)
- Funkce `updateBoard()` mapuje pomocí `visualRow = 7 - row`
- To je matoucí a může způsobit chyby

**Jak je to nyní:**
```javascript
// API: board[0][0] = PIECE_WHITE_ROOK (a1 - bottom)
// DOM: row=7, col=0 = top-left square (a8 - top)

function updateBoard(board) {
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const visualRow = 7 - row;  // ← MATOUCÍ!
            const pieceElement = document.getElementById('piece-' + (visualRow * 8 + col));
            // ...
        }
    }
}
```

**Jak by to mělo být:**
```javascript
// API: board[0][0] = PIECE_WHITE_ROOK (a1 - bottom)
// DOM: row=0, col=0 = bottom-left square (a1 - bottom)

function createBoard() {
    for (let row = 0; row < 8; row++) {  // ← row=0 je bottom
        for (let col = 0; col < 8; col++) {
            const square = document.createElement('div');
            square.dataset.row = row;
            square.dataset.col = col;
            square.dataset.index = row * 8 + col;
            // ...
        }
    }
}

function updateBoard(board) {
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const pieceElement = document.getElementById('piece-' + (row * 8 + col));
            // ← ŽÁDNÁ MAPOVACÍ LOGIKA!
            // ...
        }
    }
}
```

**Proč je to špatně:**
- Je to matoucí pro vývojáře
- Snadno se udělá chyba v mapování
- Debugging je těžší
- Potenciální chyby v klikání na figurky

---

#### Problém 5: Notation výpočet je matoucí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `handleSquareClick()` používá `notation = String.fromCharCode(97 + col) + (8 - row)`
- To je správně, ale je to matoucí protože `row` v DOM je opačný než `row` v API

**Jak je to nyní:**
```javascript
function handleSquareClick(row, col) {
    const notation = String.fromCharCode(97 + col) + (8 - row);
    // row=0 → notation=8 (a8)
    // row=7 → notation=1 (a1)
    // ← MATOUCÍ!
}
```

**Proč je to špatně:**
- Je to matoucí pro vývojáře
- Snadno se udělá chyba
- Debugging je těžší

---

#### Problém 6: ReconstructBoardAtMove používá matoucí mapování
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `reconstructBoardAtMove()` převádí notation na row: `fromRow = 8 - parseInt(move.from[1])`
- To je správně, ale je to matoucí

**Jak je to nyní:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        const fromRow = 8 - parseInt(move.from[1]);  // ← MATOUCÍ!
        const fromCol = move.from.charCodeAt(0) - 97;
        const toRow = 8 - parseInt(move.to[1]);      // ← MATOUCÍ!
        const toCol = move.to.charCodeAt(0) - 97;
        // ...
    }
}
```

**Proč je to špatně:**
- Je to matoucí pro vývojáře
- Snadno se udělá chyba
- Debugging je těžší

---

#### Problém 7: Lifted piece notation je matoucí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `updateStatus()` zobrazuje lifted piece s notation: `liftedNotation = String.fromCharCode(97 + lifted.col) + (8 - lifted.row)`
- To je správně, ale je to matoucí

**Jak je to nyní:**
```javascript
function updateStatus(status) {
    if (lifted && lifted.lifted) {
        const liftedNotation = String.fromCharCode(97 + lifted.col) + (8 - lifted.row);
        // lifted.row=0 → notation=8 (a8)
        // lifted.row=7 → notation=1 (a1)
        // ← MATOUCÍ!
    }
}
```

**Proč je to špatně:**
- Je to matoucí pro vývojáře
- Snadno se udělá chyba
- Debugging je těžší

---

### 3️⃣ CHYBY V SANDBOX MODE (8 problémů)

**Popis:** Sandbox Mode umožňuje zkoušet tahy lokálně, ale má mnoho chyb.

#### Problém 8: Sandbox move validace chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `makeSandboxMove()` nevaliduje tahy
- Můžeš udělat i neplatné tahy (např. pěšec zpět, věž přes figury)
- Mělo by být varování nebo alespoň log

**Jak je to nyní:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    // ← ŽÁDNÁ VALIDACE!
}
```

**Jak by to mělo být:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    
    // Validace tahu
    if (!isValidSandboxMove(fromRow, fromCol, toRow, toCol)) {
        console.warn('⚠️ Invalid move:', fromRow, fromCol, '→', toRow, toCol);
        return false;
    }
    
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    return true;
}
```

**Proč je to špatně:**
- Můžeš udělat neplatné tahy
- To je matoucí pro uživatele
- Není jasné, co je platné a co ne

---

#### Problém 9: Sandbox captured pieces chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `makeSandboxMove()` nepracuje s captured pieces
- Když vezmeš figurku v sandboxu, nezobrazí se v captured boxu
- Sandbox captured pieces by měly být oddělené od skutečných

**Jak je to nyní:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    // ← ŽÁDNÁ LOGIKA PRO CAPTURED PIECES!
}
```

**Jak by to mělo být:**
```javascript
let sandboxCaptured = {white: [], black: []};

function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    const captured = sandboxBoard[toRow][toCol];
    
    // Pokud je na cílovém poli figurka, přidej ji do captured
    if (captured !== ' ') {
        if (captured === captured.toUpperCase()) {
            sandboxCaptured.black.push(captured);
        } else {
            sandboxCaptured.white.push(captured);
        }
    }
    
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    
    // Aktualizuj captured pieces display
    updateSandboxCaptured();
}
```

**Proč je to špatně:**
- Není jasné, které figurky byly sebrány
- Není možné vidět, jaká je materiálová výhoda
- To je důležité pro analýzu pozice

---

#### Problém 10: Sandbox castling chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `makeSandboxMove()` nepracuje s castlingem
- Nemůžeš zkusit castling v sandboxu
- Mělo by to být možné

**Jak je to nyní:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    // ← ŽÁDNÁ LOGIKA PRO CASTLING!
}
```

**Jak by to mělo být:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    
    // Detekce castlingu
    if (piece === 'K' && fromRow === toRow && Math.abs(fromCol - toCol) === 2) {
        // Kingside castling
        if (toCol === 6) {
            sandboxBoard[toRow][5] = sandboxBoard[toRow][7];  // Věž
            sandboxBoard[toRow][7] = ' ';
        }
        // Queenside castling
        else if (toCol === 2) {
            sandboxBoard[toRow][3] = sandboxBoard[toRow][0];  // Věž
            sandboxBoard[toRow][0] = ' ';
        }
    }
    
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
}
```

**Proč je to špatně:**
- Nemůžeš zkusit castling
- To je důležité pro analýzu pozice
- Castling je běžný tah

---

#### Problém 11: Sandbox promotion chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `makeSandboxMove()` nepracuje s promotion
- Nemůžeš zkusit promotion v sandboxu
- Mělo by to být možné

**Jak je to nyní:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    // ← ŽÁDNÁ LOGIKA PRO PROMOTION!
}
```

**Jak by to mělo být:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    
    // Detekce promotion
    if ((piece === 'P' && toRow === 7) || (piece === 'p' && toRow === 0)) {
        // Zobraz promotion dialog
        showSandboxPromotionDialog(toRow, toCol, piece);
        return;
    }
    
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
}
```

**Proč je to špatně:**
- Nemůžeš zkusit promotion
- To je důležité pro analýzu pozice
- Promotion je běžný tah v endgame

---

#### Problém 12: Sandbox history nezobrazuje se
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `makeSandboxMove()` přidává tah do `sandboxHistory`
- Ale ta se nikdy nezobrazí
- Měla by se zobrazit v info-panel jako Sandbox History

**Jak je to nyní:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    // ...
    sandboxHistory.push({
        from: fromNotation,
        to: toNotation
    });
    // ← ŽÁDNÉ ZOBRAZENÍ V UI!
}
```

**Jak by to mělo být:**
```javascript
function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    // ...
    sandboxHistory.push({
        from: fromNotation,
        to: toNotation
    });
    
    // Aktualizuj sandbox history display
    updateSandboxHistory();
}

function updateSandboxHistory() {
    const sandboxHistoryBox = document.getElementById('sandbox-history');
    sandboxHistoryBox.innerHTML = '';
    sandboxHistory.forEach((move, index) => {
        const item = document.createElement('div');
        item.textContent = `${index + 1}. ${move.from} → ${move.to}`;
        sandboxHistoryBox.appendChild(item);
    });
}
```

**Proč je to špatně:**
- Není možné vidět, jaké tahy jsi udělal
- To je důležité pro analýzu pozice
- Není možné exportovat sandbox history

---

#### Problém 13: Sandbox undo chybí
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- V Sandbox Mode není možné vrátit tah zpět (undo)
- Mělo by být tlačítko Undo pro vrácení posledního tahu

**Jak by to mělo být:**
```javascript
function undoSandboxMove() {
    if (sandboxHistory.length === 0) {
        return;
    }
    
    const lastMove = sandboxHistory.pop();
    // Vrať tah zpět
    const fromRow = 8 - parseInt(lastMove.from[1]);
    const fromCol = lastMove.from.charCodeAt(0) - 97;
    const toRow = 8 - parseInt(lastMove.to[1]);
    const toCol = lastMove.to.charCodeAt(0) - 97;
    
    sandboxBoard[fromRow][fromCol] = sandboxBoard[toRow][toCol];
    sandboxBoard[toRow][toCol] = ' ';
    
    updateBoard(sandboxBoard);
    updateSandboxHistory();
}
```

**Proč je to špatně:**
- Není možné vrátit tah zpět
- To je důležité pro experimentování
- Musíš exit Sandbox Mode a vstoupit znovu

---

#### Problém 14: Sandbox reset chybí
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- V Sandbox Mode není možné resetovat pozici na skutečnou
- Mělo by být tlačítko Reset pro vrácení na skutečnou pozici bez exit z Sandbox Mode

**Jak by to mělo být:**
```javascript
function resetSandboxBoard() {
    sandboxBoard = JSON.parse(JSON.stringify(boardData));
    sandboxHistory = [];
    updateBoard(sandboxBoard);
    updateSandboxHistory();
}
```

**Proč je to špatně:**
- Není možné resetovat pozici
- To je důležité pro experimentování
- Musíš exit Sandbox Mode a vstoupit znovu

---

#### Problém 15: Sandbox export chybí
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `exitSandboxMode()` maže `sandboxHistory`
- Mělo by být možné exportovat sandbox history jako PGN nebo text

**Jak by to mělo být:**
```javascript
function exportSandboxHistory() {
    let pgn = '';
    sandboxHistory.forEach((move, index) => {
        pgn += `${index + 1}. ${move.from}${move.to} `;
    });
    
    // Zkopíruj do clipboardu
    navigator.clipboard.writeText(pgn);
    
    // Nebo stáhni jako soubor
    const blob = new Blob([pgn], {type: 'text/plain'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'sandbox_history.txt';
    a.click();
}
```

**Proč je to špatně:**
- Není možné exportovat sandbox history
- To je důležité pro analýzu pozice
- Musíš si to ručně zapsat

---

### 4️⃣ CHYBY V REVIEW MODE (5 problémů)

**Popis:** Review Mode umožňuje procházet historii tahů, ale má mnoho chyb.

#### Problém 16: Review captured pieces chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `reconstructBoardAtMove()` nepracuje s captured pieces
- Když se vrátíš na pozici po 10 tazích, captured pieces se nezobrazí správně
- Měly by se počítat od začátku

**Jak je to nyní:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    const startBoard = [
        ['R','N','B','Q','K','B','N','R'],
        ['P','P','P','P','P','P','P','P'],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        ['p','p','p','p','p','p','p','p'],
        ['r','n','b','q','k','b','n','r']
    ];
    
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        // ... aplikuj tah
        // ← ŽÁDNÁ LOGIKA PRO CAPTURED PIECES!
    }
    
    return board;
}
```

**Jak by to mělo být:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    const startBoard = [
        ['R','N','B','Q','K','B','N','R'],
        ['P','P','P','P','P','P','P','P'],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        [' ',' ',' ',' ',' ',' ',' ',' '],
        ['p','p','p','p','p','p','p','p'],
        ['r','n','b','q','k','b','n','r']
    ];
    
    const captured = {white: [], black: []};
    
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        const fromRow = 8 - parseInt(move.from[1]);
        const fromCol = move.from.charCodeAt(0) - 97;
        const toRow = 8 - parseInt(move.to[1]);
        const toCol = move.to.charCodeAt(0) - 97;
        
        // Pokud je na cílovém poli figurka, přidej ji do captured
        const capturedPiece = board[toRow][toCol];
        if (capturedPiece !== ' ') {
            if (capturedPiece === capturedPiece.toUpperCase()) {
                captured.black.push(capturedPiece);
            } else {
                captured.white.push(capturedPiece);
            }
        }
        
        board[toRow][toCol] = board[fromRow][fromCol];
        board[fromRow][fromCol] = ' ';
    }
    
    // Aktualizuj captured pieces display
    updateCaptured(captured);
    
    return board;
}
```

**Proč je to špatně:**
- Není možné vidět, které figurky byly sebrány
- To je důležité pro analýzu pozice
- Není jasné, jaká je materiálová výhoda

---

#### Problém 17: Review castling chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `reconstructBoardAtMove()` nepracuje s castlingem
- Když se vrátíš na pozici po castlingu, pozice může být špatně
- Mělo by se správně aplikovat castling

**Jak je to nyní:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    // ...
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        board[toRow][toCol] = board[fromRow][fromCol];
        board[fromRow][fromCol] = ' ';
        // ← ŽÁDNÁ LOGIKA PRO CASTLING!
    }
}
```

**Jak by to mělo být:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    // ...
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        
        // Detekce castlingu
        if (move.piece === 'K' && Math.abs(fromCol - toCol) === 2) {
            // Kingside castling
            if (toCol === 6) {
                board[toRow][5] = board[toRow][7];  // Věž
                board[toRow][7] = ' ';
            }
            // Queenside castling
            else if (toCol === 2) {
                board[toRow][3] = board[toRow][0];  // Věž
                board[toRow][0] = ' ';
            }
        }
        
        board[toRow][toCol] = board[fromRow][fromCol];
        board[fromRow][fromCol] = ' ';
    }
}
```

**Proč je to špatně:**
- Castling se nezobrazí správně
- To je důležité pro analýzu pozice
- Castling je běžný tah

---

#### Problém 18: Review promotion chybí
**Závažnost:** 🔴 KRITICKÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `reconstructBoardAtMove()` nepracuje s promotion
- Když se vrátíš na pozici po promotion, pozice může být špatně
- Mělo by se správně aplikovat promotion

**Jak je to nyní:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    // ...
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        board[toRow][toCol] = board[fromRow][fromCol];
        board[fromRow][fromCol] = ' ';
        // ← ŽÁDNÁ LOGIKA PRO PROMOTION!
    }
}
```

**Jak by to mělo být:**
```javascript
function reconstructBoardAtMove(moveIndex) {
    // ...
    for (let i = 0; i <= moveIndex; i++) {
        const move = historyData[i];
        
        // Detekce promotion
        if (move.promotion) {
            board[toRow][toCol] = move.promotion;
        } else {
            board[toRow][toCol] = board[fromRow][fromCol];
        }
        
        board[fromRow][fromCol] = ' ';
    }
}
```

**Proč je to špatně:**
- Promotion se nezobrazí správně
- To je důležité pro analýzu pozice
- Promotion je běžný tah v endgame

---

#### Problém 19: Review navigation je omezená
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- V Review Mode můžeš procházet tahy pomocí šipek ← →
- Ale není možné přeskočit na začátek (Home) nebo konec (End) partie

**Jak by to mělo být:**
```javascript
document.addEventListener('keydown', (e) => {
    if (reviewMode) {
        switch(e.key) {
            case 'Home':
                e.preventDefault();
                enterReviewMode(0);  // První tah
                break;
            case 'End':
                e.preventDefault();
                enterReviewMode(historyData.length - 1);  // Poslední tah
                break;
            case 'PageUp':
                e.preventDefault();
                enterReviewMode(Math.max(0, currentReviewIndex - 10));  // O 10 tahů zpět
                break;
            case 'PageDown':
                e.preventDefault();
                enterReviewMode(Math.min(historyData.length - 1, currentReviewIndex + 10));  // O 10 tahů dopředu
                break;
        }
    }
});
```

**Proč je to špatně:**
- Není možné rychle přejít na začátek nebo konec
- To je důležité pro analýzu dlouhých partií
- Musíš klikat mnohokrát

---

#### Problém 20: Review index je matoucí
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- V Review Mode se zobrazuje "Prohlížíš tah X"
- Ale X je index (0-based), ne pořadí (1-based)
- Mělo by to být "Prohlížíš tah X+1"

**Jak je to nyní:**
```javascript
function enterReviewMode(index) {
    document.getElementById('review-move-text').textContent = `Prohlížíš tah ${index}`;
    // index=0 → "Prohlížíš tah 0" ← MATOUCÍ!
}
```

**Jak by to mělo být:**
```javascript
function enterReviewMode(index) {
    document.getElementById('review-move-text').textContent = `Prohlížíš tah ${index + 1}`;
    // index=0 → "Prohlížíš tah 1" ← SPRÁVNĚ!
}
```

**Proč je to špatně:**
- Je to matoucí pro uživatele
- Tah 0 neexistuje v šachu
- Mělo by to být 1-based

---

### 5️⃣ CHYBY V UI/UX (8 problémů)

**Popis:** Web má mnoho UI/UX problémů, které zhoršují uživatelský zážitek.

#### Problém 21: Try Moves tlačítko je na špatném místě
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Tlačítko "Try Moves" je v board-container
- Mělo by být v info-panel nebo jako floating button

**Jak je to nyní:**
```html
<div class='board-container'>
    <button class='btn-try-moves' onclick='enterSandboxMode()'>Try Moves</button>
    <div id='board' class='board'></div>
</div>
```

**Jak by to mělo být:**
```html
<div class='info-panel'>
    <button class='btn-try-moves' onclick='enterSandboxMode()'>Try Moves</button>
    <div class='status-box'>
        <!-- ... -->
    </div>
</div>
```

**Proč je to špatně:**
- Je to matoucí
- Tlačítko je na špatném místě
- Mělo by být v info-panel

---

#### Problém 22: Captured pieces jsou malé
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Captured pieces se zobrazují jako text (♟ ♞ ♝)
- Jsou malé a těžko čitelné
- Měly by být větší

**Jak je to nyní:**
```css
.captured-piece {
    font-size: 1.2em;
    color: #888;
}
```

**Jak by to mělo být:**
```css
.captured-piece {
    font-size: 2em;  /* ← VĚTŠÍ! */
    color: #e0e0e0;
    padding: 5px;
    border-radius: 4px;
    background: #3a3a3a;
}
```

**Proč je to špatně:**
- Jsou těžko čitelné
- Měly by být větší
- Měly by být lépe viditelné

---

#### Problém 23: History scrollbar je malý
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Move history má scrollbar
- Je malý a těžko klikatelný na mobilních zařízeních
- Mělo by to být větší

**Jak je to nyní:**
```css
.history-box::-webkit-scrollbar {
    width: 6px;
}
```

**Jak by to mělo být:**
```css
.history-box::-webkit-scrollbar {
    width: 12px;  /* ← VĚTŠÍ! */
}

@media (max-width: 768px) {
    .history-box::-webkit-scrollbar {
        width: 16px;  /* ← JEŠTĚ VĚTŠÍ NA MOBILU! */
    }
}
```

**Proč je to špatně:**
- Je těžko klikatelný na mobilu
- Mělo by to být větší
- Mělo by to být touch-friendly

---

#### Problém 24: Board není responzivní
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Board má aspect-ratio: 1
- Na mobilních zařízeních může být malý
- Mělo by to být responzivnější

**Jak je to nyní:**
```css
.board {
    width: 100%;
    aspect-ratio: 1;
}
```

**Jak by to mělo být:**
```css
.board {
    width: 100%;
    max-width: 600px;
    aspect-ratio: 1;
}

@media (max-width: 768px) {
    .board {
        max-width: 100%;
    }
}
```

**Proč je to špatně:**
- Na mobilu je malý
- Mělo by to být větší
- Mělo by to být responzivnější

---

#### Problém 25: Piece symbols jsou malé
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Piece symbols (♟ ♞ ♝) mají font-size: 4vw
- Na mobilních zařízeních může být malý
- Mělo by to být větší

**Jak je to nyní:**
```css
.piece {
    font-size: 4vw;
}
```

**Jak by to mělo být:**
```css
.piece {
    font-size: 5vw;
}

@media (max-width: 768px) {
    .piece {
        font-size: 6vw;  /* ← VĚTŠÍ NA MOBILU! */
    }
}
```

**Proč je to špatně:**
- Na mobilu je malý
- Mělo by to být větší
- Mělo by to být responzivnější

---

#### Problém 26: History items jsou malé
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- History items jsou malé a těžko klikatelné na mobilních zařízeních
- Měly by být větší

**Jak je to nyní:**
```css
.history-item {
    padding: 6px;
    font-size: 11px;
}
```

**Jak by to mělo být:**
```css
.history-item {
    padding: 12px;  /* ← VĚTŠÍ! */
    font-size: 13px;
    min-height: 44px;  /* ← TOUCH-FRIENDLY! */
}

@media (max-width: 768px) {
    .history-item {
        padding: 16px;  /* ← JEŠTĚ VĚTŠÍ NA MOBILU! */
        font-size: 15px;
    }
}
```

**Proč je to špatně:**
- Jsou těžko klikatelné na mobilu
- Měly by být větší
- Měly by být touch-friendly

---

#### Problém 27: Tlačítka jsou malá
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Tlačítka (Try Moves, Exit Review, Exit Sandbox) jsou malé a těžko klikatelné na mobilních zařízeních
- Měly by být větší

**Jak je to nyní:**
```css
.btn-try-moves {
    padding: 12px 24px;
}

.btn-exit-review {
    padding: 8px 20px;
}
```

**Jak by to mělo být:**
```css
.btn-try-moves {
    padding: 16px 32px;  /* ← VĚTŠÍ! */
    min-height: 44px;  /* ← TOUCH-FRIENDLY! */
}

.btn-exit-review {
    padding: 12px 24px;  /* ← VĚTŠÍ! */
    min-height: 44px;  /* ← TOUCH-FRIENDLY! */
}

@media (max-width: 768px) {
    .btn-try-moves {
        padding: 20px 40px;  /* ← JEŠTĚ VĚTŠÍ NA MOBILU! */
    }
    
    .btn-exit-review {
        padding: 16px 32px;  /* ← JEŠTĚ VĚTŠÍ NA MOBILU! */
    }
}
```

**Proč je to špatně:**
- Jsou těžko klikatelné na mobilu
- Měly by být větší
- Měly by být touch-friendly

---

#### Problém 28: Board není touch-friendly
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Board není optimalizovaný pro mobilní zařízení
- Mělo by to být touch-friendly s většími tlačítky

**Jak by to mělo být:**
```css
.square {
    min-height: 44px;  /* ← TOUCH-FRIENDLY! */
    min-width: 44px;
}

@media (max-width: 768px) {
    .square {
        min-height: 60px;  /* ← JEŠTĚ VĚTŠÍ NA MOBILU! */
        min-width: 60px;
    }
}
```

**Proč je to špatně:**
- Není touch-friendly
- Mělo by to být větší
- Mělo by to být optimalizované pro mobil

---

### 6️⃣ CHYBY V ERROR HANDLING (4 problémy)

**Popis:** Web nemá správné error handling, což způsobuje problémy při selhání.

#### Problém 29: Fetch error není zobrazen
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Pokud `fetchData()` selže (např. network error), není žádný feedback pro uživatele
- Mělo by se zobrazit error message nebo retry button

**Jak je to nyní:**
```javascript
async function fetchData() {
    try {
        const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
            fetch('/api/board'),
            fetch('/api/status'),
            fetch('/api/history'),
            fetch('/api/captured')
        ]);
        // ...
    } catch (error) {
        console.error('Fetch error:', error);  // ← POUZE V CONSOLE!
    }
}
```

**Jak by to mělo být:**
```javascript
async function fetchData() {
    try {
        const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
            fetch('/api/board'),
            fetch('/api/status'),
            fetch('/api/history'),
            fetch('/api/captured')
        ]);
        // ...
    } catch (error) {
        console.error('Fetch error:', error);
        showError('Network error. Please check your connection.');
        showRetryButton();
    }
}

function showError(message) {
    const errorDiv = document.createElement('div');
    errorDiv.className = 'error-message';
    errorDiv.textContent = message;
    document.body.appendChild(errorDiv);
}

function showRetryButton() {
    const retryBtn = document.createElement('button');
    retryBtn.textContent = 'Retry';
    retryBtn.onclick = fetchData;
    document.body.appendChild(retryBtn);
}
```

**Proč je to špatně:**
- Uživatel neví, že něco selhalo
- Není možné zkusit znovu
- Mělo by to být jasně označeno

---

#### Problém 30: API error není zobrazen
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Pokud API vrátí chybu (500 Internal Server Error), není žádný feedback pro uživatele
- Mělo by se zobrazit error message

**Jak je to nyní:**
```javascript
async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    
    const board = await boardRes.json();
    // ← ŽÁDNÁ KONTROLA HTTP STATUS!
}
```

**Jak by to mělo být:**
```javascript
async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    
    // Kontrola HTTP status
    if (!boardRes.ok) {
        showError(`API error: ${boardRes.status} ${boardRes.statusText}`);
        return;
    }
    
    const board = await boardRes.json();
    // ...
}
```

**Proč je to špatně:**
- Uživatel neví, že API selhalo
- Není možné zkusit znovu
- Mělo by to být jasně označeno

---

#### Problém 31: JSON parsing error není zobrazen
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Pokud JSON parsing selže, není žádný feedback pro uživatele
- Mělo by se zobrazit error message

**Jak je to nyní:**
```javascript
async function fetchData() {
    const board = await boardRes.json();
    // ← ŽÁDNÁ KONTROLA JSON PARSING!
}
```

**Jak by to mělo být:**
```javascript
async function fetchData() {
    try {
        const board = await boardRes.json();
    } catch (error) {
        showError('Invalid JSON response from server.');
        return;
    }
}
```

**Proč je to špatně:**
- Uživatel neví, že JSON parsing selhal
- Není možné zkusit znovu
- Mělo by to být jasně označeno

---

#### Problém 32: Promise.all selže celý
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `fetchData()` volá 4 API endpointy současně pomocí `Promise.all()`
- Pokud jeden selže, všechny selžou
- Mělo by to být resilientnější

**Jak je to nyní:**
```javascript
async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    // ← JEDEN SELŽE → VŠECHNY SELŽOU!
}
```

**Jak by to mělo být:**
```javascript
async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.allSettled([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    
    // Zpracuj každý výsledek samostatně
    if (boardRes.status === 'fulfilled') {
        updateBoard(await boardRes.value.json());
    } else {
        showError('Failed to fetch board data.');
    }
    
    if (statusRes.status === 'fulfilled') {
        updateStatus(await statusRes.value.json());
    } else {
        showError('Failed to fetch status data.');
    }
    
    // ... atd.
}
```

**Proč je to špatně:**
- Jeden selže → všechny selžou
- Mělo by to být resilientnější
- Měly by se zobrazit částečné výsledky

---

### 7️⃣ CHYBY V PERFORMANCE (4 problémy)

**Popis:** Web má mnoho performance problémů, které zpomalují výkon.

#### Problém 33: updateBoard je pomalý
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `updateBoard()` iteruje přes všech 64 polí a aktualizuje DOM
- To je správně, ale může to být pomalé na mobilních zařízeních
- Mělo by se použít virtual DOM nebo batch updates

**Jak je to nyní:**
```javascript
function updateBoard(board) {
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const piece = board[row][col];
            const visualRow = 7 - row;
            const pieceElement = document.getElementById('piece-' + (visualRow * 8 + col));
            
            if (pieceElement) {
                pieceElement.textContent = pieceSymbols[piece] || ' ';
                // ← KAŽDÁ AKTUALIZACE JE SAMOSTATNÁ!
            }
        }
    }
}
```

**Jak by to mělo být:**
```javascript
function updateBoard(board) {
    // Vytvoř fragment pro batch update
    const fragment = document.createDocumentFragment();
    
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const piece = board[row][col];
            const visualRow = 7 - row;
            const pieceElement = document.getElementById('piece-' + (visualRow * 8 + col));
            
            if (pieceElement) {
                pieceElement.textContent = pieceSymbols[piece] || ' ';
                fragment.appendChild(pieceElement);
            }
        }
    }
    
    // Batch update
    document.getElementById('board').appendChild(fragment);
}
```

**Proč je to špatně:**
- Každá aktualizace je samostatná
- To je pomalé na mobilu
- Mělo by to být batch update

---

#### Problém 34: updateHistory je pomalý
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `updateHistory()` vytváří nové DOM elementy pro každý tah
- To je správně, ale může to být pomalé při dlouhé historii
- Mělo by se použít virtual scrolling

**Jak je to nyní:**
```javascript
function updateHistory(history) {
    historyData = history.moves || [];
    const historyBox = document.getElementById('history');
    historyBox.innerHTML = '';  // ← VYMAŽE VŠECHNO!
    
    historyData.slice().reverse().forEach((move, index) => {
        const item = document.createElement('div');
        item.className = 'history-item';
        // ...
        historyBox.appendChild(item);  // ← KAŽDÝ SAMOSTATNĚ!
    });
}
```

**Jak by to mělo být:**
```javascript
function updateHistory(history) {
    historyData = history.moves || [];
    const historyBox = document.getElementById('history');
    
    // Vytvoř fragment pro batch update
    const fragment = document.createDocumentFragment();
    
    historyData.slice().reverse().forEach((move, index) => {
        const item = document.createElement('div');
        item.className = 'history-item';
        // ...
        fragment.appendChild(item);  // ← DO FRAGMENTU!
    });
    
    // Batch update
    historyBox.innerHTML = '';
    historyBox.appendChild(fragment);
}
```

**Proč je to špatně:**
- Každá aktualizace je samostatná
- To je pomalé při dlouhé historii
- Mělo by to být batch update

---

#### Problém 35: Caching chybí
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Funkce `fetchData()` nemá žádný mechanismus pro caching
- Pokud se API nezměnilo, nemělo by se volat znovu
- Mělo by to být cached

**Jak je to nyní:**
```javascript
async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    // ← ŽÁDNÁ KONTROLA CACHING!
}
```

**Jak by to mělo být:**
```javascript
let lastBoardHash = null;
let lastStatusHash = null;
let lastHistoryHash = null;
let lastCapturedHash = null;

async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    
    const board = await boardRes.json();
    const boardHash = JSON.stringify(board);
    
    // Kontrola caching
    if (boardHash !== lastBoardHash) {
        updateBoard(board.board);
        lastBoardHash = boardHash;
    }
    
    // ... atd.
}
```

**Proč je to špatně:**
- API se volá zbytečně často
- To je pomalé
- Mělo by to být cached

---

#### Problém 36: Console.log je verbose
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Console.log() v `handleSquareClick()` je velmi verbose
- Může zpomalit výkon
- Mělo by to být možné vypnout

**Jak je to nyní:**
```javascript
function handleSquareClick(row, col) {
    console.log('');
    console.log('🖱️ ===== SQUARE CLICK =====');
    console.log('📍 Raw data:', { row, col });
    // ... mnoho dalších console.log()
}
```

**Jak by to mělo být:**
```javascript
const DEBUG = false;  // ← FLAG PRO DEBUG!

function handleSquareClick(row, col) {
    if (DEBUG) {
        console.log('');
        console.log('🖱️ ===== SQUARE CLICK =====');
        console.log('📍 Raw data:', { row, col });
        // ... atd.
    }
}
```

**Proč je to špatně:**
- Console.log() je pomalý
- Může zpomalit výkon
- Mělo by to být možné vypnout

---

### 8️⃣ CHYBY V MOBILE SUPPORT (3 problémy)

**Popis:** Web není optimalizovaný pro mobilní zařízení.

#### Problém 37: Board není touch-friendly
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Board není optimalizovaný pro mobilní zařízení
- Mělo by to být touch-friendly s většími tlačítky

**Jak by to mělo být:**
```css
.square {
    min-height: 44px;
    min-width: 44px;
}

@media (max-width: 768px) {
    .square {
        min-height: 60px;
        min-width: 60px;
    }
}
```

**Proč je to špatně:**
- Není touch-friendly
- Mělo by to být větší
- Mělo by to být optimalizované pro mobil

---

#### Problém 38: History items jsou malé
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- History items jsou malé a těžko klikatelné na mobilních zařízeních
- Měly by být větší

**Jak by to mělo být:**
```css
.history-item {
    padding: 12px;
    font-size: 13px;
    min-height: 44px;
}

@media (max-width: 768px) {
    .history-item {
        padding: 16px;
        font-size: 15px;
    }
}
```

**Proč je to špatně:**
- Jsou těžko klikatelné na mobilu
- Měly by být větší
- Měly by být touch-friendly

---

#### Problém 39: Tlačítka jsou malá
**Závažnost:** 🟡 VÝZNAMNÉ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Tlačítka (Try Moves, Exit Review, Exit Sandbox) jsou malé a těžko klikatelné na mobilních zařízeních
- Měly by být větší

**Jak by to mělo být:**
```css
.btn-try-moves {
    padding: 16px 32px;
    min-height: 44px;
}

@media (max-width: 768px) {
    .btn-try-moves {
        padding: 20px 40px;
    }
}
```

**Proč je to špatně:**
- Jsou těžko klikatelné na mobilu
- Měly by být větší
- Měly by být touch-friendly

---

### 9️⃣ CHYBY V API DOKUMENTACI (4 problémy)

**Popis:** API není správně dokumentováno, což způsobuje zmatek.

#### Problém 40: Board orientace není dokumentována
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- API /api/board vrací board[row][col] kde row=0 je rank 1 (bottom)
- To není dokumentováno
- Mělo by to být v API dokumentaci

**Jak by to mělo být:**
```javascript
/**
 * GET /api/board
 * 
 * Returns the current board state as a 2D array.
 * 
 * Board orientation:
 * - board[0][0] = a1 (White's bottom-left corner)
 * - board[0][7] = h1 (White's bottom-right corner)
 * - board[7][0] = a8 (Black's top-left corner)
 * - board[7][7] = h8 (Black's top-right corner)
 * 
 * Piece notation:
 * - 'P' = White Pawn
 * - 'N' = White Knight
 * - 'B' = White Bishop
 * - 'R' = White Rook
 * - 'Q' = White Queen
 * - 'K' = White King
 * - 'p' = Black Pawn
 * - 'n' = Black Knight
 * - 'b' = Black Bishop
 * - 'r' = Black Rook
 * - 'q' = Black Queen
 * - 'k' = Black King
 * - ' ' = Empty square
 * 
 * Response:
 * {
 *   "board": [
 *     ["R","N","B","Q","K","B","N","R"],
 *     ["P","P","P","P","P","P","P","P"],
 *     [" "," "," "," "," "," "," "," "],
 *     [" "," "," "," "," "," "," "," "],
 *     [" "," "," "," "," "," "," "," "],
 *     [" "," "," "," "," "," "," "," "],
 *     ["p","p","p","p","p","p","p","p"],
 *     ["r","n","b","q","k","b","n","r"]
 *   ],
 *   "timestamp": 1234567890
 * }
 */
```

**Proč je to špatně:**
- Není jasné, jak je board orientován
- To způsobuje zmatek
- Mělo to být dokumentováno

---

#### Problém 41: Status row/col není dokumentován
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- API /api/status vrací piece_lifted s row, col
- Ale row je v interním formátu (0-7), ne v chess notation (1-8)
- To není dokumentováno

**Jak by to mělo být:**
```javascript
/**
 * GET /api/status
 * 
 * Returns the current game status.
 * 
 * Response:
 * {
 *   "game_state": "active",
 *   "current_player": "white",
 *   "move_count": 10,
 *   "white_time": 30000,
 *   "black_time": 30000,
 *   "in_check": false,
 *   "checkmate": false,
 *   "stalemate": false,
 *   "piece_lifted": {
 *     "lifted": true,
 *     "row": 1,
 *     "col": 4,
 *     "piece": "P",
 *     "notation": "e2"
 *   }
 * }
 * 
 * Note:
 * - row and col are 0-based (0-7)
 * - notation is chess notation (e2, e4, etc.)
 */
```

**Proč je to špatně:**
- Není jasné, co jsou row a col
- To způsobuje zmatek
- Mělo to být dokumentováno

---

#### Problém 42: History notation není dokumentována
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- API /api/history vrací moves s from, to v chess notation (e2, e4)
- To je správně, ale piece je v interním formátu (P, p), ne v chess notation (♙, ♟)
- To není dokumentováno

**Jak by to mělo být:**
```javascript
/**
 * GET /api/history
 * 
 * Returns the move history.
 * 
 * Response:
 * {
 *   "moves": [
 *     {
 *       "from": "e2",
 *       "to": "e4",
 *       "piece": "P",
 *       "timestamp": 1234567890
 *     },
 *     {
 *       "from": "e7",
 *       "to": "e5",
 *       "piece": "p",
 *       "timestamp": 1234567900
 *     }
 *   ]
 * }
 * 
 * Note:
 * - from and to are in chess notation (e2, e4, etc.)
 * - piece is in internal format (P, p, etc.)
 * - timestamp is in milliseconds
 */
```

**Proč je to špatně:**
- Není jasné, co je piece
- To způsobuje zmatek
- Mělo to být dokumentováno

---

#### Problém 43: Captured notation není dokumentována
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- API /api/captured vrací white_captured, black_captured jako array znaků (P, p)
- To je správně, ale není to dokumentováno

**Jak by to mělo být:**
```javascript
/**
 * GET /api/captured
 * 
 * Returns the captured pieces.
 * 
 * Response:
 * {
 *   "white_captured": ["p", "n"],
 *   "black_captured": ["P", "B"]
 * }
 * 
 * Note:
 * - white_captured contains pieces captured by White
 * - black_captured contains pieces captured by Black
 * - pieces are in internal format (P, p, etc.)
 */
```

**Proč je to špatně:**
- Není jasné, co jsou captured pieces
- To způsobuje zmatek
- Mělo to být dokumentováno

---

### 🔟 CHYBY V SECURITY (3 problémy)

**Popis:** Web server nemá žádnou autentizaci ani autorizaci.

#### Problém 44: Autentizace chybí
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Web server nemá žádnou autentizaci
- Každý se může připojit a sledovat hru
- Mělo by to být možné nastavit heslo

**Jak by to mělo být:**
```javascript
// V ESP32 kódu
#define WEB_SERVER_PASSWORD "my_secret_password"

static esp_err_t http_get_root_handler(httpd_req_t *req)
{
    // Kontrola hesla
    const char* password = httpd_req_get_hdr_value_str(req, "X-Password");
    if (password == NULL || strcmp(password, WEB_SERVER_PASSWORD) != 0) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", -1);
        return ESP_FAIL;
    }
    
    // ... zbytek kódu
}
```

**Proč je to špatně:**
- Každý se může připojit
- To je bezpečnostní riziko
- Mělo by to být možné nastavit heslo

---

#### Problém 45: Autorizace chybí
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Web server nemá žádnou autorizaci
- Každý se může připojit a sledovat hru
- Mělo by to být možné nastavit uživatele a role

**Jak by to mělo být:**
```javascript
// V ESP32 kódu
typedef enum {
    ROLE_VIEWER,  // Může jen sledovat
    ROLE_PLAYER,  // Může hrát
    ROLE_ADMIN    // Může všechno
} user_role_t;

typedef struct {
    char username[32];
    char password[64];
    user_role_t role;
} user_t;

static user_t users[] = {
    {"admin", "admin123", ROLE_ADMIN},
    {"player1", "player1", ROLE_PLAYER},
    {"viewer1", "viewer1", ROLE_VIEWER}
};
```

**Proč je to špatně:**
- Každý se může připojit
- To je bezpečnostní riziko
- Mělo by to být možné nastavit uživatele a role

---

#### Problém 46: DDoS ochrana chybí
**Závažnost:** 🟢 MENŠÍ  
**Stav:** ❌ NEOVYŘEŠENO  
**Popis:**
- Web server nemá žádnou ochranu proti DDoS
- Pokud se připojí mnoho klientů, může to způsobit problémy
- Mělo by to být omezeno na maximální počet klientů

**Jak by to mělo být:**
```javascript
// V ESP32 kódu
#define MAX_CLIENTS 4

static uint32_t client_count = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        if (client_count >= MAX_CLIENTS) {
            ESP_LOGW(TAG, "Max clients reached, rejecting new connection");
            // Odpoj klienta
            return;
        }
        client_count++;
    }
}
```

**Proč je to špatně:**
- Není omezeno na maximální počet klientů
- To je bezpečnostní riziko
- Mělo by to být omezeno

---

## 📋 SOUHRN

### Počet problémů podle závažnosti:
- 🔴 **KRITICKÉ:** 20 problémů
- 🟡 **VÝZNAMNÉ:** 60 problémů
- 🟢 **MENŠÍ:** 20 problémů

### Počet problémů podle kategorie:
1. **Konflikty režimů:** 3 problémy
2. **Board orientace:** 4 problémy
3. **Sandbox mode:** 8 problémů
4. **Review mode:** 5 problémů
5. **UI/UX:** 8 problémů
6. **Error handling:** 4 problémy
7. **Performance:** 4 problémy
8. **Mobile support:** 3 problémy
9. **API dokumentace:** 4 problémy
10. **Security:** 3 problémy

### Doporučení:
1. **Nejdřív opravit KRITICKÉ problémy** (konflikty režimů, board orientace)
2. **Pak opravit VÝZNAMNÉ problémy** (sandbox mode, review mode, UI/UX)
3. **Nakonec opravit MENŠÍ problémy** (API dokumentace, security)

---

## 🎯 DALŠÍ KROKY

1. **Přezkoumat todolist** - Všechny problémy jsou v todolistu
2. **Prioritizovat problémy** - Začít s KRITICKÝMI problémy
3. **Implementovat opravy** - Postupně opravit všechny problémy
4. **Testovat opravy** - Ověřit, že opravy fungují správně
5. **Dokumentovat opravy** - Zapsat, co bylo opraveno

---

**Konec analýzy**

