// ============================================================================
// OPENING CATALOG (synced from data/openings_master.json via tools/openings/sync_catalog.py)
// ============================================================================
(function (global) {
    'use strict';

    /** @type {Array<object>} */
    var OPENING_LINES = [
        {
            id: 'italian_giuoco_white',
            eco: 'C50',
            name: { cs: 'Italská hra — Giuoco Piano', en: 'Italian Game — Giuoco Piano' },
            side: 'white',
            difficulty: 2,
            idea: { cs: 'Rychlý rozvoj a tlak na f7.', en: 'Rapid development and pressure on f7.' },
            start_fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
            line_uci: ['e2e4', 'e7e5', 'g1f3', 'b8c6', 'f1c4', 'f8c5', 'c2c3', 'g8f6'],
            player_ply_indices: [0, 2, 4, 6],
            checkpoint_ply_indices: [4]
        },
        {
            id: 'sicilian_odb_black',
            eco: 'B50',
            name: { cs: 'Sicilská — ODB (úvod)', en: 'Sicilian — Open Defence intro' },
            side: 'black',
            difficulty: 2,
            idea: { cs: 'Asymetrická hra proti 1.e4.', en: 'Asymmetric play against 1.e4.' },
            start_fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
            line_uci: ['e2e4', 'c7c5', 'g1f3', 'd7d6', 'd2d4', 'g8f6', 'b1c3', 'a7a6'],
            player_ply_indices: [1, 3, 5, 7],
            checkpoint_ply_indices: [3]
        },
        {
            id: 'london_system_white',
            eco: 'D02',
            name: { cs: 'Londýnský systém', en: 'London System' },
            side: 'white',
            difficulty: 1,
            idea: { cs: 'Solidní systém s Bf4 a e3.', en: 'Solid system with Bf4 and e3.' },
            start_fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
            line_uci: ['d2d4', 'd7d5', 'c1f4', 'g8f6', 'e2e3', 'e7e6', 'g1f3', 'f8d6'],
            player_ply_indices: [0, 2, 4, 6],
            checkpoint_ply_indices: [4]
        }
    ];

    function findOpeningById(id) {
        return OPENING_LINES.filter(function (x) { return x.id === id; })[0] || null;
    }

    function openingStartPayload(line, mode) {
        var body = {
            action: 'start',
            line_id: line.id,
            mode: mode || 'learn',
            side: line.side,
            start_fen: line.start_fen,
            line_uci: line.line_uci,
            player_ply_indices: line.player_ply_indices
        };
        if (line.checkpoint_ply_indices && line.checkpoint_ply_indices.length) {
            body.checkpoint_ply_indices = line.checkpoint_ply_indices;
        }
        return body;
    }

    global.OPENING_LINES = OPENING_LINES;
    global.findOpeningById = findOpeningById;
    global.openingStartPayload = openingStartPayload;
}(typeof window !== 'undefined' ? window : globalThis));
