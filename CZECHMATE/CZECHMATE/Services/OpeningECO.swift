//
//  OpeningECO.swift
//  Jednoduchý offline přehled zahájení podle prefixu FEN (startovní pozice).
//

import Foundation

enum OpeningECO {
    /// Vrací název zahájení nebo nil — pouze několik běžných vzorů.
    static func title(forFEN fen: String) -> String? {
        let t = fen.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        let parts = t.split(separator: " ")
        guard let first = parts.first else { return nil }
        let board = String(first)

        if board == "rnbqkbnr/pppppppp/8/8/8/8/pppppppp/rnbqkbnr" {
            return "Základní pozice"
        }
        if board.hasPrefix("rnbqkbnr/pppppppp/8/8/4p3/8/pppp1ppp/rnbqkbnr") {
            return "Obrana Englund (neobvyklá)"
        }
        if board.hasPrefix("rnbqkbnr/pppppppp/8/8/3p4/8/ppp2ppp/rnbqkbnr") {
            return "Scandinávská obrana"
        }
        if board.hasPrefix("rnbqkbnr/pppp1ppp/8/4p3/4p3/8/pppp1ppp/rnbqkbnr") {
            return "Otevřená hra"
        }
        return nil
    }
}
