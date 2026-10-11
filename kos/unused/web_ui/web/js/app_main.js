// ============================================================================
// CHESS WEB APP - EXTRACTED JAVASCRIPT FOR SYNTAX CHECKING
// ============================================================================

console.log('🚀 Chess JavaScript loading...');

// ============================================================================
// TAB SWITCHING (Game / Settings)
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

/**
 * PNG pieces from chess.com (public CDN URL, Staunton "neo", 150 px).
 * Requires the browser to have internet access (otherwise fallback to Unicode in setPieceElementFromFen).
 * @see https://www.chess.com/chess-themes/pieces/neo/150/wk.png
 */
const CHESSCOM_PIECE_BASE = 'https://www.chess.com/chess-themes/pieces/neo/150/';
const CHESSCOM_PIECE = {
    'K': 'wk', 'Q': 'wq', 'R': 'wr', 'B': 'wb', 'N': 'wn', 'P': 'wp',
    'k': 'bk', 'q': 'bq', 'r': 'br', 'b': 'bb', 'n': 'bn', 'p': 'bp'
};

function pieceImgSrc(ch) {
    const slug = CHESSCOM_PIECE[ch];
    return slug ? (CHESSCOM_PIECE_BASE + slug + '.png') : '';
}

function setPieceElementFromFen(el, ch) {
    if (!el) return;
    if (ch === ' ' || ch === undefined) {
        el.textContent = '';
        el.innerHTML = '';
        el.className = 'piece';
        return;
    }
    const src = pieceImgSrc(ch);
    if (src) {
        const isWhite = ch >= 'A' && ch <= 'Z';
        el.className = 'piece has-img ' + (isWhite ? 'white' : 'black');
        // BUG FIX 2: Fallback to Unicode when image fails (CDN outage/CORS/blocker)
        const unicodeFallback = pieceSymbols[ch] || ch;
        const colorClass = isWhite ? 'white' : 'black';
        el.innerHTML = '<img src="' + src + '" alt="" draggable="false" onerror="this.parentNode.textContent=\'' + unicodeFallback + '\';this.parentNode.className=\'piece ' + colorClass + '\';">';
    } else {
        el.innerHTML = '';
        el.textContent = pieceSymbols[ch] || ch;
        el.className = 'piece ' + (ch >= 'A' && ch <= 'Z' ? 'white' : 'black');
    }
}

function pieceImgHtml(ch) {
    const s = pieceImgSrc(ch);
    if (s) {
        // BUG FIX 2: Fallback to Unicode when image fails
        const unicodeFallback = pieceSymbols[ch] || ch;
        return '<img src="' + s + '" class="endgame-piece-img" alt="" draggable="false" onerror="this.parentNode.textContent=\'' + unicodeFallback + '\';this.parentNode.className=\'piece ' + (ch >= 'A' && ch <= 'Z' ? 'white' : 'black') + '\';">';
    }
    return pieceSymbols[ch] || ch;
}

/** fetchDataInFlight, boardApiAuthHeaders, fetchGameSnapshot — web/js/api.js */

let boardData = [];
let statusData = {};
/** Last puzzle state from a successful poll — show panel on HTTP outage (offline). */
let lastPuzzleSnapshotForOffline = null;
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
// Bot move is never applied automatically – visualization only (web + LED); user moves the piece physically.
let gameMode = 'pvp'; // 'pvp' or 'bot'
let botSettings = { strength: 10, side: 'white' }; // strength: 1,3,5,8,12,15 (shown as ELO in Settings)
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
/** Per-move evaluation when "Move evaluation" is on: index -> { grade, msg }. */
let moveEvaluations = {};
let endgameReportShown = false;

/** Teaching mode: each player has their own hint count. */
let hintsRemainingWhite = 999;
let hintsRemainingBlack = 999;
/** Captured piece count after the previous poll (for capture detection). */
let lastCapturedCount = 0;
/** Last hint { from, to } – no reward for an excellent move if it was the hinted move. */
var lastHintedMove = null;
/** Hint request generation – incremented on new click; stale responses are ignored. */
var hintRequestGeneration = 0;

/** devicePrefs, hint getters, UI prefs — web/js/prefs.js */

function updateBotSettingsVisibility() {
    var gm = document.getElementById('game-mode');
    var container = document.getElementById('bot-settings-container');
    if (!gm || !container) return;
    container.style.display = (gm.value === 'bot') ? 'block' : 'none';
    saveBotSettings();
}
window.updateBotSettingsVisibility = updateBotSettingsVisibility;

function saveBotSettings() {
    var modeEl = document.getElementById('game-mode');
    var strengthEl = document.getElementById('bot-strength');
    var sideEl = document.getElementById('player-side');
    if (modeEl) gameMode = modeEl.value;
    devicePrefs.botSettings = {
        mode: modeEl ? modeEl.value : 'pvp',
        strength: strengthEl ? strengthEl.value : '10',
        side: sideEl ? sideEl.value : 'white'
    };
    botSettings.strength = devicePrefs.botSettings.strength;
    botSettings.side = devicePrefs.botSettings.side;
    scheduleSaveUiPrefsToDevice();
}
window.saveBotSettings = saveBotSettings;

function loadBotSettings() {
    var s = devicePrefs.botSettings;
    if (!s || typeof s !== 'object') {
        s = { mode: 'pvp', strength: '10', side: 'white' };
    }
    var gm = document.getElementById('game-mode');
    if (gm) {
        if (s.mode) gm.value = s.mode;
        gameMode = gm.value;
    }
    var bs = document.getElementById('bot-strength');
    if (bs && s.strength != null && s.strength !== '') bs.value = String(s.strength);
    var ps = document.getElementById('player-side');
    if (ps && s.side) ps.value = s.side;
    botSettings.strength = bs ? bs.value : String(s.strength || '10');
    botSettings.side = ps ? ps.value : (s.side || 'white');
    var container = document.getElementById('bot-settings-container');
    if (container && gm) {
        container.style.display = (gm.value === 'bot') ? 'block' : 'none';
    }
    if (typeof handleRandomDraw === 'function') handleRandomDraw();
}
window.loadBotSettings = loadBotSettings;


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
            attachBoardPointerHandlers(square, row, col);
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
        // DO NOT REMOVE lifted, error-invalid, error-original - these are server-controlled
        // (z piece_lifted a error_state v JSON statusu)
        sq.classList.remove('selected', 'valid-move', 'valid-capture');
    });
    selectedSquare = null;
}

/** Threshold (px) to distinguish click vs. piece drag. */
var BOARD_DRAG_THRESHOLD_PX = 12;

function squareFromEventTarget(el) {
    if (!el || !el.closest) return null;
    var sq = el.closest('.square');
    if (!sq) return null;
    return {
        row: parseInt(sq.dataset.row, 10),
        col: parseInt(sq.dataset.col, 10)
    };
}

function coordsToNotation(row, col) {
    return String.fromCharCode(97 + col) + (row + 1);
}

function sandboxApplyMoveFromDrag(fromRow, fromCol, toRow, toCol) {
    if (fromRow === toRow && fromCol === toCol) return;
    var piece = sandboxBoard[fromRow][fromCol];
    if (piece === ' ') return;
    var dest = sandboxBoard[toRow][toCol];
    var index = toRow * 8 + toCol;
    if (dest === ' ') {
        makeSandboxMove(fromRow, fromCol, toRow, toCol);
        clearHighlights();
        return;
    }
    var isOurPiece = (piece === piece.toUpperCase()) === (dest === dest.toUpperCase());
    if (isOurPiece) {
        clearHighlights();
        selectedSquare = index;
        var elSq = document.querySelector('[data-row=\'' + toRow + '\'][data-col=\'' + toCol + '\']');
        if (elSq) elSq.classList.add('selected');
        return;
    }
    makeSandboxMove(fromRow, fromCol, toRow, toCol);
    clearHighlights();
}

async function handleRemoteDragMove(fromRow, fromCol, toRow, toCol) {
    if (fromRow === toRow && fromCol === toCol) return;
    var piece = boardData[fromRow] && boardData[fromRow][fromCol];
    if (!piece || piece === ' ') return;
    if (isWebLocked()) {
        alert('Interface is locked. Unlock via UART.');
        return;
    }
    var fromN = coordsToNotation(fromRow, fromCol);
    var toN = coordsToNotation(toRow, toCol);
    try {
        var r1 = await fetch('/api/game/virtual_action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'pickup', square: fromN })
        });
        var res1 = await r1.json().catch(function () { return {}; });
        if (!r1.ok) {
            if (r1.status === 403 && res1.message) alert(res1.message);
            await fetchData();
            return;
        }
        await fetchData();
        var r2 = await fetch('/api/game/virtual_action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'drop', square: toN })
        });
        var res2 = await r2.json().catch(function () { return {}; });
        if (!r2.ok && r2.status === 403 && res2.message) alert(res2.message);
        await fetchData();
    } catch (e) {
        console.error('Remote drag virtual_action:', e);
        await fetchData();
    }
}

function attachBoardPointerHandlers(square, row, col) {
    var dragStart = null;
    function onPointerDown(ev) {
        if (ev.button !== undefined && ev.button !== 0) return;
        if (reviewMode) return;
        dragStart = {
            row: row,
            col: col,
            x: ev.clientX,
            y: ev.clientY,
            pid: ev.pointerId,
            moved: false
        };
        try {
            square.setPointerCapture(ev.pointerId);
        } catch (e) { /* ignore */ }
        if (document.documentElement.classList.contains('web-board-focus')) {
            try {
                ev.preventDefault();
            } catch (e2) { /* ignore */ }
        }
    }
    function onPointerMove(ev) {
        if (!dragStart || dragStart.pid !== ev.pointerId) return;
        var dx = ev.clientX - dragStart.x;
        var dy = ev.clientY - dragStart.y;
        if (!dragStart.moved && (dx * dx + dy * dy) >= BOARD_DRAG_THRESHOLD_PX * BOARD_DRAG_THRESHOLD_PX) {
            dragStart.moved = true;
        }
    }
    async function onPointerUp(ev) {
        if (!dragStart || dragStart.pid !== ev.pointerId) return;
        try {
            square.releasePointerCapture(ev.pointerId);
        } catch (e) { /* ignore */ }
        var wasDrag = dragStart.moved;
        var fr = dragStart.row;
        var fc = dragStart.col;
        dragStart = null;
        if (reviewMode) return;
        if (wasDrag) {
            var targetEl = document.elementFromPoint(ev.clientX, ev.clientY);
            var to = squareFromEventTarget(targetEl);
            if (!to || (to.row === fr && to.col === fc)) return;
            if (sandboxMode) {
                sandboxApplyMoveFromDrag(fr, fc, to.row, to.col);
            } else if (remoteControlEnabled) {
                await handleRemoteDragMove(fr, fc, to.row, to.col);
            }
            return;
        }
        await handleSquareClick(row, col);
    }
    square.addEventListener('pointerdown', onPointerDown);
    square.addEventListener('pointermove', onPointerMove);
    square.addEventListener('pointerup', onPointerUp);
    square.addEventListener('pointercancel', onPointerUp);
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
        alert('Interface is locked. Unlock via UART.');
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
        alert('Error: ' + (e.message || 'cannot send command'));
    }
}

// ============================================================================
// SQUARE CLICK HANDLER
// ============================================================================

async function handleSquareClick(row, col) {
    const piece = sandboxMode ? sandboxBoard[row][col] : boardData[row][col];
    const index = row * 8 + col;

    // SANDBOX MODE (Try moves) - always local only, even if remote control is on
    if (sandboxMode) {
        if (piece === ' ' && selectedSquare !== null) {
            // Move to empty square
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
                    // Click same square – clear selection
                    clearHighlights();
                } else if (isOurPiece) {
                    // Click own piece – select another
                    clearHighlights();
                    selectedSquare = index;
                    const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);
                    if (square) square.classList.add('selected');
                } else {
                    // Click opponent piece – capture
                    makeSandboxMove(fromRow, fromCol, row, col);
                    clearHighlights();
                }
            } else {
                // No piece selected – select this one
                clearHighlights();
                selectedSquare = index;
                const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);
                if (square) square.classList.add('selected');
            }
        }
        return;
    }

    // REMOTE CONTROL MODE - send commands to ESP (only when not in sandbox)
    if (remoteControlEnabled) {
        handleRemoteControlClick(row, col);
        return;
    }

    // NORMAL MODE (not sandbox, not remote control) - no POST requests, no visual feedback
    // Web is passive display only; the game is controlled physically
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
    var banner = document.getElementById('sandbox-banner');
    if (banner) banner.classList.add('active');
    var boardEl = document.getElementById('board');
    if (boardEl) boardEl.classList.add('sandbox-active');
    clearHighlights();
    updateUndoButton();
    if (typeof console !== 'undefined' && console.log) {
        console.log('[Sandbox] enabled — moves local only, visually distinct board');
    }
}

function exitSandboxMode() {
    sandboxMode = false;
    sandboxBoard = [];
    sandboxHistory = [];
    var boardEl = document.getElementById('board');
    if (boardEl) boardEl.classList.remove('sandbox-active');
    var banner = document.getElementById('sandbox-banner');
    if (banner) banner.classList.remove('active');
    clearHighlights();
    fetchData();
    if (typeof console !== 'undefined' && console.log) {
        console.log('[Sandbox] disabled — restoring position from board (HTTP)');
    }
}

