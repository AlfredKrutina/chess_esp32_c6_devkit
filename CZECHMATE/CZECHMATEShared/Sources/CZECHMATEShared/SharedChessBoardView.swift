//
//  SharedChessBoardView.swift
//  Kompaktní šachovnice (iOS náhled / watchOS).
//

import SwiftUI

private let fileLetters: [Character] = ["a", "b", "c", "d", "e", "f", "g", "h"]

public struct SharedChessBoardView: View {
    public let board: [[String]]
    public var flipped: Bool
    public var highlightFrom: String?
    public var highlightTo: String?
    public var rankLabelWidth: CGFloat
    public var cellSize: CGFloat?

    private let light = Color(red: 0.96, green: 0.88, blue: 0.72)
    private let dark = Color(red: 0.55, green: 0.40, blue: 0.28)

    public init(
        board: [[String]],
        flipped: Bool = false,
        highlightFrom: String? = nil,
        highlightTo: String? = nil,
        rankLabelWidth: CGFloat = 18,
        cellSize: CGFloat? = nil
    ) {
        self.board = board
        self.flipped = flipped
        self.highlightFrom = highlightFrom
        self.highlightTo = highlightTo
        self.rankLabelWidth = rankLabelWidth
        self.cellSize = cellSize
    }

    private var fromIndex: (row: Int, col: Int)? {
        guard let h = highlightFrom else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var toIndex: (row: Int, col: Int)? {
        guard let h = highlightTo else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    /// Stejná logika jako CZECHMATE `ChessBoardView`: `board` má řádek 0 = rank 8; bílý dole = `flipped == false`.
    private func boardIndices(visualRow: Int, visualCol: Int, flipped: Bool) -> (row: Int, col: Int) {
        if flipped {
            (7 - visualRow, 7 - visualCol)
        } else {
            (visualRow, visualCol)
        }
    }

    public var body: some View {
        GeometryReader { geo in
            let side: CGFloat = {
                if let c = cellSize { return min(8 * c, geo.size.width - rankLabelWidth - 6) }
                return max(80, geo.size.width - rankLabelWidth - 6)
            }()
            let cell = side / 8
            VStack(spacing: 4) {
                HStack(alignment: .top, spacing: 4) {
                    rankLabelsColumn(cell: cell)
                        .frame(width: rankLabelWidth)
                    VStack(spacing: 0) {
                        ForEach(0 ..< 8, id: \.self) { vRow in
                            HStack(spacing: 0) {
                                ForEach(0 ..< 8, id: \.self) { vCol in
                                    let idx = boardIndices(visualRow: vRow, visualCol: vCol, flipped: flipped)
                                    squareView(row: idx.row, col: idx.col, cell: cell)
                                }
                            }
                        }
                    }
                    .frame(width: side, height: side)
                }
                HStack(spacing: 4) {
                    Spacer()
                        .frame(width: rankLabelWidth)
                    fileLabelsRow(cell: cell)
                        .frame(width: side)
                }
            }
            .frame(maxWidth: .infinity, alignment: .center)
        }
        .frame(maxWidth: .infinity)
    }

    private func rankLabelsColumn(cell: CGFloat) -> some View {
        VStack(spacing: 0) {
            ForEach(0 ..< 8, id: \.self) { vis in
                let r = flipped ? vis : (7 - vis)
                Text("\(r + 1)")
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .frame(width: rankLabelWidth, height: cell)
            }
        }
    }

    private func fileLabelsRow(cell: CGFloat) -> some View {
        HStack(spacing: 0) {
            ForEach(0 ..< 8, id: \.self) { vis in
                let c = flipped ? (7 - vis) : vis
                Text(String(fileLetters[c]))
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .frame(width: cell, height: 14)
            }
        }
    }

    private func squareView(row: Int, col: Int, cell: CGFloat) -> some View {
        let rankNumber = 8 - row
        let isLight = (col + rankNumber) % 2 == 0
        let piece = (row < board.count && col < board[row].count) ? board[row][col] : " "
        let isFrom = fromIndex.map { $0.row == row && $0.col == col } ?? false
        let isTo = toIndex.map { $0.row == row && $0.col == col } ?? false
        return ZStack {
            Rectangle()
                .fill(isLight ? light : dark)
            if let fen = piece.first, fen != " " {
                ChessPieceArtView(fenChar: fen, size: cell * 0.62, isLightSquare: isLight)
                    .minimumScaleFactor(0.5)
            }
        }
        .frame(width: cell, height: cell)
        .overlay {
            if isFrom {
                RoundedRectangle(cornerRadius: 3, style: .continuous)
                    .fill(Color.green.opacity(0.4))
            }
            if isTo {
                RoundedRectangle(cornerRadius: 3, style: .continuous)
                    .fill(Color.orange.opacity(0.38))
            }
        }
    }

}
