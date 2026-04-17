//
//  MoveQualitySummary.swift
//  CZECHMATE — agregace známek tahů (Stockfish) pro Analýzu.
//

import Foundation

/// Jedna časová okena (např. poslední 3 tahy strany).
struct MoveQualityWindowStats: Sendable, Equatable {
    /// Počet hodnocených tahů v okně.
    let count: Int
    /// Průměr 0…100 odvozený od `MoveGrade` (viz `MoveQualityAggregator`).
    let averageQualityPercent: Double?
    /// Průměrná ztráta v centipawnech, pokud je u tahů vyplněné `lossPawns`.
    let averageCpLoss: Double?
}

struct PlayerMoveQualityTriplet: Sendable, Equatable {
    let last3: MoveQualityWindowStats
    let last10: MoveQualityWindowStats
    let fullGame: MoveQualityWindowStats
}

struct MoveQualityBoardSummary: Sendable, Equatable {
    let white: PlayerMoveQualityTriplet
    let black: PlayerMoveQualityTriplet
}

enum MoveQualityAggregator {
    /// `moveIndex1Based` 1 = první tah (bílý), 2 = černý, …
    static func isWhiteMove(moveIndex1Based: Int) -> Bool {
        moveIndex1Based % 2 == 1
    }

    /// Heuristický převod stupně na „skóre správnosti“ 0…100 (průměr = smysl „kvality tahů“).
    static func qualityPercent(for grade: MoveGrade) -> Double {
        switch grade {
        case .best: return 100
        case .good: return 88
        case .inaccuracy: return 62
        case .mistake: return 35
        case .blunder: return 12
        case .error: return 45
        }
    }

    static func windowStats(for entries: [MoveEvalHistoryEntry]) -> MoveQualityWindowStats {
        guard !entries.isEmpty else {
            return MoveQualityWindowStats(count: 0, averageQualityPercent: nil, averageCpLoss: nil)
        }
        let percents = entries.map { qualityPercent(for: $0.grade) }
        let avgP = percents.reduce(0, +) / Double(percents.count)
        let cps = entries.compactMap(\.lossPawns).map { $0 * 100 }
        let avgCp = cps.isEmpty ? nil : cps.reduce(0, +) / Double(cps.count)
        return MoveQualityWindowStats(count: entries.count, averageQualityPercent: avgP, averageCpLoss: avgCp)
    }

    static func boardSummary(from history: [MoveEvalHistoryEntry]) -> MoveQualityBoardSummary {
        let sorted = history.sorted { $0.moveIndex1Based < $1.moveIndex1Based }
        let whiteAll = sorted.filter { isWhiteMove(moveIndex1Based: $0.moveIndex1Based) }
        let blackAll = sorted.filter { !isWhiteMove(moveIndex1Based: $0.moveIndex1Based) }

        let wTriplet = PlayerMoveQualityTriplet(
            last3: windowStats(for: Array(whiteAll.suffix(3))),
            last10: windowStats(for: Array(whiteAll.suffix(10))),
            fullGame: windowStats(for: whiteAll)
        )
        let bTriplet = PlayerMoveQualityTriplet(
            last3: windowStats(for: Array(blackAll.suffix(3))),
            last10: windowStats(for: Array(blackAll.suffix(10))),
            fullGame: windowStats(for: blackAll)
        )
        return MoveQualityBoardSummary(white: wTriplet, black: bTriplet)
    }
}