function makeSandboxMove(fromRow, fromCol, toRow, toCol) {
    const piece = sandboxBoard[fromRow][fromCol];
    const capturedPiece = sandboxBoard[toRow][toCol]; // Store captured piece (may be ' ')

    // Perform move
    sandboxBoard[toRow][toCol] = piece;
    sandboxBoard[fromRow][fromCol] = ' ';

    // Store move in history with full information
    sandboxHistory.push({
        fromRow: fromRow,
        fromCol: fromCol,
        toRow: toRow,
        toCol: toCol,
        movingPiece: piece,
        capturedPiece: capturedPiece
    });

    // Limit history to 10 moves
    if (sandboxHistory.length > 10) {
        sandboxHistory.shift(); // Remove oldest move
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
        return; // No moves to undo
    }

    // Take last move from history
    const lastMove = sandboxHistory.pop();

    // Put piece back
    sandboxBoard[lastMove.fromRow][lastMove.fromCol] = lastMove.movingPiece;

    // Restore captured piece (or empty square)
    sandboxBoard[lastMove.toRow][lastMove.toCol] = lastMove.capturedPiece;

    // Update board and button
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
        var first = onMove === 'white' ? 'White ' + w + ' | Black ' + b : 'Black ' + b + ' | White ' + w;
        btn.textContent = 'Hint (' + first + ')';
        btn.disabled = (onMove === 'white' ? w : b) <= 0;
    } else {
        btn.textContent = 'Hint';
        btn.disabled = false;
    }
    if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
}

/** In bot mode, grant rewards only for human moves (not bot moves). */
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
    var who = forSide === 'white' ? 'White' : 'Black';
    var msg = reason === 'best' ? 'Excellent move! ' + who + ' +1 hint.' : reason === 'good' ? 'Good move! ' + who + ' +1 hint.' : reason === 'capture' ? 'Piece captured! ' + who + ' +1 hint.' : '';
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

/** Move quality score for average: best=5 … blunder=1, otherwise 0. */
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

/** Average move quality for a player (side='white'|'black') over their last lastN moves. Returns 1–5 or null. */
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

