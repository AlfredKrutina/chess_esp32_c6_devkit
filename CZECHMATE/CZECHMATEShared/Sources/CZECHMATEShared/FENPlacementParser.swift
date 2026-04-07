//
//  FENPlacementParser.swift
//  CZECHMATEShared — první pole FEN → board[row][col], row 0 = rank 1 (jako firmware).
//

import Foundation

public enum FENPlacementParser {
    /// Parsuje jen piece placement z plného FEN řetězce (první token).
    public static func boardFromFullFEN(_ fen: String) -> [[String]]? {
        let t = fen.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let first = t.split(separator: " ").first else { return nil }
        return boardFromPlacement(String(first))
    }

    /// `piecePlacement` = první část FEN před první mezerou (např. `rnbqkbnr/pppppppp/...`).
    public static func boardFromPlacement(_ piecePlacement: String) -> [[String]]? {
        let ranks = piecePlacement.split(separator: "/")
        guard ranks.count == 8 else { return nil }
        var board = Array(repeating: Array(repeating: " ", count: 8), count: 8)
        for (fenRankIndex, rankStr) in ranks.enumerated() {
            let ourRow = 7 - fenRankIndex
            var col = 0
            for ch in rankStr {
                if let skip = ch.wholeNumberValue {
                    col += skip
                } else {
                    guard col < 8 else { return nil }
                    board[ourRow][col] = String(ch)
                    col += 1
                }
            }
            guard col == 8 else { return nil }
        }
        return board
    }
}
