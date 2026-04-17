//
//  MoveHintTier.swift
//  CZECHMATE — stupňovaná nápověda (LED + text): cíl → výchozí pole → celý tah.
//

import Foundation

enum MoveHintTier: Int, Codable, CaseIterable, Identifiable, Sendable {
    /// H1: jen cílové pole (kam směřuje nejsilnější tah).
    case destinationOnly = 1
    /// H2: jen pole výchozí figurky (bez kam táhnout).
    case originOnly = 2
    /// H3: plný tah from → to (dosavadní chování).
    case fullMove = 3

    var id: Int { rawValue }

    var shortTitle: String {
        switch self {
        case .destinationOnly: return "H1 · cíl"
        case .originOnly: return "H2 · odkud"
        case .fullMove: return "H3 · tah"
        }
    }

    var detailDescription: String {
        switch self {
        case .destinationOnly:
            return "Jen cílové pole na LED a v aplikaci — bez výchozího pole."
        case .originOnly:
            return "Jen pole figurky, kterou Stockfish preferuje — kam táhnout neukážeme."
        case .fullMove:
            return "Úplný tah (šipka) na desce i LED."
        }
    }

    func summaryLine(best: StockfishBestMove) -> String {
        let ev = best.evalPawns.map { String(format: " (eval %.2f)", $0) } ?? ""
        switch self {
        case .destinationOnly:
            return "Zaměření na pole \(best.to.uppercased())\(ev)"
        case .originOnly:
            return "Táhni figurou z \(best.from.uppercased())\(ev)"
        case .fullMove:
            return "\(best.from.uppercased()) → \(best.to.uppercased())\(ev)"
        }
    }
}