/** Show or hide the Teaching overview block and fill hints + quality averages. */
function updateTeachingStatsPanel() {
    var panel = document.getElementById('teaching-stats-panel');
    if (!panel) return;
    if (!getShowHintStats()) {
        panel.style.display = 'none';
        return;
    }
    panel.style.display = '';
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

// ---------- Parse eval from API (single place, no duplication) ----------
/** Normalize eval string (Unicode minus → ASCII minus). */
function normalizeEvalString(s) {
    if (s == null || typeof s !== 'string') return s;
    return String(s).replace(/\u2212/g, '-').trim();
}
/** Convert value to pawns: if |v| > 10, treat as centipawns (divide by 100). */
function toPawns(v) {
    if (v == null || typeof v !== 'number' || isNaN(v)) return null;
    if (Math.abs(v) > 10) return v / 100;
    return v;
}
/**
 * Select and parse eval from any API response object (data or raw).
 * Tries: eval (number/string), centipawns, cp, evaluation, score (number/string), result.eval.
 * @param {Object} obj - API object (e.g. raw.data or whole raw)
 * @returns {number|null} - eval in pawns, or null
 */
function parseEvalFromApiObject(obj) {
    if (!obj || typeof obj !== 'object') return null;
    var val = null;
    if (typeof obj.eval === 'number') val = toPawns(obj.eval);
    if (val == null && typeof obj.eval === 'string') { var p = parseFloat(normalizeEvalString(obj.eval)); if (!isNaN(p)) val = toPawns(p); }
    if (val == null && obj.centipawns != null) {
        var cp = typeof obj.centipawns === 'number' ? obj.centipawns : parseInt(normalizeEvalString(obj.centipawns), 10);
        if (!isNaN(cp)) val = cp / 100;
    }
    if (val == null && obj.cp != null) {
        var cp2 = typeof obj.cp === 'number' ? obj.cp : parseInt(normalizeEvalString(obj.cp), 10);
        if (!isNaN(cp2)) val = cp2 / 100;
    }
    if (val == null && obj.evaluation != null) {
        var ev = typeof obj.evaluation === 'number' ? obj.evaluation : parseFloat(normalizeEvalString(obj.evaluation));
        if (!isNaN(ev)) val = toPawns(ev);
    }
    if (val == null && typeof obj.score === 'number' && !isNaN(obj.score)) val = toPawns(obj.score);
    if (val == null && typeof obj.score === 'string') { var sc = parseFloat(normalizeEvalString(obj.score)); if (!isNaN(sc)) val = toPawns(sc); }
    if (val == null && obj.result != null && typeof obj.result === 'object' && typeof obj.result.eval === 'number') val = toPawns(obj.result.eval);
    return val;
}

/**
 * Fetch best move and optional evaluation from Stockfish API (POST).
 * Used for hints, move evaluation and bot. Returns { from, to, eval, text, san, continuationArr, mate, winChance } or null.
 * @param {string} fen - FEN position
 * @param {number} [depthOverride] - Optional depth 1–18; if omitted, uses getHintDepth() (hints) or bot uses botSettings.strength
 *
 * Expected API response format (chess-api.com; other backends may use different keys):
 * - from, to (strings, e.g. "e2", "e4") or move (e.g. "e2e4") for the best move
 * - eval (number in pawns; negative = black better) or centipawns/cp (number, divide by 100) or evaluation/score for grading
 * - Optional: text, san, continuationArr, mate, winChance for hint explanation
 * Response may be at root or under .data / .result; all are handled.
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
        var data = (raw && typeof raw.data === 'object' && raw.data !== null) ? raw.data
            : (raw && typeof raw.result === 'object' && raw.result !== null) ? raw.result
            : (raw && typeof raw.bestMove === 'object' && raw.bestMove !== null) ? raw.bestMove
            : raw;
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
        var evalVal = parseEvalFromApiObject(data);
        if (evalVal == null && raw && data !== raw) evalVal = parseEvalFromApiObject(raw);
        if (evalVal == null && typeof console !== 'undefined' && console.warn) {
            console.warn('[Eval Staging] evalVal still null – data keys:', data ? Object.keys(data) : [], 'raw keys:', raw ? Object.keys(raw) : [], 'data.eval:', data && data.eval, 'raw.eval:', raw && raw.eval, 'raw.centipawns:', raw && raw.centipawns);
        }
        if (typeof console !== 'undefined' && console.log) {
            console.log('[Eval Staging] API parsed evalVal (pawns):', evalVal);
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

/** Pass-through for API hint text (kept English for the UI). */
function hintTextToCzech(s) {
    if (!s || typeof s !== 'string') return '';
    return s;
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
    parts.push('The computer suggests: play ' + san + '.');

    var e = data.eval;
    if (e != null && typeof e === 'number') {
        if (e > 0.3) parts.push('White has a slight advantage.');
        else if (e < -0.3) parts.push('Black has a slight advantage.');
        else parts.push('The position is equal.');
    }

    if (Array.isArray(data.continuationArr) && data.continuationArr.length > 0) {
        var first = data.continuationArr.slice(0, 4).map(formatUciMove).join(', ');
        parts.push('Then you could play something like ' + first + '.');
    }

    if (data.mate != null && typeof data.mate === 'number') {
        if (data.mate === 0) parts.push('Checkmate!');
        else if (data.mate > 0) parts.push('White mates in ' + data.mate + '!');
        else parts.push('Black mates in ' + (-data.mate) + '!');
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

/** Show hint block with error/info message (e.g. no internet). */
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
 * then show a short message and color by quality (best=green, blunder=red, …).
 */
/** Normalize UCI move to 4 chars (from+to) for comparison. */
function normalizeUci(from, to) {
    var s = ((from || '') + (to || '')).toLowerCase().replace(/[^a-h1-8]/g, '');
    return s.length >= 4 ? s.slice(0, 4) : s;
}

/** True if the evaluation for the given move count is still valid (no new game / next move). */
function isEvaluationStillValid(historyLength) {
    return (historyData && historyData.length) === historyLength;
}

/** chess-api.com returns eval from White's perspective (negative = black winning). Do not invert. */
var API_EVAL_SIDE_TO_MOVE = false;
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
        showMoveEvaluation('Move evaluation requires an internet connection (WiFi).', 'error');
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
            var errMsg = 'Move evaluation was not available. Check your internet connection.';
            showMoveEvaluation(errMsg, 'error');
            moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };
            renderHistoryList();
            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
            return;
        }
        var bestUci = normalizeUci(beforeData.from, beforeData.to);
        if (playedUci === bestUci) {
            var msgBest = 'Excellent move! That was the best move.';
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
                var msgInacc = 'A better move was ' + bestFormatted + '.';
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
                msg = 'Inaccuracy. Better was ' + bestFormatted + '.';
                grade = 'inaccuracy';
            } else {
                var whiteJustMoved = (historyLength - 1) % 2 === 0;
                var scoreDrop = whiteJustMoved ? (evalBefore - evalAfter) : (evalAfter - evalBefore);
                if (scoreDrop < 0) scoreDrop = 0;
                if (scoreDrop <= 0.20) {
                    msg = 'Good move.';
                    grade = 'good';
                    var sideGood = (historyLength - 1) % 2 === 0 ? 'white' : 'black';
                    if (getHintAwardGood()) addHintReward('good', sideGood);
                } else if (scoreDrop <= 0.50) {
                    msg = 'Inaccuracy. Better was ' + bestFormatted + '.';
                    grade = 'inaccuracy';
                } else if (scoreDrop <= 1.00) {
                    msg = 'Mistake. The position got worse. Better was ' + bestFormatted + '.';
                    grade = 'mistake';
                } else {
                    msg = 'Blunder. Better was ' + bestFormatted + '.';
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
            var errMsg = 'Move evaluation was not available. Check your internet connection.';
            showMoveEvaluation(errMsg, 'error');
            moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };
            renderHistoryList();
            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
        });
    }).catch(function () {
        if (!isEvaluationStillValid(historyLength)) return;
        var errMsg = 'Move evaluation was not available. Check your internet connection.';
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
    if (statusData && statusData.board_setup_tutorial === true) return;
    if (statusData && statusData.puzzle && statusData.puzzle.setup_active === true) return;
    if (statusData && statusData.puzzle && statusData.puzzle.active === true) return;
    if (typeof openingIsActive === 'function' && openingIsActive(statusData)) return;
    if (isWebLocked()) {
        showHintError('Interface is locked. Unlock via UART.');
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
        showHintError('You have no hints left on this turn. Earn one with an excellent move or by capturing a piece.');
        return;
    }

    const btn = document.getElementById('hint-btn');
    if (btn) {
        btn.disabled = true;
        btn.textContent = 'Loading…';
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
        showHintError('Hint requires an internet connection (WiFi).');
        if (btn) updateHintButtonLabel();
        return;
    }

    var myGen = ++hintRequestGeneration;
    try {
        const fen = boardAndStatusToFen(boardData, status, historyData);
        if (!fen) {
            if (limit > 0) {
                var pFen = (status.current_player === 'Black') ? 'black' : 'white';
                if (pFen === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
                updateHintButtonLabel();
            }
            if (console.warn) console.warn('[Hint] Could not build FEN');
            showHintError('Cannot load position. Refresh the page.');
            if (btn) updateHintButtonLabel();
            return;
        }
        const move = await fetchStockfishBestMove(fen);
        if (myGen !== hintRequestGeneration) {
            if (limit > 0) {
                var pStale = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';
                if (pStale === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
            }
            if (btn) btn.disabled = false;
            updateHintButtonLabel();
            return;
        }
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
            showHintError('Hint is not available. Try again later or check your internet connection.');
            if (btn) updateHintButtonLabel();
        }
    } catch (err) {
        if (myGen !== hintRequestGeneration) {
            if (limit > 0) {
                var pStaleC = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';
                if (pStaleC === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
            }
            if (btn) updateHintButtonLabel();
            return;
        }
        if (limit > 0) {
            var p = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';
            if (p === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;
            updateHintButtonLabel();
        }
        if (console.error) console.error('[Hint] requestHint error:', err.message);
        showHintError('Hint is not available. Try again later or check your internet connection.');
        if (btn) updateHintButtonLabel();
    }
}
window.requestHint = requestHint;

// ============================================================================
// UPDATE FUNCTIONS
// ============================================================================

function updateBoard(board) {
    // In sandbox do not overwrite boardData (keeps real board position for fetch after exit).
    var skipReplaceBoardData = sandboxMode && board === sandboxBoard;
    // Only clear hint when board actually changed (new move), not on every periodic fetch
    var boardUnchanged = !skipReplaceBoardData && boardData && board.length === 8 && boardData.length === 8 &&
        JSON.stringify(board) === JSON.stringify(boardData);
    if (!skipReplaceBoardData) {
        boardData = board;
    }
    const loading = document.getElementById('loading');
    if (loading) loading.style.display = 'none';

    if (!boardUnchanged) {
        document.querySelectorAll('.square').forEach(sq => {
            sq.classList.remove('hint-from', 'hint-to');
        });
        hideHintExplanation();
    }

    // DO NOT ADD clearHighlights() - highlights are controlled via updateStatus()
    // (lifted, error-invalid, error-original are server-controlled states)

    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const piece = board[row][col];
            const pieceElement = document.getElementById('piece-' + (row * 8 + col));
            if (pieceElement) {
                setPieceElementFromFen(pieceElement, piece);
            }
        }
    }
}

// ============================================================================
// ENDGAME REPORT FUNCTIONS
// ============================================================================

// Show endgame report on the web
async function showEndgameReport(gameEnd) {
    console.log('🏆 showEndgameReport() called with:', gameEnd);

    // If banner is already shown, do nothing (avoid redraw)
    if (endgameReportShown && document.getElementById('endgame-banner')) {
        console.log('Endgame report already shown, skipping...');
        return;
    }

    // Load advantage history for the chart
    let advantageDataLocal = { history: [], white_checks: 0, black_checks: 0, white_castles: 0, black_castles: 0 };
    try {
        const response = await fetch('/api/advantage');
        advantageDataLocal = await response.json();
        console.log('Advantage data loaded:', advantageDataLocal);
    } catch (e) {
        console.error('Failed to load advantage data:', e);
    }

    // Determine result and colors
    let title = '';
    let subtitle = '';
    let accentColor = '#4CAF50';
    let bgGradient = 'linear-gradient(135deg, #1e3a1e, #2d4a2d)';

    if (gameEnd.winner === 'Draw') {
        title = 'DRAW';
        subtitle = gameEnd.reason;
        accentColor = '#FF9800';
        bgGradient = 'linear-gradient(135deg, #3a2e1e, #4a3e2d)';
    } else {
        title = `${gameEnd.winner.toUpperCase()} WON!`;
        subtitle = gameEnd.reason;
        accentColor = gameEnd.winner === 'White' ? '#4CAF50' : '#2196F3';
        bgGradient = gameEnd.winner === 'White' ? 'linear-gradient(135deg, #1e3a1e, #2d4a2d)' : 'linear-gradient(135deg, #1e2a3a, #2d3a4a)';
    }

    // Get statistics
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
    const materialText = materialDiff > 0 ? `White +${materialDiff}` : materialDiff < 0 ? `Black +${-materialDiff}` : 'Equal';

    // Create SVG advantage chart (like chess.com)
    let graphSVG = '';
    if (advantageDataLocal.history && advantageDataLocal.history.length > 1) {
        const history = advantageDataLocal.history;
        const width = 280;
        const height = 100;
        const maxAdvantage = Math.max(10, ...history.map(Math.abs));
        const scaleY = height / (2 * maxAdvantage);
        const scaleX = width / (history.length - 1);

        // Create points for polyline (0,0 is top-left, y grows downward)
        let points = history.map((adv, i) => {
            const x = i * scaleX;
            const y = height / 2 - adv * scaleY;  // Flip Y (White on top, Black at bottom)
            return `${x},${y}`;
        }).join(' ');

        // Create polygon for filled area
        let areaPoints = `0,${height / 2} ${points} ${width},${height / 2}`;

        graphSVG = `<svg width="280" height="100" style="border-radius:6px;background:rgba(0,0,0,0.2);">
            <!-- Center line (equal position) -->
            <line x1="0" y1="${height / 2}" x2="${width}" y2="${height / 2}" stroke="#555" stroke-width="1" stroke-dasharray="3,3"/>
            <!-- Filled area under the curve -->
            <polygon points="${areaPoints}" fill="${accentColor}" opacity="0.2"/>
            <!-- Advantage curve -->
            <polyline points="${points}" fill="none" stroke="${accentColor}" stroke-width="2" stroke-linejoin="round"/>
            <!-- End dots -->
            <circle cx="0" cy="${height / 2}" r="3" fill="${accentColor}"/>
            <circle cx="${(history.length - 1) * scaleX}" cy="${height / 2 - history[history.length - 1] * scaleY}" r="4" fill="${accentColor}"/>
            <!-- Popisky -->
            <text x="5" y="12" fill="#888" font-size="10" font-weight="600">White</text>
            <text x="5" y="${height - 2}" fill="#888" font-size="10" font-weight="600">Black</text>
        </svg>`;
    }

    // Create new banner - LEFT OF THE BOARD, NOT CENTERED!
    const banner = document.createElement('div');
    banner.id = 'endgame-banner';

    // On mobile - different placement (top, full width)
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
                    Game progress
                </h3>
                ${graphSVG}
                <div style="display:flex;justify-content:space-between;margin-top:8px;font-size:11px;color:#888;">
                    <span>Start</span>
                    <span>Move ${advantageDataLocal.count || 0}</span>
                </div>
            </div>` : ''}
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;">
                    Statistics
                </h3>
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;font-size:13px;">
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Moves</div>
                        <div style="color:#e0e0e0;font-weight:600;">White ${whiteMoves} | Black ${blackMoves}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Material</div>
                        <div style="color:${accentColor};font-weight:600;">${materialText}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Captured</div>
                        <div style="color:#e0e0e0;font-weight:600;">White ${whiteCaptured.length} | Black ${blackCaptured.length}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Total</div>
                        <div style="color:#e0e0e0;font-weight:600;">${statusData.move_count} moves</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Checks</div>
                        <div style="color:#e0e0e0;font-weight:600;">White ${advantageDataLocal.white_checks || 0} | Black ${advantageDataLocal.black_checks || 0}</div>
                    </div>
                    <div style="background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;">
                        <div style="color:#888;font-size:11px;margin-bottom:4px;">Castles</div>
                        <div style="color:#e0e0e0;font-weight:600;">White ${advantageDataLocal.white_castles || 0} | Black ${advantageDataLocal.black_castles || 0}</div>
                    </div>
                </div>
            </div>
            <div style="background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;">
                <h3 style="margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;">
                    Captured pieces
                </h3>
                <div style="margin-bottom:10px;">
                    <div style="color:#888;font-size:11px;margin-bottom:4px;">White captured (${whiteCaptured.length})</div>
                    <div style="font-size:20px;line-height:1.4;">${whiteCaptured.map(p => pieceImgHtml(p)).join(' ') || '−'}</div>
                </div>
                <div>
                    <div style="color:#888;font-size:11px;margin-bottom:4px;">Black captured (${blackCaptured.length})</div>
                    <div style="font-size:20px;line-height:1.4;">${blackCaptured.map(p => pieceImgHtml(p)).join(' ') || '−'}</div>
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

    // Add CSS animations if they do not exist yet
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
    endgameReportShown = true;  // Mark as shown
    console.log('🏆 ENDGAME REPORT SHOWN - banner displayed (left side)');
}

// Hide endgame report (but keep flag for toggle)
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
        // Already shown -> hide
        hideEndgameReport();
    } else {
        // Not shown -> show again (if we have data)
        if (window.lastGameEndData) {
            showEndgameReport(window.lastGameEndData);
        }
    }
}

// Show toggle button
function showEndgameToggleButton() {
    // Check whether the button already exists
    if (document.getElementById('endgame-toggle-btn')) return;

    const button = document.createElement('button');
    button.id = 'endgame-toggle-btn';
    button.innerHTML = 'Report';
    button.title = 'Show/hide endgame report';
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

// Hide toggle button
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

/**
 * Show/hide the "Bot" panel and set its text.
 * Panel is visible only when gameMode === 'bot'.
 * @param {string} [text] - Status text. If omitted, derived from status (guidance when a piece is lifted) or "Your turn!".
 * @param {object} [status] - Current API status; if piece_lifted and we have a bot suggestion, show guidance.
 */
function updateBotStatusPanel(text, status) {
    var panel = document.getElementById('bot-status-panel');
    var textEl = document.getElementById('bot-status-text');
    if (!panel || !textEl) return;
    if (gameMode !== 'bot') {
        panel.style.display = 'none';
        return;
    }
    if (text !== undefined) {
        textEl.textContent = text;
    } else if (status && status.piece_lifted && status.piece_lifted.lifted && lastSuggestedMove) {
        textEl.textContent = 'Place the piece on ' + lastSuggestedMove.to + '.';
    } else if (textEl.textContent === '—' || textEl.textContent.trim() === '') {
        textEl.textContent = 'Your turn!';
    }
    panel.style.display = '';
}

/**
 * "Puzzle" panel (like Bot) — encouraging text from board feedback; works offline (board poll only).
 * @param {object} status - status from API or { puzzle: {...} }
 * @param {{offline?:boolean}} [opts]
 */
function updatePuzzleStatusPanel(status, opts) {
    opts = opts || {};
    var offline = !!opts.offline;
    var panel = document.getElementById('puzzle-status-panel');
    var textEl = document.getElementById('puzzle-status-text');
    var subEl = document.getElementById('puzzle-status-sub');
    var titleEl = panel ? panel.querySelector('.game-title') : null;
    if (!panel || !textEl) return;

    var p = status && status.puzzle ? status.puzzle : null;
    if (!p) {
        panel.style.display = 'none';
        panel.className = 'game-block puzzle-status-panel';
        if (subEl) {
            subEl.textContent = '';
            subEl.style.display = 'none';
        }
        if (titleEl) titleEl.textContent = 'Puzzle';
        return;
    }

    var fb = String(p.feedback || 'none').toLowerCase();
    var show = false;
    var mode = 'play';
    var main = '';
    var sub = '';

    if (p.setup_active === true && p.active !== true) {
        show = true;
        mode = 'setup';
        main = 'Set up the physical position by the LEDs — step by step.';
        sub = 'Details are in the Puzzles window (top).';
    } else if (p.active === true) {
        show = true;
        if (fb === 'wrong') {
            mode = 'wrong';
            main = 'Not quite — put the piece back and try again. That is how you learn!';
            sub = p.message ? String(p.message) : '';
        } else if (fb === 'illegal') {
            mode = 'illegal';
            main = 'That move is not legal here — try another square. Every master started somewhere.';
            sub = p.message ? String(p.message) : '';
        } else {
            mode = 'play';
            main = 'Your move — find the best continuation. Good luck!';
            sub = (p.teaser && String(p.teaser).length) ? String(p.teaser) : (p.title ? String(p.title) : '');
        }
    } else if (fb === 'solved') {
        show = true;
        mode = 'solved';
        main = 'Great! That is how it is done — puzzle complete.';
        sub = (p.title ? 'Puzzle: ' + p.title : '') + (p.teaser ? (p.title ? ' — ' : '') + p.teaser : '');
    }

    if (!show) {
        panel.style.display = 'none';
        panel.className = 'game-block puzzle-status-panel';
        if (titleEl) titleEl.textContent = 'Puzzle';
        if (subEl) {
            subEl.textContent = '';
            subEl.style.display = 'none';
        }
        return;
    }

    if (offline) {
        sub = (sub ? sub + ' ' : '') + 'Live board status is unavailable (web connection). It will refresh when the board network is back.';
    }

    if (titleEl) {
        titleEl.textContent = mode === 'setup'
            ? 'Puzzle · setup'
            : mode === 'solved'
                ? 'Puzzle · hotovo'
                : 'Puzzle · your turn';
    }
    panel.style.display = '';
    panel.className = 'game-block puzzle-status-panel puzzle-status-panel--' + mode +
        (offline ? ' puzzle-status-panel--offline' : '');
    textEl.textContent = main;
    if (subEl) {
        if (sub && String(sub).trim().length > 0) {
            subEl.textContent = sub.trim();
            subEl.style.display = 'block';
        } else {
            subEl.textContent = '';
            subEl.style.display = 'none';
        }
    }
}

/** Clear bot UI: state, LED interval, hint classes, and "Bot is playing" / "Computer" message. */
function clearBotSuggestion() {
    var hadDomHints = !!document.querySelector('.square.hint-from, .square.hint-to');
    var needServerClear = lastSuggestedFen !== null || lastSuggestedMove !== null || hadDomHints;
    lastSuggestedFen = null;
    lastSuggestedMove = null;
    stopBotHintRefresh();
    document.querySelectorAll('.square').forEach(function (sq) { sq.classList.remove('hint-from', 'hint-to'); });
    var msgEl = document.getElementById('castling-pending-message');
    if (msgEl && (msgEl.textContent.indexOf('Bot') !== -1 || msgEl.textContent.indexOf('Computer') !== -1)) msgEl.style.display = 'none';
    updateBotStatusPanel('Your turn!');
    if (needServerClear) {
        fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    }
}

/**
 * Bot move is NOT applied automatically – visualization on web and LED only.
 * User must physically move the piece; the move is applied after DROP from the matrix.
 * Call only /api/game/hint_highlight, never /api/move or virtual_action for the bot.
 */
/** Bot uses shared fetchStockfishBestMove with depth = botSettings.strength. */
async function playBotMove(fen, generation) {
    if (botThinking || !fen || typeof fen !== 'string') return;
    botThinking = true;
    var statusEl = document.getElementById('game-state');

    updateBotStatusPanel('Thinking');
    try {
        if (statusEl) statusEl.textContent = 'Bot is choosing a move';
        console.log('🤖 Bot suggests move (visualization only – user moves physically)... Generation:', generation);
        var botDepth = parseInt(botSettings.strength, 10) || 10;
        var move = await fetchStockfishBestMove(fen, botDepth);

        if (generation !== gameGeneration) {
            console.warn('🤖 Bot move aborted: Game generation changed (New game started).');
            lastSuggestedMove = null;
            stopBotHintRefresh();
            return;
        }

        if (move) {
            console.log('🤖 Bot suggests:', move.from, '->', move.to, '(shown on web and LED; you make the move on the board)');
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
                var status = statusData;
                if (status && status.piece_lifted && status.piece_lifted.lifted) {
                    if (getBotLedTargetOnlyAfterLift()) {
                        var liftedFrom = String.fromCharCode(97 + status.piece_lifted.col) + (status.piece_lifted.row + 1);
                        if (liftedFrom.toLowerCase() === lastSuggestedMove.from.toLowerCase()) {
                            fetch('/api/game/hint_highlight', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({ to: lastSuggestedMove.to })
                            }).catch(function () {});
                            return;
                        }
                    } else {
                        return;
                    }
                }
                fetch('/api/game/hint_highlight', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ from: lastSuggestedMove.from, to: lastSuggestedMove.to })
                }).catch(function () {});
            }, BOT_HINT_REFRESH_MS);
            var panelMsg = 'Playing ' + move.from + '-' + move.to + '. Lift the piece from ' + move.from + '.';
            updateBotStatusPanel(panelMsg);
            showHintOnBoard(move.from, move.to);
        } else {
            console.warn('Bot: no move (API error or timeout).');
            if (typeof console !== 'undefined' && console.warn) {
                console.warn('[Bot] API failed, clearing lastSuggestedFen for retry');
            }
            lastSuggestedFen = null;
            updateBotStatusPanel('API error');
        }
    } finally {
        botThinking = false;
    }
}

function checkBotTurn(status, fen) {
    if (status && status.board_setup_tutorial === true) {
        clearBotSuggestion();
        return;
    }
    if (status && status.puzzle && status.puzzle.setup_active === true) {
        clearBotSuggestion();
        return;
    }
    if (status && status.puzzle && status.puzzle.active === true) {
        clearBotSuggestion();
        return;
    }
    if (typeof openingIsActive === 'function' && openingIsActive(status)) {
        clearBotSuggestion();
        return;
    }
    if (gameMode !== 'bot') {
        clearBotSuggestion();
        return;
    }
    /* As soon as the position changes (new FEN), someone moved – clear bot suggestion immediately,
       independent of current_player (backend may catch up later). */
    var clearedDueToFenChange = false;
    if (lastSuggestedFen && fen && fen !== lastSuggestedFen) {
        clearBotSuggestion();
        clearedDueToFenChange = true;
    }
    if (!status || typeof status !== 'object') {
        clearBotSuggestion();
        return;
    }
    if (status.castling_in_progress === true) {
        clearBotSuggestion();
        return;
    }
    if (status.game_state !== 'active' && status.game_state !== 'playing') {
        clearBotSuggestion();
        return;
    }
    if (status.game_end && status.game_end.ended) {
        clearBotSuggestion();
        return;
    }
    if (status.current_player !== 'White' && status.current_player !== 'Black') {
        clearBotSuggestion();
        return;
    }
    if (!fen || typeof fen !== 'string' || fen.length < 20) {
        clearBotSuggestion();
        return;
    }

    var isBotTurn = (botSettings.side === 'white') !== (status.current_player === 'White');
    if (typeof console !== 'undefined' && console.log) console.log('checkBotTurn: isBotTurn=', isBotTurn, 'current_player=', status.current_player);

    if (!isBotTurn) {
        clearBotSuggestion();
        return;
    }
    /* User lifted a piece – making the bot's move. Do not call clearBotSuggestion (lastSuggestedMove
       must stay) so the Bot panel can show "Place the piece on X." in updateBotStatusPanel. */
    if (status.piece_lifted && status.piece_lifted.lifted) {
        return;
    }
    /* After clearing due to FEN change in this call, do not call playBotMove – race possible (FEN already new, current_player still bot). */
    if (clearedDueToFenChange) return;
    if (!botThinking && fen !== lastSuggestedFen) {
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

// Matrix guard UI: web/js/matrix_guard.js (concatenated into chess_app.js)


// ============================================================================
// SETUP TUTORIAL (starting position — web + LED)
// ============================================================================

const SETUP_TUTORIAL_REFRESH_MS = 600;
const SETUP_TUTORIAL_FAST_POLL_MS = 400;
const SETUP_TUTORIAL_OCC_STABLE_TICKS = 2;

/** 32 steps: white 1st rank, white pawns, black 8th rank, black pawns. */
const SETUP_TUTORIAL_STEPS = [
    { sq: 'a1', piece: 'r', label: 'White rook → a1' },
    { sq: 'b1', piece: 'n', label: 'White knight → b1' },
    { sq: 'c1', piece: 'b', label: 'White bishop → c1' },
    { sq: 'd1', piece: 'q', label: 'White queen → d1' },
    { sq: 'e1', piece: 'k', label: 'White king → e1' },
    { sq: 'f1', piece: 'b', label: 'White bishop → f1' },
    { sq: 'g1', piece: 'n', label: 'White knight → g1' },
    { sq: 'h1', piece: 'r', label: 'White rook → h1' },
    { sq: 'a2', piece: 'p', label: 'White pawn → a2' },
    { sq: 'b2', piece: 'p', label: 'White pawn → b2' },
    { sq: 'c2', piece: 'p', label: 'White pawn → c2' },
    { sq: 'd2', piece: 'p', label: 'White pawn → d2' },
    { sq: 'e2', piece: 'p', label: 'White pawn → e2' },
    { sq: 'f2', piece: 'p', label: 'White pawn → f2' },
    { sq: 'g2', piece: 'p', label: 'White pawn → g2' },
    { sq: 'h2', piece: 'p', label: 'White pawn → h2' },
    { sq: 'a8', piece: 'R', label: 'Black rook → a8' },
    { sq: 'b8', piece: 'N', label: 'Black knight → b8' },
    { sq: 'c8', piece: 'B', label: 'Black bishop → c8' },
    { sq: 'd8', piece: 'Q', label: 'Black queen → d8' },
    { sq: 'e8', piece: 'K', label: 'Black king → e8' },
    { sq: 'f8', piece: 'B', label: 'Black bishop → f8' },
    { sq: 'g8', piece: 'N', label: 'Black knight → g8' },
    { sq: 'h8', piece: 'R', label: 'Black rook → h8' },
    { sq: 'a7', piece: 'P', label: 'Black pawn → a7' },
    { sq: 'b7', piece: 'P', label: 'Black pawn → b7' },
    { sq: 'c7', piece: 'P', label: 'Black pawn → c7' },
    { sq: 'd7', piece: 'P', label: 'Black pawn → d7' },
    { sq: 'e7', piece: 'P', label: 'Black pawn → e7' },
    { sq: 'f7', piece: 'P', label: 'Black pawn → f7' },
    { sq: 'g7', piece: 'P', label: 'Black pawn → g7' },
    { sq: 'h7', piece: 'P', label: 'Black pawn → h7' }
];

let setupTutorialPhase = null;
let setupTutorialStepIndex = 0;
let setupTutorialLedIntervalId = null;
let setupTutorialFastPollId = null;
let setupTutorialOccStable = 0;

function setupTutorialSquareToIndex(sq) {
    var c = sq.charCodeAt(0) - 97;
    var r = parseInt(sq.charAt(1), 10) - 1;
    return r * 8 + c;
}

function setupTutorialStopLedRefresh() {
    if (setupTutorialLedIntervalId) {
        clearInterval(setupTutorialLedIntervalId);
        setupTutorialLedIntervalId = null;
    }
}

function setupTutorialStopFastPoll() {
    if (setupTutorialFastPollId) {
        clearInterval(setupTutorialFastPollId);
        setupTutorialFastPollId = null;
    }
}

function setupTutorialApplyLed(sq) {
    return fetch('/api/game/hint_highlight', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ to: sq })
    }).catch(function () {});
}

function setupTutorialStartLedRefresh(sq) {
    setupTutorialStopLedRefresh();
    setupTutorialApplyLed(sq);
    setupTutorialLedIntervalId = setInterval(function () {
        setupTutorialApplyLed(sq);
    }, SETUP_TUTORIAL_REFRESH_MS);
}

function setupTutorialClearLed() {
    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
}

function openSetupTutorialIntro() {
    var ov = document.getElementById('setup-tutorial-overlay');
    if (!ov) return;
    try {
        if (!devicePrefs.chessTutorialsEnabled) return;
    } catch (e) { return; }
    ov.style.display = 'flex';
    ov.classList.add('overlay-visible');
    var intro = document.getElementById('setup-tutorial-intro-panel');
    var run = document.getElementById('setup-tutorial-run-panel');
    var done = document.getElementById('setup-tutorial-done-panel');
    if (intro) intro.style.display = '';
    if (run) run.style.display = 'none';
    if (done) done.style.display = 'none';
    setupTutorialUpdateIntroWarnings(statusData || {});
}

function setupTutorialCloseIntro() {
    var ov = document.getElementById('setup-tutorial-overlay');
    if (ov) {
        ov.classList.remove('overlay-visible');
        ov.style.display = 'none';
    }
}

function setupTutorialUpdateIntroWarnings(st) {
    var w = document.getElementById('setup-tutorial-warn');
    if (!w) return;
    var parts = [];
    if (st.light_mode === 'lamp') {
        parts.push('Lamp mode may override game LEDs — switch to Board in Device.');
    }
    if (st.matrix_guard_active) {
        parts.push('Matrix guard is active — finish returning pieces or exit guard mode.');
    }
    if (parts.length) {
        w.textContent = parts.join(' ');
        w.style.display = '';
    } else {
        w.textContent = '';
        w.style.display = 'none';
    }
}

function setupTutorialRenderStep() {
    var st = SETUP_TUTORIAL_STEPS[setupTutorialStepIndex];
    if (!st) return;
    var pe = document.getElementById('setup-tutorial-piece-display');
    var ins = document.getElementById('setup-tutorial-instruction');
    var pr = document.getElementById('setup-tutorial-progress');
    if (pe) {
        var isWhitePc = (st.piece >= 'a' && st.piece <= 'z');
        var src = pieceImgSrc(st.piece);
        if (src) {
            pe.className = 'setup-tutorial-piece-glyph piece has-img ' +
                (isWhitePc ? 'white' : 'black');
            pe.innerHTML = '<img src="' + src + '" alt="" draggable="false">';
        } else {
            pe.innerHTML = '';
            pe.textContent = pieceSymbols[st.piece] || '?';
            pe.className = 'setup-tutorial-piece-glyph piece ' +
                (isWhitePc ? 'white' : 'black');
        }
    }
    if (ins) ins.textContent = 'Place the piece on square ' + st.sq.toUpperCase();
    if (pr) pr.textContent = 'Krok ' + (setupTutorialStepIndex + 1) + ' / ' + SETUP_TUTORIAL_STEPS.length + ' — ' + st.label;
    setupTutorialStartLedRefresh(st.sq);
    setupTutorialOccStable = 0;
}

async function setupTutorialBegin() {
    try {
        var res = await fetch('/api/game/setup_tutorial', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'start' })
        });
        if (!res.ok) {
            if (typeof console !== 'undefined' && console.warn) console.warn('setup_tutorial start', res.status);
            return;
        }
    } catch (e) {
        if (typeof console !== 'undefined' && console.warn) console.warn(e);
        return;
    }
    setupTutorialPhase = 'run';
    setupTutorialStepIndex = 0;
    var intro = document.getElementById('setup-tutorial-intro-panel');
    var run = document.getElementById('setup-tutorial-run-panel');
    if (intro) intro.style.display = 'none';
    if (run) run.style.display = '';
    setupTutorialRenderStep();
    setupTutorialFastPollId = setInterval(setupTutorialPollOccupancy, SETUP_TUTORIAL_FAST_POLL_MS);
}

function setupTutorialPollOccupancy() {
    if (setupTutorialPhase !== 'run') return;
    fetch('/api/status')
        .then(function (r) { return r.json(); })
        .then(function (st) {
            if (!st.matrix_occupied || !Array.isArray(st.matrix_occupied)) return;
            var stp = SETUP_TUTORIAL_STEPS[setupTutorialStepIndex];
            if (!stp) return;
            var idx = setupTutorialSquareToIndex(stp.sq);
            if (Number(st.matrix_occupied[idx]) === 1) {
                setupTutorialOccStable++;
                if (setupTutorialOccStable >= SETUP_TUTORIAL_OCC_STABLE_TICKS) {
                    setupTutorialAdvance(true);
                }
            } else {
                setupTutorialOccStable = 0;
            }
        })
        .catch(function () {});
}

function setupTutorialAdvance(fromAuto) {
    if (setupTutorialPhase !== 'run') return;
    setupTutorialStopLedRefresh();
    setupTutorialClearLed();
    setupTutorialOccStable = 0;
    setupTutorialStepIndex++;
    if (setupTutorialStepIndex >= SETUP_TUTORIAL_STEPS.length) {
        setupTutorialStopFastPoll();
        setupTutorialPhase = 'done';
        var run = document.getElementById('setup-tutorial-run-panel');
        var done = document.getElementById('setup-tutorial-done-panel');
        if (run) run.style.display = 'none';
        if (done) done.style.display = '';
        return;
    }
    setupTutorialRenderStep();
}

function setupTutorialBack() {
    if (setupTutorialPhase !== 'run') return;
    if (setupTutorialStepIndex <= 0) return;
    setupTutorialStopLedRefresh();
    setupTutorialClearLed();
    setupTutorialOccStable = 0;
    setupTutorialStepIndex--;
    setupTutorialRenderStep();
}

function setupTutorialSkip() {
    setupTutorialAdvance(false);
}

async function setupTutorialCancel() {
    setupTutorialStopLedRefresh();
    setupTutorialStopFastPoll();
    setupTutorialClearLed();
    setupTutorialPhase = null;
    try {
        await fetch('/api/game/setup_tutorial', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'cancel' })
        });
    } catch (e) {}
    var ov = document.getElementById('setup-tutorial-overlay');
    if (ov) {
        ov.classList.remove('overlay-visible');
        ov.style.display = 'none';
    }
    var intro = document.getElementById('setup-tutorial-intro-panel');
    var run = document.getElementById('setup-tutorial-run-panel');
    var done = document.getElementById('setup-tutorial-done-panel');
    if (intro) intro.style.display = '';
    if (run) run.style.display = 'none';
    if (done) done.style.display = 'none';
    if (typeof fetchData === 'function') fetchData();
}

async function setupTutorialFinish() {
    try {
        var res = await fetch('/api/game/setup_tutorial', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'finish' })
        });
        var data = await res.json().catch(function () { return {}; });
        if (!res.ok) {
            var msg = (data && data.error) ? data.error : 'Check the physical position (ranks 1–2 and 7–8 full, 3–6 empty).';
            alert(msg);
            return;
        }
    } catch (e) {
        alert('Network error while finishing.');
        return;
    }
    setupTutorialStopLedRefresh();
    setupTutorialStopFastPoll();
    setupTutorialPhase = null;
    var ov = document.getElementById('setup-tutorial-overlay');
    if (ov) {
        ov.classList.remove('overlay-visible');
        ov.style.display = 'none';
    }
    var intro = document.getElementById('setup-tutorial-intro-panel');
    var run = document.getElementById('setup-tutorial-run-panel');
    var done = document.getElementById('setup-tutorial-done-panel');
    if (intro) intro.style.display = '';
    if (run) run.style.display = 'none';
    if (done) done.style.display = 'none';
    if (typeof fetchData === 'function') fetchData();
    if (typeof global.openingOnSetupTutorialDone === 'function') {
        global.openingOnSetupTutorialDone();
    }
}

window.openSetupTutorialIntro = openSetupTutorialIntro;
window.setupTutorialBegin = setupTutorialBegin;
window.setupTutorialCloseIntro = setupTutorialCloseIntro;
window.setupTutorialBack = setupTutorialBack;
window.setupTutorialSkip = setupTutorialSkip;
window.setupTutorialCancel = setupTutorialCancel;
window.setupTutorialFinish = setupTutorialFinish;

const PUZZLE_DEFS = [
    { id: 1, difficulty: 1, title: 'Mate in 1 – Queen on the back rank',
        teaser: 'Classic motif: open f-file, queen delivers mate on f8.',
        fen: '7k/7p/8/8/8/8/5Q2/6K1 w - - 0 1' },
    { id: 2, difficulty: 2, title: 'Mate in 1 – Queen along the file',
        teaser: 'Attack along the file: queen rises from b2 to b8.',
        fen: '6k1/5ppp/8/8/8/8/1Q6/6K1 w - - 0 1' },
    { id: 3, difficulty: 3, title: 'Mate in 1 – Rook takes rook',
        teaser: 'Back-rank tactic: white rook takes black on e8 and mates the king.',
        fen: '4r1k1/5ppp/8/8/8/8/4R3/4K3 w - - 0 1' },
    { id: 4, difficulty: 4, title: 'Mate in 1 – Scholar\'s mate',
        teaser: 'Well-known demo position: bishop on c4, queen on h5 — mate on f7.',
        fen: 'r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1' },
    { id: 5, difficulty: 5, title: 'Mate in 1 – Queen to d8',
        teaser: 'Central strike: queen from d4 delivers mate on d8.',
        fen: '6k1/5ppp/8/8/3Q4/8/6PP/6K1 w - - 0 1' }
];
let selectedPuzzleId = 1;
let puzzleSetupPhase = null;
let puzzleSetupStepIndex = 0;
let puzzleSetupSteps = [];
let puzzleSetupFastPollId = null;
let puzzleSetupLedIntervalId = null;
let puzzleSetupOccStable = 0;

function puzzleSquareToIndex(sq) {
    var c = sq.charCodeAt(0) - 97;
    var r = parseInt(sq.charAt(1), 10) - 1;
    return r * 8 + c;
}

function buildPuzzleSetupStepsFromFen(fen) {
    var steps = [];
    if (!fen) return steps;
    var boardPart = fen.split(' ')[0];
    var row = 7;
    var col = 0;
    var i;
    for (i = 0; i < boardPart.length; i++) {
        var ch = boardPart.charAt(i);
        if (ch === '/') {
            row--;
            col = 0;
            continue;
        }
        if (ch >= '1' && ch <= '8') {
            col += parseInt(ch, 10);
            continue;
        }
        var sq = String.fromCharCode(97 + col) + (row + 1);
        var sym = pieceSymbols[ch] || ch;
        steps.push({
            sq: sq,
            piece: ch,
            label: sym + ' → ' + sq.toUpperCase()
        });
        col++;
    }
    return steps;
}

function puzzleSetupStopLedRefresh() {
    if (puzzleSetupLedIntervalId) {
        clearInterval(puzzleSetupLedIntervalId);
        puzzleSetupLedIntervalId = null;
    }
}

function puzzleSetupStopFastPoll() {
    if (puzzleSetupFastPollId) {
        clearInterval(puzzleSetupFastPollId);
        puzzleSetupFastPollId = null;
    }
}

function puzzleSetupApplyLed(sq) {
    return fetch('/api/game/hint_highlight', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ to: sq })
    }).catch(function () {});
}

function puzzleSetupStartLedRefresh(sq) {
    puzzleSetupStopLedRefresh();
    puzzleSetupApplyLed(sq);
    puzzleSetupLedIntervalId = setInterval(function () {
        puzzleSetupApplyLed(sq);
    }, SETUP_TUTORIAL_REFRESH_MS);
}

function puzzleRenderList() {
    var list = document.getElementById('puzzle-list');
    if (!list) return;
    list.innerHTML = '';
    PUZZLE_DEFS.forEach(function (p) {
        var btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'set-btn set-btn-sm';
        btn.style.textAlign = 'left';
        btn.style.opacity = (p.id === selectedPuzzleId) ? '1' : '0.85';
        btn.textContent = '[' + p.difficulty + '/5] ' + p.title + ' - ' + p.teaser;
        btn.onclick = function () {
            selectedPuzzleId = p.id;
            puzzleRenderList();
        };
        list.appendChild(btn);
    });
}

function puzzleUpdateGuidedMessage(status) {
    var box = document.getElementById('puzzle-guided-message');
    if (!box) return;
    var p = status && status.puzzle ? status.puzzle : null;
    if (!p) {
        box.textContent = '';
        return;
    }
    if (p.message && p.message.length > 0) {
        box.textContent = p.message;
    } else if (p.active === true) {
        box.textContent = 'Puzzle is running. Play the required move.';
    } else if (p.setup_active === true) {
        box.textContent = 'Set up the pieces by the LEDs (same order as starting position).';
    } else {
        box.textContent = '';
    }
}

function puzzleResetPanelsToIntro() {
    var intro = document.getElementById('puzzle-intro-panel');
    var setup = document.getElementById('puzzle-setup-panel');
    var conf = document.getElementById('puzzle-confirm-panel');
    if (intro) intro.style.display = '';
    if (setup) setup.style.display = 'none';
    if (conf) conf.style.display = 'none';
}

function puzzleSetOverlayVisible(vis) {
    var ov = document.getElementById('puzzle-overlay');
    if (!ov) return;
    if (vis) {
        ov.style.display = 'flex';
        ov.classList.add('overlay-visible');
    } else {
        ov.classList.remove('overlay-visible');
        ov.style.display = 'none';
    }
}

function openPuzzleIntro() {
    var ov = document.getElementById('puzzle-overlay');
    if (!ov) return;
    puzzleResetPanelsToIntro();
    puzzleSetOverlayVisible(true);
    puzzleRenderList();
    puzzleUpdateGuidedMessage(statusData || {});
}

async function puzzlePrepareSelected() {
    try {
        var res = await fetch('/api/game/puzzle', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'prepare', id: selectedPuzzleId })
        });
        if (!res.ok) {
            if (typeof console !== 'undefined' && console.warn) console.warn('puzzle prepare', res.status);
            return;
        }
    } catch (e) {
        if (typeof console !== 'undefined' && console.warn) console.warn(e);
        return;
    }
    if (typeof fetchData === 'function') await fetchData();
    var def = PUZZLE_DEFS.filter(function (x) { return x.id === selectedPuzzleId; })[0];
    puzzleSetupSteps = buildPuzzleSetupStepsFromFen(def ? def.fen : '');
    puzzleSetupStepIndex = 0;
    puzzleSetupOccStable = 0;
    var intro = document.getElementById('puzzle-intro-panel');
    var setup = document.getElementById('puzzle-setup-panel');
    var conf = document.getElementById('puzzle-confirm-panel');
    if (puzzleSetupSteps.length === 0) {
        puzzleSetupPhase = 'confirm';
        if (intro) intro.style.display = 'none';
        if (setup) setup.style.display = 'none';
        if (conf) conf.style.display = '';
        if (typeof fetchData === 'function') {
            fetchData().then(function () {
                var w = document.getElementById('puzzle-confirm-warn');
                if (w && statusData && statusData.puzzle) {
                    w.textContent = statusData.puzzle.physical_match === false
                        ? 'Warning: the physical board may not match exactly.'
                        : 'You can start the puzzle.';
                    w.style.display = '';
                }
            });
        }
        return;
    }
    puzzleSetupPhase = 'run';
    if (intro) intro.style.display = 'none';
    if (setup) setup.style.display = '';
    if (conf) conf.style.display = 'none';
    puzzleSetupRenderStep();
    puzzleSetupFastPollId = setInterval(puzzleSetupPollOccupancy, SETUP_TUTORIAL_FAST_POLL_MS);
}

function puzzleSetupRenderStep() {
    var st = puzzleSetupSteps[puzzleSetupStepIndex];
    var pe = document.getElementById('puzzle-setup-piece-display');
    var ins = document.getElementById('puzzle-setup-instruction');
    var pr = document.getElementById('puzzle-setup-progress');
    if (!st) return;
    if (pe) {
        var isW = (st.piece >= 'a' && st.piece <= 'z');
        var srcP = pieceImgSrc(st.piece);
        if (srcP) {
            pe.className = 'setup-tutorial-piece-glyph piece has-img ' + (isW ? 'white' : 'black');
            pe.innerHTML = '<img src="' + srcP + '" alt="" draggable="false">';
        } else {
            pe.innerHTML = '';
            pe.textContent = pieceSymbols[st.piece] || st.piece || '?';
            pe.className = 'setup-tutorial-piece-glyph piece ' + (isW ? 'white' : 'black');
        }
    }
    if (ins) ins.textContent = 'Place the piece on square ' + st.sq.toUpperCase();
    if (pr) {
        pr.textContent = 'Krok ' + (puzzleSetupStepIndex + 1) + ' / ' + puzzleSetupSteps.length + ' — ' + st.label;
    }
    puzzleSetupStartLedRefresh(st.sq);
    puzzleSetupOccStable = 0;
}

function puzzleSetupPollOccupancy() {
    if (puzzleSetupPhase !== 'run') return;
    fetch('/api/status')
        .then(function (r) { return r.json(); })
        .then(function (st) {
            if (!st.puzzle || st.puzzle.setup_active !== true) return;
            if (!st.matrix_occupied || !Array.isArray(st.matrix_occupied)) return;
            var stp = puzzleSetupSteps[puzzleSetupStepIndex];
            if (!stp) return;
            var idx = puzzleSquareToIndex(stp.sq);
            if (Number(st.matrix_occupied[idx]) === 1) {
                puzzleSetupOccStable++;
                if (puzzleSetupOccStable >= SETUP_TUTORIAL_OCC_STABLE_TICKS) {
                    puzzleSetupAdvance(true);
                }
            } else {
                puzzleSetupOccStable = 0;
            }
        })
        .catch(function () {});
}

function puzzleSetupAdvance(fromAuto) {
    if (puzzleSetupPhase !== 'run') return;
    puzzleSetupStopLedRefresh();
    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    puzzleSetupOccStable = 0;
    puzzleSetupStepIndex++;
    if (puzzleSetupStepIndex >= puzzleSetupSteps.length) {
        puzzleSetupStopFastPoll();
        puzzleSetupPhase = 'confirm';
        var setup = document.getElementById('puzzle-setup-panel');
        var conf = document.getElementById('puzzle-confirm-panel');
        if (setup) setup.style.display = 'none';
        if (conf) conf.style.display = '';
        if (typeof fetchData === 'function') {
            fetchData().then(function () {
                var w = document.getElementById('puzzle-confirm-warn');
                if (w && statusData && statusData.puzzle) {
                    if (statusData.puzzle.physical_match === false) {
                        w.textContent = 'Warning: the physical board may not match exactly (sensors do not know piece type). You can still start the puzzle.';
                        w.style.display = '';
                    } else {
                        w.textContent = 'Physical occupancy matches the position.';
                        w.style.display = '';
                    }
                }
            });
        }
        return;
    }
    puzzleSetupRenderStep();
}

function puzzleSetupBack() {
    if (puzzleSetupPhase !== 'run') return;
    if (puzzleSetupStepIndex <= 0) return;
    puzzleSetupStopLedRefresh();
    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    puzzleSetupOccStable = 0;
    puzzleSetupStepIndex--;
    puzzleSetupRenderStep();
}

function puzzleSetupSkip() {
    puzzleSetupAdvance(false);
}

async function puzzleExecuteStart() {
    if (statusData && statusData.puzzle && statusData.puzzle.physical_match === false) {
        if (!window.confirm('The physical board may not match the expected position. Start the puzzle?')) {
            return;
        }
    }
    try {
        await fetch('/api/game/puzzle', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'start', id: selectedPuzzleId })
        });
    } catch (e) {
        if (typeof console !== 'undefined' && console.warn) console.warn(e);
    }
    puzzleSetupPhase = null;
    puzzleResetPanelsToIntro();
    puzzleSetOverlayVisible(false);
    if (typeof fetchData === 'function') fetchData();
}

async function puzzleBackToIntroFromSetup() {
    puzzleSetupStopFastPoll();
    puzzleSetupStopLedRefresh();
    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    puzzleSetupPhase = null;
    try {
        await fetch('/api/game/puzzle', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'cancel' })
        });
    } catch (e) {}
    puzzleResetPanelsToIntro();
    if (typeof fetchData === 'function') fetchData();
}

async function puzzleCancel() {
    puzzleSetupStopFastPoll();
    puzzleSetupStopLedRefresh();
    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    puzzleSetupPhase = null;
    try {
        await fetch('/api/game/puzzle', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'cancel' })
        });
    } catch (e) {}
    puzzleResetPanelsToIntro();
    puzzleSetOverlayVisible(false);
    if (typeof fetchData === 'function') fetchData();
}

window.openPuzzleIntro = openPuzzleIntro;
window.puzzlePrepareSelected = puzzlePrepareSelected;
window.puzzleExecuteStart = puzzleExecuteStart;
window.puzzleCancel = puzzleCancel;
window.puzzleSetupBack = puzzleSetupBack;
window.puzzleSetupSkip = puzzleSetupSkip;
window.puzzleBackToIntroFromSetup = puzzleBackToIntroFromSetup;

function updateStatus(status) {
    statusData = status;
    // ... existing implementation ...
    const gameStateEl = document.getElementById('game-state');
    const playerEl = document.getElementById('current-player');
    if (gameStateEl) gameStateEl.textContent = status.game_state || '-';
    if (playerEl) {
        const p = status.current_player;
        playerEl.textContent = (p === 'White') ? 'White' : (p === 'Black') ? 'Black' : (p || '-');
    }
    document.getElementById('move-count').textContent = status.move_count || 0;
    document.getElementById('in-check').textContent = status.in_check ? 'Ano' : 'Ne';

    // Brightness (Settings) – sync slider and label from status
    const b = status.brightness;
    if (typeof b === 'number' && b >= 0 && b <= 100) {
        const valueEl = document.getElementById('brightness-value');
        const sliderEl = document.getElementById('brightness-slider');
        if (valueEl) valueEl.textContent = b + '%';
        if (sliderEl && Number(sliderEl.value) !== b) sliderEl.value = b;
    }
    const gl = status.led_guidance_level;
    if (typeof gl === 'number' && gl >= 1 && gl <= 5) {
        const ledGuidanceEl = document.getElementById('led-guidance-level');
        if (ledGuidanceEl && String(ledGuidanceEl.value) !== String(gl)) {
            ledGuidanceEl.value = String(gl);
        }
    }
    puzzleUpdateGuidedMessage(status);
    updatePuzzleStatusPanel(status, { offline: false });
    if (typeof openingTrainerOnStatusUpdate === 'function') {
        openingTrainerOnStatusUpdate(status);
    }

    // Lamp (Settings) – mode, on/off, R/G/B from status
    const lightMode = status.light_mode;
    const lightState = status.light_state;
    const lr = status.light_r, lg = status.light_g, lb = status.light_b;
    const btnGame = document.getElementById('light-mode-game');
    const btnLamp = document.getElementById('light-mode-lamp');
    const lampControls = document.getElementById('light-lamp-controls');
    if (btnGame && btnLamp) {
        const isLamp = lightMode === 'lamp';
        btnGame.classList.toggle('active', !isLamp);
        btnLamp.classList.toggle('active', isLamp);
        if (lampControls) lampControls.style.display = isLamp ? '' : 'none';
    }
    const toggleEl = document.getElementById('light-state-toggle');
    if (toggleEl && typeof lightState === 'boolean' && toggleEl.checked !== lightState) toggleEl.checked = lightState;
    const rEl = document.getElementById('light-r'), gEl = document.getElementById('light-g'), bEl = document.getElementById('light-b');
    const rVal = document.getElementById('light-r-value'), gVal = document.getElementById('light-g-value'), bVal = document.getElementById('light-b-value');
    if (typeof lr === 'number' && lr >= 0 && lr <= 255 && rEl && rVal) { rEl.value = lr; rVal.textContent = lr; }
    if (typeof lg === 'number' && lg >= 0 && lg <= 255 && gEl && gVal) { gEl.value = lg; gVal.textContent = lg; }
    if (typeof lb === 'number' && lb >= 0 && lb <= 255 && bEl && bVal) { bEl.value = lb; bVal.textContent = lb; }

    // Promotion modal – show when backend waits for promotion choice (game_state === "promotion")
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
        if (liftedPieceEl) {
            const sl = pieceImgSrc(lifted.piece);
            if (sl) {
                liftedPieceEl.innerHTML = '<img src="' + sl + '" class="lifted-piece-img" alt="">';
            } else {
                liftedPieceEl.innerHTML = '';
                liftedPieceEl.textContent = pieceSymbols[lifted.piece] || '-';
            }
        }
        if (liftedPosEl) liftedPosEl.textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);
        const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);
        if (square) square.classList.add('lifted');
    } else {
        if (liftedPieceEl) {
            liftedPieceEl.innerHTML = '';
            liftedPieceEl.textContent = '-';
        }
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
        if (status.restore_state && status.restore_state.boot_new_game_triggered) {
            matrixGuardHidePanel();
            castlingMsg.textContent = 'A new game was started: 2 device starts detected without a move within 1 minute.';
            castlingMsg.style.display = 'block';
            castlingMsg.style.background = 'rgba(23,162,184,0.14)';
            castlingMsg.style.borderColor = 'rgba(23,162,184,0.45)';
            castlingMsg.style.color = '#4dd0e1';
        } else if (status.matrix_guard_active) {
            matrixGuardShowPanel(matrixGuardBuildMessage(status));
        } else {
            matrixGuardHidePanel();
            if (status.restore_state && status.restore_state.snapshot_restore_failed) {
                castlingMsg.textContent = 'Error restoring game from NVS — game runs from default / last known position. Check the log.';
                castlingMsg.style.display = 'block';
                castlingMsg.style.background = 'rgba(255,87,34,0.14)';
                castlingMsg.style.borderColor = 'rgba(255,87,34,0.45)';
                castlingMsg.style.color = '#ff8a65';
            } else if (status.restore_state && status.restore_state.snapshot_save_failed) {
                castlingMsg.textContent = 'Warning: saving game to NVS failed — last move may be missing after a power loss.';
                castlingMsg.style.display = 'block';
                castlingMsg.style.background = 'rgba(255,152,0,0.14)';
                castlingMsg.style.borderColor = 'rgba(255,152,0,0.45)';
                castlingMsg.style.color = '#ffb74d';
            } else if (status.castling_in_progress && status.castling_from && status.castling_to) {
                castlingMsg.textContent = 'Complete castling: move the rook from ' + status.castling_from + ' to ' + status.castling_to + '.';
                castlingMsg.style.display = 'block';
                castlingMsg.style.background = 'rgba(255,193,7,0.12)';
                castlingMsg.style.borderColor = 'rgba(255,193,7,0.4)';
                castlingMsg.style.color = '#ffc107';
            } else {
                if (status.game_end && status.game_end.ended) castlingMsg.style.display = 'none';
                else if (status.game_state !== 'active' && status.game_state !== 'playing') castlingMsg.style.display = 'none';
                else {
                    var keepMsg = castlingMsg.textContent.indexOf('Draw:') === 0 || castlingMsg.textContent.indexOf('hint') !== -1;
                    if (!keepMsg) castlingMsg.style.display = 'none';
                }
            }
        }
    }

    // Bot panel – show only in vs-bot mode; guidance when a piece is lifted
    updateBotStatusPanel(undefined, status);

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

    // Web lock: disable New game and Hint when locked
    var locked = !!(status.web_locked);
    var newGameBtn = document.getElementById('new-game-btn');
    var hintBtn = document.getElementById('hint-btn');
    if (newGameBtn) newGameBtn.disabled = locked;
    if (hintBtn) {
        if (locked) {
            hintBtn.disabled = true;
            hintBtn.title = 'Interface is locked';
        } else if (status.board_setup_tutorial === true) {
            hintBtn.disabled = true;
            hintBtn.title = 'Setup tutorial is running';
        } else if (typeof openingIsActive === 'function' && openingIsActive(status)) {
            hintBtn.disabled = true;
            hintBtn.title = 'Opening training is running';
        } else {
            if (typeof updateHintButtonLabel === 'function') updateHintButtonLabel();
        }
    }

    var tutOv = document.getElementById('setup-tutorial-overlay');
    if (tutOv && tutOv.style.display === 'flex') {
        setupTutorialUpdateIntroWarnings(status);
    }
}

function getGradeLabel(grade) {
    switch (grade) {
        case 'best': return 'Excellent';
        case 'good': return 'Good';
        case 'inaccuracy': return 'Inaccuracy';
        case 'mistake': return 'Mistake';
        case 'blunder': return 'Blunder';
        case 'unknown': return '—';
        case 'error': return 'Error';
        default: return '—';
    }
}

/** Short move-quality label for the overview (2–4 chars). */
function getGradeShortLabel(grade) {
    switch (grade) {
        case 'best': return 'Exc.';
        case 'good': return 'Good';
        case 'inaccuracy': return 'Inac.';
        case 'mistake': return 'Mist.';
        case 'blunder': return 'Blun.';
        case 'error': return '!';
        case 'unknown': return '—';
        default: return '—';
    }
}

function escapeHtml(s) {
    if (s == null) return '';
    return String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

/** How many recent moves in the compact preview (2–4). */
var HISTORY_PREVIEW_COUNT = 4;
var historyFullExpanded = false;
var historyDetailIndex = null;

function wireHistoryToolbarOnce() {
    var btn = document.getElementById('history-toggle-full');
    if (!btn || btn._historyWired) return;
    btn._historyWired = true;
    btn.addEventListener('click', function (e) {
        e.preventDefault();
        historyFullExpanded = !historyFullExpanded;
        btn.setAttribute('aria-expanded', historyFullExpanded ? 'true' : 'false');
        btn.textContent = historyFullExpanded ? 'Hide full history' : 'Full history';
        renderHistoryList();
    });
}

function createHistoryItemElement(move, actualIndex) {
    var item = document.createElement('div');
    item.className = 'history-item';
    item.dataset.moveIndex = String(actualIndex);
    var moveNum = Math.floor(actualIndex / 2) + 1;
    var isWhite = actualIndex % 2 === 0;
    var prefix = isWhite ? moveNum + '. ' : '';
    item.appendChild(document.createTextNode(prefix + move.from + ' → ' + move.to));
    var ev = moveEvaluations[actualIndex];
    if (ev) {
        var badge = document.createElement('span');
        badge.className = 'move-eval-badge move-eval-badge--' + (ev.grade || 'good');
        var fullTitle = ev.msg || getGradeLabel(ev.grade);
        badge.title = fullTitle;
        badge.setAttribute('aria-label', fullTitle);
        badge.textContent = getGradeShortLabel(ev.grade);
        item.appendChild(document.createTextNode(' '));
        item.appendChild(badge);
    }
    item.addEventListener('click', function (e) {
        e.stopPropagation();
        toggleHistoryMoveDetail(actualIndex);
    });
    return item;
}

function toggleHistoryMoveDetail(actualIndex) {
    var panel = document.getElementById('history-move-detail');
    if (!panel) return;
    if (historyDetailIndex === actualIndex) {
        panel.style.display = 'none';
        historyDetailIndex = null;
        return;
    }
    historyDetailIndex = actualIndex;
    var ev = moveEvaluations[actualIndex];
    var move = historyData[actualIndex];
    var san = move ? (move.from + ' → ' + move.to) : '—';
    var gradeLine = ev ? getGradeLabel(ev.grade) : 'Not evaluated';
    var gExtra = ev && ev.grade ? (' history-detail-grade--' + ev.grade) : '';
    var msg = ev && ev.msg
        ? ev.msg
        : ('Move evaluation is not available. Turn on "Move evaluation" in Settings and play ' +
            'with an internet connection — quality is filled in for each new move.');
    panel.innerHTML =
        '<div class="history-detail-inner">' +
        '<div class="history-detail-san">' + escapeHtml(san) + '</div>' +
        '<div class="history-detail-grade' + gExtra + '">' + escapeHtml(gradeLine) + '</div>' +
        '<p class="history-detail-msg">' + escapeHtml(msg) + '</p>' +
        '<button type="button" class="history-detail-review-btn btn-history-review">' +
        'Show position on board</button></div>';
    var rb = panel.querySelector('.history-detail-review-btn');
    if (rb) {
        rb.addEventListener('click', function (ev2) {
            ev2.stopPropagation();
            enterReviewMode(actualIndex);
        });
    }
    panel.style.display = 'block';
}

function renderHistoryList() {
    var previewEl = document.getElementById('history-preview');
    var fullEl = document.getElementById('history');
    var wrap = document.getElementById('history-full-wrap');
    wireHistoryToolbarOnce();

    if (previewEl && fullEl) {
        var total = historyData.length;
        var rev;
        previewEl.innerHTML = '';
        fullEl.innerHTML = '';
        if (historyFullExpanded) {
            previewEl.style.display = 'none';
            if (wrap) wrap.style.display = 'block';
            for (rev = 0; rev < total; rev++) {
                var ai = total - 1 - rev;
                fullEl.appendChild(createHistoryItemElement(historyData[ai], ai));
            }
        } else {
            previewEl.style.display = '';
            if (wrap) wrap.style.display = 'none';
            var nPrev = Math.min(HISTORY_PREVIEW_COUNT, total);
            for (rev = 0; rev < nPrev; rev++) {
                var ai2 = total - 1 - rev;
                previewEl.appendChild(createHistoryItemElement(historyData[ai2], ai2));
            }
        }
        var tbtn = document.getElementById('history-toggle-full');
        if (tbtn) {
            tbtn.style.display = total > HISTORY_PREVIEW_COUNT ? 'inline-block' : 'none';
        }
        if (historyDetailIndex != null &&
            (historyDetailIndex < 0 || historyDetailIndex >= total)) {
            historyDetailIndex = null;
            var p = document.getElementById('history-move-detail');
            if (p) p.style.display = 'none';
        }
        return;
    }

    var historyBox = document.getElementById('history');
    if (!historyBox) return;
    historyBox.innerHTML = '';
    historyData.slice().reverse().forEach((move, index) => {
        var actualIndex = historyData.length - 1 - index;
        historyBox.appendChild(createHistoryItemElement(move, actualIndex));
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
        const s = pieceImgSrc(p);
        if (s) {
            piece.innerHTML = '<img src="' + s + '" alt="">';
        } else {
            piece.textContent = pieceSymbols[p] || p;
        }
        whiteBox.appendChild(piece);
    });
    captured.black_captured.forEach(p => {
        const piece = document.createElement('div');
        piece.className = 'captured-piece';
        const s2 = pieceImgSrc(p);
        if (s2) {
            piece.innerHTML = '<img src="' + s2 + '" alt="">';
        } else {
            piece.textContent = pieceSymbols[p] || p;
        }
        blackBox.appendChild(piece);
    });
}

async function fetchData() {
    if (reviewMode || sandboxMode) return;
    if (fetchDataInFlight) return;
    fetchDataInFlight = true;
    try {
        var payload = await fetchGameSnapshot();
        var board = payload.board;
        var status = payload.status;
        var history = payload.history;
        var captured = payload.captured;
        if (payload.clock) {
            lastSnapshotClockInfo = payload.clock;
            lastSnapshotClockAt = Date.now();
            applyTimerInfo(payload.clock);
        } else {
            lastSnapshotClockInfo = null;
            lastSnapshotClockAt = 0;
        }
        updateBoard(board.board);
        updateStatus(status);
        if (status && status.puzzle) {
            var pu = status.puzzle;
            if (pu.active || pu.setup_active || (pu.feedback && String(pu.feedback).toLowerCase() !== 'none')) {
                lastPuzzleSnapshotForOffline = {
                    active: !!pu.active,
                    setup_active: !!pu.setup_active,
                    feedback: pu.feedback || 'none',
                    message: pu.message || '',
                    title: pu.title || '',
                    teaser: pu.teaser || ''
                };
            } else {
                lastPuzzleSnapshotForOffline = null;
            }
        } else {
            lastPuzzleSnapshotForOffline = null;
        }
        updateHistory(history);
        updateCaptured(captured);

        var newHistoryLength = (history.moves && history.moves.length) ? history.moves.length : 0;
        var currentFen = boardAndStatusToFen(boardData, status, history);

        if (newHistoryLength === 0) {
            lastFen = null;
            lastHistoryLength = 0;
            moveEvaluations = {};
            historyFullExpanded = false;
            historyDetailIndex = null;
            var hmd = document.getElementById('history-move-detail');
            if (hmd) hmd.style.display = 'none';
            var htf = document.getElementById('history-toggle-full');
            if (htf) {
                htf.textContent = 'Full history';
                htf.setAttribute('aria-expanded', 'false');
            }
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
                var lastMoveByBot = gameMode === 'bot' && ((botSettings.side === 'black' && lastMoveByWhite) || (botSettings.side === 'white' && !lastMoveByWhite));
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
            if (!castlingInProgress && newHistoryLength >= lastHistoryLength) {
                lastFen = currentFen;
                lastHistoryLength = newHistoryLength;
            }
        }
        checkBotTurn(status, currentFen);
    } catch (error) {
        console.error('Fetch error:', error);
        if (lastPuzzleSnapshotForOffline) {
            updatePuzzleStatusPanel({ puzzle: lastPuzzleSnapshotForOffline }, { offline: true });
        }
    } finally {
        fetchDataInFlight = false;
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

    var botLedTargetEl = document.getElementById('bot-led-target-only-after-lift');
    if (botLedTargetEl) botLedTargetEl.checked = getBotLedTargetOnlyAfterLift();

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

function ensureBoardFocusExitButton() {
    if (document.getElementById('web-board-focus-exit')) return;
    var b = document.createElement('button');
    b.id = 'web-board-focus-exit';
    b.type = 'button';
    b.className = 'web-board-focus-exit-btn';
    b.textContent = 'Full app';
    b.setAttribute('aria-label', 'Show full app');
    b.onclick = function () {
        exitWebBoardFocusMode();
    };
    document.body.appendChild(b);
}

function exitWebBoardFocusMode() {
    document.documentElement.classList.remove('web-board-focus');
    document.body.classList.remove('web-board-focus');
    try {
        document.body.style.position = '';
        document.body.style.width = '';
    } catch (e) { /* ignore */ }
    var x = document.getElementById('web-board-focus-exit');
    if (x) x.remove();
    try {
        localStorage.setItem('chessWebBoardFocus', '0');
    } catch (e2) { /* ignore */ }
}

/**
 * Board + clocks only: no page scrolling; drag pieces when web control is on.
 * Enable: URL ?focus=1 or ?board=1, or localStorage chessWebBoardFocus=1
 */
function initWebBoardFocusMode() {
    try {
        var p = new URLSearchParams(window.location.search);
        var q = p.get('focus') === '1' || p.get('board') === '1';
        var ls = false;
        try {
            ls = localStorage.getItem('chessWebBoardFocus') === '1';
        } catch (e0) { /* ignore */ }
        if (!q && !ls) return;
        document.documentElement.classList.add('web-board-focus');
        document.body.classList.add('web-board-focus');
        try {
            localStorage.setItem('chessWebBoardFocus', '1');
        } catch (e1) { /* ignore */ }
        var rc = document.getElementById('remote-control-enabled');
        if (rc && !rc.checked) {
            rc.checked = true;
            if (typeof toggleRemoteControl === 'function') toggleRemoteControl();
        }
        ensureBoardFocusExitButton();
        if (typeof console !== 'undefined' && console.log) {
            console.log('[staging] web-board-focus: board + clocks only; drag pieces (remote control on)');
        }
    } catch (e) {
        if (typeof console !== 'undefined' && console.warn) console.warn('initWebBoardFocusMode', e);
    }
}

window.exitWebBoardFocusMode = exitWebBoardFocusMode;

async function bootstrapChessWebUi() {
    try {
        await loadUiPrefsFromDevice();
    } catch (e) {
        if (typeof console !== 'undefined' && console.warn) {
            console.warn('bootstrapChessWebUi loadUiPrefsFromDevice', e);
        }
    }
    applyDevicePrefsToDom();
    initWebBoardFocusMode();
    console.log('🚀 Creating chess board...');
    initializeApp();
    console.log('✅ Chess JavaScript loaded successfully!');
    console.log('⏱️ About to initialize timer system...');
    try {
        initTimerSystem();
        console.log('✅ initTimerSystem() called successfully');
    } catch (error) {
        console.error('❌ CRITICAL ERROR in initTimerSystem():', error);
        if (error && error.stack) console.error('Stack:', error.stack);
    }
}

bootstrapChessWebUi();

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
/** -1 = not yet determined; 1000 when time control is active, 8000 when off. */
let timerPollMs = -1;
/** Fresh `clock` from GET /api/game/snapshot — saves GET /api/timer in updateTimerDisplay. */
let lastSnapshotClockInfo = null;
let lastSnapshotClockAt = 0;
const SNAPSHOT_CLOCK_FRESH_MS = 900;
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

    // Check whether time control is active
    const isTimerActive = timerData.config && timerData.config.type !== 0;

    if (isTimerActive) {
        const formattedTime = formatTime(timeMs);
        timeElement.textContent = formattedTime;
        playerElement.classList.remove('low-time', 'critical-time');
        if (timeMs < 5000) playerElement.classList.add('critical-time');
        else if (timeMs < 30000) playerElement.classList.add('low-time');
    } else {
        // Without time control - show "--:--" and remove all warning classes
        timeElement.textContent = '--:--';
        playerElement.classList.remove('low-time', 'critical-time', 'active');
        return; // Do nothing else
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

    // Check whether time control is active
    if (timerInfo.config.type === 0) {
        // Without time control - hide progress bars
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
    // Do not check warnings if time control is not active
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
    // Do not check expiry if time control is not active
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
    selectedTimeControl = parseInt(select.value, 10);
    toggleCustomSettings();
    if (applyBtn) applyBtn.disabled = false;
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
    selectedTimeControl = parseInt(timeControlSelect.value, 10);
    toggleCustomSettings();
    // Enable button if a time control is selected (not 0 = None)
    if (selectedTimeControl !== 0 && applyButton) {
        applyButton.disabled = false;
    }
    console.log('🔵 Starting timer update loop immediately...');
    // Start timer loop immediately (no delay)
    startTimerUpdateLoop();
}

function rescheduleTimerPollInterval() {
    var active = timerData.config && timerData.config.type !== 0;
    var want = active ? 1000 : 8000;
    if (timerPollMs === want && timerUpdateInterval) {
        return;
    }
    timerPollMs = want;
    if (timerUpdateInterval) {
        clearInterval(timerUpdateInterval);
    }
    timerUpdateInterval = setInterval(async () => {
        try {
            await updateTimerDisplay();
        } catch (error) {
            console.error('Timer update loop error:', error);
        }
    }, timerPollMs);
}

function startTimerUpdateLoop() {
    if (timerUpdateInterval) {
        clearInterval(timerUpdateInterval);
        timerUpdateInterval = null;
    }
    timerPollMs = -1;
    updateTimerDisplay().catch(e => console.error('Initial timer update failed:', e));
}

function applyTimerInfo(timerInfo) {
    timerData = timerInfo;
    const tcSel = document.getElementById('time-control-select');
    if (tcSel && timerInfo.config && typeof timerInfo.config.type === 'number') {
        var tt = timerInfo.config.type;
        selectedTimeControl = tt;
        if (tcSel.value !== String(tt)) {
            tcSel.value = String(tt);
        }
        toggleCustomSettings();
    }
    updatePlayerTime('white', timerInfo.white_time_ms);
    updatePlayerTime('black', timerInfo.black_time_ms);
    updateActivePlayer(timerInfo.is_white_turn);
    updateProgressBars(timerInfo);
    updateTimerStats(timerInfo);
    const pauseBtn = document.getElementById('pause-timer');
    const resumeBtn = document.getElementById('resume-timer');
    const resetBtn = document.getElementById('reset-timer');
    const isTimerActive = timerInfo.config && timerInfo.config.type !== 0;
    if (pauseBtn) pauseBtn.disabled = !isTimerActive;
    if (resumeBtn) resumeBtn.disabled = !isTimerActive;
    if (resetBtn) resetBtn.disabled = !isTimerActive;
    if (isTimerActive) {
        checkTimeWarnings(timerInfo);
        if (timerInfo.time_expired) {
            handleTimeExpiration(timerInfo);
        }
    }
    rescheduleTimerPollInterval();
}

async function updateTimerDisplay() {
    try {
        if (lastSnapshotClockInfo && (Date.now() - lastSnapshotClockAt) < SNAPSHOT_CLOCK_FRESH_MS) {
            applyTimerInfo(lastSnapshotClockInfo);
            return;
        }
        const response = await fetch('/api/timer');
        if (response.ok) {
            const timerInfo = await response.json();
            applyTimerInfo(timerInfo);
        } else {
            console.error('Timer update failed:', response.status);
            rescheduleTimerPollInterval();
        }
    } catch (error) {
        console.error('Timer update error:', error);
        rescheduleTimerPollInterval();
    }
}

async function applyTimeControl() {
    const timeControlSelect = document.getElementById('time-control-select');
    const timeControlType = parseInt(timeControlSelect.value);
    let config = { type: timeControlType };
    if (timeControlType === 14) {
        const minutes = parseInt(document.getElementById('custom-minutes').value);
        const increment = parseInt(document.getElementById('custom-increment').value);
        if (minutes < 1 || minutes > 180) { alert('Minutes must be 1–180'); return; }
        if (increment < 0 || increment > 60) { alert('Increment must be 0–60 seconds'); return; }
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
            showTimeWarning('Time control set.', 'info');
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
            showTimeWarning('Clock paused', 'info');
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
            showTimeWarning('Clock resumed', 'info');
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
                showTimeWarning('Clock reset', 'info');
                console.log('✅ Timer reset successfully');
                await updateTimerDisplay();
            }
        } catch (error) {
            console.error('❌ Error resetting timer:', error);
        }
    }
}

// ============================================================================
// BRIGHTNESS (Settings → Device) – for inline onchange on brightness-slider
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
            if (typeof console !== 'undefined' && console.log) console.log('Brightness set to', num + '%');
        } else {
            console.warn('Brightness setting failed:', data.message || response.status);
        }
    } catch (err) {
        console.error('Brightness setting error:', err.message);
    }
}
window.setBrightness = setBrightness;

async function setLedGuidanceLevel(level) {
    const n = Math.min(5, Math.max(1, parseInt(level, 10)));
    if (Number.isNaN(n)) {
        return;
    }
    try {
        const response = await fetch('/api/settings/led_guidance', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ level: n })
        });
        const data = response.ok ? await response.json().catch(() => ({})) : {};
        if (data.success === false) {
            console.warn('LED hint setting failed:', data.message || response.status);
        }
    } catch (err) {
        console.error('LED hint setting error:', err.message);
    }
}
window.setLedGuidanceLevel = setLedGuidanceLevel;

// ============================================================================
// LAMP MODE (Settings → Device: Board / Lamp, color)
// ============================================================================

function showLightError(msg) {
    const el = document.getElementById('light-error-msg');
    if (el) {
        el.textContent = msg || 'Send error';
        el.style.display = '';
        setTimeout(function () { el.style.display = 'none'; el.textContent = ''; }, 2500);
    }
}

async function setLightModeGame() {
    try {
        const res = await fetch('/api/light/game_mode', { method: 'POST', headers: { 'Content-Type': 'application/json' } });
        if (res.ok) {
            const btnGame = document.getElementById('light-mode-game');
            const btnLamp = document.getElementById('light-mode-lamp');
            const lampControls = document.getElementById('light-lamp-controls');
            if (btnGame) btnGame.classList.add('active');
            if (btnLamp) btnLamp.classList.remove('active');
            if (lampControls) lampControls.style.display = 'none';
        } else {
            showLightError('Switch to board mode failed');
        }
    } catch (e) {
        console.warn('setLightModeGame failed:', e.message);
        showLightError('Network error');
    }
}

async function setLightModeLamp(r, g, b, state) {
    const rr = Math.min(255, Math.max(0, parseInt(r, 10) || 255));
    const gg = Math.min(255, Math.max(0, parseInt(g, 10) || 255));
    const bb = Math.min(255, Math.max(0, parseInt(b, 10) || 255));
    try {
        const res = await fetch('/api/light/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: !!state, r: rr, g: gg, b: bb })
        });
        if (res.ok) {
            const btnGame = document.getElementById('light-mode-game');
            const btnLamp = document.getElementById('light-mode-lamp');
            const lampControls = document.getElementById('light-lamp-controls');
            if (btnGame) btnGame.classList.remove('active');
            if (btnLamp) btnLamp.classList.add('active');
            if (lampControls) lampControls.style.display = '';
        } else {
            const data = await res.json().catch(function () { return {}; });
            showLightError(data.message || 'Device not ready (503)');
        }
    } catch (e) {
        console.warn('setLightModeLamp failed:', e.message);
        showLightError('Network error');
    }
}

let lightCommandDebounceId = null;
function sendLightCommandDebounced() {
    if (lightCommandDebounceId) clearTimeout(lightCommandDebounceId);
    lightCommandDebounceId = setTimeout(function () {
        lightCommandDebounceId = null;
        const rEl = document.getElementById('light-r'), gEl = document.getElementById('light-g'), bEl = document.getElementById('light-b');
        const toggleEl = document.getElementById('light-state-toggle');
        if (!rEl || !gEl || !bEl) return;
        const r = parseInt(rEl.value, 10) || 0, g = parseInt(gEl.value, 10) || 0, b = parseInt(bEl.value, 10) || 0;
        const state = toggleEl ? toggleEl.checked : true;
        setLightModeLamp(r, g, b, state);
    }, 120);
}

function initLightControls() {
    const btnGame = document.getElementById('light-mode-game');
    const btnLamp = document.getElementById('light-mode-lamp');
    const toggleEl = document.getElementById('light-state-toggle');
    const ledGuidanceEl = document.getElementById('led-guidance-level');
    const rEl = document.getElementById('light-r'), gEl = document.getElementById('light-g'), bEl = document.getElementById('light-b');
    if (btnGame) btnGame.addEventListener('click', setLightModeGame);
    if (btnLamp) btnLamp.addEventListener('click', function () {
        const r = rEl ? parseInt(rEl.value, 10) : 255, g = gEl ? parseInt(gEl.value, 10) : 255, b = bEl ? parseInt(bEl.value, 10) : 255;
        setLightModeLamp(r, g, b, true);
    });
    if (toggleEl) toggleEl.addEventListener('change', sendLightCommandDebounced);
    if (ledGuidanceEl) {
        ledGuidanceEl.addEventListener('change', function () {
            setLedGuidanceLevel(ledGuidanceEl.value);
        });
    }
    function updateRgbLabel(id, valueId) {
        const el = document.getElementById(id), val = document.getElementById(valueId);
        if (el && val) { val.textContent = el.value; }
    }
    if (rEl) { rEl.addEventListener('input', function () { updateRgbLabel('light-r', 'light-r-value'); sendLightCommandDebounced(); }); }
    if (gEl) { gEl.addEventListener('input', function () { updateRgbLabel('light-g', 'light-g-value'); sendLightCommandDebounced(); }); }
    if (bEl) { bEl.addEventListener('input', function () { updateRgbLabel('light-b', 'light-b-value'); sendLightCommandDebounced(); }); }
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initLightControls);
} else {
    setTimeout(initLightControls, 100);
}

// ============================================================================
// NEW GAME (Settings → action bar "New game") – for inline onclick
// ============================================================================

function getConfirmNewGameEnabled() {
    const cb = document.getElementById('confirm-new-game');
    if (cb) return cb.checked;
    return !!devicePrefs.chess_confirm_new_game;
}

async function startNewGame() {
    if (isWebLocked()) {
        alert('Interface is locked. Unlock via UART.');
        return;
    }
    if (getConfirmNewGameEnabled()) {
        if (!confirm('Are you sure you want to start a new game? The current game will end.')) {
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
            if (typeof console !== 'undefined' && console.log) console.log('New game started. Mode:', gameMode, 'Player Side:', botSettings.side);

            await fetchData();

            if (gameMode === 'bot' && botSettings.side === 'black') {
                // Player is Black -> Bot is White -> Bot moves immediately
                // We pass initial FEN (start pos)
                const startFen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';
                setTimeout(() => checkBotTurn({ current_player: 'White', game_state: 'active' }, startFen), 500);
            }
        } else {
            console.warn('New game failed:', response.status);
        }
    } catch (err) {
        console.error('Error startNewGame:', err.message);
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
        msgEl.textContent = drawn === 'white' ? 'Draw: You play White.' : 'Draw: You play Black.';
        msgEl.style.display = 'block';
    }
    if (typeof saveBotSettings === 'function') saveBotSettings();
}
window.handleRandomDraw = handleRandomDraw;

// ============================================================================
// PROMOTION MODAL (Q/R/B/N and Cancel) – for inline onclick
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
        console.error('Error selectPromotion:', err.message);
    }
}

function cancelPromotion() {
    const modal = document.getElementById('promotion-modal');
    if (modal) modal.style.display = 'none';
    // Unblock the game with the default choice (Queen)
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
    if (!e.target.closest('.history-block')) {
        var pd = document.getElementById('history-move-detail');
        if (pd) {
            pd.style.display = 'none';
            historyDetailIndex = null;
        }
    }
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
            headers: boardApiAuthHeaders({ 'Content-Type': 'application/json' }),
            body: JSON.stringify({ ssid: ssid, password: password })
        });
        const data = await response.json();
        if (data.success) {
            alert('WiFi saved. Press "Connect STA".');
        } else {
            alert('Saving WiFi failed: ' + data.message);
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

async function connectSTA() {
    try {
        const response = await fetch('/api/wifi/connect', {
            method: 'POST',
            headers: boardApiAuthHeaders()
        });
        const data = await response.json();
        if (data.success) {
            alert('Connecting to WiFi...');
            setTimeout(updateWiFiStatus, 1500);
        } else {
            alert('Connection failed: ' + data.message);
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

async function disconnectSTA() {
    try {
        const response = await fetch('/api/wifi/disconnect', {
            method: 'POST',
            headers: boardApiAuthHeaders()
        });
        const data = await response.json();
        if (data.success) {
            alert('Disconnected from WiFi');
            setTimeout(updateWiFiStatus, 1000);
        } else {
            alert('Disconnect failed: ' + data.message);
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

async function clearWiFiConfig() {
    if (!confirm('Really delete the saved WiFi configuration? ESP will disconnect from the network.')) {
        return;
    }
    try {
        const response = await fetch('/api/wifi/clear', {
            method: 'POST',
            headers: boardApiAuthHeaders()
        });
        const data = await response.json();
        if (data.success) {
            alert('WiFi configuration deleted.');
            setTimeout(updateWiFiStatus, 500);
        } else {
            alert('Delete failed: ' + (data.message || 'unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

/** Entire NVS partition + restart (same as BLE `factory_reset`). */
async function factoryResetDevice() {
    if (!confirm('Factory reset: erases EVERYTHING in NVS (WiFi, MQTT, saved game, UI) and restarts the board. Continue?')) {
        return;
    }
    if (!confirm('Last chance: really erase the entire NVS flash?')) {
        return;
    }
    try {
        const response = await fetch('/api/system/factory_reset', {
            method: 'POST',
            headers: boardApiAuthHeaders({ 'Content-Type': 'application/json' }),
            body: JSON.stringify({ confirm: 'erase_all_nvs' })
        });
        var data = {};
        try {
            data = await response.json();
        } catch (e) {
            data = {};
        }
        if (response.ok && data.success) {
            alert('Reset scheduled — the board will reboot shortly with empty NVS.');
        } else {
            alert('Factory reset failed: ' + (data.message || response.status));
        }
    } catch (error) {
        console.error('factoryResetDevice:', error);
        alert('Error during factory reset');
    }
}

/** True if API returns stored STA SSID from NVS (not a firmware placeholder). */
function wifiStatusHasSavedStaSsid(staSsid) {
    if (staSsid == null || typeof staSsid !== 'string') {
        return false;
    }
    const s = staSsid.trim();
    if (s === '') {
        return false;
    }
    const lower = s.toLowerCase();
    if (lower === 'not configured' || lower === 'nenastaveno') {
        return false;
    }
    return true;
}

async function updateWiFiStatus() {
    try {
        const response = await fetch('/api/wifi/status');
        const data = await response.json();
        document.getElementById('ap-ssid').textContent = data.ap_ssid || 'ESP32-CzechMate';
        document.getElementById('ap-ip').textContent = data.ap_ip || '192.168.4.1';
        document.getElementById('ap-clients').textContent = data.ap_clients || 0;
        document.getElementById('sta-ssid').textContent =
            wifiStatusHasSavedStaSsid(data.sta_ssid) ? data.sta_ssid : 'Not set';
        document.getElementById('sta-ip').textContent = data.sta_ip || 'Not connected';
        document.getElementById('sta-connected').textContent = data.sta_connected ? 'ano' : 'ne';
        if (wifiStatusHasSavedStaSsid(data.sta_ssid)) {
            document.getElementById('wifi-ssid').value = data.sta_ssid;
        }
        // Device: Lock and Online (data from the same API)
        const lockEl = document.getElementById('web-lock-status');
        if (lockEl) {
            lockEl.textContent = data.locked ? 'Locked' : 'Unlocked';
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
window.factoryResetDevice = factoryResetDevice;

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
            alert('Demo mode switch failed: ' + (data.message || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error toggling demo mode:', error);
        alert('Error switching demo mode');
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
                statusEl.textContent = 'On';
                statusEl.style.color = '#4CAF50';
                statusEl.style.fontWeight = 'bold';
            } else {
                statusEl.textContent = 'Off';
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
        console.error('Error updating demo mode status:', error);
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

// Initialize starting position check setting on page load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', loadStartingPositionCheck);
} else {
    loadStartingPositionCheck();
}
