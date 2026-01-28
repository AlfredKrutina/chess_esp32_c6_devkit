// ============================================================================
// CHESS WEB APP - EXTRACTED JAVASCRIPT FOR SYNTAX CHECKING
// ============================================================================

console.log('🚀 Chess JavaScript loading...');

// ============================================================================
// PIECE SYMBOLS AND GLOBAL VARIABLES
// ============================================================================

const pieceSymbols = {
    'R': '♜', 'N': '♞', 'B': '♝', 'Q': '♛', 'K': '♚', 'P': '♟',
    'r': '♖', 'n': '♘', 'b': '♗', 'q': '♕', 'k': '♔', 'p': '♙',
    ' ': ' '
};

let boardData = [];
let statusData = {};
let historyData = [];
let capturedData = { white_captured: [], black_captured: [] };
let advantageData = { history: [], white_checks: 0, black_checks: 0, white_castles: 0, black_castles: 0 };
let selectedSquare = null;
let reviewMode = false;
let currentReviewIndex = -1;
let initialBoard = [];
let sandboxMode = false;
let remoteControlEnabled = false;
let sandboxBoard = [];
let sandboxHistory = [];
let endgameReportShown = false;

// ============================================================================
// BOARD FUNCTIONS
// ============================================================================

function createBoard() {
    const board = document.getElementById('board');
    board.innerHTML = '';
    for (let row = 7; row >= 0; row--) {
        for (let col = 0; col < 8; col++) {
            const square = document.createElement('div');
            square.className = 'square ' + ((row + col) % 2 === 0 ? 'light' : 'dark');
            square.dataset.row = row;
            square.dataset.col = col;
            square.dataset.index = row * 8 + col;
            square.onclick = () => handleSquareClick(row, col);
            const piece = document.createElement('div');
            piece.className = 'piece';
            piece.id = 'piece-' + (row * 8 + col);
            square.appendChild(piece);
            board.appendChild(square);
        }
    }
}

function clearHighlights() {
    document.querySelectorAll('.square').forEach(sq => {
        // NEMAZAT lifted, error-invalid, error-original - tyto jsou řízené serverem
        // (z piece_lifted a error_state v JSON statusu)
        sq.classList.remove('selected', 'valid-move', 'valid-capture');
    });
    selectedSquare = null;
}

// ============================================================================
// REMOTE CONTROL LOGIC
// ============================================================================

function toggleRemoteControl() {
    const checkbox = document.getElementById('remote-control-enabled');
    remoteControlEnabled = checkbox.checked;
    console.log('Remote control:', remoteControlEnabled);
    
    if (!remoteControlEnabled) {
        clearHighlights();
    }
}

async function handleRemoteControlClick(row, col) {
    const notation = String.fromCharCode(97 + col) + (row + 1);
    let action = 'pickup';
    
    // Determine action based on currently lifted piece status
    // Note: statusData is updated from backend
    if (statusData && statusData.piece_lifted && statusData.piece_lifted.lifted) {
        action = 'drop';
    }
    
    console.log(`Remote control: ${action} at ${notation}`);
    
    // Visual feedback immediately (optimistic update)
    const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);
    if (square) {
        square.style.boxShadow = action === 'pickup' ? 
            'inset 0 0 20px rgba(255, 255, 0, 0.8)' : 
            'inset 0 0 20px rgba(0, 255, 0, 0.8)';
        
        setTimeout(() => {
            if (square) square.style.boxShadow = '';
        }, 500);
    }
    
    try {
        const response = await fetch('/api/game/virtual_action', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({action: action, square: notation})
        });
        const res = await response.json();
        console.log('Remote action response:', res);
        
        if (!res.success) {
            alert('Remote action failed: ' + res.message);
        }
    } catch (e) {
        console.error('Remote action error:', e);
    }
}

// ============================================================================
// SQUARE CLICK HANDLER
// ============================================================================

async function handleSquareClick(row, col) {
    const piece = sandboxMode ? sandboxBoard[row][col] : boardData[row][col];
    const index = row * 8 + col;

    // REMOTE CONTROL MODE - posílat příkazy na ESP
    if (remoteControlEnabled) {
        handleRemoteControlClick(row, col);
        return;
    }

    // SANDBOX MODE - vše lokálně, žádné POST requesty (včetně braní figurek)
    if (sandboxMode) {
        if (piece === ' ' && selectedSquare !== null) {
            // Tah na prázdné pole
            const fromRow = Math.floor(selectedSquare / 8);
            const fromCol = selectedSquare % 8;
            makeSandboxMove(fromRow, fromCol, row, col);
            clearHighlights();
        } else if (piece !== ' ') {
            if (selectedSquare !== null) {
                const fromRow = Math.floor(selectedSquare / 8);
                const fromCol = selectedSquare % 8;
                const selectedPiece = sandboxBoard[fromRow][fromCol];
                const isSameSquare = (fromRow === row && fromCol === col);
                const isOurPiece = (selectedPiece === selectedPiece.toUpperCase()) === (piece === piece.toUpperCase());

                if (isSameSquare) {
                    // Klik na stejné pole – zrušit výběr
                    clearHighlights();
                } else if (isOurPiece) {
                    // Klik na vlastní figurku – vybrat jinou
                    clearHighlights();
                    selectedSquare = index;
                    const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);
                    if (square) square.classList.add('selected');
                } else {
                    // Klik na soupeřovu figurku – brát (capture)
                    makeSandboxMove(fromRow, fromCol, row, col);
                    clearHighlights();
                }
            } else {
                // Žádná figurka vybraná – vybrat tuto
                clearHighlights();
                selectedSquare = index;
                const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);
                if (square) square.classList.add('selected');
            }
        }
        return;
    }

    // NORMÁLNÍ REŽIM (ne sandbox, ne remote control) - žádné POST requesty, žádný vizuální feedback
    // Web je jen pasivní zobrazení, hra se ovládá fyzicky
    return;
}

// ============================================================================
// REVIEW MODE
// ============================================================================

function reconstructBoardAtMove(moveIndex) {
    const startBoard = [
        ['R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'],
        ['P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'],
        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],
        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],
        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],
        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],
        ['p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'],
        ['r', 'n', 'b', 'q', 'k', 'b', 'n', 'r']
    ];
    const board = JSON.parse(JSON.stringify(startBoard));
    for (let i = 0; i <= moveIndex && i < historyData.length; i++) {
        const move = historyData[i];
        const fromRow = parseInt(move.from[1]) - 1;
        const fromCol = move.from.charCodeAt(0) - 97;
        const toRow = parseInt(move.to[1]) - 1;
        const toCol = move.to.charCodeAt(0) - 97;
        board[toRow][toCol] = board[fromRow][fromCol];
        board[fromRow][fromCol] = ' ';
    }
    return board;
}

