// ============================================================================
// CHESS WEB APP - EXTRACTED JAVASCRIPT FOR SYNTAX CHECKING
// ============================================================================

console.log('🚀 Chess JavaScript loading...');

// ============================================================================
// TAB SWITCHING (Hra / Nastavení)
// ============================================================================

function switchTab(tabId) {
    const tabs = document.querySelectorAll('.tab-content');
    const buttons = document.querySelectorAll('.tab-btn');
    tabs.forEach(t => { t.classList.remove('active'); });
    buttons.forEach(b => { b.classList.remove('active'); });
    const tab = document.getElementById(tabId);
    const btn = document.getElementById('btn-' + tabId);
    if (tab) tab.classList.add('active');
    if (btn) btn.classList.add('active');
    if (typeof console !== 'undefined' && console.log) {
        console.log('Tab switched to:', tabId);
    }
}
window.switchTab = switchTab;

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
// BOT MODE STATE
let gameMode = 'pvp'; // 'pvp' or 'bot'
let botSettings = { strength: 10, side: 'white' }; // strength: 1,3,5,8,12,15 (zobrazeno jako ELO v Nastavení)
let botThinking = false;
let gameGeneration = 0; // Incremented on New Game to invalidate stale bot requests
/** FEN for which we already suggested a bot move; avoids re-triggering every poll until player moves. */
let lastSuggestedFen = null;
/** Last bot move we showed (from/to); re-sent to LED so game_task doesn't overwrite it. */
let lastSuggestedMove = null;
/** Interval ID for re-applying bot hint to LED (cleared when player moves or new game). */
let botHintRefreshIntervalId = null;

let sandboxBoard = [];
let sandboxHistory = [];
/** For move evaluation: FEN after last fetch; length of history after last fetch. */
let lastFen = null;
let lastHistoryLength = -1;
/** Per-move evaluation when "Zhodnocení tahu" is on: index -> { grade, msg }. */
let moveEvaluations = {};
let endgameReportShown = false;

/** Výukový režim: každý hráč má vlastní počet nápověd. */
let hintsRemainingWhite = 999;
let hintsRemainingBlack = 999;
/** Počet sebraných figur po minulém pollu (pro detekci sebrání). */
let lastCapturedCount = 0;
/** Poslední nápověda { from, to } – odměna za výborný tah se nedává, pokud byl tah stejný. */
var lastHintedMove = null;

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

// Remote control: one click = one action (pickup or drop), same as backup / physical board.
async function handleRemoteControlClick(row, col) {
    if (isWebLocked()) {
        alert('Rozhraní je zamčeno. Odemkněte přes UART.');
        return;
    }
    const notation = String.fromCharCode(97 + col) + (row + 1);
    const action = (statusData && statusData.piece_lifted && statusData.piece_lifted.lifted) ? 'drop' : 'pickup';
    const squareEl = document.querySelector(`[data-row='${row}'][data-col='${col}']`);

    if (squareEl) {
        squareEl.style.boxShadow = action === 'pickup'
            ? 'inset 0 0 20px rgba(255, 255, 0, 0.8)'
            : 'inset 0 0 20px rgba(0, 255, 0, 0.8)';
        setTimeout(function () { if (squareEl) squareEl.style.boxShadow = ''; }, 500);
    }
    console.log('Remote control:', action, 'at', notation);

    try {
        const response = await fetch('/api/game/virtual_action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: action, square: notation })
        });
        const res = await response.json().catch(function () { return {}; });
        if (!response.ok) {
            if (response.status === 403 && res.message) alert(res.message);
            else console.warn('Virtual action failed:', response.status, res.message);
        }
        await fetchData();
    } catch (e) {
        console.error('Remote virtual_action error:', e);
        await fetchData();
        alert('Chyba: ' + (e.message || 'nelze odeslat příkaz'));
    }
}

// ============================================================================
// SQUARE CLICK HANDLER
// ============================================================================

