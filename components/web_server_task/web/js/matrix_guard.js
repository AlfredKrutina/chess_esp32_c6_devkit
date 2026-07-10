// ============================================================================
// MATRIX GUARD UI (Phase 4A — edit web/js/matrix_guard.js, run concat_web_js.py)
// ============================================================================
(function (global) {
    'use strict';

    function matrixGuardMaskToSquares(low, high) {
        const squares = [];
        const lowVal = Number(low) >>> 0;
        const highVal = Number(high) >>> 0;
        for (let i = 0; i < 32; i++) {
            if ((lowVal & (1 << i)) !== 0) {
                const col = i % 8;
                const row = Math.floor(i / 8);
                squares.push(String.fromCharCode(97 + col) + (row + 1));
            }
        }
        for (let i = 0; i < 32; i++) {
            if ((highVal & (1 << i)) !== 0) {
                const idx = i + 32;
                const col = idx % 8;
                const row = Math.floor(idx / 8);
                squares.push(String.fromCharCode(97 + col) + (row + 1));
            }
        }
        return squares;
    }

    function matrixGuardEnsureStyles() {
        if (document.getElementById('matrix-guard-styles')) return;
        var style = document.createElement('style');
        style.id = 'matrix-guard-styles';
        style.textContent = [
            '.matrix-guard-shell{display:flex;flex-direction:column;gap:10px;margin:0 0 12px;padding:12px 14px;border-radius:12px;',
            'border:1px solid rgba(220,53,69,0.42);background:rgba(220,53,69,0.12);}',
            '.matrix-guard-shell__head{display:flex;gap:10px;align-items:flex-start;}',
            '.matrix-guard-shell__icon{flex:0 0 auto;width:32px;height:32px;border-radius:999px;display:flex;align-items:center;justify-content:center;',
            'background:rgba(220,53,69,0.22);color:#ffb4b4;font-size:15px;line-height:1;}',
            '.matrix-guard-shell__title{margin:0 0 4px;font-size:14px;font-weight:600;color:#ff9a9a;}',
            '.matrix-guard-shell__message{margin:0;padding:0;border:none;background:transparent!important;color:#ffd0d0!important;',
            'font-size:13px;line-height:1.45;display:block!important;box-shadow:none!important;}',
            '.matrix-guard-shell__foot{display:flex;flex-direction:column;align-items:flex-end;gap:4px;}',
            '.matrix-guard-shell__action{padding:8px 14px;border-radius:8px;border:1px solid rgba(255,255,255,0.24);',
            'background:rgba(255,255,255,0.08);color:#fff;font-size:13px;font-weight:600;cursor:pointer;',
            'transition:background .15s,border-color .15s,opacity .15s,transform .15s;}',
            '.matrix-guard-shell__action:hover:not(:disabled){background:rgba(255,255,255,0.14);border-color:rgba(255,255,255,0.36);}',
            '.matrix-guard-shell__action:active:not(:disabled){transform:scale(0.98);}',
            '.matrix-guard-shell__action:disabled{opacity:.55;cursor:wait;}',
            '.matrix-guard-shell__action:focus-visible{outline:2px solid rgba(255,255,255,0.55);outline-offset:2px;}',
            '.matrix-guard-shell__hint{margin:0;font-size:11px;line-height:1.35;color:rgba(255,210,210,0.78);text-align:right;}'
        ].join('');
        document.head.appendChild(style);
    }

    function matrixGuardHidePanel() {
        var shell = document.getElementById('matrix-guard-shell');
        if (shell) shell.style.display = 'none';
        var castlingMsg = document.getElementById('castling-pending-message');
        if (castlingMsg) castlingMsg.classList.remove('matrix-guard-shell__message');
    }

    function matrixGuardShowPanel(message) {
        matrixGuardEnsureStyles();
        var castlingMsg = document.getElementById('castling-pending-message');
        if (!castlingMsg || !castlingMsg.parentNode) return;

        var shell = document.getElementById('matrix-guard-shell');
        if (!shell) {
            shell = document.createElement('div');
            shell.id = 'matrix-guard-shell';
            shell.className = 'matrix-guard-shell';
            shell.innerHTML =
                '<div class="matrix-guard-shell__head">' +
                '<span class="matrix-guard-shell__icon" aria-hidden="true">▦</span>' +
                '<div><p class="matrix-guard-shell__title">Srovnejte desku</p></div></div>' +
                '<div class="matrix-guard-shell__foot">' +
                '<button type="button" id="matrix-guard-clear-btn" class="matrix-guard-shell__action">Obnovit hru</button>' +
                '<p class="matrix-guard-shell__hint">Jen když jsou figurky fyzicky srovnané</p></div>';
            castlingMsg.parentNode.insertBefore(shell, castlingMsg);
            shell.insertBefore(castlingMsg, shell.querySelector('.matrix-guard-shell__foot'));
            castlingMsg.classList.add('matrix-guard-shell__message');

            var btn = document.getElementById('matrix-guard-clear-btn');
            if (btn) {
                btn.addEventListener('click', function () {
                    btn.disabled = true;
                    fetch('/api/game/guard_clear', { method: 'POST' })
                        .then(function () { return fetch('/api/status'); })
                        .then(function (r) { return r.json(); })
                        .then(function (st) { updateStatus(st); })
                        .catch(function () {})
                        .finally(function () { btn.disabled = false; });
                });
            }
        }

        castlingMsg.textContent = message;
        castlingMsg.style.display = 'block';
        shell.style.display = 'flex';
    }

    function matrixGuardBuildMessage(status) {
        var liftedSquares = matrixGuardMaskToSquares(status.matrix_guard_lifted_low, status.matrix_guard_lifted_high);
        var droppedSquares = matrixGuardMaskToSquares(status.matrix_guard_dropped_low, status.matrix_guard_dropped_high);
        var all = Array.from(new Set(liftedSquares.concat(droppedSquares)));
        var hint = all.length > 0 ? ' (' + all.join(', ') + ')' : '';
        var resync = status.restore_state && status.restore_state.resync_required;
        if (resync) {
            return 'Po startu nesedí fyzická deska s uloženou hrou. Srovnejte figurky podle LED' + hint + '.';
        }
        return 'Hra je pozastavena. Srovnejte figurky podle LED na desce' + hint + ' — hra pokračuje automaticky.';
    }

    global.matrixGuardMaskToSquares = matrixGuardMaskToSquares;
    global.matrixGuardEnsureStyles = matrixGuardEnsureStyles;
    global.matrixGuardHidePanel = matrixGuardHidePanel;
    global.matrixGuardShowPanel = matrixGuardShowPanel;
    global.matrixGuardBuildMessage = matrixGuardBuildMessage;
})(typeof window !== 'undefined' ? window : globalThis);