function enterReviewMode(index) {
    reviewMode = true;
    currentReviewIndex = index;
    const banner = document.getElementById('review-banner');
    banner.classList.add('active');
    document.getElementById('review-move-text').textContent = `Reviewing move ${index + 1}`;
    const reconstructedBoard = reconstructBoardAtMove(index);
    updateBoard(reconstructedBoard);
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('move-from', 'move-to');
    });
    if (index >= 0 && index < historyData.length) {
        const move = historyData[index];
        const fromRow = parseInt(move.from[1]) - 1;
        const fromCol = move.from.charCodeAt(0) - 97;
        const toRow = parseInt(move.to[1]) - 1;
        const toCol = move.to.charCodeAt(0) - 97;
        const fromSquare = document.querySelector(`[data-row='${fromRow}'][data-col='${fromCol}']`);
        const toSquare = document.querySelector(`[data-row='${toRow}'][data-col='${toCol}']`);
        if (fromSquare) fromSquare.classList.add('move-from');
        if (toSquare) toSquare.classList.add('move-to');
    }
    document.querySelectorAll('.history-item').forEach(item => {
        item.classList.remove('selected');
    });
    const selectedItem = document.querySelector(`[data-move-index='${index}']`);
    if (selectedItem) {
        selectedItem.classList.add('selected');
        // Removed scrollIntoView - causes unwanted scroll on mobile when using navigation arrows
        // History item stays highlighted but page doesn't scroll away from board/banner
    }
}

function exitReviewMode() {
    reviewMode = false;
    currentReviewIndex = -1;
    document.getElementById('review-banner').classList.remove('active');
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('move-from', 'move-to');
    });
    document.querySelectorAll('.history-item').forEach(item => {
        item.classList.remove('selected');
    });
    fetchData();
}

// ============================================================================
// SANDBOX MODE
// ============================================================================

function enterSandboxMode() {
    sandboxMode = true;
    sandboxBoard = JSON.parse(JSON.stringify(boardData));
    sandboxHistory = [];
    const banner = document.getElementById('sandbox-banner');
    banner.classList.add('active');
    clearHighlights();
    updateUndoButton(); // Aktualizovat tlačítko undo při vstupu do sandbox mode
}

function exitSandboxMode() {
    sandboxMode = false;
    sandboxBoard = [];
    sandboxHistory = [];
    document.getElementById('sandbox-banner').classList.remove('active');
    clearHighlights();
    fetchData();
}

function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    const capturedPiece = sandboxBoard[toRow][toCol]; // Uložit captured piece (může být ' ')
    
    // Provedení tahu
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';
    
    // Uložit tah do historie s kompletními informacemi
    sandboxHistory.push({
        fromRow: fromRow,
        fromCol: fromCol,
        toRow: toRow,
        toCol: toCol,
        movingPiece: piece,
        capturedPiece: capturedPiece
    });
    
    // Omezit historii na 10 tahů
    if (sandboxHistory.length > 10) {
        sandboxHistory.shift(); // Odstranit nejstarší tah
    }
    
    updateBoard(sandboxBoard);
    updateUndoButton();
}

function updateUndoButton() {
    const undoBtn = document.getElementById('sandbox-undo-btn');
    if (!undoBtn) return;
    
    const availableUndos = sandboxHistory.length;
    const maxUndos = 10;
    
    undoBtn.textContent = `↶ Undo (${availableUndos}/${maxUndos})`;
    undoBtn.disabled = availableUndos === 0;
}

function undoSandboxMove() {
    if (sandboxHistory.length === 0) {
        return; // Žádné tahy k vrácení
    }
    
    // Vzít poslední tah z historie
    const lastMove = sandboxHistory.pop();
    
    // Vrátit figurku zpět
    sandboxBoard[lastMove.fromRow][lastMove.fromCol] = lastMove.movingPiece;
    
    // Obnovit captured piece (nebo prázdné pole)
    sandboxBoard[lastMove.toRow][lastMove.toCol] = lastMove.capturedPiece;
    
    // Aktualizovat board a tlačítko
    updateBoard(sandboxBoard);
    updateUndoButton();
    clearHighlights();
}

// ============================================================================
// UPDATE FUNCTIONS
// ============================================================================

function updateBoard(board) {
    boardData = board;
    const loading = document.getElementById('loading');
    if (loading) loading.style.display = 'none';

    // NEPŘIDÁVAT clearHighlights() - highlights jsou řízené přes updateStatus()
    // (lifted, error-invalid, error-original jsou serverem řízené stavy)

    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const piece = board[row][col];
            const pieceElement = document.getElementById('piece-' + (row * 8 + col));
            if (pieceElement) {
                pieceElement.textContent = pieceSymbols[piece] || ' ';
                if (piece !== ' ') {
                    pieceElement.className = 'piece ' + (piece === piece.toUpperCase() ? 'white' : 'black');
                } else {
                    pieceElement.className = 'piece';
                }
            }
        }
    }
}

// ============================================================================
// ENDGAME REPORT FUNCTIONS
// ============================================================================

