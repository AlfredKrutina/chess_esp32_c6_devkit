// ============================================================================
// API LAYER (Phase 4A.2 — edit web/js/api.js, run concat_web_js.py)
// ============================================================================
(function (global) {
    'use strict';

    /** Prevents overlapping fetchData calls on a slow network / overloaded HTTPD. */
    global.fetchDataInFlight = false;

    /**
     * Headers for admin POST — adds Bearer from localStorage `czechmate_api_token`
     * (64 hex from UART `API_TOKEN`) when stored.
     * @param {Object.<string,string>} [base] base headers (e.g. Content-Type)
     */
    function boardApiAuthHeaders(base) {
        const h = {};
        if (base) {
            for (const k of Object.keys(base)) {
                h[k] = base[k];
            }
        }
        try {
            const t = localStorage.getItem('czechmate_api_token');
            if (t && String(t).trim()) {
                h['Authorization'] = 'Bearer ' + String(t).trim();
            }
        } catch (e) {}
        return h;
    }

    /**
     * GET JSON with simple error handling.
     * @param {string} url
     * @returns {Promise<*>}
     */
    async function apiGetJson(url) {
        const res = await fetch(url);
        if (!res.ok) {
            throw new Error('HTTP ' + res.status + ' for ' + url);
        }
        return res.json();
    }

    /**
     * POST JSON — returns parsed body or {} for non-JSON responses.
     * @param {string} url
     * @param {*} body
     * @param {Object.<string,string>} [extraHeaders]
     */
    async function apiPostJson(url, body, extraHeaders) {
        const headers = boardApiAuthHeaders(
            Object.assign({ 'Content-Type': 'application/json' }, extraHeaders || {})
        );
        const res = await fetch(url, {
            method: 'POST',
            headers: headers,
            body: typeof body === 'string' ? body : JSON.stringify(body)
        });
        return res.json().catch(function () { return {}; });
    }

    /**
     * Loads game snapshot (prefers /api/game/snapshot, falls back to 4 endpoints).
     * @returns {Promise<{board: *, status: *, history: *, captured: *, clock: *|null, fromSnapshot: boolean}>}
     */
    async function fetchGameSnapshot() {
        var snapRes = await fetch('/api/game/snapshot');
        if (snapRes.ok) {
            var data = await snapRes.json();
            return {
                board: { board: data.board, timestamp: data.timestamp },
                status: data.status,
                history: data.history,
                captured: data.captured,
                clock: data.clock || null,
                fromSnapshot: true
            };
        }
        const results = await Promise.all([
            fetch('/api/board'),
            fetch('/api/status'),
            fetch('/api/history'),
            fetch('/api/captured')
        ]);
        return {
            board: await results[0].json(),
            status: await results[1].json(),
            history: await results[2].json(),
            captured: await results[3].json(),
            clock: null,
            fromSnapshot: false
        };
    }

    global.boardApiAuthHeaders = boardApiAuthHeaders;
    global.apiGetJson = apiGetJson;
    global.apiPostJson = apiPostJson;
    global.fetchGameSnapshot = fetchGameSnapshot;
})(typeof window !== 'undefined' ? window : globalThis);
