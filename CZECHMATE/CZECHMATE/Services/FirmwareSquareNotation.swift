//
//  FirmwareSquareNotation.swift
//  Mapování „e2“ → indexy do `board[][]` (shodné s CZECHMATEShared.ChessSquareNotation): řádek 0 = rank 8, řádek 7 = rank 1.
//

import Foundation

enum FirmwareSquareNotation {
    /// Indexy do pole `snapshot.board` (stejná konvence jako `context/snapshot_golden.json`).
    static func indices(from notation: String) -> (row: Int, col: Int)? {
        let t = notation.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard t.count >= 2,
              let file = t.first,
              let rank = Int(t.dropFirst()),
              rank >= 1, rank <= 8
        else { return nil }
        let col = Int(file.asciiValue! - Character("a").asciiValue!)
        guard (0 ..< 8).contains(col) else { return nil }
        let row = 8 - rank
        guard (0 ..< 8).contains(row) else { return nil }
        return (row, col)
    }
}