// Zobrazit endgame report na webu
async function showEndgameReport(gameEnd) {
    console.log('🏆 showEndgameReport() called with:', gameEnd);

    // Pokud už je banner zobrazen, nedělat nic (aby se nepřekresloval)
    if (endgameReportShown && document.getElementById('endgame-banner')) {
        console.log('Endgame report already shown, skipping...');
        return;
    }

    // Načíst advantage history pro graf
    let advantageDataLocal = { history: [], white_checks: 0, black_checks: 0, white_castles: 0, black_castles: 0 };
    try {
        const response = await fetch('/api/advantage');
        advantageDataLocal = await response.json();
        console.log('Advantage data loaded:', advantageDataLocal);
    } catch (e) {
        console.error('Failed to load advantage data:', e);
    }

    // Určit výsledek a barvy
    let emoji = '🏆';
    let title = '';
    let subtitle = '';
    let accentColor = '#4CAF50';
    let bgGradient = 'linear-gradient(135deg, #1e3a1e, #2d4a2d)';

    if (gameEnd.winner === 'Draw') {
        emoji = '🤝';
        title = 'REMÍZA';
        subtitle = gameEnd.reason;
        accentColor = '#FF9800';
        bgGradient = 'linear-gradient(135deg, #3a2e1e, #4a3e2d)';
    } else {
        emoji = gameEnd.winner === 'White' ? '⚪' : '⚫';
        title = `${gameEnd.winner.toUpperCase()} VYHRÁL!`;
        subtitle = gameEnd.reason;
        accentColor = gameEnd.winner === 'White' ? '#4CAF50' : '#2196F3';
        bgGradient = gameEnd.winner === 'White' ? 'linear-gradient(135deg, #1e3a1e, #2d4a2d)' : 'linear-gradient(135deg, #1e2a3a, #2d3a4a)';
    }

    // Získat statistiky
    const whiteMoves = Math.ceil(statusData.move_count / 2);
    const blackMoves = Math.floor(statusData.move_count / 2);
    const whiteCaptured = capturedData.white_captured || [];
    const blackCaptured = capturedData.black_captured || [];

    // Material advantage
    const pieceValues = { p: 1, n: 3, b: 3, r: 5, q: 9, P: 1, N: 3, B: 3, R: 5, Q: 9 };
    let whiteMaterial = 0, blackMaterial = 0;
    whiteCaptured.forEach(p => whiteMaterial += pieceValues[p] || 0);
    blackCaptured.forEach(p => blackMaterial += pieceValues[p] || 0);
    const materialDiff = whiteMaterial - blackMaterial;
    const materialText = materialDiff > 0 ? `White +${materialDiff}` : materialDiff < 0 ? `Black +${-materialDiff}` : 'Vyrovnáno';

    // Vytvořit SVG graf výhody (jako chess.com)
    let graphSVG = '';
    if (advantageDataLocal.history && advantageDataLocal.history.length > 1) {
        const history = advantageDataLocal.history;
        const width = 280;
        const height = 100;
        const maxAdvantage = Math.max(10, ...history.map(Math.abs));
        const scaleY = height / (2 * maxAdvantage);
        const scaleX = width / (history.length - 1);

        // Vytvořit body pro polyline (0,0 je nahoře vlevo, y roste dolů)
        let points = history.map((adv, i) => {
            const x = i * scaleX;
            const y = height / 2 - adv * scaleY;  // Převrátit Y (White nahoře, Black dole)
            return `${x},${y}`;
        }).join(' ');

        // Vytvořit polygon pro vyplněnou oblast
        let areaPoints = `0,${height / 2} ${points} ${width},${height / 2}`;

        graphSVG = `<svg width="280" height="100" style="border-radius:6px;background:rgba(0,0,0,0.2);">
            <!-- Středová čára (vyrovnaná pozice) -->
            <line x1="0" y1="${height / 2}" x2="${width}" y2="${height / 2}" stroke="#555" stroke-width="1" stroke-dasharray="3,3"/>
            <!-- Vyplněná oblast pod křivkou -->
            <polygon points="${areaPoints}" fill="${accentColor}" opacity="0.2"/>
            <!-- Křivka výhody -->
            <polyline points="${points}" fill="none" stroke="${accentColor}" stroke-width="2" stroke-linejoin="round"/>
            <!-- Tečky na koncích -->
            <circle cx="0" cy="${height / 2}" r="3" fill="${accentColor}"/>
            <circle cx="${(history.length - 1) * scaleX}" cy="${height / 2 - history[history.length - 1] * scaleY}" r="4" fill="${accentColor}"/>
            <!-- Popisky -->
            <text x="5" y="12" fill="#888" font-size="10" font-weight="600">White</text>
            <text x="5" y="${height - 2}" fill="#888" font-size="10" font-weight="600">Black</text>
        </svg>`;
    }

    // Vytvořit nový banner - VLEVO OD BOARDU, NE UPROSTŘED!
    const banner = document.createElement('div');
    banner.id = 'endgame-banner';

    // Na mobilu - jiné umístění (nahoře, plná šířka)
    if (window.innerWidth <= 768) {
        banner.style.cssText = `
            position: fixed;
            left: 10px;
            right: 10px;
            top: 10px;
            width: auto;
            max-height: 80vh;
            transform: none;
            overflow-y: auto;
            background: ${bgGradient};
            border: 2px solid ${accentColor};
            border-radius: 12px;
            padding: 0;
            box-shadow: 0 8px 32px rgba(0,0,0,0.6);
            z-index: 9999;
            animation: slideInTop 0.4s ease-out;
        `;
    } else {
        banner.style.cssText = `
            position: fixed;
            left: 10px;
            top: 50%;
            transform: translateY(-50%);
            width: 320px;
            max-height: 90vh;
            overflow-y: auto;
            background: ${bgGradient};
            border: 2px solid ${accentColor};
            border-radius: 12px;
            padding: 0;
            box-shadow: 0 8px 32px rgba(0,0,0,0.6), 0 0 40px ${accentColor}40;
            z-index: 9999;
            animation: slideInLeft 0.4s ease-out;
            backdrop-filter: blur(10px);
        `;
    }

    // HTML obsah
    banner.innerHTML = `
        <div style="background:${accentColor};padding:20px;text-align:center;border-radius:10px 10px 0 0;">
            <div style="font-size:64px;margin-bottom:8px;">${emoji}</div>
            <h2 style="margin:0;color:white;font-size:24px;font-weight:700;text-shadow:0 2px 4px rgba(0,0,0,0.4);">${title}</h2>
            <p style="margin:8px 0 0 0;color:rgba(255,255,255,0.9);font-size:14px;font-weight:500;">${subtitle}</p>
        </div>
        <div style="padding:20px;">
            ${graphSVG ? `
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;display:flex;align-items:center;gap:8px;">
                    <span>📈</span> Průběh hry
                </h3>
                ${graphSVG}
                <div style="display:flex;justify-content:space-between;margin-top:8px;font-size:11px;color:#888;">
                    <span>Začátek</span>
                    <span>Tah ${advantageDataLocal.count || 0}</span>
                </div>
            </div>` : ''}
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;display:flex;align-items:center;gap:8px;">
                    <span>📊</span> Statistiky
                </h3>
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;font-size:13px;">
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Tahy</div>
                        <div style="color:#e0e0e0;font-weight:600;">⚪ ${whiteMoves} | ⚫ ${blackMoves}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Materiál</div>
                        <div style="color:${accentColor};font-weight:600;">${materialText}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Sebráno</div>
                        <div style="color:#e0e0e0;font-weight:600;">⚪ ${whiteCaptured.length} | ⚫ ${blackCaptured.length}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Celkem</div>
                        <div style="color:#e0e0e0;font-weight:600;">${statusData.move_count} tahů</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Šachy</div>
                        <div style="color:#e0e0e0;font-weight:600;">⚪ ${advantageDataLocal.white_checks || 0} | ⚫ ${advantageDataLocal.black_checks || 0}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Rošády</div>
                        <div style="color:#e0e0e0;font-weight:600;">⚪ ${advantageDataLocal.white_castles || 0} | ⚫ ${advantageDataLocal.black_castles || 0}</div>
                    </div>
                </div>
            </div>
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;display:flex;align-items:center;gap:8px;">
                    <span>⚔️</span> Sebrané figurky
                </h3>
                <div style="margin-bottom:10px;">
                    <div style="color:#888;font-size:11px;margin-bottom:4px;">White sebral (${whiteCaptured.length})</div>
                    <div style="font-size:20px;line-height:1.4;">${whiteCaptured.map(p => pieceSymbols[p] || p).join(' ') || '−'}</div>
                </div>
                <div>
                    <div style="color:#888;font-size:11px;margin-bottom:4px;">Black sebral (${blackCaptured.length})</div>
                    <div style="font-size:20px;line-height:1.4;">${blackCaptured.map(p => pieceSymbols[p] || p).join(' ') || '−'}</div>
                </div>
            </div>
            <button onclick="hideEndgameReport()" style="
                width:100%;
                padding:14px;
                font-size:16px;
                background:${accentColor};
                color:white;
                border:none;
                border-radius:8px;
                cursor:pointer;
                font-weight:600;
                box-shadow:0 4px 12px rgba(0,0,0,0.3);
                transition:all 0.2s;
            " onmouseover="this.style.transform='translateY(-2px)';this.style.boxShadow='0 6px 16px rgba(0,0,0,0.4)'" onmouseout="this.style.transform='translateY(0)';this.style.boxShadow='0 4px 12px rgba(0,0,0,0.3)'">
                ✓ OK
            </button>
        </div>
    `;

    // Přidat CSS animace pokud ještě neexistují
    if (!document.getElementById('endgame-animations')) {
        const style = document.createElement('style');
        style.id = 'endgame-animations';
        style.textContent = `
            @keyframes slideInLeft {
                from { transform: translateY(-50%) translateX(-100%); opacity: 0; }
                to { transform: translateY(-50%) translateX(0); opacity: 1; }
            }
            @keyframes slideInTop {
                from { transform: translateY(-100%); opacity: 0; }
                to { transform: translateY(0); opacity: 1; }
            }
        `;
        document.head.appendChild(style);
    }

    document.body.appendChild(banner);
    endgameReportShown = true;  // Označit, že je zobrazený
    console.log('🏆 ENDGAME REPORT SHOWN - banner displayed (left side)');
}

