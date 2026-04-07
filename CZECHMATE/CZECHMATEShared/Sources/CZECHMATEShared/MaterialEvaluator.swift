//
//  MaterialEvaluator.swift
//  CZECHMATE Shared — hrubý přehled materiálu (Q=9 …) pro Watch / Live Activity.
//

import Foundation

public enum MaterialEvaluator {
    /// Součet hodnot figur (bílé − černé). Kladné = navrch bílý.
    public static func materialDeltaPawns(board: [[String]]) -> Int {
        var white = 0
        var black = 0
        for row in board {
            for cell in row {
                guard let ch = cell.first, ch != " " else { continue }
                let v = pieceValue(ch)
                if ch.isUppercase {
                    white += v
                } else {
                    black += v
                }
            }
        }
        return white - black
    }

    /// Krátký text pro UI (hodinky, Live Activity).
    public static func advantageSummary(board: [[String]]) -> String {
        let d = materialDeltaPawns(board: board)
        if d == 0 {
            return "Materiál: remíza"
        }
        if d > 0 {
            return "Navrch bílý (+\(d))"
        }
        return "Navrch černý (+\(-d))"
    }

    private static func pieceValue(_ ch: Character) -> Int {
        switch ch {
        case "P", "p": return 1
        case "N", "n", "B", "b": return 3
        case "R", "r": return 5
        case "Q", "q": return 9
        case "K", "k": return 0
        default: return 0
        }
    }
}
