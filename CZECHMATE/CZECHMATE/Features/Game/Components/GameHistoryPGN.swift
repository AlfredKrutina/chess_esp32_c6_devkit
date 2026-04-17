//
//  GameHistoryPGN.swift
//  CZECHMATE — jednoduchý PGN export z historie snímku (SAN nebo zástupná notace).
//

import Foundation

enum GameHistoryPGN {
    /// Výsledek PGN tagu `[Result]` z modelu souhrnu.
    static func resultTag(from winner: GameEndWinnerSide) -> String {
        switch winner {
        case .white: return "1-0"
        case .black: return "0-1"
        case .draw: return "1/2-1/2"
        }
    }

    /// Minimální PGN: tagy Event/Site/Date/Result + řádek tahů (čísla + SAN / fallback).
    static func build(moves: [GameHistoryMove], result: GameEndWinnerSide, event: String = "CZECHMATE Board") -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.locale = Locale(identifier: "en_US_POSIX")
        dateFormatter.timeZone = TimeZone(secondsFromGMT: 0)
        dateFormatter.dateFormat = "yyyy.MM.dd"
        let dateStr = dateFormatter.string(from: Date())

        var lines: [String] = [
            "[Event \"\(escapePGN(event))\"]",
            "[Site \"?\"]",
            "[Date \"\(dateStr)\"]",
            "[Result \"\(resultTag(from: result))\"]",
            "",
        ]

        let sanMoves = moves.enumerated().map { i, m -> String in
            if let s = m.san?.trimmingCharacters(in: .whitespacesAndNewlines), !s.isEmpty {
                return MoveHistoryNotation.prettifySAN(s)
            }
            if let f = m.from, let t = m.to {
                return "\(f)-\(t)"
            }
            return "…"
        }

        var moveLine = ""
        var fullMove = 1
        var i = 0
        while i < sanMoves.count {
            if i % 2 == 0 {
                moveLine += "\(fullMove). \(sanMoves[i])"
                if i + 1 < sanMoves.count {
                    moveLine += " \(sanMoves[i + 1]) "
                } else {
                    moveLine += " "
                }
                fullMove += 1
                i += 2
            } else {
                i += 1
            }
        }
        moveLine += resultTag(from: result)
        lines.append(moveLine.trimmingCharacters(in: .whitespaces))
        return lines.joined(separator: "\n")
    }

    private static func escapePGN(_ s: String) -> String {
        s.replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
    }
}