// Skrýt endgame report (ale zachovat flag pro toggle)
function hideEndgameReport() {
    console.log('Hiding endgame report...');
    const banner = document.getElementById('endgame-banner');
    if (banner) {
        banner.remove();
        console.log('Endgame report hidden (can be toggled back)');
    }
}

// Toggle endgame report (show/hide)
function toggleEndgameReport() {
    const banner = document.getElementById('endgame-banner');
    if (banner) {
        // Uz je zobrazen -> skryj
        hideEndgameReport();
    } else {
        // Neni zobrazen -> znovu zobraz (pokud mame data)
        if (window.lastGameEndData) {
            showEndgameReport(window.lastGameEndData);
        }
    }
}

// Zobrazit toggle button
function showEndgameToggleButton() {
    // Zjistit zda uz button existuje
    if (document.getElementById('endgame-toggle-btn')) return;

    const button = document.createElement('button');
    button.id = 'endgame-toggle-btn';
    button.innerHTML = '🏆 Report';
    button.title = 'Show/Hide Endgame Report';
    button.style.cssText = `
        position: fixed;
        top: 10px;
        left: 10px;
        padding: 10px 16px;
        background: linear-gradient(135deg, #4CAF50, #45a049);
        color: white;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        font-weight: 600;
        font-size: 14px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        z-index: 10000;
        transition: all 0.2s;
    `;
    button.onmouseover = function () {
        this.style.transform = 'translateY(-2px)';
        this.style.boxShadow = '0 6px 16px rgba(0,0,0,0.4)';
    };
    button.onmouseout = function () {
        this.style.transform = 'translateY(0)';
        this.style.boxShadow = '0 4px 12px rgba(0,0,0,0.3)';
    };
    button.onclick = toggleEndgameReport;
    document.body.appendChild(button);
}

// Skrýt toggle button
function hideEndgameToggleButton() {
    const button = document.getElementById('endgame-toggle-btn');
    if (button) {
        button.remove();
    }
}

// ============================================================================
// STATUS UPDATE FUNCTION
// ============================================================================

function updateStatus(status) {
    statusData = status;
    document.getElementById('game-state').textContent = status.game_state || '-';
    document.getElementById('current-player').textContent = status.current_player || '-';
    document.getElementById('move-count').textContent = status.move_count || 0;
    document.getElementById('in-check').textContent = status.in_check ? 'Yes' : 'No';

    // ERROR STATE - vždy nejprve odstranit všechny error classes
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('error-invalid', 'error-original');
    });

    // LIFTED PIECE - vždy nejprve odstranit všechny lifted classes
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('lifted');
    });

    // Zobrazit lifted piece (zelená)
    const lifted = status.piece_lifted;
    if (lifted && lifted.lifted) {
        document.getElementById('lifted-piece').textContent = pieceSymbols[lifted.piece] || '-';
        document.getElementById('lifted-position').textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);
        const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);
        if (square) square.classList.add('lifted'); // Zelená - zvednutá figurka
    } else {
        document.getElementById('lifted-piece').textContent = '-';
        document.getElementById('lifted-position').textContent = '-';
    }

    // Zobrazit error state (červená na invalid, modrá na original)
    if (status.error_state && status.error_state.active) {
        // Invalid position (červená - kde je figurka nyní na nevalidní pozici)
        if (status.error_state.invalid_pos) {
            const invalidCol = status.error_state.invalid_pos.charCodeAt(0) - 97;
            const invalidRow = parseInt(status.error_state.invalid_pos[1]) - 1;
            const invalidSquare = document.querySelector(`[data-row='${invalidRow}'][data-col='${invalidCol}']`);
            if (invalidSquare) invalidSquare.classList.add('error-invalid'); // Červená - nevalidní pozice
        }
        // Original position (modrá - kde byla figurka původně)
        if (status.error_state.original_pos) {
            const originalCol = status.error_state.original_pos.charCodeAt(0) - 97;
            const originalRow = parseInt(status.error_state.original_pos[1]) - 1;
            const originalSquare = document.querySelector(`[data-row='${originalRow}'][data-col='${originalCol}']`);
            if (originalSquare) originalSquare.classList.add('error-original'); // Modrá - původní pozice
        }
    }

    // ENDGAME REPORT - zobrazit pouze JEDNOU, po prvnim skonceni
    if (status.game_end && status.game_end.ended) {
        // Ulozit data pro pozdejsi toggle
        window.lastGameEndData = status.game_end;

        // Zobrazit report jen pokud jeste nebyl nikdy zobrazen
        if (!endgameReportShown) {
            console.log('Game ended, showing endgame report...');
            showEndgameReport(status.game_end);
        }

        // Zobrazit toggle button (jen pokud je hra skoncena)
        showEndgameToggleButton();
    } else {
        // Hra je aktivni - skryj report i toggle button
        if (endgameReportShown) {
            console.log('Game restarted, clearing endgame report...');
            hideEndgameReport();
        }
        endgameReportShown = false;  // Reset flagu po restartu
        window.lastGameEndData = null;
        hideEndgameToggleButton();
    }

}

