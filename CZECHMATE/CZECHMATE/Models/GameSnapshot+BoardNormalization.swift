//
//  GameSnapshot+BoardNormalization.swift
//  MCU `game_get_board_json` řadí řádky 0…7 jako v `board[][]` na ESP (řádek 0 = a1–h1, bílá základna).
//  Zbytek aplikace (golden JSON, FirmwareSquareNotation, RemoteChessMoveLegality) očekává řádek 0 = rank 8 (černá nahoře).
//

import Foundation

extension GameSnapshot {
    /// Dekódování těla `GET /api/game/snapshot` / BLE / WS + sjednocení orientace řádků desky.
    static func decodeFromBoardDataRepairingAndNormalizing(_ data: Data) throws -> GameSnapshot {
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(data)
        let decoder = JSONDecoder.forGameSnapshot()
        let snap = try decoder.decode(GameSnapshot.self, from: fixed)
        return snap.normalizingBoardRowsFromFirmwareExportIfNeeded()
    }

    /// Pokud snímek vypadá jako export z MCU (bílý král blíž řádku 0 než černý), převrátí řádky na kanonický pořádek.
    func normalizingBoardRowsFromFirmwareExportIfNeeded() -> GameSnapshot {
        guard Self.boardAppearsFirmwareWhiteNearRowZero(board) else { return self }
        let flipped = Array(board.reversed())
        let newStatus = status.withPieceLiftedRowMirroredVertically()
        AppDebugLog.staging("GameSnapshot: normalizace řádků desky (MCU a1↓ → rank8 nahoře)")
        return GameSnapshot(
            stateVersion: stateVersion,
            board: flipped,
            timestamp: timestamp,
            status: newStatus,
            history: history,
            captured: captured,
            clock: clock,
            gameId: gameId
        )
    }

    /// `true`, pokud jsou na desce oba králové a bílý je na menším indexu řádku než černý (vývoz z `game_task` před zrcadlením).
    private static func boardAppearsFirmwareWhiteNearRowZero(_ board: [[String]]) -> Bool {
        guard board.count == 8 else { return false }
        var whiteKingRow: Int?
        var blackKingRow: Int?
        for r in 0 ..< 8 {
            guard board[r].count == 8 else { return false }
            for c in 0 ..< 8 {
                let raw = board[r][c].trimmingCharacters(in: .whitespaces)
                guard let ch = raw.first, ch != " " else { continue }
                if ch == "K" { whiteKingRow = r }
                if ch == "k" { blackKingRow = r }
            }
        }
        guard let wr = whiteKingRow, let br = blackKingRow else { return false }
        return wr < br
    }
}
