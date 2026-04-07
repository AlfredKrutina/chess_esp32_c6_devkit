//
//  FENParser.swift
//  CZECHMATE — piece placement → board[row][col], row 0 = rank 1 (jako firmware).
//

import Foundation

enum FENParser {
    /// Parsuje první pole FEN (postavení figur), např. `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR`
    static func parseBoard(fromFEN fen: String) -> [[String]]? {
        let trimmed = fen.trimmingCharacters(in: .whitespacesAndNewlines)
        let parts = trimmed.split(separator: " ")
        guard let first = parts.first else { return nil }
        return parsePiecePlacement(String(first))
    }

    static func parsePiecePlacement(_ s: String) -> [[String]]? {
        let ranks = s.split(separator: "/")
        guard ranks.count == 8 else { return nil }
        var rows: [[String]] = Array(repeating: Array(repeating: " ", count: 8), count: 8)
        for (i, rankStr) in ranks.enumerated() {
            let rowIndex = 7 - i
            var row: [String] = []
            for ch in rankStr {
                if ch.isNumber {
                    guard let n = ch.wholeNumberValue, n >= 1, n <= 8 else { return nil }
                    for _ in 0 ..< n {
                        row.append(" ")
                    }
                } else {
                    row.append(String(ch))
                }
            }
            guard row.count == 8 else { return nil }
            rows[rowIndex] = row
        }
        return rows
    }

    /// Text pro UI („Bílý na tahu“ / „Černý na tahu“) z druhého pole FEN.
    static func sideToMoveDescription(fromFEN fen: String) -> String {
        let parts = fen.split(separator: " ")
        guard parts.count >= 2 else { return "Na tahu" }
        let side = parts[1]
        if side == "w" { return "Bílý na tahu" }
        if side == "b" { return "Černý na tahu" }
        return "Na tahu"
    }
}