function updateHistory(history) {
    historyData = history.moves || [];
    const historyBox = document.getElementById('history');
    historyBox.innerHTML = '';
    historyData.slice().reverse().forEach((move, index) => {
        const item = document.createElement('div');
        item.className = 'history-item';
        const actualIndex = historyData.length - 1 - index;
        item.dataset.moveIndex = actualIndex;
        const moveNum = Math.floor(actualIndex / 2) + 1;
        const isWhite = actualIndex % 2 === 0;
        const prefix = isWhite ? moveNum + '. ' : '';
        item.textContent = prefix + move.from + ' → ' + move.to;
        item.onclick = () => enterReviewMode(actualIndex);
        historyBox.appendChild(item);
    });
}

function updateCaptured(captured) {
    capturedData = captured;
    const whiteBox = document.getElementById('white-captured');
    const blackBox = document.getElementById('black-captured');
    whiteBox.innerHTML = '';
    blackBox.innerHTML = '';
    captured.white_captured.forEach(p => {
        const piece = document.createElement('div');
        piece.className = 'captured-piece';
        piece.textContent = pieceSymbols[p] || p;
        whiteBox.appendChild(piece);
    });
    captured.black_captured.forEach(p => {
        const piece = document.createElement('div');
        piece.className = 'captured-piece';
        piece.textContent = pieceSymbols[p] || p;
        blackBox.appendChild(piece);
    });
}

async function fetchData() {
    if (reviewMode || sandboxMode) return;
    try {
        const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
            fetch('/api/board'),
            fetch('/api/status'),
            fetch('/api/history'),
            fetch('/api/captured')
        ]);
        const board = await boardRes.json();
        const status = await statusRes.json();
        const history = await historyRes.json();
        const captured = await capturedRes.json();
        updateBoard(board.board);
        updateStatus(status);
        updateHistory(history);
        updateCaptured(captured);
    } catch (error) {
        console.error('Fetch error:', error);
    }
}

function initializeApp() {
    console.log('🎮 Initializing Chess App...');
    createBoard();

    // Inject Demo Mode section at bottom
    injectDemoModeSection();

    fetchData();
    setInterval(fetchData, 2000); // Reduced from 500ms to 2s (4× fewer requests)
    console.log('✅ Chess App initialized');
}

/**
 * Inject Demo Mode control section into DOM
 * Placed at bottom, below all main content
 */
function injectDemoModeSection() {
    const container = document.querySelector('.container') || document.body;

    const demoSection = document.createElement('div');
    demoSection.style.cssText = 'margin-top:30px; padding:20px; background:#2a2a2a; border-radius:8px; border-left:4px solid #666;';
    demoSection.innerHTML = `
        <h3 style="color:#999;font-size:0.9em;margin-bottom:15px;text-transform:uppercase;letter-spacing:1px;">🤖 Demo Mode</h3>
        <div style="display:flex;align-items:center;gap:15px;margin-bottom:10px;">
            <button id="btnDemoMode" onclick="toggleDemoMode()" 
                    style="padding:10px 20px;background:#008CBA;color:white;border:2px solid #007396;
                           border-radius:6px;cursor:pointer;font-size:14px;font-weight:600;transition:all 0.3s;">
                Toggle Demo Mode
            </button>
            <span id="demoStatus" style="font-size:14px;color:#999;">⚫ Off</span>
        </div>
        <p style="font-size:12px;color:#666;margin:0;font-style:italic;">
            Automatic chess game playback. Touch board to interrupt.
        </p>
    `;

    container.appendChild(demoSection);
    console.log('✅ Demo Mode section injected');
}

console.log('🚀 Creating chess board...');
initializeApp(); // Call the new initialization function
console.log('✅ Chess JavaScript loaded successfully!');
console.log('⏱️ About to initialize timer system...');

// ============================================================================
// TIMER SYSTEM
// ============================================================================

let timerData = {
    white_time_ms: 0,
    black_time_ms: 0,
    timer_running: false,
    is_white_turn: true,
    game_paused: false,
    time_expired: false,
    config: null,
    total_moves: 0,
    avg_move_time_ms: 0
};
let timerUpdateInterval = null;
let selectedTimeControl = 0;

// ========== HELPER FUNCTIONS (must be defined before use) ==========