async function handleSquareClick(row, col) {
    const piece = sandboxMode ? sandboxBoard[row][col] : boardData[row][col];
    const index = row * 8 + col;

    // SANDBOX MODE (Zkusit tahy) - vždy jen lokálně, i když je zapnuté dálkové ovládání
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

    // REMOTE CONTROL MODE - posílat příkazy na ESP (jen když nejsme v sandboxu)
    if (remoteControlEnabled) {
        handleRemoteControlClick(row, col);
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
    document.getElementById('review-move-text').textContent = `Prohlížíš tah ${index + 1}`;
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

    undoBtn.textContent = `Undo (${availableUndos}/${maxUndos})`;
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
// HINT (STOCKFISH) - FEN and best move
// ============================================================================

/**
 * Build FEN from current board, status and history.
 * Board: row 0 = rank 1 (white back), row 7 = rank 8 (black back). FEN ranks 8..1.
 * Simplified: default castling KQkq, no en passant (sufficient for best-move).
 * Returns '' if board/status invalid or board not 8x8.
 */
function boardAndStatusToFen(board, status, history) {
    if (!board || !status || !Array.isArray(board) || board.length !== 8) return '';
    const rows = [];
    for (let r = 7; r >= 0; r--) {
        if (!board[r] || board[r].length !== 8) return '';
        let rank = '';
        let empty = 0;
        for (let c = 0; c < 8; c++) {
            const p = board[r][c];
            if (p === ' ' || p === '') {
                empty++;
            } else {
                if (empty) { rank += empty; empty = 0; }
                rank += p;
            }
        }
        if (empty) rank += empty;
        rows.push(rank);
    }
    const piecePlacement = rows.join('/');
    const sideToMove = (status.current_player === 'White') ? 'w' : 'b';
    const castling = 'KQkq';
    const ep = '-';
    const halfmove = (history && history.moves) ? history.moves.length : 0;
    const fullmove = Math.floor(halfmove / 2) + 1;
    const fen = piecePlacement + ' ' + sideToMove + ' ' + castling + ' ' + ep + ' 0 ' + fullmove;
    if (fen.length < 20 || fen.length > 120) return '';
    return fen;
}

/** Hint depth 1–18 from settings (localStorage). Used by fetchStockfishBestMove for hints. */
function getHintDepth() {
    try {
        var d = parseInt(localStorage.getItem('chessHintDepth') || '10', 10);
        d = isNaN(d) ? 10 : d;
        return Math.min(18, Math.max(1, d));
    } catch (e) {
        return 10;
    }
}

/** Depth for move evaluation (Zhodnocení tahu). Uses at least 12 so evals are meaningful; max 18. */
function getEvaluationDepth() {
    var hint = getHintDepth();
    return Math.min(18, Math.max(hint, 12));
}

/** Whether to show move evaluation after each move (localStorage). */
function getEvaluateMoveEnabled() {
    try {
        return localStorage.getItem('chessEvaluateMove') === 'true';
    } catch (e) {
        return false;
    }
}

/** Počet nápověd na partii (0 = neomezeno). */
function getHintLimit() {
    try {
        var n = parseInt(localStorage.getItem('chessHintLimit') || '0', 10);
        return isNaN(n) || n < 0 ? 0 : n;
    } catch (e) {
        return 0;
    }
}

/** Přidat nápovědu za výborný tah (localStorage). */
function getHintAwardBest() {
    try {
        return localStorage.getItem('chessHintAwardBest') !== 'false';
    } catch (e) {
        return true;
    }
}

/** Přidat nápovědu za dobrý tah (localStorage). */
function getHintAwardGood() {
    try {
        return localStorage.getItem('chessHintAwardGood') === 'true';
    } catch (e) {
        return false;
    }
}

/** Přidat nápovědu za sebrání figurky (localStorage). */
function getHintAwardCapture() {
    try {
        return localStorage.getItem('chessHintAwardCapture') === 'true';
    } catch (e) {
        return false;
    }
}

/** Zobrazit blok „Výukový přehled“ (nápovědy + kvalita tahů). */
function getShowHintStats() {
    try {
        return localStorage.getItem('chessShowHintStats') === 'true';
    } catch (e) {
        return false;
    }
}

function getCurrentPlayerHints() {
    var p = (statusData && statusData.current_player) ? statusData.current_player : 'White';
    return p === 'White' ? hintsRemainingWhite : hintsRemainingBlack;
}

function updateHintButtonLabel() {
    var btn = document.getElementById('hint-btn');
    if (!btn) return;
    var limit = getHintLimit();
    var onMove = (statusData && statusData.current_player) === 'Black' ? 'black' : 'white';
    if (limit > 0) {
        var w = hintsRemainingWhite, b = hintsRemainingBlack;
        var first = onMove === 'white' ? 'Bílý ' + w + ' | Černý ' + b : 'Černý ' + b + ' | Bílý ' + w;
        btn.textContent = 'Nápověda (' + first + ')';
        btn.disabled = (onMove === 'white' ? w : b) <= 0;
    } else {
        btn.textContent = 'Nápověda';
        btn.disabled = false;
    }
    if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
}

/** V režimu bota udělujeme odměny jen za tahy člověka (ne za tahy bota). */
function isHumanSideInBotMode(forSide) {
    if (gameMode !== 'bot' || !forSide) return true;
    return (botSettings.side === 'white' && forSide === 'black') || (botSettings.side === 'black' && forSide === 'white');
}

function addHintReward(reason, forSide) {
    var limit = getHintLimit();
    if (limit <= 0 || !forSide) return;
    if (gameMode === 'bot' && !isHumanSideInBotMode(forSide)) return;
    if (forSide === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
    updateHintButtonLabel();
    var who = forSide === 'white' ? 'Bílý' : 'Černý';
    var msg = reason === 'best' ? 'Výborný tah! ' + who + ' +1 nápověda.' : reason === 'good' ? 'Dobrý tah! ' + who + ' +1 nápověda.' : reason === 'capture' ? 'Sebrání figurky! ' + who + ' +1 nápověda.' : '';
    if (!msg) return;
    var el = document.getElementById('castling-pending-message');
    if (el) {
        el.textContent = msg;
        el.style.display = 'block';
        el.style.background = 'rgba(33, 150, 243, 0.2)';
        el.style.borderColor = '#2196F3';
        el.style.color = '#e0e0e0';
        setTimeout(function () {
            if (el.textContent === msg) el.style.display = 'none';
        }, 2500);
    }
}

/** Skóre kvality tahu pro průměr: best=5 … blunder=1, jinak 0. */
function gradeToScore(grade) {
    switch (grade) {
        case 'best': return 5;
        case 'good': return 4;
        case 'inaccuracy': return 3;
        case 'mistake': return 2;
        case 'blunder': return 1;
        default: return 0;
    }
}

/** Průměrná kvalita tahů hráče (side='white'|'black') za posledních lastN tahů toho hráče. Vrací číslo 1–5 nebo null. */
function getAverageGradeForPlayer(side, lastN) {
    var indices = [];
    var isWhite = (side === 'white');
    for (var i = (historyData.length || 0) - 1; i >= 0 && indices.length < lastN; i--) {
        if ((i % 2 === 0) === isWhite) indices.push(i);
    }
    if (indices.length === 0) return null;
    var sum = 0, count = 0;
    indices.forEach(function (idx) {
        var ev = moveEvaluations[idx];
        if (ev && ev.grade) {
            var s = gradeToScore(ev.grade);
            if (s > 0) { sum += s; count++; }
        }
    });
    if (count === 0) return null;
    return Math.round((sum / count) * 10) / 10;
}

/** Zobrazí nebo skryje blok Výukový přehled a naplní nápovědy + průměry kvality. */
function updateTeachingStatsPanel() {
    var panel = document.getElementById('teaching-stats-panel');
    if (!panel) return;
    if (!getShowHintStats()) {
        panel.style.display = 'none';
        return;
    }
    panel.style.display = 'block';
    var limit = getHintLimit();
    var wHints = limit > 0 ? hintsRemainingWhite : '—';
    var bHints = limit > 0 ? hintsRemainingBlack : '—';
    if (limit <= 0) {
        try {
            var elW = document.getElementById('teaching-stats-white-hints');
            var elB = document.getElementById('teaching-stats-black-hints');
            if (elW) elW.textContent = '—';
            if (elB) elB.textContent = '—';
        } catch (e) {}
    } else {
        var elW = document.getElementById('teaching-stats-white-hints');
        var elB = document.getElementById('teaching-stats-black-hints');
        if (elW) elW.textContent = String(hintsRemainingWhite);
        if (elB) elB.textContent = String(hintsRemainingBlack);
    }
    function fmtAvg(v) { return v != null ? v.toFixed(1) : '—'; }
    var w5 = getAverageGradeForPlayer('white', 5), w15 = getAverageGradeForPlayer('white', 15), wAll = getAverageGradeForPlayer('white', 9999);
    var b5 = getAverageGradeForPlayer('black', 5), b15 = getAverageGradeForPlayer('black', 15), bAll = getAverageGradeForPlayer('black', 9999);
    var el;
    el = document.getElementById('teaching-stats-white-avg5'); if (el) el.textContent = fmtAvg(w5);
    el = document.getElementById('teaching-stats-white-avg15'); if (el) el.textContent = fmtAvg(w15);
    el = document.getElementById('teaching-stats-white-avgAll'); if (el) el.textContent = fmtAvg(wAll);
    el = document.getElementById('teaching-stats-black-avg5'); if (el) el.textContent = fmtAvg(b5);
    el = document.getElementById('teaching-stats-black-avg15'); if (el) el.textContent = fmtAvg(b15);
    el = document.getElementById('teaching-stats-black-avgAll'); if (el) el.textContent = fmtAvg(bAll);
}
if (typeof window !== 'undefined') window.updateTeachingStatsPanel = updateTeachingStatsPanel;

/**
 * Fetch best move and explanation from Chess-API.com (POST).
 * Returns { from, to, eval, text, san, continuationArr, mate, winChance } or null.
 * depthOverride: optional; if number, use for API; else use getHintDepth() (for hints).
 */
async function fetchStockfishBestMove(fen, depthOverride) {
    var depth = (typeof depthOverride === 'number' && depthOverride >= 1 && depthOverride <= 18)
        ? depthOverride
        : getHintDepth();
    const TIMEOUT_MS = 15000;
    const API_URL = 'https://chess-api.com/v1';
    if (typeof console !== 'undefined' && console.log) console.log('[Hint] Stockfish request (chess-api.com), FEN length:', fen.length, 'depth:', depth);

    let controller = null;
    try {
        controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
        const timeoutId = controller ? setTimeout(function () { if (controller) controller.abort(); }, TIMEOUT_MS) : null;
        const opts = {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ fen: fen, depth: depth })
        };
        if (controller) opts.signal = controller.signal;
        const res = await fetch(API_URL, opts);
        if (controller && timeoutId) clearTimeout(timeoutId);

        if (!res.ok) {
            if (console.warn) console.warn('[Hint] Stockfish HTTP', res.status, res.statusText);
            return null;
        }
        const raw = await res.json().catch(function () { return null; });
        if (!raw) {
            if (console.warn) console.warn('[Hint] Stockfish invalid JSON');
            return null;
        }
        var data = raw && typeof raw.data === 'object' && raw.data !== null ? raw.data : (raw && typeof raw.result === 'object' && raw.result !== null ? raw.result : raw);
        var from = data.from;
        var to = data.to;
        if (typeof from !== 'string' || typeof to !== 'string' || from.length !== 2 || to.length !== 2) {
            var move = data.move;
            if (typeof move === 'string' && move.length >= 4) {
                from = move.substring(0, 2).toLowerCase();
                to = move.substring(2, 4).toLowerCase();
            } else {
                if (console.warn) console.warn('[Hint] Stockfish response missing from/to or move:', data);
                return null;
            }
        } else {
            from = from.toLowerCase();
            to = to.toLowerCase();
        }
        if (console.log) console.log('[Hint] Best move:', from, '->', to);
        var evalVal = null;
        function toPawns(v) {
            if (v == null || typeof v !== 'number' || isNaN(v)) return null;
            if (Math.abs(v) > 10) return v / 100;
            return v;
        }
        if (typeof data.eval === 'number') evalVal = toPawns(data.eval);
        if (evalVal == null && typeof data.eval === 'string') { var p = parseFloat(data.eval); if (!isNaN(p)) evalVal = toPawns(p); }
        if (evalVal == null && data.centipawns != null) {
            var cp = typeof data.centipawns === 'number' ? data.centipawns : parseInt(data.centipawns, 10);
            if (!isNaN(cp)) evalVal = cp / 100;
        }
        if (evalVal == null && data.cp != null) {
            var cp2 = typeof data.cp === 'number' ? data.cp : parseInt(data.cp, 10);
            if (!isNaN(cp2)) evalVal = cp2 / 100;
        }
        if (evalVal == null && data.evaluation != null) {
            var ev = typeof data.evaluation === 'number' ? data.evaluation : parseFloat(data.evaluation);
            if (!isNaN(ev)) evalVal = toPawns(ev);
        }
        if (evalVal == null && typeof data.score === 'number' && !isNaN(data.score)) evalVal = toPawns(data.score);
        if (evalVal == null && data.result != null && typeof data.result === 'object' && typeof data.result.eval === 'number') evalVal = toPawns(data.result.eval);
        if (typeof console !== 'undefined' && console.log) {
            console.log('[Eval Staging] API raw data.eval:', data.eval, 'data.centipawns:', data.centipawns, 'parsed evalVal (pawns):', evalVal);
        }
        return {
            from: from,
            to: to,
            eval: evalVal,
            text: typeof data.text === 'string' ? data.text : '',
            san: typeof data.san === 'string' ? data.san : (from + '-' + to),
            continuationArr: Array.isArray(data.continuationArr) ? data.continuationArr : [],
            mate: data.mate != null && typeof data.mate === 'number' ? data.mate : null,
            winChance: typeof data.winChance === 'number' ? data.winChance : null
        };
    } catch (err) {
        if (controller && controller.signal && controller.signal.aborted) {
            if (console.warn) console.warn('[Hint] Stockfish timeout');
        } else {
            if (console.error) console.error('[Hint] Stockfish error:', err.message);
        }
        return null;
    }
}

/** Show hint on board (hint-from, hint-to) and optionally send to LED. */
function showHintOnBoard(from, to) {
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('hint-from', 'hint-to');
    });
    const fromCol = from.charCodeAt(0) - 97;
    const fromRow = parseInt(from[1], 10) - 1;
    const toCol = to.charCodeAt(0) - 97;
    const toRow = parseInt(to[1], 10) - 1;
    const fromSquare = document.querySelector('[data-row="' + fromRow + '"][data-col="' + fromCol + '"]');
    const toSquare = document.querySelector('[data-row="' + toRow + '"][data-col="' + toCol + '"]');
    if (fromSquare) fromSquare.classList.add('hint-from');
    if (toSquare) toSquare.classList.add('hint-to');
}

