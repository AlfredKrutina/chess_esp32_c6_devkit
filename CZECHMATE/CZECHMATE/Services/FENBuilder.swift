//
//  FENBuilder.swift
//  CZECHMATE
//
//  Stejně zjednodušený FEN jako `boardAndStatusToFen` v chess_app.js (KQkq, bez EP).
//

import Foundation

enum FENBuilder {
    /// Řádky board: row 0 = rank 1 (bílá strana dole na fyzické desce v JSON z firmware).
    /// `history.moves.count` odpovídá `halfmove` v chess_app.js pro výpočet fullmove.
    static func boardAndStatusToFEN(board: [[String]], status: GameStatus, history: MoveHistory) -> String? {
        guard board.count == 8 else { return nil }
        var rows: [String] = []
        for r in stride(from: 7, through: 0, by: -1) {
            guard board[r].count == 8 else { return nil }
            var rank = ""
            var empty = 0
            for c in 0 ..< 8 {
                let cell = board[r][c]
                let ch = cell.first.map(String.init) ?? " "
                if ch == " " || ch.isEmpty {
                    empty += 1
                } else {
                    if empty > 0 {
                        rank += String(empty)
                        empty = 0
                    }
                    rank += ch
                }
            }
            if empty > 0 { rank += String(empty) }
            rows.append(rank)
        }
        let piecePlacement = rows.joined(separator: "/")
        let sideToMove = status.currentPlayer == "White" ? "w" : "b"
        let n = history.moves.count
        let fullmove = max(1, n / 2 + 1)
        let fen = "\(piecePlacement) \(sideToMove) KQkq - 0 \(fullmove)"
        guard fen.count >= 20, fen.count <= 120 else { return nil }
        return fen
    }
}