function formatTime(timeMs) {
    const totalSeconds = Math.ceil(timeMs / 1000);
    const hours = Math.floor(totalSeconds / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    if (hours > 0) {
        return hours + ':' + minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0');
    } else {
        return minutes + ':' + seconds.toString().padStart(2, '0');
    }
}

function updatePlayerTime(player, timeMs) {
    const timeElement = document.getElementById(player + '-time');
    const playerElement = document.getElementById(player + '-timer');
    if (!timeElement || !playerElement) return;

    // Zkontrolovat zda je časová kontrola aktivní
    const isTimerActive = timerData.config && timerData.config.type !== 0;

    if (isTimerActive) {
        const formattedTime = formatTime(timeMs);
        timeElement.textContent = formattedTime;
        playerElement.classList.remove('low-time', 'critical-time');
        if (timeMs < 5000) playerElement.classList.add('critical-time');
        else if (timeMs < 30000) playerElement.classList.add('low-time');
    } else {
        // Bez časové kontroly - zobrazit "--:--" a odstranit všechny warning třídy
        timeElement.textContent = '--:--';
        playerElement.classList.remove('low-time', 'critical-time', 'active');
        return; // Nedělat nic dalšího
    }

    if ((player === 'white' && timerData.is_white_turn) || (player === 'black' && !timerData.is_white_turn)) {
        playerElement.classList.add('active');
    } else {
        playerElement.classList.remove('active');
    }
}

function updateActivePlayer(isWhiteTurn) {
    const whiteIndicator = document.getElementById('white-move-indicator');
    const blackIndicator = document.getElementById('black-move-indicator');
    if (whiteIndicator && blackIndicator) {
        whiteIndicator.classList.toggle('active', isWhiteTurn);
        blackIndicator.classList.toggle('active', !isWhiteTurn);
    }
}

function updateProgressBars(timerInfo) {
    if (!timerInfo || !timerInfo.config) {
        console.warn('Timer info missing config:', timerInfo);
        return;
    }

    // Zkontrolovat zda je časová kontrola aktivní
    if (timerInfo.config.type === 0) {
        // Bez časové kontroly - skrýt progress bary
        const whiteProgress = document.getElementById('white-progress');
        const blackProgress = document.getElementById('black-progress');
        if (whiteProgress) whiteProgress.style.width = '0%';
        if (blackProgress) blackProgress.style.width = '0%';
        return;
    }

    const initialTime = timerInfo.config.initial_time_ms;
    if (initialTime === 0) return;
    const whiteProgress = document.getElementById('white-progress');
    const blackProgress = document.getElementById('black-progress');
    if (whiteProgress) {
        const whitePercent = (timerInfo.white_time_ms / initialTime) * 100;
        whiteProgress.style.width = Math.max(0, Math.min(100, whitePercent)) + '%';
    }
    if (blackProgress) {
        const blackPercent = (timerInfo.black_time_ms / initialTime) * 100;
        blackProgress.style.width = Math.max(0, Math.min(100, blackPercent)) + '%';
    }
}

function updateTimerStats(timerInfo) {
    const avgMoveTimeElement = document.getElementById('avg-move-time');
    const totalMovesElement = document.getElementById('total-moves');
    if (avgMoveTimeElement) {
        avgMoveTimeElement.textContent = timerInfo.avg_move_time_ms > 0 ? formatTime(timerInfo.avg_move_time_ms) : '-';
    }
    if (totalMovesElement) {
        totalMovesElement.textContent = timerInfo.total_moves || 0;
    }
}

function checkTimeWarnings(timerInfo) {
    // Nekontrolovat upozornění pokud není časová kontrola aktivní
    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) {
        return;
    }

    const currentPlayerTime = timerInfo.is_white_turn ? timerInfo.white_time_ms : timerInfo.black_time_ms;
    if (currentPlayerTime < 5000 && !timerInfo.warning_5s_shown) {
        showTimeWarning('Critical! Less than 5 seconds!', 'critical');
    } else if (currentPlayerTime < 10000 && !timerInfo.warning_10s_shown) {
        showTimeWarning('Warning! Less than 10 seconds!', 'warning');
    } else if (currentPlayerTime < 30000 && !timerInfo.warning_30s_shown) {
        showTimeWarning('Low time! Less than 30 seconds!', 'info');
    }
}

function showTimeWarning(message, type) {
    const notification = document.createElement('div');
    notification.className = 'time-warning ' + type;
    notification.textContent = message;
    notification.style.cssText = 'position: fixed; top: 20px; right: 20px; padding: 15px 20px; border-radius: 8px; color: white; font-weight: 600; z-index: 1000; animation: slideInRight 0.3s ease;';
    switch (type) {
        case 'critical': notification.style.background = '#F44336'; break;
        case 'warning': notification.style.background = '#FF9800'; break;
        case 'info': notification.style.background = '#2196F3'; break;
    }
    document.body.appendChild(notification);
    setTimeout(() => {
        notification.style.animation = 'slideOutRight 0.3s ease';
        setTimeout(() => {
            if (notification.parentNode) notification.parentNode.removeChild(notification);
        }, 300);
    }, 3000);
}

function handleTimeExpiration(timerInfo) {
    // Nekontrolovat expiraci pokud není časová kontrola aktivní
    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) {
        return;
    }

    const expiredPlayer = timerInfo.is_white_turn ? 'White' : 'Black';
    showTimeWarning('Time expired! ' + expiredPlayer + ' lost on time.', 'critical');
    const pauseBtn = document.getElementById('pause-timer');
    const resumeBtn = document.getElementById('resume-timer');
    if (pauseBtn) pauseBtn.disabled = true;
    if (resumeBtn) resumeBtn.disabled = true;
}

function toggleCustomSettings() {
    const customSettings = document.getElementById('custom-time-settings');
    if (!customSettings) return;
    if (selectedTimeControl === 14) {
        customSettings.style.display = 'block';
    } else {
        customSettings.style.display = 'none';
    }
}

function changeTimeControl() {
    const select = document.getElementById('time-control-select');
    const applyBtn = document.getElementById('apply-time-control');
    if (!select) return;
    selectedTimeControl = parseInt(select.value);
    toggleCustomSettings();
    if (applyBtn) applyBtn.disabled = false;
    localStorage.setItem('chess_time_control', selectedTimeControl.toString());
}

// ========== TIMER INITIALIZATION AND MAIN FUNCTIONS ==========

function initTimerSystem() {
    console.log('🔵 Initializing timer system...');
    // Check if DOM elements exist before accessing them
    const timeControlSelect = document.getElementById('time-control-select');
    const applyButton = document.getElementById('apply-time-control');
    if (!timeControlSelect) {
        console.warn('⚠️ Timer controls not ready yet, retrying in 100ms...');
        setTimeout(() => initTimerSystem(), 100);
        return;
    }
    const savedTimeControl = localStorage.getItem('chess_time_control');
    if (savedTimeControl) {
        selectedTimeControl = parseInt(savedTimeControl);
        timeControlSelect.value = selectedTimeControl;
    } else {
        selectedTimeControl = parseInt(timeControlSelect.value);
    }
    toggleCustomSettings();
    // Enable button if a time control is selected (not 0 = None)
    if (selectedTimeControl !== 0 && applyButton) {
        applyButton.disabled = false;
    }
    console.log('🔵 Starting timer update loop immediately...');
    // Start timer loop immediately (no delay)
    startTimerUpdateLoop();
}

function startTimerUpdateLoop() {
    console.log('✅ Timer update loop starting... (will update every 1000ms)');
    if (timerUpdateInterval) {
        console.log('⚠️ Clearing existing timer interval');
        clearInterval(timerUpdateInterval);
    }
    timerUpdateInterval = setInterval(async () => {
        try {
            await updateTimerDisplay();
        } catch (error) {
            console.error('❌ Timer update loop error:', error);
        }
    }, 1000); // Optimized from 200ms to 1s (5× fewer requests, still responsive)
    console.log('✅ Timer interval set successfully, ID:', timerUpdateInterval);
    // Initial immediate update
    console.log('⏱️ Calling initial timer update...');
    updateTimerDisplay().catch(e => console.error('❌ Initial timer update failed:', e));
}

