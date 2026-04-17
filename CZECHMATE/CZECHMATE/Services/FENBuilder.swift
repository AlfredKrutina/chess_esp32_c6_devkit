//
//  FENBuilder.swift
//  CZECHMATE
//
//  Stejně zjednodušený FEN jako `boardAndStatusToFen` v chess_app.js (KQkq, bez EP).
//

import Foundation

enum FENBuilder {
    /// Řádky `board` jako po `GameSnapshot.decodeFromBoardDataRepairingAndNormalizing`: řádek 0 = rank 8 (černá nahoře), 7 = rank 1.
    /// `history.moves.count` odpovídá `halfmove` v chess_app.js pro výpočet fullmove.
    static func boardAndStatusToFEN(board: [[String]], status: GameStatus, halfmoveCount n: Int) -> String? {
        guard board.count == 8 else { return nil }
        var rows: [String] = []
        for r in 0 ..< 8 {
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
        let sideToMove = sideToMoveChar(from: status.currentPlayer)
        let fullmove = max(1, n / 2 + 1)
        let fen = "\(piecePlacement) \(sideToMove) KQkq - 0 \(fullmove)"
        guard fen.count >= 20, fen.count <= 120 else { return nil }
        return fen
    }

    private static func sideToMoveChar(from currentPlayer: String) -> String {
        let t = currentPlayer.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if t == "black" || t == "b" { return "b" }
        if t == "white" || t == "w" { return "w" }
        return currentPlayer == "Black" ? "b" : "w"
    }
}