/** Replace key English phrases from API text with Czech (for hint explanation). */
function hintTextToCzech(s) {
    if (!s || typeof s !== 'string') return '';
    var t = s
        .replace(/\bWhite is winning\b/gi, 'Bílý vyhrává')
        .replace(/\bBlack is winning\b/gi, 'Černý vyhrává')
        .replace(/\bWhite is better\b/gi, 'Bílý je lépe')
        .replace(/\bBlack is better\b/gi, 'Černý je lépe')
        .replace(/\bThe game is balanced\.?\b/gi, 'Hra je vyrovnaná.')
        .replace(/\bgame is balanced\.?\b/gi, 'hra je vyrovnaná.')
        .replace(/\bDepth \d+\b/gi, function (m) { return 'Hloubka ' + m.replace(/\D/g, ''); })
        .replace(/\bMove\s+/gi, 'Tah ');
    return t;
}

/** Format UCI move (e2e4) as e2–e4. */
function formatUciMove(uci) {
    if (!uci || uci.length < 4) return uci || '';
    return uci.substring(0, 2) + '–' + uci.substring(2, 4);
}

/** Build one short, child-friendly sentence for the hint (easy to read). */
function buildHintMessage(data) {
    var parts = [];
    var san = (data.san || (data.from + '–' + data.to)).trim();
    parts.push('Počítač radí: zahraj tah ' + san + '.');

    var e = data.eval;
    if (e != null && typeof e === 'number') {
        if (e > 0.3) parts.push('Bílý má trochu výhodu.');
        else if (e < -0.3) parts.push('Černý má trochu výhodu.');
        else parts.push('Teď je to vyrovnané.');
    }

    if (Array.isArray(data.continuationArr) && data.continuationArr.length > 0) {
        var first = data.continuationArr.slice(0, 4).map(formatUciMove).join(', ');
        parts.push('Pak můžeš hrát třeba ' + first + '.');
    }

    if (data.mate != null && typeof data.mate === 'number') {
        if (data.mate === 0) parts.push('Je mat!');
        else if (data.mate > 0) parts.push('Za ' + data.mate + ' tahů bude mat bílého!');
        else parts.push('Za ' + (-data.mate) + ' tahů bude mat černého!');
    }

    return parts.join(' ');
}

/** Show hint explanation block (one simple message for children). */
function showHintExplanation(data) {
    var el = document.getElementById('hint-explanation');
    if (!el) return;
    var msgEl = document.getElementById('hint-explanation-message');
    if (!msgEl) return;
    msgEl.textContent = buildHintMessage(data);
    el.style.display = 'block';
}

/** Show hint block with error/info message (např. žádný internet). */
function showHintError(message) {
    var el = document.getElementById('hint-explanation');
    if (!el) return;
    var msgEl = document.getElementById('hint-explanation-message');
    if (!msgEl) return;
    msgEl.textContent = message;
    el.style.display = 'block';
}

/** Hide hint explanation block (on new move / fetchData). */
function hideHintExplanation() {
    var el = document.getElementById('hint-explanation');
    if (el) el.style.display = 'none';
}

/** Grade: 'best' | 'good' | 'inaccuracy' | 'mistake' | 'blunder' | 'error' */
function showMoveEvaluation(text, grade) {
    var el = document.getElementById('move-evaluation');
    if (!el) return;
    var msgEl = document.getElementById('move-evaluation-message');
    if (msgEl) msgEl.textContent = text;
    grade = grade || 'good';
    el.className = 'move-evaluation move-evaluation--' + grade;
    el.style.display = 'block';
}

/** Hide move evaluation block. */
function hideMoveEvaluation() {
    var el = document.getElementById('move-evaluation');
    if (el) el.style.display = 'none';
}

/**
 * Evaluate the last played move: call API for position before and (if needed) after,
 * then show a short Czech message and barvu podle kvality (best=zelená, blunder=červená, …).
 */
/** Normalize UCI move to 4 chars (from+to) for comparison. */
function normalizeUci(from, to) {
    var s = ((from || '') + (to || '')).toLowerCase().replace(/[^a-h1-8]/g, '');
    return s.length >= 4 ? s.slice(0, 4) : s;
}

/** True pokud je hodnocení pro daný počet tahů stále platné (nebyla nová hra / další tah). */
function isEvaluationStillValid(historyLength) {
    return (historyData && historyData.length) === historyLength;
}

/** API vrací eval z pohledu strany na tahu; bez převodu je scoreDrop příliš malý a nikdy neukáže Chyba/Vážná chyba. */
var API_EVAL_SIDE_TO_MOVE = true;
function evalToWhitePerspective(fen, evalRaw) {
    if (fen == null || evalRaw == null || typeof evalRaw !== 'number') return evalRaw;
    if (!API_EVAL_SIDE_TO_MOVE) return evalRaw;
    var blackToMove = fen.indexOf(' b ') >= 0;
    if (blackToMove) {
        if (typeof console !== 'undefined' && console.log) console.log('[Eval Staging] converted to White perspective, raw:', evalRaw, '->', -evalRaw);
        return -evalRaw;
    }
    return evalRaw;
}