async function updateTimerDisplay() {
    try {
        console.log('⏱️ updateTimerDisplay() called, fetching /api/timer...');
        const response = await fetch('/api/timer');
        console.log('⏱️ /api/timer response status:', response.status);
        if (response.ok) {
            const timerInfo = await response.json();
            timerData = timerInfo;
            // Format time for logging
            const whiteTime = formatTime(timerInfo.white_time_ms);
            const blackTime = formatTime(timerInfo.black_time_ms);
            console.log('⏱️ Timer:', timerInfo.config ? timerInfo.config.name : 'NO CONFIG', '| White:', whiteTime, '(' + timerInfo.white_time_ms + 'ms)', '| Black:', blackTime, '(' + timerInfo.black_time_ms + 'ms)');
            updatePlayerTime('white', timerInfo.white_time_ms);
            updatePlayerTime('black', timerInfo.black_time_ms);
            updateActivePlayer(timerInfo.is_white_turn);
            updateProgressBars(timerInfo);
            updateTimerStats(timerInfo);
            // Disable/enable timer controls podle config.type
            const pauseBtn = document.getElementById('pause-timer');
            const resumeBtn = document.getElementById('resume-timer');
            const resetBtn = document.getElementById('reset-timer');
            const isTimerActive = timerInfo.config && timerInfo.config.type !== 0;
            if (pauseBtn) pauseBtn.disabled = !isTimerActive;
            if (resumeBtn) resumeBtn.disabled = !isTimerActive;
            if (resetBtn) resetBtn.disabled = !isTimerActive;
            // Pouze pokud je časová kontrola aktivní
            if (isTimerActive) {
                checkTimeWarnings(timerInfo);
                if (timerInfo.time_expired) {
                    handleTimeExpiration(timerInfo);
                }
            }
        } else {
            console.error('❌ Timer update failed:', response.status);
        }
    } catch (error) {
        console.error('❌ Timer update error:', error);
    }
}

async function applyTimeControl() {
    const timeControlSelect = document.getElementById('time-control-select');
    const timeControlType = parseInt(timeControlSelect.value);
    let config = { type: timeControlType };
    if (timeControlType === 14) {
        const minutes = parseInt(document.getElementById('custom-minutes').value);
        const increment = parseInt(document.getElementById('custom-increment').value);
        if (minutes < 1 || minutes > 180) { alert('Minutes must be between 1 and 180'); return; }
        if (increment < 0 || increment > 60) { alert('Increment must be between 0 and 60 seconds'); return; }
        config.custom_minutes = minutes;
        config.custom_increment = increment;
    }
    try {
        console.log('Applying time control:', config);
        const response = await fetch('/api/timer/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        if (response.ok) {
            const responseText = await response.text();
            console.log('✅ Time control response:', responseText);
            // Wait for backend to process the command
            await new Promise(resolve => setTimeout(resolve, 500));
            // Refresh timer display multiple times to ensure update
            for (let i = 0; i < 5; i++) {
                await updateTimerDisplay();
                await new Promise(resolve => setTimeout(resolve, 300));
            }
            showTimeWarning('Time control applied!', 'info');
            const applyBtn = document.getElementById('apply-time-control');
            if (applyBtn) applyBtn.disabled = true;
        } else {
            const errorText = await response.text();
            console.error('Failed to apply time control:', response.status, errorText);
            throw new Error('Failed to apply time control: ' + errorText);
        }
    } catch (error) {
        console.error('Error applying time control:', error);
        showTimeWarning('Error setting time control: ' + error.message, 'critical');
    }
}

async function pauseTimer() {
    try {
        const response = await fetch('/api/timer/pause', { method: 'POST' });
        if (response.ok) {
            const pauseBtn = document.getElementById('pause-timer');
            const resumeBtn = document.getElementById('resume-timer');
            if (pauseBtn) pauseBtn.style.display = 'none';
            if (resumeBtn) resumeBtn.style.display = 'inline-block';
            showTimeWarning('Timer paused', 'info');
        }
    } catch (error) {
        console.error('❌ Error pausing timer:', error);
    }
}

async function resumeTimer() {
    try {
        const response = await fetch('/api/timer/resume', { method: 'POST' });
        if (response.ok) {
            const pauseBtn = document.getElementById('pause-timer');
            const resumeBtn = document.getElementById('resume-timer');
            if (pauseBtn) pauseBtn.style.display = 'inline-block';
            if (resumeBtn) resumeBtn.style.display = 'none';
            showTimeWarning('Timer resumed', 'info');
        }
    } catch (error) {
        console.error('❌ Error resuming timer:', error);
    }
}

async function resetTimer() {
    if (confirm('Really reset timer?')) {
        try {
            const response = await fetch('/api/timer/reset', { method: 'POST' });
            if (response.ok) {
                showTimeWarning('Timer reset', 'info');
                console.log('✅ Timer reset successfully');
                await updateTimerDisplay();
            }
        } catch (error) {
            console.error('❌ Error resetting timer:', error);
        }
    }
}

// Expose timer functions globally for inline onclick handlers
window.changeTimeControl = changeTimeControl;
window.applyTimeControl = applyTimeControl;
window.pauseTimer = pauseTimer;
window.resumeTimer = resumeTimer;
window.resetTimer = resetTimer;
window.hideEndgameReport = hideEndgameReport;
window.toggleRemoteControl = toggleRemoteControl;
window.undoSandboxMove = undoSandboxMove;

// Initialize timer system immediately (will retry if DOM not ready)
console.log('⏱️ Exposing timer functions and calling initTimerSystem()...');
try {
    initTimerSystem();
    console.log('✅ initTimerSystem() called successfully');
} catch (error) {
    console.error('❌ CRITICAL ERROR in initTimerSystem():', error);
    console.error('Stack:', error.stack);
}

// ============================================================================
// KEYBOARD SHORTCUTS AND EVENT HANDLERS
// ============================================================================

document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        if (reviewMode) {
            exitReviewMode();
        } else if (sandboxMode) {
            exitSandboxMode();
        } else {
            clearHighlights();
        }
    }
    if (historyData.length === 0) return;
    switch (e.key) {
        case 'ArrowLeft':
            e.preventDefault();
            if (reviewMode && currentReviewIndex > 0) {
                enterReviewMode(currentReviewIndex - 1);
            } else if (!reviewMode && !sandboxMode && historyData.length > 0) {
                enterReviewMode(historyData.length - 1);
            }
            break;
        case 'ArrowRight':
            e.preventDefault();
            if (reviewMode && currentReviewIndex < historyData.length - 1) {
                enterReviewMode(currentReviewIndex + 1);
            }
            break;
    }
});

