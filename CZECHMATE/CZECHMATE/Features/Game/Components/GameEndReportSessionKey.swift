//
//  GameEndReportSessionKey.swift
//  CZECHMATE — stabilní identifikátor konce partie (auto-souhrn, UserDefaults).
//

import Foundation

enum GameEndReportSessionKey {
    /// Jednoznačný klíč ukončené partie pro auto-otevření / porovnání s minulou.
    /// Preferuje `game_id` z JSON, jinak `move_count` + signatura desky (ne `state_version`).
    static func fingerprint(for snapshot: GameSnapshot) -> String? {
        guard snapshot.status.isGameFinished else { return nil }
        if let gid = snapshot.gameId?.trimmingCharacters(in: .whitespacesAndNewlines), !gid.isEmpty {
            return "gid:\(gid)"
        }
        let boardSig = snapshot.board.map { $0.joined(separator: ",") }.joined(separator: "|")
        return "end-\(snapshot.status.moveCount)-\(boardSig)"
    }
}
