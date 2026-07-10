// ============================================================================
// DEVICE / UI PREFERENCES (Phase 4A.3 — edit web/js/prefs.js, run concat_web_js.py)
// ============================================================================
(function (global) {
    'use strict';

    /** Web UI preference (zdroj pravdy: NVS přes GET/POST /api/settings/ui). */
    var devicePrefs = {
        version: 1,
        chessHintDepth: 10,
        chessEvaluateMove: false,
        chessHintLimit: 0,
        chessHintAwardBest: true,
        chessHintAwardGood: false,
        chessHintAwardCapture: false,
        chessShowHintStats: false,
        chessBotLedTargetOnlyAfterLift: false,
        chessTutorialsEnabled: false,
        chess_confirm_new_game: false,
        botSettings: { mode: 'pvp', strength: '10', side: 'white' }
    };
    var uiPrefsSaveTimer = null;
    var uiPrefsHydratedFromServer = false;

    function mergePrefsFromObject(prefs) {
        if (!prefs || typeof prefs !== 'object') return;
        var k;
        for (k in prefs) {
            if (Object.prototype.hasOwnProperty.call(prefs, k)) {
                devicePrefs[k] = prefs[k];
            }
        }
    }

    async function loadUiPrefsFromDevice() {
        try {
            var r = await fetch('/api/settings/ui');
            if (!r.ok) return;
            var data = await r.json();
            if (data && typeof data.version === 'number') {
                devicePrefs.version = data.version;
            }
            if (data && data.prefs && typeof data.prefs === 'object') {
                mergePrefsFromObject(data.prefs);
            }
            var empty = !data || !data.prefs ||
                (typeof data.prefs === 'object' && Object.keys(data.prefs).length === 0);
            if (empty) {
                migrateLocalStorageToDevicePrefsOnce();
            }
            uiPrefsHydratedFromServer = true;
        } catch (e) {
            if (typeof console !== 'undefined' && console.warn) {
                console.warn('loadUiPrefsFromDevice', e);
            }
        }
    }

    function migrateLocalStorageToDevicePrefsOnce() {
        try {
            if (typeof sessionStorage !== 'undefined' &&
                sessionStorage.getItem('chessUiPrefsMigrated') === '1') {
                return;
            }
            var keys = ['chessHintDepth', 'chessEvaluateMove', 'chessHintLimit', 'chessHintAwardBest',
                'chessHintAwardGood', 'chessHintAwardCapture', 'chessShowHintStats',
                'chessBotLedTargetOnlyAfterLift', 'chessTutorialsEnabled', 'chess_confirm_new_game', 'chessBotSettings'];
            var i;
            var any = false;
            for (i = 0; i < keys.length; i++) {
                try {
                    if (localStorage.getItem(keys[i]) != null) { any = true; break; }
                } catch (e2) { /* ignore */ }
            }
            if (!any) return;

            var d = localStorage.getItem('chessHintDepth');
            if (d != null) devicePrefs.chessHintDepth = Math.min(18, Math.max(1, parseInt(d, 10) || 10));
            if (localStorage.getItem('chessEvaluateMove') === 'true') devicePrefs.chessEvaluateMove = true;
            if (localStorage.getItem('chessEvaluateMove') === 'false') devicePrefs.chessEvaluateMove = false;
            d = localStorage.getItem('chessHintLimit');
            if (d != null) devicePrefs.chessHintLimit = Math.max(0, parseInt(d, 10) || 0);
            if (localStorage.getItem('chessHintAwardBest') != null) {
                devicePrefs.chessHintAwardBest = localStorage.getItem('chessHintAwardBest') !== 'false';
            }
            devicePrefs.chessHintAwardGood = localStorage.getItem('chessHintAwardGood') === 'true';
            devicePrefs.chessHintAwardCapture = localStorage.getItem('chessHintAwardCapture') === 'true';
            devicePrefs.chessShowHintStats = localStorage.getItem('chessShowHintStats') === 'true';
            devicePrefs.chessBotLedTargetOnlyAfterLift = localStorage.getItem('chessBotLedTargetOnlyAfterLift') === 'true';
            devicePrefs.chessTutorialsEnabled = localStorage.getItem('chessTutorialsEnabled') === 'true';
            devicePrefs.chess_confirm_new_game = localStorage.getItem('chess_confirm_new_game') === 'true';
            var bot = localStorage.getItem('chessBotSettings');
            if (bot) {
                try { devicePrefs.botSettings = JSON.parse(bot); } catch (e3) { /* ignore */ }
            }
            if (typeof sessionStorage !== 'undefined') {
                sessionStorage.setItem('chessUiPrefsMigrated', '1');
            }
            scheduleSaveUiPrefsToDevice(true);
        } catch (e) { /* ignore */ }
    }

    function buildUiPrefsPayload() {
        return JSON.stringify({
            version: devicePrefs.version || 1,
            prefs: {
                chessHintDepth: devicePrefs.chessHintDepth,
                chessEvaluateMove: !!devicePrefs.chessEvaluateMove,
                chessHintLimit: devicePrefs.chessHintLimit,
                chessHintAwardBest: !!devicePrefs.chessHintAwardBest,
                chessHintAwardGood: !!devicePrefs.chessHintAwardGood,
                chessHintAwardCapture: !!devicePrefs.chessHintAwardCapture,
                chessShowHintStats: !!devicePrefs.chessShowHintStats,
                chessBotLedTargetOnlyAfterLift: !!devicePrefs.chessBotLedTargetOnlyAfterLift,
                chessTutorialsEnabled: !!devicePrefs.chessTutorialsEnabled,
                chess_confirm_new_game: !!devicePrefs.chess_confirm_new_game,
                botSettings: devicePrefs.botSettings && typeof devicePrefs.botSettings === 'object'
                    ? devicePrefs.botSettings
                    : { mode: 'pvp', strength: '10', side: 'white' }
            }
        });
    }

    function scheduleSaveUiPrefsToDevice(immediate) {
        if (uiPrefsSaveTimer) {
            clearTimeout(uiPrefsSaveTimer);
            uiPrefsSaveTimer = null;
        }
        var delay = immediate ? 0 : 400;
        uiPrefsSaveTimer = setTimeout(function () {
            uiPrefsSaveTimer = null;
            fetch('/api/settings/ui', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: buildUiPrefsPayload()
            }).catch(function () { /* ignore */ });
        }, delay);
    }

    function syncHintSettingsFromDom() {
        var el = document.getElementById('hint-limit');
        if (el) devicePrefs.chessHintLimit = Math.max(0, parseInt(el.value, 10) || 0);
        el = document.getElementById('hint-award-best');
        if (el) devicePrefs.chessHintAwardBest = el.checked;
        el = document.getElementById('hint-award-good');
        if (el) devicePrefs.chessHintAwardGood = el.checked;
        el = document.getElementById('hint-award-capture');
        if (el) devicePrefs.chessHintAwardCapture = el.checked;
        el = document.getElementById('show-hint-stats');
        if (el) devicePrefs.chessShowHintStats = el.checked;
        scheduleSaveUiPrefsToDevice();
    }

    function tutorialsPanelSetVisible(on) {
        var w = document.getElementById('tutorials-panel-body');
        if (w) w.style.display = on ? 'block' : 'none';
        var b = document.getElementById('setup-tutorial-open-btn');
        if (b) b.disabled = !on;
    }

    function saveTutorialsEnabled(on) {
        devicePrefs.chessTutorialsEnabled = !!on;
        tutorialsPanelSetVisible(!!on);
        scheduleSaveUiPrefsToDevice();
    }

    function wireConfirmNewGameCheckbox() {
        var cb = document.getElementById('confirm-new-game');
        if (!cb || cb._prefsWired) return;
        cb._prefsWired = true;
        cb.addEventListener('change', function () {
            devicePrefs.chess_confirm_new_game = !!cb.checked;
            scheduleSaveUiPrefsToDevice();
        });
    }

    function applyDevicePrefsToDom() {
        var depthEl = document.getElementById('hint-depth');
        if (depthEl) depthEl.value = getHintDepth();
        var evaluateMoveEl = document.getElementById('evaluate-move-enabled');
        if (evaluateMoveEl) evaluateMoveEl.checked = getEvaluateMoveEnabled();
        var hl = document.getElementById('hint-limit');
        if (hl) hl.value = String(getHintLimit());
        var hb = document.getElementById('hint-award-best');
        if (hb) hb.checked = getHintAwardBest();
        var hg = document.getElementById('hint-award-good');
        if (hg) hg.checked = getHintAwardGood();
        var hc = document.getElementById('hint-award-capture');
        if (hc) hc.checked = getHintAwardCapture();
        var hs = document.getElementById('show-hint-stats');
        if (hs) hs.checked = getShowHintStats();
        var botLed = document.getElementById('bot-led-target-only-after-lift');
        if (botLed) botLed.checked = getBotLedTargetOnlyAfterLift();
        var tut = document.getElementById('chess-tutorials-enabled');
        if (tut) {
            tut.checked = !!devicePrefs.chessTutorialsEnabled;
            tutorialsPanelSetVisible(!!devicePrefs.chessTutorialsEnabled);
        }
        var cng = document.getElementById('confirm-new-game');
        if (cng) cng.checked = !!devicePrefs.chess_confirm_new_game;
        wireConfirmNewGameCheckbox();
        loadBotSettings();
        if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();
    }

    // Starting position check functions
    function loadStartingPositionCheck() {
        var checkbox = document.getElementById('starting-position-check');
        if (!checkbox) return;
        fetch('/api/settings/start_pos_check')
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (typeof data.enabled === 'boolean') {
                    checkbox.checked = data.enabled;
                }
            })
            .catch(function(e) {
                console.log('Failed to load starting position check setting:', e);
            });
    }

    function saveStartingPositionCheck(enabled) {
        fetch('/api/settings/start_pos_check', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled: !!enabled })
        })
        .then(function(r) { return r.json(); })
        .then(function(data) {
            if (data.success) {
                console.log('Starting position check saved:', data.enabled);
            } else {
                console.error('Failed to save starting position check:', data.message);
            }
        })
        .catch(function(e) {
            console.error('Error saving starting position check:', e);
        });
    }

    function onHintDepthChange(v) {
        devicePrefs.chessHintDepth = v;
        scheduleSaveUiPrefsToDevice();
    }

    function onEvaluateMoveChange(checked) {
        devicePrefs.chessEvaluateMove = !!checked;
        scheduleSaveUiPrefsToDevice();
    }

    /** Hint depth 1–18 from settings (NVS devicePrefs). Used by fetchStockfishBestMove for hints. */
    function getHintDepth() {
        try {
            var d = parseInt(devicePrefs.chessHintDepth, 10);
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
            return devicePrefs.chessEvaluateMove === true;
        } catch (e) {
            return false;
        }
    }

    /** Počet nápověd na partii (0 = neomezeno). */
    function getHintLimit() {
        try {
            var n = parseInt(devicePrefs.chessHintLimit, 10);
            return isNaN(n) || n < 0 ? 0 : n;
        } catch (e) {
            return 0;
        }
    }

    /** Přidat nápovědu za výborný tah (devicePrefs). */
    function getHintAwardBest() {
        try {
            return devicePrefs.chessHintAwardBest !== false;
        } catch (e) {
            return true;
        }
    }

    /** Přidat nápovědu za dobrý tah (localStorage). */
    function getHintAwardGood() {
        try {
            return devicePrefs.chessHintAwardGood === true;
        } catch (e) {
            return false;
        }
    }

    /** Přidat nápovědu za sebrání figurky (localStorage). */
    function getHintAwardCapture() {
        try {
            return devicePrefs.chessHintAwardCapture === true;
        } catch (e) {
            return false;
        }
    }

    /** Zobrazit blok „Výukový přehled“ (nápovědy + kvalita tahů). */
    function getShowHintStats() {
        try {
            return devicePrefs.chessShowHintStats === true;
        } catch (e) {
            return false;
        }
    }

    /** Po zvednutí figurky bota zobrazit na LED jen cílové pole (výchozí vypnuto). */
    function getBotLedTargetOnlyAfterLift() {
        try {
            return devicePrefs.chessBotLedTargetOnlyAfterLift === true;
        } catch (e) {
            return false;
        }
    }

    function setBotLedTargetOnlyAfterLift(checked) {
        devicePrefs.chessBotLedTargetOnlyAfterLift = !!checked;
        scheduleSaveUiPrefsToDevice();
    }

    global.devicePrefs = devicePrefs;
    global.uiPrefsHydratedFromServer = uiPrefsHydratedFromServer;
    global.mergePrefsFromObject = mergePrefsFromObject;
    global.loadUiPrefsFromDevice = loadUiPrefsFromDevice;
    global.scheduleSaveUiPrefsToDevice = scheduleSaveUiPrefsToDevice;
    global.applyDevicePrefsToDom = applyDevicePrefsToDom;
    global.getHintDepth = getHintDepth;
    global.getEvaluationDepth = getEvaluationDepth;
    global.getEvaluateMoveEnabled = getEvaluateMoveEnabled;
    global.getHintLimit = getHintLimit;
    global.getHintAwardBest = getHintAwardBest;
    global.getHintAwardGood = getHintAwardGood;
    global.getHintAwardCapture = getHintAwardCapture;
    global.getShowHintStats = getShowHintStats;
    global.getBotLedTargetOnlyAfterLift = getBotLedTargetOnlyAfterLift;
    global.tutorialsPanelSetVisible = tutorialsPanelSetVisible;
    global.syncHintSettingsFromDom = syncHintSettingsFromDom;
    global.saveTutorialsEnabled = saveTutorialsEnabled;
    global.loadStartingPositionCheck = loadStartingPositionCheck;
    global.saveStartingPositionCheck = saveStartingPositionCheck;
    global.onHintDepthChange = onHintDepthChange;
    global.onEvaluateMoveChange = onEvaluateMoveChange;
    global.setBotLedTargetOnlyAfterLift = setBotLedTargetOnlyAfterLift;
})(typeof window !== 'undefined' ? window : globalThis);