function evaluateMoveAsync(fenBefore, fenAfter, playedMove, historyLength) {
    if (!statusData.internet_connected) {
        showMoveEvaluation('Zhodnocení vyžaduje připojení k internetu (WiFi).', 'error');
        return;
    }
    var playedUci = normalizeUci(playedMove.from, playedMove.to);
    if (playedUci.length < 4) return;

    var hintedForThisMove = lastHintedMove;
    lastHintedMove = null;

    var evalDepth = getEvaluationDepth();
    fetchStockfishBestMove(fenBefore, evalDepth).then(function (beforeData) {
        if (!isEvaluationStillValid(historyLength)) return;
        if (!beforeData) {
            var errMsg = 'Zhodnocení nebylo k dispozici. Zkontrolujte připojení k internetu.';
            showMoveEvaluation(errMsg, 'error');
            moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };
            renderHistoryList();
            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
            return;
        }
        var bestUci = normalizeUci(beforeData.from, beforeData.to);
        if (playedUci === bestUci) {
            var msgBest = 'Výborný tah! Byl to nejlepší tah.';
            showMoveEvaluation(msgBest, 'best');
            moveEvaluations[historyLength - 1] = { grade: 'best', msg: msgBest };
            var sideBest = (historyLength - 1) % 2 === 0 ? 'white' : 'black';
            var hintedSameMove = hintedForThisMove && playedUci === normalizeUci(hintedForThisMove.from, hintedForThisMove.to);
            if (getHintAwardBest() && !hintedSameMove) addHintReward('best', sideBest);
            renderHistoryList();
            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
            return;
        }
        var bestFormatted = formatUciMove(bestUci);
        fetchStockfishBestMove(fenAfter, evalDepth).then(function (afterData) {
            if (!isEvaluationStillValid(historyLength)) return;
            if (!afterData) {
                var msgInacc = 'Lepší byl tah ' + bestFormatted + '.';
                showMoveEvaluation(msgInacc, 'inaccuracy');
                moveEvaluations[historyLength - 1] = { grade: 'inaccuracy', msg: msgInacc };
                renderHistoryList();
                if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
                return;
            }
            var hasEvalBefore = beforeData.eval != null && typeof beforeData.eval === 'number';
            var hasEvalAfter = afterData.eval != null && typeof afterData.eval === 'number';
            var evalBefore = hasEvalBefore ? evalToWhitePerspective(fenBefore, beforeData.eval) : null;
            var evalAfter = hasEvalAfter ? evalToWhitePerspective(fenAfter, afterData.eval) : null;
            var msg, grade;
            if (!hasEvalBefore || !hasEvalAfter) {
                if (typeof console !== 'undefined' && console.warn) {
                    console.warn('[Eval Staging] Missing eval – hasEvalBefore:', hasEvalBefore, 'hasEvalAfter:', hasEvalAfter, 'beforeData keys:', beforeData ? Object.keys(beforeData) : [], 'afterData keys:', afterData ? Object.keys(afterData) : []);
                }
                msg = 'Slabší tah. Lepší bylo ' + bestFormatted + '.';
                grade = 'inaccuracy';
            } else {
                var whiteJustMoved = (historyLength - 1) % 2 === 0;
                var scoreDrop = whiteJustMoved ? (evalBefore - evalAfter) : (evalAfter - evalBefore);
                if (scoreDrop < 0) scoreDrop = 0;
                if (scoreDrop <= 0.20) {
                    msg = 'Dobrý tah.';
                    grade = 'good';
                    var sideGood = (historyLength - 1) % 2 === 0 ? 'white' : 'black';
                    if (getHintAwardGood()) addHintReward('good', sideGood);
                } else if (scoreDrop <= 0.50) {
                    msg = 'Slabší tah. Lepší bylo ' + bestFormatted + '.';
                    grade = 'inaccuracy';
                } else if (scoreDrop <= 1.00) {
                    msg = 'Chyba. Pozice se zhoršila. Lepší bylo ' + bestFormatted + '.';
                    grade = 'mistake';
                } else {
                    msg = 'Vážná chyba. Lepší bylo ' + bestFormatted + '.';
                    grade = 'blunder';
                }
                if (typeof console !== 'undefined' && console.log) {
                    console.log('[Eval Staging] hasEvalBefore:', hasEvalBefore, 'hasEvalAfter:', hasEvalAfter, 'evalBefore:', evalBefore, 'evalAfter:', evalAfter, 'scoreDrop:', scoreDrop.toFixed(3), 'whiteJustMoved:', whiteJustMoved, 'grade:', grade);
                }
            }
            showMoveEvaluation(msg, grade);
            moveEvaluations[historyLength - 1] = { grade: grade, msg: msg };
            renderHistoryList();
            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
        }).catch(function () {
            if (!isEvaluationStillValid(historyLength)) return;
            var errMsg = 'Zhodnocení nebylo k dispozici. Zkontrolujte připojení k internetu.';
            showMoveEvaluation(errMsg, 'error');
            moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };
            renderHistoryList();
            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
        });
    }).catch(function () {
        if (!isEvaluationStillValid(historyLength)) return;
        var errMsg = 'Zhodnocení nebylo k dispozici. Zkontrolujte připojení k internetu.';
        showMoveEvaluation(errMsg, 'error');
        moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };
        renderHistoryList();
        if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
    });
}

function isWebLocked() {
    return !!(statusData && statusData.web_locked);
}

