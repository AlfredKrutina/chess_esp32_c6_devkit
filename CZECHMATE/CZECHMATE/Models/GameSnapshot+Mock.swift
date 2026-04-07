//
//  GameSnapshot+Mock.swift
//

import Foundation

extension GameSnapshot {
    /// Minimální platný snímek pro mock transport / náhled UI.
    static func mockStartingPosition() throws -> GameSnapshot {
        // Řádky 0 = černá hlína (rank 8), 7 = bílá (rank 1) — stejně jako JSON z ESP.
        let fenRows = [
            ["r", "n", "b", "q", "k", "b", "n", "r"],
            ["p", "p", "p", "p", "p", "p", "p", "p"],
            [".", ".", ".", ".", ".", ".", ".", "."],
            [".", ".", ".", ".", ".", ".", ".", "."],
            [".", ".", ".", ".", ".", ".", ".", "."],
            [".", ".", ".", ".", ".", ".", ".", "."],
            ["P", "P", "P", "P", "P", "P", "P", "P"],
            ["R", "N", "B", "Q", "K", "B", "N", "R"],
        ]
        let rows = fenRows
        let json: [String: Any] = [
            "state_version": 1,
            "board": rows,
            "timestamp": UInt64(Date().timeIntervalSince1970),
            "status": [
                "game_state": "active",
                "current_player": "white",
                "move_count": 0,
                "white_time": 300,
                "black_time": 300,
                "in_check": false,
                "game_end": ["ended": false],
            ],
            "history": ["moves": []],
            "captured": [
                "white_captured": [],
                "black_captured": [],
            ],
        ]
        let raw = try JSONSerialization.data(withJSONObject: json, options: [])
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(raw)
        let dec = JSONDecoder.forGameSnapshot()
        return try dec.decode(GameSnapshot.self, from: fixed)
    }
}
