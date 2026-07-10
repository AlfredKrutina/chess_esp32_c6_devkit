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
        var ot = status && status.opening_training;
        if (ot && ot.feedback) {
            if ((ot.feedback === 'wrong' || ot.feedback === 'illegal') &&
                ot.feedback !== openingLastFeedback) {
                openingDrillErrors++;
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

    async function openingStartLesson(lineId, mode) {
        var line = global.findOpeningById ? global.findOpeningById(lineId) : null;
        if (!line) throw new Error('Opening not found: ' + lineId);
        var body = global.openingStartPayload(line, mode || 'learn');
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
    global.openingIsCheckpoint = openingIsCheckpoint;
    global.openingTrainerOnStatusUpdate = openingTrainerOnStatusUpdate;
    global.openingStartLesson = openingStartLesson;
    global.openingCancelLesson = openingCancelLesson;
    global.openingCheckpointAck = openingCheckpointAck;
    global.openingPostAction = openingPostAction;
    global.openingStopHintRefresh = openingStopHintRefresh;
    global.openingGetStars = openingGetStars;
    global.openingRecordCompletion = openingRecordCompletion;
    global.openingIsCurriculumUnlocked = openingIsCurriculumUnlocked;
    global.openingLinesDueForReview = openingLinesDueForReview;
}(typeof window !== 'undefined' ? window : globalThis));
