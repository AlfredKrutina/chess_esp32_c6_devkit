//
//  ChessSetupPieceGlyph.swift
//  Unicode symboly pro velký náhled v průvodci postavením.
//

import Foundation

enum ChessSetupPieceGlyph {
    static func symbol(for fenChar: Character) -> String {
        switch fenChar {
        case "K": return "♔"
        case "Q": return "♕"
        case "R": return "♖"
        case "B": return "♗"
        case "N": return "♘"
        case "P": return "♙"
        case "k": return "♚"
        case "q": return "♛"
        case "r": return "♜"
        case "b": return "♝"
        case "n": return "♞"
        case "p": return "♟"
        default: return String(fenChar)
        }
    }
}
