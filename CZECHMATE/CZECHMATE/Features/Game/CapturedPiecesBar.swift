//
//  CapturedPiecesBar.swift
//  CZECHMATE — materiál sebraný z desky (porovnání se startovní sadou).
//

import CZECHMATEShared
import SwiftUI

enum CapturedPiecesBar {
    /// Obě barvy mají krále — rozumná pozice pro výpočet materiálu (ne prázdná šablona).
    static func isPlayablePosition(board: [[String]]) -> Bool {
        var hasWhiteKing = false
        var hasBlackKing = false
        for row in board {
            for cell in row {
                guard let ch = cell.first, ch != " " else { continue }
                if ch == "K" { hasWhiteKing = true }
                if ch == "k" { hasBlackKing = true }
            }
        }
        return hasWhiteKing && hasBlackKing
    }

    /// Figurky, které chybí bílému (sebral černý).
    static func missingWhitePieceChars(board: [[String]]) -> [Character] {
        missingPieceChars(board: board, isWhitePieces: true)
    }

    /// Figurky, které chybí černému (sebral bílý).
    static func missingBlackPieceChars(board: [[String]]) -> [Character] {
        missingPieceChars(board: board, isWhitePieces: false)
    }

    private static func missingPieceChars(board: [[String]], isWhitePieces: Bool) -> [Character] {
        let start: [Character: Int] = isWhitePieces
            ? ["K": 1, "Q": 1, "R": 2, "B": 2, "N": 2, "P": 8]
            : ["k": 1, "q": 1, "r": 2, "b": 2, "n": 2, "p": 8]
        var onBoard: [Character: Int] = [:]
        for row in board {
            for cell in row {
                guard let ch = cell.first, ch != " " else { continue }
                if isWhitePieces {
                    guard ch.isUppercase else { continue }
                } else {
                    guard ch.isLowercase else { continue }
                }
                onBoard[ch, default: 0] += 1
            }
        }
        let order: [Character] = isWhitePieces
            ? ["Q", "R", "B", "N", "P", "K"]
            : ["q", "r", "b", "n", "p", "k"]
        var out: [Character] = []
        for piece in order {
            let need = start[piece] ?? 0
            let have = onBoard[piece] ?? 0
            let missing = max(0, need - have)
            guard missing > 0 else { continue }
            for _ in 0 ..< missing {
                out.append(piece)
            }
        }
        return out
    }
}

struct CapturedPiecesStrip: View {
    let board: [[String]]

    var body: some View {
        let wLost = CapturedPiecesBar.missingWhitePieceChars(board: board)
        let bLost = CapturedPiecesBar.missingBlackPieceChars(board: board)
        HStack(alignment: .top, spacing: 16) {
            VStack(alignment: .leading, spacing: 6) {
                Text("Sebráno bílému")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.secondary)
                flowSymbols(wLost, alignment: .leading)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            VStack(alignment: .trailing, spacing: 6) {
                Text("Sebráno černému")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.secondary)
                flowSymbols(bLost, alignment: .trailing)
            }
            .frame(maxWidth: .infinity, alignment: .trailing)
        }
        .padding(.vertical, 4)
    }

    @ViewBuilder
    private func flowSymbols(_ pieces: [Character], alignment: HorizontalAlignment) -> some View {
        if pieces.isEmpty {
            Text("—")
                .font(.subheadline)
                .foregroundStyle(.tertiary)
        } else {
            LazyVGrid(
                columns: [GridItem(.adaptive(minimum: 28), alignment: alignment == .leading ? .leading : .trailing)],
                alignment: alignment == .leading ? .leading : .trailing,
                spacing: 4
            ) {
                ForEach(Array(pieces.enumerated()), id: \.offset) { _, ch in
                    ChessPieceArtView(fenChar: ch, size: 26, isLightSquare: true)
                }
            }
        }
    }
}
