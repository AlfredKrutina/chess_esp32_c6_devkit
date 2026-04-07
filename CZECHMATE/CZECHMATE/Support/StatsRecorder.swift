//
//  StatsRecorder.swift
//  CZECHMATE — jednoduché lokální statistiky (UserDefaults).
//

import Foundation

enum StatsRecorder {
    private static let peakMovesKey = "czechmate.stats.peakMoveCount"
    private static let snapshotsKey = "czechmate.stats.snapshotUpdates"

    static func observeSnapshot(_ snap: GameSnapshot) {
        let v = Int(snap.status.moveCount)
        let old = UserDefaults.standard.integer(forKey: peakMovesKey)
        if v > old {
            UserDefaults.standard.set(v, forKey: peakMovesKey)
        }
        let n = UserDefaults.standard.integer(forKey: snapshotsKey)
        UserDefaults.standard.set(n + 1, forKey: snapshotsKey)
    }

    static var peakMoveCount: Int {
        UserDefaults.standard.integer(forKey: peakMovesKey)
    }

    static var snapshotUpdateCount: Int {
        UserDefaults.standard.integer(forKey: snapshotsKey)
    }
}
