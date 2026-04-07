//
//  BoardSetupFENSteps.swift
//  Pořadí polí jako `buildPuzzleSetupStepsFromFen` na webu — řádek FEN zleva doprava, rank 8 → 1.
//

import Foundation

enum BoardSetupFENSteps {
    /// Kroky „polož figurku na pole“ v pořadí čtení FEN (placement).
    static func build(from fen: String) -> [(square: String, piece: Character, label: String)] {
        let trimmed = fen.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let first = trimmed.split(separator: " ").first else { return [] }
        let boardPart = String(first)
        var steps: [(String, Character, String)] = []
        var row = 7
        var col = 0
        for ch in boardPart {
            if ch == "/" {
                row -= 1
                col = 0
                continue
            }
            if let skip = ch.wholeNumberValue {
                col += skip
                continue
            }
            let file = Character(UnicodeScalar(97 + col)!)
            let rank = row + 1
            let sq = "\(file)\(rank)"
            let label = Self.czechLabel(piece: ch, square: sq)
            steps.append((sq, ch, label))
            col += 1
        }
        return steps
    }

    /// Index do `matrix_occupied` (0…63), stejně jako JS `setupTutorialSquareToIndex`.
    static func matrixIndex(forSquare notation: String) -> Int? {
        let t = notation.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard t.count >= 2,
              let file = t.first,
              let rank = Int(t.dropFirst()),
              rank >= 1, rank <= 8
        else { return nil }
        let c = Int(file.asciiValue! - Character("a").asciiValue!)
        let r = rank - 1
        guard (0 ..< 8).contains(c), (0 ..< 8).contains(r) else { return nil }
        return r * 8 + c
    }

    private static func czechLabel(piece: Character, square: String) -> String {
        let up = piece.isUppercase
        let pieceWord: String = {
            switch piece.lowercased().first {
            case "k": return up ? "Bílý král" : "Černý král"
            case "q": return up ? "Bílá dáma" : "Černá dáma"
            case "r": return up ? "Bílá věž" : "Černá věž"
            case "b": return up ? "Bílý střelec" : "Černý střelec"
            case "n": return up ? "Bílý jezdec" : "Černý jezdec"
            case "p": return up ? "Bílý pěšec" : "Černý pěšec"
            default: return "Figurka"
            }
        }()
        return "\(pieceWord) → \(square.uppercased())"
    }
}