async function requestHint() {
    if (sandboxMode || reviewMode) return;
    if (isWebLocked()) {
        showHintError('Rozhraní je zamčeno. Odemkněte přes UART.');
        return;
    }
    const status = statusData || {};
    const state = (status.game_state || '').toLowerCase();
    if (state !== 'active' && state !== 'playing') return;
    if (status.game_end && status.game_end.ended) return;
    if (state === 'promotion') return;
    if (status.castling_in_progress) return;

    var limit = getHintLimit();
    var currentHints = getCurrentPlayerHints();
    if (limit > 0 && currentHints <= 0) {
        showHintError('Na tahu nemáte žádnou nápovědu. Získejte ji výborným tahem nebo sebráním figurky.');
        return;
    }

    const btn = document.getElementById('hint-btn');
    if (btn) {
        btn.disabled = true;
        btn.textContent = 'Načítám…';
    }

    if (limit > 0) {
        var p = (status.current_player === 'Black') ? 'black' : 'white';
        if (p === 'white') hintsRemainingWhite--; else hintsRemainingBlack--;
        updateHintButtonLabel();
    }

    if (!status.internet_connected) {
        if (limit > 0) {
            var p0 = (status.current_player === 'Black') ? 'black' : 'white';
            if (p0 === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
            updateHintButtonLabel();
        }
        showHintError('Nápověda vyžaduje připojení k internetu (WiFi).');
        if (btn) updateHintButtonLabel();
        return;
    }

    try {
        const fen = boardAndStatusToFen(boardData, status, historyData);
        if (!fen) {
            if (limit > 0) {
                var pFen = (status.current_player === 'Black') ? 'black' : 'white';
                if (pFen === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
                updateHintButtonLabel();
            }
            if (console.warn) console.warn('[Hint] Could not build FEN');
            showHintError('Nelze načíst pozici. Obnovte stránku.');
            if (btn) updateHintButtonLabel();
            return;
        }
        const move = await fetchStockfishBestMove(fen);
        if (move) {
            showHintOnBoard(move.from, move.to);
            showHintExplanation(move);
            lastHintedMove = { from: move.from, to: move.to };
            try {
                await fetch('/api/game/hint_highlight', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ from: move.from, to: move.to })
                });
            } catch (e) {
                if (console.warn) console.warn('[Hint] LED highlight failed:', e.message);
            }
            if (btn) updateHintButtonLabel();
        } else {
            if (limit > 0) {
                var p = (status.current_player === 'Black') ? 'black' : 'white';
                if (p === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
                updateHintButtonLabel();
            }
            showHintError('Nápověda není k dispozici. Zkuste později nebo zkontrolujte připojení k internetu.');
            if (btn) updateHintButtonLabel();
        }
    } catch (err) {
        if (limit > 0) {
            var p = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';
            if (p === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
            updateHintButtonLabel();
        }
        if (console.error) console.error('[Hint] requestHint error:', err.message);
        showHintError('Nápověda není k dispozici. Zkuste později nebo zkontrolujte připojení k internetu.');
        if (btn) updateHintButtonLabel();
    }
}
window.requestHint = requestHint;

// ============================================================================
// UPDATE FUNCTIONS
// ============================================================================

function updateBoard(board) {
    // Only clear hint when board actually changed (new move), not on every periodic fetch
    var boardUnchanged = boardData && board.length === 8 && boardData.length === 8 &&
        JSON.stringify(board) === JSON.stringify(boardData);
    boardData = board;
    const loading = document.getElementById('loading');
    if (loading) loading.style.display = 'none';

    if (!boardUnchanged) {
        document.querySelectorAll('.square').forEach(sq => {
            sq.classList.remove('hint-from', 'hint-to');
        });
        hideHintExplanation();
    }

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
    let title = '';
    let subtitle = '';
    let accentColor = '#4CAF50';
    let bgGradient = 'linear-gradient(135deg, #1e3a1e, #2d4a2d)';

    if (gameEnd.winner === 'Draw') {
        title = 'REMÍZA';
        subtitle = gameEnd.reason;
        accentColor = '#FF9800';
        bgGradient = 'linear-gradient(135deg, #3a2e1e, #4a3e2d)';
    } else {
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
            <h2 style="margin:0;color:white;font-size:24px;font-weight:700;text-shadow:0 2px 4px rgba(0,0,0,0.4);">${title}</h2>
            <p style="margin:8px 0 0 0;color:rgba(255,255,255,0.9);font-size:14px;font-weight:500;">${subtitle}</p>
        </div>
        <div style="padding:20px;">
            ${graphSVG ? `
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;">
                    Průběh hry
                </h3>
                ${graphSVG}
                <div style="display:flex;justify-content:space-between;margin-top:8px;font-size:11px;color:#888;">
                    <span>Začátek</span>
                    <span>Tah ${advantageDataLocal.count || 0}</span>
                </div>
            </div>` : ''}
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;">
                    Statistiky
                </h3>
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;font-size:13px;">
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Tahy</div>
                        <div style="color:#e0e0e0;font-weight:600;">Bílý ${whiteMoves} | Černý ${blackMoves}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Materiál</div>
                        <div style="color:${accentColor};font-weight:600;">${materialText}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Sebráno</div>
                        <div style="color:#e0e0e0;font-weight:600;">Bílý ${whiteCaptured.length} | Černý ${blackCaptured.length}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Celkem</div>
                        <div style="color:#e0e0e0;font-weight:600;">${statusData.move_count} tahů</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Šachy</div>
                        <div style="color:#e0e0e0;font-weight:600;">Bílý ${advantageDataLocal.white_checks || 0} | Černý ${advantageDataLocal.black_checks || 0}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Rošády</div>
                        <div style="color:#e0e0e0;font-weight:600;">Bílý ${advantageDataLocal.white_castles || 0} | Černý ${advantageDataLocal.black_castles || 0}</div>
                    </div>
                </div>
            </div>
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;">
                    Sebrané figurky
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
                OK
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
    button.innerHTML = 'Zpráva';
    button.title = 'Zobrazit/skrýt zprávu o konci hry';
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
// BOT LOGIC
// ============================================================================

const BOT_API_TIMEOUT_MS = 15000;
const BOT_SQUARE_REGEX = /^[a-h][1-8]$/;
const BOT_HINT_REFRESH_MS = 500;

function stopBotHintRefresh() {
    if (botHintRefreshIntervalId) {
        clearInterval(botHintRefreshIntervalId);
        botHintRefreshIntervalId = null;
    }
}

async function fetchStockfishBestMove(fen) {
    const depth = parseInt(botSettings.strength, 10) || 10;
    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
    const timeoutId = controller ? setTimeout(function () { controller.abort(); }, BOT_API_TIMEOUT_MS) : null;
    try {
        const opts = {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ fen: fen, depth: depth })
        };
        if (controller && controller.signal) opts.signal = controller.signal;
        const res = await fetch('https://chess-api.com/v1', opts);
        if (!res.ok) {
            if (console.warn) console.warn('Bot API HTTP', res.status, res.statusText);
            return null;
        }
        const data = await res.json().catch(function () { return null; });
        if (!data) {
            if (console.warn) console.warn('Bot API: invalid JSON');
            return null;
        }
        var from, to;
        if (data.from != null && data.to != null) {
            from = String(data.from).toLowerCase();
            to = String(data.to).toLowerCase();
        } else if (data.move && data.move.length >= 4) {
            from = data.move.substring(0, 2).toLowerCase();
            to = data.move.substring(2, 4).toLowerCase();
        }
        if (from && to && BOT_SQUARE_REGEX.test(from) && BOT_SQUARE_REGEX.test(to)) return { from: from, to: to };
        if (from || to) {
            if (console.warn) console.warn('Bot API: neplatný formát pole', data);
        } else if (console.warn) {
            console.warn('Bot API: missing from/to or move', data);
        }
        return null;
    } catch (e) {
        if (e && e.name === 'AbortError') {
            if (console.warn) console.warn('Bot API: timeout after', BOT_API_TIMEOUT_MS, 'ms');
        } else {
            console.error('Bot fetch error:', e);
        }
        return null;
    } finally {
        if (timeoutId) clearTimeout(timeoutId);
    }
}

async function playBotMove(fen, generation) {
    if (botThinking || !fen || typeof fen !== 'string') return;
    botThinking = true;
    var msgEl = document.getElementById('castling-pending-message');
    var statusEl = document.getElementById('game-state');

    try {
        if (statusEl) statusEl.textContent = 'Počítač přemýšlí...';
        console.log('🤖 Bot starts thinking... Generation:', generation);
        var move = await fetchStockfishBestMove(fen);

        if (generation !== gameGeneration) {
            console.warn('🤖 Bot move aborted: Game generation changed (New game started).');
            lastSuggestedMove = null;
            stopBotHintRefresh();
            return;
        }

        if (move) {
            console.log('🤖 Bot plays:', move.from, '->', move.to, '| FEN:', fen.length > 20 ? fen.substring(0, 30) + '...' : fen, '| mode:', gameMode, '| gen:', generation);
            lastSuggestedMove = { from: move.from, to: move.to };
            stopBotHintRefresh();
            try {
                await fetch('/api/game/hint_highlight', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ from: move.from, to: move.to })
                });
            } catch (e) {
                console.error('Failed to light up LEDs for bot:', e);
            }
            botHintRefreshIntervalId = setInterval(function () {
                if (!lastSuggestedMove) {
                    stopBotHintRefresh();
                    return;
                }
                fetch('/api/game/hint_highlight', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ from: lastSuggestedMove.from, to: lastSuggestedMove.to })
                }).catch(function () {});
            }, BOT_HINT_REFRESH_MS);
            if (msgEl) {
                msgEl.textContent = 'Bot hraje: ' + move.from + ' → ' + move.to;
                msgEl.style.display = 'block';
                msgEl.style.background = 'rgba(76, 175, 80, 0.2)';
                msgEl.style.borderColor = '#4CAF50';
                msgEl.style.color = '#e0e0e0';
            }
            showHintOnBoard(move.from, move.to);
        } else {
            console.warn('Bot: žádný tah (API chyba nebo timeout).');
            if (typeof console !== 'undefined' && console.warn) {
                console.warn('[Bot] API failed, clearing lastSuggestedFen for retry');
            }
            lastSuggestedFen = null;
            if (msgEl) {
                msgEl.textContent = 'Počítač nemohl najít tah (zkontrolujte internet).';
                msgEl.style.display = 'block';
                msgEl.style.background = 'rgba(244, 67, 54, 0.2)';
                msgEl.style.borderColor = '#f44336';
                msgEl.style.color = '#e0e0e0';
                setTimeout(function () {
                    if (msgEl.textContent.indexOf('nemohl najít tah') !== -1) msgEl.style.display = 'none';
                }, 5000);
            }
        }
    } finally {
        botThinking = false;
    }
}

