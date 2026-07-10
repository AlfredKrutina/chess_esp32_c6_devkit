// ============================================================================
// OPENING TRAINER UI (HTTP /api/game/opening — parita s Flutter + BLE)
// ============================================================================
(function (global) {
    'use strict';

    var OPENING_HINT_REFRESH_MS = 600;
    var OPENING_PROGRESS_KEY = 'opening_progress_v1';
    var openingHintIntervalId = null;
    var openingActiveLineId = null;
    var openingActiveMode = 'learn';
    var openingDrillErrors = 0;
    var openingLastFeedback = 'none';
    var openingProgressSaved = false;
    var openingAwaitingSetupRetry = false;

    function openingLoadProgress() {
        try {
            var raw = localStorage.getItem(OPENING_PROGRESS_KEY);
            return raw ? JSON.parse(raw) : {};
        } catch (e) {
            return {};
        }
    }

    function openingSaveProgress(map) {
        try {
            localStorage.setItem(OPENING_PROGRESS_KEY, JSON.stringify(map));
        } catch (e) {}
    }

    function openingRecordCompletion(lineId, mode, drillErrors, timedSuccess) {
        var all = openingLoadProgress();
        var prev = all[lineId] || { stars: 0, best_drill_errors: 99 };
        var stars = prev.stars || 0;
        var best = prev.best_drill_errors != null ? prev.best_drill_errors : 99;
        if (mode === 'learn' && stars < 1) stars = 1;
        if (mode === 'drill') {
            if (stars < 1) stars = 1;
            if (drillErrors <= 2 && stars < 2) stars = 2;
            if (drillErrors < best) best = drillErrors;
        }
        if (mode === 'timed' && timedSuccess) {
            if (stars < 1) stars = 1;
            if (stars < 3) stars = 3;
        }
        if (mode === 'mirror') {
            if (drillErrors <= 2 && stars < 2) stars = 2;
            if (stars < 4) stars = 4;
            if (drillErrors < best) best = drillErrors;
        }
        all[lineId] = {
            stars: stars,
            best_drill_errors: best,
            last_completed_at: new Date().toISOString()
        };
        openingSaveProgress(all);
        return all[lineId];
    }

    function openingGetStars(lineId) {
        var all = openingLoadProgress();
        return (all[lineId] && all[lineId].stars) || 0;
    }

    function openingSquareFromIndex(index) {
        var col = index % 8;
        var row = Math.floor(index / 8);
        return String.fromCharCode(97 + col) + String(row + 1);
    }

    function openingCheckpointMismatches(status) {
        var ot = status && status.opening_training;
        if (!ot || !status.matrix_occupied || !ot.checkpoint_expected_occupied) {
            return [];
        }
        var matrix = status.matrix_occupied;
        var expected = ot.checkpoint_expected_occupied;
        var out = [];
        for (var i = 0; i < 64; i++) {
            if (matrix[i] !== expected[i]) {
                var sq = openingSquareFromIndex(i);
                if (expected[i] === 1 && matrix[i] === 0) {
                    out.push('Polož figurku na ' + sq);
                } else if (expected[i] === 0 && matrix[i] === 1) {
                    out.push('Zvedni figurku z ' + sq);
                } else {
                    out.push('Uprav pole ' + sq);
                }
            }
        }
        return out;
    }

    function openingStopHintRefresh() {
        if (openingHintIntervalId) {
            clearInterval(openingHintIntervalId);
            openingHintIntervalId = null;
        }
    }

    function openingStartHintRefresh() {
        openingStopHintRefresh();
        openingHintIntervalId = setInterval(function () {
            openingPostAction({ action: 'hint' }).catch(function () {});
        }, OPENING_HINT_REFRESH_MS);
    }

    async function openingPostAction(body) {
        var res = await fetch('/api/game/opening', {
            method: 'POST',
            headers: global.boardApiAuthHeaders
                ? global.boardApiAuthHeaders({ 'Content-Type': 'application/json' })
                : { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        if (!res.ok) {
            var err = await res.json().catch(function () { return {}; });
            throw new Error(err.error || ('HTTP ' + res.status));
        }
        return res.json().catch(function () { return {}; });
    }

    function openingIsActive(status) {
        var ot = status && status.opening_training;
        return !!(ot && (ot.active || ot.feedback === 'opponent_turn'));
    }

    function openingIsSetupPhase(status) {
        var ot = status && status.opening_training;
        return !!(ot && ot.setup_phase && !ot.active);
    }

    function openingIsCheckpoint(status) {
        var ot = status && status.opening_training;
        return !!(ot && (ot.feedback === 'checkpoint' || ot.awaiting_checkpoint_ack));
    }

    function openingEnsureRationaleEl(panel) {
        var el = document.getElementById('opening-trainer-rationale');
        if (!el && panel) {
            el = document.createElement('div');
            el.id = 'opening-trainer-rationale';
            el.className = 'opening-trainer-rationale';
            var subEl = document.getElementById('opening-trainer-sub');
            if (subEl && subEl.parentNode) {
                subEl.parentNode.insertBefore(el, subEl);
            } else {
                panel.appendChild(el);
            }
        }
        return el;
    }

    function openingRationaleText(line) {
        if (!line || !line.rationale) return '';
        var r = line.rationale;
        var parts = [];
        if (r.summary && r.summary.cs) parts.push(r.summary.cs);
        if (r.why_this_line && r.why_this_line.cs) {
            parts.push(r.why_this_line.cs);
        }
        if (r.instead_of && r.instead_of.cs) {
            parts.push('Místo: ' + r.instead_of.cs);
        }
        if (r.when_to_play && r.when_to_play.cs) {
            parts.push('Kdy: ' + r.when_to_play.cs);
        }
        return parts.join(' · ');
    }

    function openingCommonMistakeHint(line, ot) {
        if (!line || !line.common_mistakes || !ot || !ot.last_wrong_uci) return '';
        var ply = ot.ply_index;
        var wrong = String(ot.last_wrong_uci).toLowerCase();
        for (var i = 0; i < line.common_mistakes.length; i++) {
            var m = line.common_mistakes[i];
            if (m.at_ply_index === ply && String(m.wrong_uci).toLowerCase() === wrong) {
                return (m.hint && m.hint.cs) ? m.hint.cs : '';
            }
        }
        return '';
    }

    function openingEnsureMiniboardStyles() {
        if (document.getElementById('opening-miniboard-styles')) return;
        var style = document.createElement('style');
        style.id = 'opening-miniboard-styles';
        style.textContent = [
            '.opening-trainer-board {',
            '  display: grid;',
            '  grid-template-columns: repeat(8, 1fr);',
            '  width: min(280px, 100%);',
            '  aspect-ratio: 1;',
            '  margin: 0 auto 12px;',
            '  border-radius: 8px;',
            '  overflow: hidden;',
            '  box-shadow: 0 2px 8px rgba(0,0,0,0.35);',
            '}',
            '.opening-miniboard-square {',
            '  position: relative;',
            '  display: flex;',
            '  align-items: center;',
            '  justify-content: center;',
            '}',
            '.opening-miniboard-square.light { background: #f0d9b5; }',
            '.opening-miniboard-square.dark { background: #b58863; }',
            '.opening-miniboard-square.opening-hint-from {',
            '  box-shadow: inset 0 0 0 9999px rgba(255, 213, 79, 0.55);',
            '}',
            '.opening-miniboard-square.opening-hint-to {',
            '  box-shadow: inset 0 0 0 9999px rgba(118, 255, 122, 0.55);',
            '}',
            '.opening-miniboard-square.opening-checkpoint {',
            '  box-shadow: inset 0 0 0 9999px rgba(103, 58, 183, 0.55);',
            '}',
            '.opening-miniboard-square .piece {',
            '  width: 90%;',
            '  height: 90%;',
            '  display: flex;',
            '  align-items: center;',
            '  justify-content: center;',
            '  font-size: clamp(14px, 4vw, 28px);',
            '}',
            '.opening-miniboard-square .piece img {',
            '  width: 100%;',
            '  height: 100%;',
            '  object-fit: contain;',
            '}'
        ].join('\n');
        document.head.appendChild(style);
    }

    function openingEnsureMiniboardEl(panel) {
        openingEnsureMiniboardStyles();
        var el = document.getElementById('opening-trainer-board');
        if (!el) {
            el = document.createElement('div');
            el.id = 'opening-trainer-board';
            el.className = 'opening-trainer-board';
            el.setAttribute('aria-label', 'Miniboard — logická pozice lekce');
            var textEl = document.getElementById('opening-trainer-text');
            if (textEl && textEl.parentNode) {
                textEl.parentNode.insertBefore(el, textEl);
            } else if (panel.firstChild) {
                panel.insertBefore(el, panel.firstChild);
            } else {
                panel.appendChild(el);
            }
            for (var row = 7; row >= 0; row--) {
                for (var col = 0; col < 8; col++) {
                    var sq = document.createElement('div');
                    sq.className = 'opening-miniboard-square ' +
                        ((row + col) % 2 === 0 ? 'light' : 'dark');
                    sq.dataset.row = String(row);
                    sq.dataset.col = String(col);
                    var piece = document.createElement('div');
                    piece.className = 'opening-miniboard-piece';
                    piece.id = 'opening-piece-' + (row * 8 + col);
                    sq.appendChild(piece);
                    el.appendChild(sq);
                }
            }
        }
        return el;
    }

    function openingUpdateMiniboard(status) {
        var panel = document.getElementById('opening-trainer-panel');
        if (!panel) return;
        var boardEl = openingEnsureMiniboardEl(panel);
        var board = status && status.board;
        var ot = status && status.opening_training;
        if (!board || !Array.isArray(board) || board.length !== 8) {
            boardEl.style.display = 'none';
            return;
        }
        boardEl.style.display = 'grid';
        for (var row = 0; row < 8; row++) {
            if (!board[row] || board[row].length !== 8) continue;
            for (var col = 0; col < 8; col++) {
                var pieceEl = document.getElementById('opening-piece-' + (row * 8 + col));
                if (pieceEl && typeof setPieceElementFromFen === 'function') {
                    setPieceElementFromFen(pieceEl, board[row][col]);
                }
                var sq = boardEl.querySelector('[data-row="' + row + '"][data-col="' + col + '"]');
                if (sq) {
                    sq.classList.remove('opening-hint-from', 'opening-hint-to', 'opening-checkpoint');
                }
            }
        }
        if (ot && ot.expected_from) {
            document.querySelectorAll('#opening-trainer-board .opening-miniboard-square').forEach(function (sq) {
                var r = parseInt(sq.dataset.row, 10);
                var c = parseInt(sq.dataset.col, 10);
                var alg = String.fromCharCode(97 + c) + (r + 1);
                if (ot.expected_from === alg) sq.classList.add('opening-hint-from');
                if (ot.expected_to === alg) sq.classList.add('opening-hint-to');
            });
        }
        if (ot && (ot.feedback === 'checkpoint' || ot.awaiting_checkpoint_ack)) {
            var matrix = status.matrix_occupied;
            var expected = ot.checkpoint_expected_occupied;
            if (matrix && expected && matrix.length >= 64 && expected.length >= 64) {
                for (var i = 0; i < 64; i++) {
                    if (matrix[i] !== expected[i]) {
                        var alg = openingSquareFromIndex(i);
                        document.querySelectorAll('#opening-trainer-board .opening-miniboard-square').forEach(function (sq) {
                            var r = parseInt(sq.dataset.row, 10);
                            var c = parseInt(sq.dataset.col, 10);
                            var name = String.fromCharCode(97 + c) + (r + 1);
                            if (name === alg) sq.classList.add('opening-checkpoint');
                        });
                    }
                }
            }
        }
    }

    function openingUpdatePanel(status) {
        var panel = document.getElementById('opening-trainer-panel');
        var textEl = document.getElementById('opening-trainer-text');
        var subEl = document.getElementById('opening-trainer-sub');
        if (!panel || !textEl) return;

        var ot = status && status.opening_training;
        if (!ot || (!ot.active && ot.feedback !== 'complete' && !ot.setup_phase)) {
            panel.style.display = 'none';
            openingActiveLineId = null;
            openingAwaitingSetupRetry = false;
            openingStopHintRefresh();
            return;
        }

        panel.style.display = '';
        openingUpdateMiniboard(status);
        var line = openingActiveLineId && global.findOpeningById
            ? global.findOpeningById(openingActiveLineId)
            : null;
        var rationaleEl = openingEnsureRationaleEl(panel);
        var showRationale = openingActiveMode === 'learn' && line && line.rationale &&
            (ot.player_ply_index || 0) === 0;
        if (rationaleEl) {
            if (showRationale) {
                rationaleEl.textContent = openingRationaleText(line);
                rationaleEl.style.display = '';
            } else {
                rationaleEl.style.display = 'none';
            }
        }
        var title = line && line.name ? (line.name.cs || line.id) : (ot.line_id || 'Opening');
        textEl.textContent = title + ' — tah ' +
            String((ot.player_ply_index || 0) + 1) + '/' + String(ot.player_ply_total || '?');
        var mistakeHint = openingCommonMistakeHint(line, ot);

        if (openingIsSetupPhase(status)) {
            subEl.textContent = ot.physical_match === false
                ? 'Deska nesedí se startem — použij průvodce rozestavením, pak „Zkusit znovu“.'
                : 'Kontroluji fyzickou desku…';
            openingStopHintRefresh();
            return;
        }

        if (openingIsCheckpoint(status)) {
            var mismatches = openingCheckpointMismatches(status);
            if (ot.physical_synced) {
                subEl.textContent = 'Deska sedí — potvrď a pokračuj.';
            } else if (mismatches.length === 0) {
                subEl.textContent = 'Srovnej fyzickou desku s logickou pozicí…';
            } else {
                subEl.textContent = mismatches.slice(0, 6).join(' · ');
            }
            openingStopHintRefresh();
        } else if (ot.feedback === 'complete') {
            subEl.textContent = 'Linie dokončena.';
            openingStopHintRefresh();
        } else if (ot.feedback === 'mistake_hint') {
            subEl.textContent = 'Po 3 chybách — ' +
                (ot.expected_from || '?') + ' → ' + (ot.expected_to || '?');
            if (ot.active) openingStartHintRefresh();
        } else if (ot.feedback === 'opponent_turn' || ot.awaiting_opponent_physical) {
            subEl.textContent = 'Tah soupeře — zvedni z ' +
                (ot.expected_from || '?') + ' a polož na ' + (ot.expected_to || '?');
            if (ot.opponent_mode === 'physical') openingStartHintRefresh();
        } else {
            subEl.textContent = 'Táhni na desce: ' +
                (ot.expected_from || '?') + ' → ' + (ot.expected_to || '?');
            if ((ot.feedback === 'wrong' || ot.feedback === 'illegal') && mistakeHint) {
                subEl.textContent = mistakeHint;
            }
            if (ot.active) openingStartHintRefresh();
        }
    }

    function openingTrainerOnStatusUpdate(status) {
        var ot = status && status.opening_training;
        if (ot && ot.feedback) {
            if ((ot.feedback === 'wrong' || ot.feedback === 'illegal') &&
                ot.feedback !== openingLastFeedback) {
                openingDrillErrors++;
            }
            if (openingIsSetupPhase(status) && openingActiveLineId) {
                openingAwaitingSetupRetry = true;
            }
            if (ot.feedback === 'complete' && openingLastFeedback !== 'complete' &&
                openingActiveLineId && !openingProgressSaved) {
                openingRecordCompletion(
                    openingActiveLineId,
                    openingActiveMode,
                    openingDrillErrors,
                    true
                );
                openingProgressSaved = true;
            }
            openingLastFeedback = ot.feedback;
        }
        openingUpdatePanel(status);
        var hintBtn = document.getElementById('hint-btn');
        if (hintBtn && openingIsActive(status)) {
            hintBtn.disabled = true;
            hintBtn.title = 'Běží trénink zahájení';
        }
    }

    async function openingRetryAfterSetup() {
        if (!openingActiveLineId) return;
        await openingStartLesson(openingActiveLineId, openingActiveMode);
    }

    async function openingOnSetupTutorialDone() {
        if (!openingAwaitingSetupRetry || !openingActiveLineId) return;
        openingAwaitingSetupRetry = false;
        try {
            await openingRetryAfterSetup();
        } catch (e) {
            if (typeof console !== 'undefined' && console.warn) {
                console.warn('opening retry after setup', e);
            }
        }
    }

    async function openingStartLesson(lineId, mode, opponentMode) {
        var line = global.findOpeningById ? global.findOpeningById(lineId) : null;
        if (!line) throw new Error('Opening not found: ' + lineId);
        var body = global.openingStartPayload(line, mode || 'learn', opponentMode || 'physical');
        await openingPostAction(body);
        openingActiveLineId = lineId;
        openingActiveMode = mode || 'learn';
        openingDrillErrors = 0;
        openingLastFeedback = 'none';
        openingProgressSaved = false;
    }

    async function openingCancelLesson() {
        openingStopHintRefresh();
        openingActiveLineId = null;
        openingActiveMode = 'learn';
        openingDrillErrors = 0;
        openingProgressSaved = false;
        await openingPostAction({ action: 'cancel' });
        fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    }

    async function openingCheckpointAck() {
        await openingPostAction({ action: 'checkpoint_ack' });
    }

    function openingCountStarsInCurriculum(curriculumId, minStars) {
        var curricula = global.OPENING_CURRICULA || [];
        var curriculum = curricula.filter(function (c) {
            return c.id === curriculumId;
        })[0];
        if (!curriculum) return 0;
        var count = 0;
        curriculum.line_ids.forEach(function (id) {
            if (openingGetStars(id) >= minStars) count++;
        });
        return count;
    }

    function openingTotalStarsAtLeast(minStars) {
        var all = openingLoadProgress();
        var count = 0;
        Object.keys(all).forEach(function (id) {
            if ((all[id].stars || 0) >= minStars) count++;
        });
        return count;
    }

    function openingIsCurriculumUnlocked(curriculumId) {
        if (curriculumId === 'basics_white') return true;
        if (curriculumId === 'basics_black') {
            return openingCountStarsInCurriculum('basics_white', 1) >= 2;
        }
        if (curriculumId === 'classical_deep') {
            return openingCountStarsInCurriculum('basics_white', 2) >= 1 ||
                openingCountStarsInCurriculum('basics_black', 2) >= 1;
        }
        if (curriculumId === 'systems') {
            return openingTotalStarsAtLeast(2) >= 5;
        }
        return true;
    }

    function openingLinesDueForReview(reviewAfterDays) {
        reviewAfterDays = reviewAfterDays || 3;
        var all = openingLoadProgress();
        var due = [];
        var now = Date.now();
        Object.keys(all).forEach(function (id) {
            var p = all[id];
            if (!p || (p.stars || 0) < 2 || !p.last_completed_at) return;
            var last = Date.parse(p.last_completed_at);
            if (isNaN(last)) return;
            if ((now - last) >= reviewAfterDays * 86400000) due.push(id);
        });
        return due;
    }

    global.openingIsActive = openingIsActive;
    global.openingIsSetupPhase = openingIsSetupPhase;
    global.openingIsCheckpoint = openingIsCheckpoint;
    global.openingTrainerOnStatusUpdate = openingTrainerOnStatusUpdate;
    global.openingStartLesson = openingStartLesson;
    global.openingRetryAfterSetup = openingRetryAfterSetup;
    global.openingOnSetupTutorialDone = openingOnSetupTutorialDone;
    global.openingCancelLesson = openingCancelLesson;
    global.openingCheckpointAck = openingCheckpointAck;
    global.openingPostAction = openingPostAction;
    global.openingStopHintRefresh = openingStopHintRefresh;
    global.openingGetStars = openingGetStars;
    global.openingRecordCompletion = openingRecordCompletion;
    global.openingIsCurriculumUnlocked = openingIsCurriculumUnlocked;
    global.openingLinesDueForReview = openingLinesDueForReview;
}(typeof window !== 'undefined' ? window : globalThis));
