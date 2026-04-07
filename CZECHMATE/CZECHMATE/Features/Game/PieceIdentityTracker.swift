//
//  PieceIdentityTracker.swift
//  Stabilní identity figurek pro matchedGeometryEffect — plynulý „přesun“ mezi políčky.
//

import Foundation

/// Drží UUID na poli; při změně šachovnice se ID přesouvá s figurkou (běžný tah, braní, rošáda, promoce).
struct PieceIdentityTracker {
    private var ids: [[String]]
    private var previousBoard: [[String]]?

    init() {
        ids = Array(repeating: Array(repeating: "", count: 8), count: 8)
    }

    /// Aktualizace při novém `board`; vrať mřížku ID stejného tvaru jako `board`.
    mutating func sync(board: [[String]]) -> [[String]] {
        guard board.count == 8 else { return ids }
        for r in 0 ..< 8 where board[r].count != 8 {
            return ids
        }

        guard let prev = previousBoard else {
            bootstrap(board)
            previousBoard = board
            return ids
        }

        if boardsEqual(prev, board) {
            return ids
        }

        applyTransition(from: prev, to: board)
        previousBoard = board
        return ids
    }

    // MARK: - Interní

    private mutating func bootstrap(_ board: [[String]]) {
        for r in 0 ..< 8 {
            for c in 0 ..< 8 {
                let ch = char(board, r, c)
                ids[r][c] = ch == " " ? "" : UUID().uuidString
            }
        }
    }

    private mutating func applyTransition(from prev: [[String]], to next: [[String]]) {
        var changed: [(Int, Int)] = []
        for r in 0 ..< 8 {
            for c in 0 ..< 8 {
                if char(prev, r, c) != char(next, r, c) {
                    changed.append((r, c))
                }
            }
        }

        switch changed.count {
        case 2:
            handleTwoCells(changed[0], changed[1], prev: prev, next: next)
        case 4:
            handleFourCells(changed, prev: prev, next: next)
        default:
            reassignChanged(changed, next: next)
        }
    }

    /// Typický tah nebo braní: přesně dvě pole se liší.
    private mutating func handleTwoCells(_ a: (Int, Int), _ b: (Int, Int), prev: [[String]], next: [[String]]) {
        let pa = char(prev, a.0, a.1), pb = char(prev, b.0, b.1)
        let na = char(next, a.0, a.1), nb = char(next, b.0, b.1)

        let aWasSource = pa != " " && na == " "
        let bWasSource = pb != " " && nb == " "

        if aWasSource && !bWasSource {
            moveIdentity(from: a, to: b, movingPieceOnDest: nb)
        } else if bWasSource && !aWasSource {
            moveIdentity(from: b, to: a, movingPieceOnDest: na)
        } else {
            reassignChanged([a, b], next: next)
        }
    }

    /// Rošáda: čtyři pole (král + věž) — párování podle stejného znaku figury (K/R, k/r).
    private mutating func handleFourCells(_ cells: [(Int, Int)], prev: [[String]], next: [[String]]) {
        var sources: [(Int, Int, Character)] = []
        var dests: [(Int, Int, Character)] = []
        for (r, c) in cells {
            let p0 = char(prev, r, c), p1 = char(next, r, c)
            if p0 != " " && p1 == " " {
                sources.append((r, c, p0))
            }
            if p1 != " " && p0 != p1 {
                dests.append((r, c, p1))
            }
        }
        guard sources.count == 2, dests.count == 2 else {
            reassignChanged(cells, next: next)
            return
        }
        var destLeft = dests
        for s in sources {
            guard let idx = destLeft.firstIndex(where: { $0.2 == s.2 }) else {
                reassignChanged(cells, next: next)
                return
            }
            let d = destLeft.remove(at: idx)
            let id = ids[s.0][s.1]
            ids[s.0][s.1] = ""
            ids[d.0][d.1] = id
        }
    }

    private mutating func moveIdentity(from: (Int, Int), to: (Int, Int), movingPieceOnDest: Character) {
        let id = ids[from.0][from.1]
        guard !id.isEmpty else {
            ids[to.0][to.1] = UUID().uuidString
            return
        }
        ids[from.0][from.1] = ""
        ids[to.0][to.1] = id
        _ = movingPieceOnDest
    }

    private mutating func reassignChanged(_ cells: [(Int, Int)], next: [[String]]) {
        for (r, c) in cells {
            let ch = char(next, r, c)
            ids[r][c] = ch == " " ? "" : UUID().uuidString
        }
    }

    private func boardsEqual(_ a: [[String]], _ b: [[String]]) -> Bool {
        for r in 0 ..< 8 {
            for c in 0 ..< 8 {
                if char(a, r, c) != char(b, r, c) { return false }
            }
        }
        return true
    }

    private func char(_ board: [[String]], _ r: Int, _ c: Int) -> Character {
        guard r >= 0, r < 8, c >= 0, c < 8, r < board.count, c < board[r].count else { return " " }
        let s = board[r][c].trimmingCharacters(in: .whitespaces)
        return s.first ?? " "
    }
}
