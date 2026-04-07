//
//  ChessSquareNotation.swift
//  CZECHMATEShared — UCI „e2“, row 0 = rank 1.
//

import Foundation

public enum ChessSquareNotation {
    public static func indices(from notation: String) -> (row: Int, col: Int)? {
        let t = notation.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard t.count >= 2,
              let file = t.first,
              let rank = Int(t.dropFirst()),
              rank >= 1, rank <= 8
        else { return nil }
        let col = Int(file.asciiValue! - Character("a").asciiValue!)
        guard (0 ..< 8).contains(col) else { return nil }
        let row = rank - 1
        return (row, col)
    }
}