function checkBotTurn(status, fen) {
    if (gameMode !== 'bot') {
        lastSuggestedFen = null;
        lastSuggestedMove = null;
        stopBotHintRefresh();
        document.querySelectorAll('.square').forEach(function (sq) { sq.classList.remove('hint-from', 'hint-to'); });
        return;
    }
    if (!status || typeof status !== 'object') return;
    if (status.game_state !== 'active' && status.game_state !== 'playing') return;
    if (status.game_end && status.game_end.ended) return;
    if (status.current_player !== 'White' && status.current_player !== 'Black') return;
    if (!fen || typeof fen !== 'string' || fen.length < 20) return;

    var isBotTurn = (botSettings.side === 'white') !== (status.current_player === 'White');
    if (typeof console !== 'undefined' && console.log) console.log('checkBotTurn: isBotTurn=', isBotTurn, 'current_player=', status.current_player);

    var msgEl = document.getElementById('castling-pending-message');
    if (!isBotTurn) {
        lastSuggestedFen = null;
        lastSuggestedMove = null;
        stopBotHintRefresh();
        document.querySelectorAll('.square').forEach(function (sq) { sq.classList.remove('hint-from', 'hint-to'); });
        if (msgEl && (msgEl.textContent.indexOf('Bot hraje') !== -1 || msgEl.textContent.indexOf('Počítač') !== -1)) msgEl.style.display = 'none';
    } else if (!botThinking && fen !== lastSuggestedFen) {
        if (typeof console !== 'undefined' && console.log) {
            console.log('[Bot Staging] checkBotTurn: game_state=', status.game_state, 'fen.length=', fen.length, 'calling playBotMove');
        }
        lastSuggestedFen = fen;
        playBotMove(fen, gameGeneration);
    } else if (isBotTurn && fen === lastSuggestedFen && typeof console !== 'undefined' && console.log) {
        console.log('[Bot Staging] checkBotTurn: skipping (fen === lastSuggestedFen), game_state=', status.game_state);
    }
}

// ============================================================================
// STATUS UPDATE FUNCTION
// ============================================================================

function updateStatus(status) {
    statusData = status;
    // ... existing implementation ...
    const gameStateEl = document.getElementById('game-state');
    const playerEl = document.getElementById('current-player');
    if (gameStateEl) gameStateEl.textContent = status.game_state || '-';
    if (playerEl) {
        const p = status.current_player;
        playerEl.textContent = (p === 'White') ? 'Bílý' : (p === 'Black') ? 'Černý' : (p || '-');
    }
    document.getElementById('move-count').textContent = status.move_count || 0;
    document.getElementById('in-check').textContent = status.in_check ? 'Ano' : 'Ne';

    // Jas (Nastavení) – synchronizovat slider a label ze statusu
    const b = status.brightness;
    if (typeof b === 'number' && b >= 0 && b <= 100) {
        const valueEl = document.getElementById('brightness-value');
        const sliderEl = document.getElementById('brightness-slider');
        if (valueEl) valueEl.textContent = b + '%';
        if (sliderEl && Number(sliderEl.value) !== b) sliderEl.value = b;
    }

    // Promotion modal – zobrazit, když backend čeká na volbu promoce (game_state === "promotion")
    const promoModal = document.getElementById('promotion-modal');
    if (promoModal) {
        if (status.game_state === 'promotion') {
            promoModal.style.display = 'flex';
        } else {
            promoModal.style.display = 'none';
        }
    }

    // ERROR STATE
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('error-invalid', 'error-original');
    });

    // LIFTED PIECE
    document.querySelectorAll('.square').forEach(sq => {
        sq.classList.remove('lifted');
    });

    const lifted = status.piece_lifted;
    const liftedPieceEl = document.getElementById('lifted-piece');
    const liftedPosEl = document.getElementById('lifted-position');
    if (lifted && lifted.lifted) {
        if (liftedPieceEl) liftedPieceEl.textContent = pieceSymbols[lifted.piece] || '-';
        if (liftedPosEl) liftedPosEl.textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);
        const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);
        if (square) square.classList.add('lifted');
    } else {
        if (liftedPieceEl) liftedPieceEl.textContent = '-';
        if (liftedPosEl) liftedPosEl.textContent = '-';
    }

    // Error state classes
    if (status.error_state && status.error_state.active) {
        if (status.error_state.invalid_pos) {
            const invalidCol = status.error_state.invalid_pos.charCodeAt(0) - 97;
            const invalidRow = parseInt(status.error_state.invalid_pos[1]) - 1;
            const invalidSquare = document.querySelector(`[data-row='${invalidRow}'][data-col='${invalidCol}']`);
            if (invalidSquare) invalidSquare.classList.add('error-invalid');
        }
        if (status.error_state.original_pos) {
            const originalCol = status.error_state.original_pos.charCodeAt(0) - 97;
            const originalRow = parseInt(status.error_state.original_pos[1]) - 1;
            const originalSquare = document.querySelector(`[data-row='${originalRow}'][data-col='${originalCol}']`);
            if (originalSquare) originalSquare.classList.add('error-original');
        }
    }

    // Castling message vs Bot message
    var castlingMsg = document.getElementById('castling-pending-message');
    if (castlingMsg) {
        // Prioritize Castling Message over Bot Message
        if (status.castling_in_progress && status.castling_from && status.castling_to) {
            castlingMsg.textContent = 'Dokončete rošádu: přesuňte věž z ' + status.castling_from + ' na ' + status.castling_to + '.';
            castlingMsg.style.display = 'block';
            // Reset style to warning for castling
            castlingMsg.style.background = 'rgba(255,193,7,0.12)';
            castlingMsg.style.borderColor = 'rgba(255,193,7,0.4)';
            castlingMsg.style.color = '#ffc107';
        } else {
            // If NOT castling, check if we should keep Bot Message or hide it
            // Bot logic handles showing/hiding bot message, so we only hide if NO bot message logic is active
            // But simpler: just hide if not castling AND not bot thinking/turn (handled in checkBotTurn)
            var keepMsg = castlingMsg.textContent.indexOf('Bot') !== -1 || castlingMsg.textContent.indexOf('Losování:') === 0;
            if (!keepMsg) castlingMsg.style.display = 'none';
        }
    }

    // ENDGAME REPORT
    if (status.game_end && status.game_end.ended) {
        window.lastGameEndData = status.game_end;
        if (!endgameReportShown) {
            console.log('Game ended, showing endgame report...');
            showEndgameReport(status.game_end);
        }
        showEndgameToggleButton();
    } else {
        if (endgameReportShown) {
            console.log('Game restarted, clearing endgame report...');
            hideEndgameReport();
        }
        endgameReportShown = false;
        window.lastGameEndData = null;
        hideEndgameToggleButton();
    }

    // Web lock: zakázat Nová hra a Nápověda při zamčení
    var locked = !!(status.web_locked);
    var newGameBtn = document.getElementById('new-game-btn');
    var hintBtn = document.getElementById('hint-btn');
    if (newGameBtn) newGameBtn.disabled = locked;
    if (hintBtn) {
        if (locked) {
            hintBtn.disabled = true;
            hintBtn.title = 'Rozhraní je zamčeno';
        } else {
            if (typeof updateHintButtonLabel === 'function') updateHintButtonLabel();
        }
    }
}

