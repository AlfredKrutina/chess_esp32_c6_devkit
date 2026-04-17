//
//  GameEndReportTiming.swift
//  CZECHMATE — Časové řady pro souhrn partie (grafy z razítek tahů).
//

import Foundation

/// Jedna „polotahová“ hodnota: čas od předchozího tahu podle `timestamp` z desky.
struct EndgameThinkPlyPoint: Identifiable, Equatable {
    var id: Int { plyIndex }
    /// 1 = první tah bílého …
    let plyIndex: Int
    let isWhite: Bool
    /// Sekundy od předchozího záznamu v historii; u prvního tahu `nil`, pokud není čas před startem.
    let secondsFromPrevious: Double?
}

/// Bod křivky „kolik minut uběhlo od začátku“ po daném půltahu.
struct EndgameCumulativePoint: Identifiable, Equatable {
    var id: Int { plyIndex }
    let plyIndex: Int
    let totalSeconds: Double
}

enum GameEndReportTiming {
    /// Odvozeno z `timestamp` u tahů (stejné jednotky jako firmware — obvykle sekundy od startu MCU / partie).
    static func thinkPlyPoints(from moves: [GameHistoryMove]) -> [EndgameThinkPlyPoint] {
        moves.enumerated().map { i, m in
            let prev: Double?
            if i == 0 {
                prev = nil
            } else {
                let a = moves[i - 1].timestamp
                let b = m.timestamp
                if let t0 = a, let t1 = b, t1 >= t0 {
                    prev = Double(t1 - t0)
                } else {
                    prev = nil
                }
            }
            return EndgameThinkPlyPoint(
                plyIndex: i + 1,
                isWhite: i % 2 == 0,
                secondsFromPrevious: prev
            )
        }
    }

    static func cumulativePoints(from think: [EndgameThinkPlyPoint]) -> [EndgameCumulativePoint] {
        var sum: Double = 0
        var out: [EndgameCumulativePoint] = []
        for p in think {
            if let d = p.secondsFromPrevious {
                sum += d
                out.append(EndgameCumulativePoint(plyIndex: p.plyIndex, totalSeconds: sum))
            }
        }
        return out
    }

    static func shareTimingLines(from moves: [GameHistoryMove], maxLines: Int = 24) -> [String] {
        let pts = thinkPlyPoints(from: moves)
        guard pts.contains(where: { $0.secondsFromPrevious != nil }) else { return [] }
        var lines: [String] = ["Čas mezi tahy (z razítek desky):"]
        for p in pts.prefix(maxLines) {
            let side = p.isWhite ? "B" : "Č"
            if let s = p.secondsFromPrevious {
                lines.append("  \(p.plyIndex).\(side)  \(formatSeconds(s))")
            } else {
                lines.append("  \(p.plyIndex).\(side)  —")
            }
        }
        if pts.count > maxLines {
            lines.append("  … (+ \(pts.count - maxLines) tahů)")
        }
        return lines
    }

    static func formatSeconds(_ s: Double) -> String {
        if s < 60 { return String(format: "%.1f s", s) }
        let m = Int(s) / 60
        let r = s - Double(m * 60)
        if r < 0.5 { return "\(m) min" }
        return String(format: "%d min %.0f s", m, r)
    }

    /// Průměrný čas na tah (sekundy) podle strany — jen tahy s `secondsFromPrevious`.
    static func averageSecondsPerSide(from think: [EndgameThinkPlyPoint]) -> (white: Double?, black: Double?) {
        var wSum = 0.0, wCount = 0
        var bSum = 0.0, bCount = 0
        for p in think {
            guard let s = p.secondsFromPrevious else { continue }
            if p.isWhite {
                wSum += s
                wCount += 1
            } else {
                bSum += s
                bCount += 1
            }
        }
        let wAvg = wCount > 0 ? wSum / Double(wCount) : nil
        let bAvg = bCount > 0 ? bSum / Double(bCount) : nil
        return (wAvg, bAvg)
    }

    /// Celkový průměr přes všechny měřené půltahy.
    static func overallAverageSeconds(from think: [EndgameThinkPlyPoint]) -> Double? {
        let vals = think.compactMap(\.secondsFromPrevious)
        guard !vals.isEmpty else { return nil }
        return vals.reduce(0, +) / Double(vals.count)
    }
}