// Click outside to deselect
document.addEventListener('click', (e) => {
    if (!e.target.closest('.square') && !e.target.closest('.history-item')) {
        if (!reviewMode) {
            clearHighlights();
        }
    }
});

// ============================================================================
// WIFI FUNCTIONS
// ============================================================================

async function saveWiFiConfig() {
    const ssid = document.getElementById('wifi-ssid').value;
    const password = document.getElementById('wifi-password').value;
    if (!ssid || !password) {
        alert('SSID and password are required');
        return;
    }
    try {
        const response = await fetch('/api/wifi/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ssid: ssid, password: password })
        });
        const data = await response.json();
        if (data.success) {
            alert('WiFi config saved. Now press "Connect STA".');
        } else {
            alert('Failed to save WiFi config: ' + data.message);
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

async function connectSTA() {
    try {
        const response = await fetch('/api/wifi/connect', { method: 'POST' });
        const data = await response.json();
        if (data.success) {
            alert('Connecting to WiFi...');
            setTimeout(updateWiFiStatus, 1500);
        } else {
            alert('Failed to connect: ' + data.message);
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

async function disconnectSTA() {
    try {
        const response = await fetch('/api/wifi/disconnect', { method: 'POST' });
        const data = await response.json();
        if (data.success) {
            alert('Disconnected from WiFi');
            setTimeout(updateWiFiStatus, 1000);
        } else {
            alert('Failed to disconnect: ' + data.message);
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

async function updateWiFiStatus() {
    try {
        const response = await fetch('/api/wifi/status');
        const data = await response.json();
        document.getElementById('ap-ssid').textContent = data.ap_ssid || 'ESP32-CzechMate';
        document.getElementById('ap-ip').textContent = data.ap_ip || '192.168.4.1';
        document.getElementById('ap-clients').textContent = data.ap_clients || 0;
        document.getElementById('sta-ssid').textContent = data.sta_ssid || 'Not configured';
        document.getElementById('sta-ip').textContent = data.sta_ip || 'Not connected';
        document.getElementById('sta-connected').textContent = data.sta_connected ? 'true' : 'false';
        if (data.sta_ssid && data.sta_ssid !== 'Not configured') {
            document.getElementById('wifi-ssid').value = data.sta_ssid;
        }
    } catch (error) {
        console.error('Failed to update WiFi status:', error);
    }
}

// Expose WiFi functions globally for inline onclick handlers
window.saveWiFiConfig = saveWiFiConfig;
window.connectSTA = connectSTA;
window.disconnectSTA = disconnectSTA;

// Start WiFi status update loop (every 5 seconds)
let wifiStatusInterval = null;
function startWiFiStatusUpdateLoop() {
    if (wifiStatusInterval) {
        clearInterval(wifiStatusInterval);
    }
    // Initial update
    updateWiFiStatus();
    // Update every 5 seconds
    wifiStatusInterval = setInterval(updateWiFiStatus, 10000); // Reduced from 5s to 10s
}

// Start WiFi status updates when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', startWiFiStatusUpdateLoop);
} else {
    startWiFiStatusUpdateLoop();
}

// ============================================================================
// DEMO MODE (SCREENSAVER) FUNCTIONS
// ============================================================================

/**
 * Toggle demo/screensaver mode on or off
 */
async function toggleDemoMode() {
    try {
        // Get current state
        const currentlyEnabled = await isDemoModeEnabled();
        const newState = !currentlyEnabled;

        // Send toggle request
        const response = await fetch('/api/demo/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled: newState })
        });

        const data = await response.json();

        if (data.success) {
            console.log('✅ Demo mode toggled:', newState ? 'ON' : 'OFF');
            // Update status immediately
            await updateDemoModeStatus();
        } else {
            console.error('❌ Failed to toggle demo mode');
            alert('Failed to toggle demo mode: ' + (data.message || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error toggling demo mode:', error);
        alert('Error toggling demo mode');
    }
}

/**
 * Check if demo mode is currently enabled
 * @returns {Promise<boolean>} True if enabled
 */
async function isDemoModeEnabled() {
    try {
        const response = await fetch('/api/demo/status');
        const data = await response.json();
        return data.enabled === true;
    } catch (error) {
        console.error('Failed to check demo mode status:', error);
        return false;
    }
}

/**
 * Update demo mode status indicator in UI
 */
async function updateDemoModeStatus() {
    try {
        const enabled = await isDemoModeEnabled();
        const statusEl = document.getElementById('demoStatus');
        const btnEl = document.getElementById('btnDemoMode');

        if (statusEl) {
            if (enabled) {
                statusEl.textContent = '🟢 Active';
                statusEl.style.color = '#4CAF50';
                statusEl.style.fontWeight = 'bold';
            } else {
                statusEl.textContent = '⚫ Off';
                statusEl.style.color = '#999';
                statusEl.style.fontWeight = 'normal';
            }
        }

        if (btnEl) {
            if (enabled) {
                btnEl.classList.add('btn-active');
                btnEl.style.backgroundColor = '#4CAF50';
                btnEl.style.borderColor = '#45a049';
            } else {
                btnEl.classList.remove('btn-active');
                btnEl.style.backgroundColor = '#008CBA';
                btnEl.style.borderColor = '#007396';
            }
        }
    } catch (error) {
        console.error('Failed to update demo mode status:', error);
    }
}

// Expose demo mode functions globally
window.toggleDemoMode = toggleDemoMode;
window.updateDemoModeStatus = updateDemoModeStatus;

// Start demo mode status update loop (every 3 seconds)
let demoModeStatusInterval = null;
function startDemoModeStatusUpdateLoop() {
    if (demoModeStatusInterval) {
        clearInterval(demoModeStatusInterval);
    }
    // Initial update
    updateDemoModeStatus();
    // Update every 3 seconds
    demoModeStatusInterval = setInterval(updateDemoModeStatus, 5000); // Reduced from 3s to 5s
}

// Start demo mode status updates when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', startDemoModeStatusUpdateLoop);
} else {
    startDemoModeStatusUpdateLoop();
}

// Helper functions for move history navigation

function goToMove(index) {
    if (!historyData || historyData.length === 0) return;

    // Special case: -1 means go to last move
    if (index === -1) {
        index = historyData.length - 1;
    }

    // Clamp index to valid range
    index = Math.max(0, Math.min(index, historyData.length - 1));

    enterReviewMode(index);
}

function prevReviewMove() {
    if (!reviewMode || currentReviewIndex <= 0) return;
    enterReviewMode(currentReviewIndex - 1);
}

function nextReviewMove() {
    if (!reviewMode || currentReviewIndex >= historyData.length - 1) return;
    enterReviewMode(currentReviewIndex + 1);
}