function getGradeLabel(grade) {
    switch (grade) {
        case 'best': return 'Výborný';
        case 'good': return 'Dobrý';
        case 'inaccuracy': return 'Slabší';
        case 'mistake': return 'Chyba';
        case 'blunder': return 'Vážná chyba';
        case 'unknown': return '—';
        case 'error': return 'Chyba';
        default: return '—';
    }
}

function renderHistoryList() {
    const historyBox = document.getElementById('history');
    if (!historyBox) return;
    historyBox.innerHTML = '';
    const showEval = getEvaluateMoveEnabled();
    historyData.slice().reverse().forEach((move, index) => {
        const item = document.createElement('div');
        item.className = 'history-item';
        const actualIndex = historyData.length - 1 - index;
        item.dataset.moveIndex = actualIndex;
        const moveNum = Math.floor(actualIndex / 2) + 1;
        const isWhite = actualIndex % 2 === 0;
        const prefix = isWhite ? moveNum + '. ' : '';
        item.textContent = prefix + move.from + ' → ' + move.to;
        if (showEval && moveEvaluations[actualIndex]) {
            const ev = moveEvaluations[actualIndex];
            const badge = document.createElement('span');
            badge.className = 'move-eval-badge move-eval-badge--' + (ev.grade || 'good');
            badge.title = ev.msg || getGradeLabel(ev.grade);
            badge.textContent = getGradeLabel(ev.grade);
            item.appendChild(document.createTextNode(' '));
            item.appendChild(badge);
        }
        item.onclick = () => enterReviewMode(actualIndex);
        historyBox.appendChild(item);
    });
}

function updateHistory(history) {
    historyData = history.moves || [];
    renderHistoryList();
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

        var newHistoryLength = (history.moves && history.moves.length) ? history.moves.length : 0;
        var currentFen = boardAndStatusToFen(boardData, status, history);

        if (newHistoryLength === 0) {
            lastFen = null;
            lastHistoryLength = 0;
            moveEvaluations = {};
            lastCapturedCount = (capturedData.white_captured || []).length + (capturedData.black_captured || []).length;
            hideMoveEvaluation();
        } else {
            if (!getEvaluateMoveEnabled()) {
                hideMoveEvaluation();
            }
            var castlingInProgress = status.castling_in_progress === true;
            if (getEvaluateMoveEnabled() && !castlingInProgress && lastHistoryLength >= 0 && newHistoryLength === lastHistoryLength + 1 && lastFen) {
                var lastMove = historyData[newHistoryLength - 1];
                var lastMoveByWhite = (newHistoryLength - 1) % 2 === 0;
                var lastMoveByBot = gameMode === 'bot' && ((botSettings.side === 'white' && lastMoveByWhite) || (botSettings.side === 'black' && !lastMoveByWhite));
                if (lastMove && lastMove.from && lastMove.to && !lastMoveByBot) {
                    evaluateMoveAsync(lastFen, currentFen, lastMove, newHistoryLength);
                }
            }
            var totalCaptured = (capturedData.white_captured || []).length + (capturedData.black_captured || []).length;
            if (newHistoryLength === lastHistoryLength + 1 && totalCaptured > lastCapturedCount && getHintAwardCapture()) {
                var sideCapture = status.current_player === 'White' ? 'black' : 'white';
                addHintReward('capture', sideCapture);
            }
            lastCapturedCount = totalCaptured;
            if (!castlingInProgress) {
                lastFen = currentFen;
                lastHistoryLength = newHistoryLength;
            }
        }
        checkBotTurn(status, currentFen);
    } catch (error) {
        console.error('Fetch error:', error);
    }
}

function initializeApp() {
    console.log('Initializing Chess App...');
    createBoard();

    var depthEl = document.getElementById('hint-depth');
    if (depthEl) {
        var d = getHintDepth();
        depthEl.value = d;
    }
    var evaluateMoveEl = document.getElementById('evaluate-move-enabled');
    if (evaluateMoveEl) evaluateMoveEl.checked = getEvaluateMoveEnabled();

    var limit = getHintLimit();
    var n = limit > 0 ? limit : 999;
    hintsRemainingWhite = n;
    hintsRemainingBlack = n;
    lastCapturedCount = 0;
    updateHintButtonLabel();

    fetchData();
    setInterval(fetchData, 2000); // Reduced from 500ms to 2s (4× fewer requests)
    console.log('✅ Chess App initialized');
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
        showTimeWarning('Kritické! Méně než 5 sekund!', 'critical');
    } else if (currentPlayerTime < 10000 && !timerInfo.warning_10s_shown) {
        showTimeWarning('Varování! Méně než 10 sekund!', 'warning');
    } else if (currentPlayerTime < 30000 && !timerInfo.warning_30s_shown) {
        showTimeWarning('Málo času! Méně než 30 sekund!', 'info');
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

    const expiredPlayer = timerInfo.is_white_turn ? 'Bílý' : 'Černý';
    showTimeWarning('Čas vypršel! ' + expiredPlayer + ' prohrál na čas.', 'critical');
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
        if (minutes < 1 || minutes > 180) { alert('Minuty musí být 1–180'); return; }
        if (increment < 0 || increment > 60) { alert('Increment musí být 0–60 sekund'); return; }
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
            showTimeWarning('Časová kontrola nastavena.', 'info');
            const applyBtn = document.getElementById('apply-time-control');
            if (applyBtn) applyBtn.disabled = true;
        } else {
            const errorText = await response.text();
            console.error('Failed to apply time control:', response.status, errorText);
            throw new Error('Failed to apply time control: ' + errorText);
        }
    } catch (error) {
        console.error('Error applying time control:', error);
        showTimeWarning('Chyba nastavení časové kontroly: ' + error.message, 'critical');
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
            showTimeWarning('Časomíra pozastavena', 'info');
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
            showTimeWarning('Časomíra pokračuje', 'info');
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
                showTimeWarning('Časomíra resetována', 'info');
                console.log('✅ Timer reset successfully');
                await updateTimerDisplay();
            }
        } catch (error) {
            console.error('❌ Error resetting timer:', error);
        }
    }
}

// ============================================================================
// BRIGHTNESS (Nastavení → Zařízení) – for inline onchange on brightness-slider
// ============================================================================

async function setBrightness(value) {
    const num = Math.min(100, Math.max(0, parseInt(value, 10)));
    if (Number.isNaN(num)) return;
    try {
        const response = await fetch('/api/settings/brightness', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ brightness: num })
        });
        const data = response.ok ? await response.json().catch(() => ({})) : {};
        if (data.success !== false) {
            if (typeof console !== 'undefined' && console.log) console.log('Jas nastaven na', num + '%');
        } else {
            console.warn('Nastavení jasu selhalo:', data.message || response.status);
        }
    } catch (err) {
        console.error('Chyba nastavení jasu:', err.message);
    }
}
window.setBrightness = setBrightness;

// ============================================================================
// NEW GAME (Nastavení → action bar „Nová hra“) – for inline onclick
// ============================================================================

function getConfirmNewGameEnabled() {
    const cb = document.getElementById('confirm-new-game');
    if (cb) return cb.checked;
    try {
        return localStorage.getItem('chess_confirm_new_game') === 'true';
    } catch (e) {
        return false;
    }
}

