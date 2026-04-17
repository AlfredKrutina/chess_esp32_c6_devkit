//
//  WatchCompanionEndgameSummary.swift
//  CZECHMATE — krátké texty konce partie pro Watch (a shodné klíče v application context).
//

import Foundation

enum WatchCompanionEndgameSummary {
    /// Klíče odpovídají `UnifiedGameState.init(from:)` v CZECHMATEShared.
    static func contextFields(snapshot: GameSnapshot, boardClock: BoardTimerHTTPState?) -> [String: Any] {
        let st = snapshot.status
        let gameEnded =
            st.gameEnd?.ended == true
            || st.checkmate == true
            || st.stalemate == true
        guard gameEnded else { return [:] }

        let report = GameEndReportModel(snapshot: snapshot)
        let clock = boardClock ?? snapshot.clock

        var statsParts: [String] = ["Tahů: \(report.moveCount)"]
        if let d = report.durationSummary {
            statsParts.append(d)
        }
        if let t = clock {
            let w = formatClockShort(ms: t.whiteTimeMs)
            let b = formatClockShort(ms: t.blackTimeMs)
            statsParts.append("Čas \(w) · \(b)")
        }

        return [
            "watchEndgameHeadline": report.headline,
            "watchEndgameScoreLine": report.scoreLine,
            "watchEndgameReason": report.terminationLine,
            "watchEndgameStats": statsParts.joined(separator: " • "),
        ]
    }

    private static func formatClockShort(ms: UInt32) -> String {
        let sec = ms / 1000
        let m = sec / 60
        let s = sec % 60
        return String(format: "%d:%02d", m, s)
    }
}
