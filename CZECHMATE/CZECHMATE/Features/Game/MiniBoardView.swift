//
//  MiniBoardView.swift
//  CZECHMATE — kompaktní náhled pozice (Analýza, Puzzle, nápověda).
//

import CZECHMATEShared
import SwiftUI

/// Malá neinteraktivní šachovnice; `board` ve stejném formátu jako z ESP (řádek 0 = rank 1).
struct MiniBoardView: View {
    let board: [[String]]
    var cellSide: CGFloat = 20

    private let light = Color(red: 0.96, green: 0.88, blue: 0.72)
    private let dark = Color(red: 0.55, green: 0.40, blue: 0.28)

    var body: some View {
        let side = cellSide * 8
        VStack(spacing: 0) {
            ForEach(0 ..< 8, id: \.self) { vRow in
                HStack(spacing: 0) {
                    ForEach(0 ..< 8, id: \.self) { vCol in
                        let r = 7 - vRow
                        let c = vCol
                        square(row: r, col: c)
                    }
                }
            }
        }
        .frame(width: side, height: side)
        .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 6, style: .continuous)
                .stroke(Color.secondary.opacity(0.25), lineWidth: 1)
        )
        .shadow(color: .black.opacity(0.12), radius: 4, y: 2)
    }

    @ViewBuilder
    private func square(row: Int, col: Int) -> some View {
        let isLight = (row + col) % 2 == 0
        let piece = pieceChar(row: row, col: col)
        ZStack {
            Rectangle().fill(isLight ? light : dark)
            ChessPieceArtView(fenChar: piece, size: cellSide * 0.52, isLightSquare: isLight)
                .minimumScaleFactor(0.5)
        }
        .frame(width: cellSide, height: cellSide)
    }

    private func pieceChar(row: Int, col: Int) -> Character {
        guard row >= 0, row < 8, col >= 0, col < 8,
              row < board.count, col < board[row].count else { return " " }
        return board[row][col].first ?? " "
    }

}

/// Náhled z FEN řetězce (např. z Lichess).
struct MiniBoardFromFENView: View {
    let fen: String
    var cellSide: CGFloat = 20

    var body: some View {
        Group {
            if let b = FENParser.parseBoard(fromFEN: fen) {
                MiniBoardView(board: b, cellSide: cellSide)
            } else {
                Image(systemName: "exclamationmark.triangle")
                    .foregroundStyle(.tertiary)
            }
        }
    }
}

#Preview {
    MiniBoardFromFENView(fen: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        .padding()
}
