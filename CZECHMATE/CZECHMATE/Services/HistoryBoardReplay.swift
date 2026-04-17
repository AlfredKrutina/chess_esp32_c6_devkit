//
//  HistoryBoardReplay.swift
//  Lokální přehrání pozice z historie tahů (základní pravidla vč. rošády a promoci).
//

import Foundation

enum HistoryBoardReplay {
    /// Vrátí pozici po prvních `moveCount` tazích; při nepodporovaném tahu `nil`.
    static func board(afterApplyingFirst moveCount: Int, moves: [HistoryMove]) -> [[String]]? {
        guard moveCount >= 0, moveCount <= moves.count else { return nil }
        var b = standardStart()
        for i in 0 ..< moveCount {
            guard apply(moves[i], to: &b) else { return nil }
        }
        return b
    }

    private static func standardStart() -> [[String]] {
        [
            ["r", "n", "b", "q", "k", "b", "n", "r"],
            ["p", "p", "p", "p", "p", "p", "p", "p"],
            [" ", " ", " ", " ", " ", " ", " ", " "],
            [" ", " ", " ", " ", " ", " ", " ", " "],
            [" ", " ", " ", " ", " ", " ", " ", " "],
            [" ", " ", " ", " ", " ", " ", " ", " "],
            ["P", "P", "P", "P", "P", "P", "P", "P"],
            ["R", "N", "B", "Q", "K", "B", "N", "R"],
        ]
    }

    private static func apply(_ m: HistoryMove, to b: inout [[String]]) -> Bool {
        guard let fromSquare = m.from, let toSquare = m.to,
              let from = FirmwareSquareNotation.indices(from: fromSquare),
              let to = FirmwareSquareNotation.indices(from: toSquare)
        else { return false }
        let piece = b[from.row][from.col].trimmingCharacters(in: .whitespaces)
        guard piece != " ", !piece.isEmpty else { return false }

        let isWhite = piece.first?.isUppercase == true
        let pieceLower = piece.lowercased()

        // Rošáda (král o 2 pole)
        if pieceLower == "k", abs(to.col - from.col) == 2 {
            return applyCastling(from: from, to: to, isWhite: isWhite, board: &b)
        }

        let destBefore = b[to.row][to.col]
        let isCapture = destBefore != " " && !destBefore.isEmpty

        // Braní mimochodem — zjednodušeně: nepokrýváme (vrátí nil výš)
        if pieceLower == "p", from.col != to.col, !isCapture {
            return false
        }

        b[to.row][to.col] = b[from.row][from.col]
        b[from.row][from.col] = " "

        // Promoce: cíl na poslední řádě a pěšec
        if pieceLower == "p", (to.row == 0 || to.row == 7) {
            let promoted: String
            if let p = m.piece, p.count == 1, let ch = p.first {
                let u = String(ch)
                promoted = isWhite ? u.uppercased() : u.lowercased()
            } else {
                promoted = isWhite ? "Q" : "q"
            }
            b[to.row][to.col] = promoted
        }

        return true
    }

    private static func applyCastling(
        from: (row: Int, col: Int),
        to: (row: Int, col: Int),
        isWhite: Bool,
        board: inout [[String]]
    ) -> Bool {
        let row = from.row
        guard row == to.row, abs(to.col - from.col) == 2 else { return false }
        let king = board[from.row][from.col]
        guard king.lowercased() == "k" else { return false }

        board[to.row][to.col] = king
        board[from.row][from.col] = " "

        if to.col > from.col {
            // Královská strana
            let rookFromCol = 7
            let rookToCol = to.col - 1
            let rook = board[row][rookFromCol]
            guard rook.lowercased() == "r" else { return false }
            board[row][rookToCol] = rook
            board[row][rookFromCol] = " "
        } else {
            let rookFromCol = 0
            let rookToCol = to.col + 1
            let rook = board[row][rookFromCol]
            guard rook.lowercased() == "r" else { return false }
            board[row][rookToCol] = rook
            board[row][rookFromCol] = " "
        }
        return true
    }
}
