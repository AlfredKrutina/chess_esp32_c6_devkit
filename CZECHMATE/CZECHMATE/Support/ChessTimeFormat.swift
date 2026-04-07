//
//  ChessTimeFormat.swift
//  CZECHMATE
//

import Foundation

enum ChessTimeFormat {
    /// Zobrazí sekundy jako mm:ss (hodnoty z API jsou typicky v sekundách).
    static func mmss(_ seconds: UInt32) -> String {
        let s = Int(seconds)
        return String(format: "%d:%02d", s / 60, s % 60)
    }

    /// Zobrazení pro plynulý lokální odpočet (`Double` sekundy). Pod 20 s jedna desetina; pod 10 s tvar „09.5“.
    static func displayClockInterpolated(_ seconds: Double) -> String {
        let s = max(0, seconds)
        if s < 10 {
            let whole = Int(floor(s))
            let frac = min(9, max(0, Int((s - Double(whole)) * 10 + 0.0001)))
            return String(format: "%02d.%d", whole, frac)
        }
        if s < 20 {
            let whole = Int(floor(s))
            let m = whole / 60
            let sec = whole % 60
            let frac = min(9, max(0, Int((s - floor(s)) * 10 + 0.0001)))
            return String(format: "%d:%02d.%d", m, sec, frac)
        }
        return mmss(UInt32(s.rounded(.down)))
    }
}

/// Lokální interpolace mezi snímky ESP (zdroj pravdy = poslední `GET /api/timer`).
enum BoardClockInterpolation {
    /// Zbývající čas z `white_time_ms` / `black_time_ms` (ne z `status.white_time` ve snapshotu — tam jsou jiná data).
    static func remainingSecondsFromMilliseconds(
        baseMilliseconds: UInt32?,
        isActive: Bool,
        timerRunning: Bool,
        clockReceivedAt: Date?,
        now: Date
    ) -> Double {
        guard let ms = baseMilliseconds else { return 0 }
        let baseSec = Double(ms) / 1000.0
        if !timerRunning || !isActive {
            return baseSec
        }
        guard let t0 = clockReceivedAt else { return baseSec }
        let elapsed = now.timeIntervalSince(t0)
        return max(0, baseSec - elapsed)
    }

    /// Fallback když `/api/timer` je nedostupný — statická hodnota bez odpočtu.
    static func remainingSecondsStatic(fromSnapshotSeconds: UInt32?) -> Double {
        Double(fromSnapshotSeconds ?? 0)
    }
}
