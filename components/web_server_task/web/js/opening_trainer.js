// ============================================================================
// OPENING TRAINER UI (HTTP /api/game/opening — parita s Flutter + BLE)
// ============================================================================
(function (global) {
    'use strict';

    var OPENING_HINT_REFRESH_MS = 600;
    var openingHintIntervalId = null;
    var openingActiveLineId = null;

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
        return !!(status && status.opening_training && status.opening_training.active);
    }

    function openingIsCheckpoint(status) {
        var ot = status && status.opening_training;
        return !!(ot && (ot.feedback === 'checkpoint' || ot.awaiting_checkpoint_ack));
    }

    function openingUpdatePanel(status) {
        var panel = document.getElementById('opening-trainer-panel');
        var textEl = document.getElementById('opening-trainer-text');
        var subEl = document.getElementById('opening-trainer-sub');
        if (!panel || !textEl) return;

        var ot = status && status.opening_training;
        if (!ot || (!ot.active && ot.feedback !== 'complete')) {
            panel.style.display = 'none';
            openingActiveLineId = null;
            openingStopHintRefresh();
            return;
        }

        panel.style.display = '';
        var line = openingActiveLineId && global.findOpeningById
            ? global.findOpeningById(openingActiveLineId)
            : null;
        var title = line && line.name ? (line.name.cs || line.id) : (ot.line_id || 'Opening');
        textEl.textContent = title + ' — tah ' +
            String((ot.player_ply_index || 0) + 1) + '/' + String(ot.player_ply_total || '?');

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
        } else {
            subEl.textContent = 'Táhni na desce: ' +
                (ot.expected_from || '?') + ' → ' + (ot.expected_to || '?');
            if (ot.active) openingStartHintRefresh();
        }
    }

    function openingTrainerOnStatusUpdate(status) {
        openingUpdatePanel(status);
        var hintBtn = document.getElementById('hint-btn');
        if (hintBtn && openingIsActive(status)) {
            hintBtn.disabled = true;
            hintBtn.title = 'Běží trénink zahájení';
        }
    }

    async function openingStartLesson(lineId, mode) {
        var line = global.findOpeningById ? global.findOpeningById(lineId) : null;
        if (!line) throw new Error('Opening not found: ' + lineId);
        var body = global.openingStartPayload(line, mode || 'learn');
        await openingPostAction(body);
        openingActiveLineId = lineId;
    }

    async function openingCancelLesson() {
        openingStopHintRefresh();
        openingActiveLineId = null;
        await openingPostAction({ action: 'cancel' });
        fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});
    }

    async function openingCheckpointAck() {
        await openingPostAction({ action: 'checkpoint_ack' });
    }

    global.openingIsActive = openingIsActive;
    global.openingIsCheckpoint = openingIsCheckpoint;
    global.openingTrainerOnStatusUpdate = openingTrainerOnStatusUpdate;
    global.openingStartLesson = openingStartLesson;
    global.openingCancelLesson = openingCancelLesson;
    global.openingCheckpointAck = openingCheckpointAck;
    global.openingPostAction = openingPostAction;
    global.openingStopHintRefresh = openingStopHintRefresh;
}(typeof window !== 'undefined' ? window : globalThis));