async function startNewGame() {
    if (isWebLocked()) {
        alert('Rozhraní je zamčeno. Odemkněte přes UART.');
        return;
    }
    if (getConfirmNewGameEnabled()) {
        if (!confirm('Opravdu chcete začít novou hru? Aktuální partie bude ukončena.')) {
            return;
        }
    }

    // UPDATE BOT SETTINGS FROM UI
    const modeEl = document.getElementById('game-mode');
    const strengthEl = document.getElementById('bot-strength');
    const sideEl = document.getElementById('player-side');

    if (modeEl) gameMode = modeEl.value;

    botSettings.strength = (strengthEl) ? strengthEl.value : 10;
    let sidePref = (sideEl) ? sideEl.value : 'white';

    if (sidePref === 'random') {
        sidePref = (Math.random() < 0.5) ? 'white' : 'black';
        console.log('Random side (fallback):', sidePref);
    }
    botSettings.side = sidePref;

    botThinking = false; // Reset bot state
    lastSuggestedFen = null;
    lastSuggestedMove = null;
    stopBotHintRefresh();
    var limit = getHintLimit();
    var n = limit > 0 ? limit : 999;
    hintsRemainingWhite = n;
    hintsRemainingBlack = n;
    lastCapturedCount = 0;
    lastHintedMove = null;
    updateHintButtonLabel();
    gameGeneration++; // INVALIDATE OLD BOT REQUESTS
    console.log('Starting New Game. Generation:', gameGeneration);

    try {
        const response = await fetch('/api/game/new', { method: 'POST' });
        if (response.ok) {
            if (typeof console !== 'undefined' && console.log) console.log('Nová hra spuštěna. Mode:', gameMode, 'Player Side:', botSettings.side);

            await fetchData();

            if (gameMode === 'bot' && botSettings.side === 'black') {
                // Player is Black -> Bot is White -> Bot moves immediately
                // We pass initial FEN (start pos)
                const startFen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';
                setTimeout(() => checkBotTurn({ current_player: 'White', game_state: 'active' }, startFen), 500);
            }
        } else {
            console.warn('Nová hra selhala:', response.status);
        }
    } catch (err) {
        console.error('Chyba startNewGame:', err.message);
    }
}
window.startNewGame = startNewGame;

function handleRandomDraw() {
    var sideEl = document.getElementById('player-side');
    var msgEl = document.getElementById('random-draw-result');
    if (!sideEl) return;
    if (sideEl.value !== 'random') {
        if (msgEl) {
            msgEl.style.display = 'none';
            msgEl.textContent = '';
        }
        return;
    }
    var drawn = Math.random() < 0.5 ? 'white' : 'black';
    sideEl.value = drawn;
    if (msgEl) {
        msgEl.textContent = drawn === 'white' ? 'Losování: Hrajete za bílého.' : 'Losování: Hrajete za černého.';
        msgEl.style.display = 'block';
    }
    if (typeof saveBotSettings === 'function') saveBotSettings();
}
window.handleRandomDraw = handleRandomDraw;

(function initConfirmNewGamePref() {
    function apply() {
        var cb = document.getElementById('confirm-new-game');
        if (!cb) return;
        try {
            var saved = localStorage.getItem('chess_confirm_new_game');
            cb.checked = saved === 'true';
        } catch (e) { }
        cb.addEventListener('change', function () {
            try {
                localStorage.setItem('chess_confirm_new_game', cb.checked);
            } catch (e) { }
        });
    }
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', apply);
    } else {
        apply();
    }
})();

// ============================================================================
// PROMOTION MODAL (Q/R/B/N a Zrušit) – for inline onclick
// ============================================================================

async function selectPromotion(choice) {
    const modal = document.getElementById('promotion-modal');
    if (modal) modal.style.display = 'none';
    try {
        const body = JSON.stringify({ action: 'promote', choice: String(choice).toUpperCase().slice(0, 1) });
        const response = await fetch('/api/game/virtual_action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: body
        });
        if (response.ok) await fetchData();
    } catch (err) {
        console.error('Chyba selectPromotion:', err.message);
    }
}

function cancelPromotion() {
    const modal = document.getElementById('promotion-modal');
    if (modal) modal.style.display = 'none';
    // Odblokovat hru výchozí volbou (Dáma)
    selectPromotion('Q');
}
window.selectPromotion = selectPromotion;
window.cancelPromotion = cancelPromotion;

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
        alert('SSID a heslo jsou povinné');
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
            alert('WiFi uloženo. Stiskněte „Připojit STA“.');
        } else {
            alert('Uložení WiFi selhalo: ' + data.message);
        }
    } catch (error) {
        alert('Chyba: ' + error.message);
    }
}

async function connectSTA() {
    try {
        const response = await fetch('/api/wifi/connect', { method: 'POST' });
        const data = await response.json();
        if (data.success) {
            alert('Připojování k WiFi...');
            setTimeout(updateWiFiStatus, 1500);
        } else {
            alert('Připojení selhalo: ' + data.message);
        }
    } catch (error) {
        alert('Chyba: ' + error.message);
    }
}

async function disconnectSTA() {
    try {
        const response = await fetch('/api/wifi/disconnect', { method: 'POST' });
        const data = await response.json();
        if (data.success) {
            alert('Odpojeno od WiFi');
            setTimeout(updateWiFiStatus, 1000);
        } else {
            alert('Odpojení selhalo: ' + data.message);
        }
    } catch (error) {
        alert('Chyba: ' + error.message);
    }
}

async function clearWiFiConfig() {
    if (!confirm('Opravdu smazat uloženou WiFi konfiguraci? ESP se odpojí od sítě.')) {
        return;
    }
    try {
        const response = await fetch('/api/wifi/clear', { method: 'POST' });
        const data = await response.json();
        if (data.success) {
            alert('WiFi konfigurace smazána.');
            setTimeout(updateWiFiStatus, 500);
        } else {
            alert('Smazání selhalo: ' + (data.message || 'neznámá chyba'));
        }
    } catch (error) {
        alert('Chyba: ' + error.message);
    }
}

async function updateWiFiStatus() {
    try {
        const response = await fetch('/api/wifi/status');
        const data = await response.json();
        document.getElementById('ap-ssid').textContent = data.ap_ssid || 'ESP32-CzechMate';
        document.getElementById('ap-ip').textContent = data.ap_ip || '192.168.4.1';
        document.getElementById('ap-clients').textContent = data.ap_clients || 0;
        document.getElementById('sta-ssid').textContent = data.sta_ssid || 'Nenastaveno';
        document.getElementById('sta-ip').textContent = data.sta_ip || 'Nepřipojeno';
        document.getElementById('sta-connected').textContent = data.sta_connected ? 'ano' : 'ne';
        if (data.sta_ssid && data.sta_ssid !== 'Nenastaveno') {
            document.getElementById('wifi-ssid').value = data.sta_ssid;
        }
        // Zařízení: Zámek a Online (data z téhož API)
        const lockEl = document.getElementById('web-lock-status');
        if (lockEl) {
            lockEl.textContent = data.locked ? 'Zamčeno' : 'Odemčeno';
            lockEl.style.color = data.locked ? '#e53935' : '#43a047';
        }
        const onlineEl = document.getElementById('web-online-status');
        if (onlineEl) {
            onlineEl.textContent = data.online ? 'Online' : 'Offline';
            onlineEl.style.color = data.online ? '#43a047' : '#e53935';
        }
    } catch (error) {
        console.error('Failed to update WiFi status:', error);
    }
}

// Expose WiFi functions globally for inline onclick handlers
window.saveWiFiConfig = saveWiFiConfig;
window.connectSTA = connectSTA;
window.disconnectSTA = disconnectSTA;
window.clearWiFiConfig = clearWiFiConfig;

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
            alert('Přepnutí demo režimu selhalo: ' + (data.message || 'Neznámá chyba'));
        }
    } catch (error) {
        console.error('Error toggling demo mode:', error);
        alert('Chyba při přepnutí demo režimu');
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
 * Update demo mode status indicator in UI.
 * Syncs checkbox in Settings tab (demo-enabled) when the removed demoStatus/btnDemoMode are not present.
 */
async function updateDemoModeStatus() {
    try {
        const enabled = await isDemoModeEnabled();
        const statusEl = document.getElementById('demoStatus');
        const btnEl = document.getElementById('btnDemoMode');
        const demoCheckbox = document.getElementById('demo-enabled');

        if (statusEl) {
            if (enabled) {
                statusEl.textContent = 'Zapnuto';
                statusEl.style.color = '#4CAF50';
                statusEl.style.fontWeight = 'bold';
            } else {
                statusEl.textContent = 'Vypnuto';
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

        if (demoCheckbox && demoCheckbox.checked !== enabled) {
            demoCheckbox.checked = enabled;
        }
    } catch (error) {
        console.error('Chyba aktualizace stavu demo režimu:', error);
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
