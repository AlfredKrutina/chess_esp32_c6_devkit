//
//  ChessBoardView.swift
//  CZECHMATE
//

import CZECHMATEShared
import SwiftUI
#if os(iOS)
import UIKit
#endif

/// Řádky `board` jako ve snímku po normalizaci: row 0 = rank 8 (černá nahoře), col 0 = a.
/// Nepřevrácený pohled: černá strana nahoře, bílá dole — `r = visRow`, `c = visCol`.
/// `flipped`: pohled o 180° — `r = 7 - visRow`, `c = 7 - visCol`.
struct ChessBoardView: View {
    let board: [[String]]
    var flipped: Bool = false
    var highlightFrom: String?
    var highlightTo: String?
    var lastMoveFrom: String?
    var lastMoveTo: String?
    var remoteSelectionSquare: String?
    /// Červený režim opravy tahu (MCU) — neplatné pole / původní pole.
    var errorInvalidSquare: String?
    var errorOriginalSquare: String?
    /// Lokální flash po zamítnutém tahu (stejná pole jako MCU).
    var invalidMoveFlashFrom: String?
    var invalidMoveFlashTo: String?
    var onRemoteSquareTap: ((String) -> Void)?
    /// Animace změny figury (volitelně vypnutelné v Nastavení).
    var animatePieces: Bool = true
    /// Popisky řad a sloupců (a–h, 1–8) — v Nastavení.
    var showCoordinates: Bool = true
    /// Barvy polí — Nastavení → Grafický styl šachovnice.
    var boardStyle: ChessBoardStyle = .wooden
    /// Odlišná paleta (béžová / fialová) pro režim Sandbox — jen vizuál.
    var sandboxFieldColors: Bool = false

    @Binding var zoom: CGFloat
    @State private var pinchBase: CGFloat = 1.0
    @Namespace private var pieceSpace
    @State private var pieceIdTracker = PieceIdentityTracker()
    @State private var pieceIdGrid: [[String]] = Array(repeating: Array(repeating: "", count: 8), count: 8)

    init(
        board: [[String]],
        flipped: Bool = false,
        highlightFrom: String? = nil,
        highlightTo: String? = nil,
        lastMoveFrom: String? = nil,
        lastMoveTo: String? = nil,
        zoom: Binding<CGFloat> = .constant(1.0),
        remoteSelectionSquare: String? = nil,
        errorInvalidSquare: String? = nil,
        errorOriginalSquare: String? = nil,
        invalidMoveFlashFrom: String? = nil,
        invalidMoveFlashTo: String? = nil,
        onRemoteSquareTap: ((String) -> Void)? = nil,
        animatePieces: Bool = true,
        showCoordinates: Bool = true,
        boardStyle: ChessBoardStyle = .wooden,
        sandboxFieldColors: Bool = false
    ) {
        self.board = board
        self.flipped = flipped
        self.highlightFrom = highlightFrom
        self.highlightTo = highlightTo
        self.lastMoveFrom = lastMoveFrom
        self.lastMoveTo = lastMoveTo
        _zoom = zoom
        self.remoteSelectionSquare = remoteSelectionSquare
        self.errorInvalidSquare = errorInvalidSquare
        self.errorOriginalSquare = errorOriginalSquare
        self.invalidMoveFlashFrom = invalidMoveFlashFrom
        self.invalidMoveFlashTo = invalidMoveFlashTo
        self.onRemoteSquareTap = onRemoteSquareTap
        self.animatePieces = animatePieces
        self.showCoordinates = showCoordinates
        self.boardStyle = boardStyle
        self.sandboxFieldColors = sandboxFieldColors
    }

    private var fromIndex: (row: Int, col: Int)? {
        guard let h = highlightFrom else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var toIndex: (row: Int, col: Int)? {
        guard let h = highlightTo else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var lastFromIndex: (row: Int, col: Int)? {
        guard let h = lastMoveFrom else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var lastToIndex: (row: Int, col: Int)? {
        guard let h = lastMoveTo else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var remoteSelIndex: (row: Int, col: Int)? {
        guard let h = remoteSelectionSquare else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var errorInvalidIndex: (row: Int, col: Int)? {
        guard let h = errorInvalidSquare else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var errorOriginalIndex: (row: Int, col: Int)? {
        guard let h = errorOriginalSquare else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var invalidFlashFromIndex: (row: Int, col: Int)? {
        guard let h = invalidMoveFlashFrom else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    private var invalidFlashToIndex: (row: Int, col: Int)? {
        guard let h = invalidMoveFlashTo else { return nil }
        return ChessSquareNotation.indices(from: h)
    }

    /// Indexy do `board[][]` z mřížky na displeji (horní řádek = rank 8 při `flipped == false`).
    private func boardIndices(visualRow: Int, visualCol: Int) -> (row: Int, col: Int) {
        if flipped {
            (7 - visualRow, 7 - visualCol)
        } else {
            (visualRow, visualCol)
        }
    }

    /// Klíč pro animaci stavu šachovnice (stejná pozice = stejný klíč).
    private var boardAnimationKey: String {
        board.map { $0.joined(separator: ",") }.joined(separator: "|")
    }

    var body: some View {
        GeometryReader { geo in
            let rankW: CGFloat = showCoordinates ? 20 : 0
            let side = max(120, geo.size.width - rankW - (showCoordinates ? 6 : 0))
            let cell = side / 8
            VStack(spacing: 4) {
                HStack(alignment: .top, spacing: 4) {
                    if showCoordinates {
                        rankLabelsColumn(cell: cell)
                            .frame(width: rankW)
                    }
                    VStack(spacing: 0) {
                        ForEach(0 ..< 8, id: \.self) { vRow in
                            HStack(spacing: 0) {
                                ForEach(0 ..< 8, id: \.self) { vCol in
                                    let idx = boardIndices(visualRow: vRow, visualCol: vCol)
                                    squareView(row: idx.row, col: idx.col, cell: cell)
                                }
                            }
                        }
                    }
                    .frame(width: side, height: side)
                    .animation(.spring(response: 0.3, dampingFraction: 0.7), value: boardAnimationKey)
                }
                if showCoordinates {
                    HStack(spacing: 4) {
                        Spacer()
                            .frame(width: rankW)
                        fileLabelsRow(cell: cell)
                            .frame(width: side)
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .center)
            .onAppear {
                pieceIdGrid = pieceIdTracker.sync(board: board)
            }
            .onChange(of: boardAnimationKey) { _, _ in
                pieceIdGrid = pieceIdTracker.sync(board: board)
            }
            .onChange(of: flipped) { _, _ in
                pieceIdTracker = PieceIdentityTracker()
                pieceIdGrid = pieceIdTracker.sync(board: board)
            }
            #if os(iOS)
            .scaleEffect(zoom)
            .contentShape(Rectangle())
            // Současná gesta — nesmí „sežrat“ jednoduché klepnutí na pole (vzdálené tahy).
            .simultaneousGesture(
                MagnificationGesture()
                    .onChanged { value in
                        let z = pinchBase * value
                        zoom = min(max(z, 1.0), 2.0)
                    }
                    .onEnded { _ in
                        pinchBase = zoom
                    }
            )
            .simultaneousGesture(
                TapGesture(count: 2)
                    .onEnded { _ in
                        zoom = 1.0
                        pinchBase = 1.0
                        HapticSettings.lightImpactIfEnabled()
                    }
            )
            .onChange(of: zoom) { _, new in
                pinchBase = new
            }
            .onAppear {
                pinchBase = zoom
            }
            #endif
        }
        .frame(maxWidth: .infinity)
        // Nevkládat vlastní `aspectRatio` sem: rodič dává bounding box (typicky čtverec z `aspectRatio(1, .fit)`).
        // Dřív 1.06 + `clipped()` zmenšovalo výšku pro `GeometryReader` — spodní řada (řada 1) a písmena a–h byly oříznuté.
        // `scaleEffect` zoom layoutem neroste; při silném pinch může mírně přetéct — drží to vnější karta kolem desky.
        #if os(iOS)
        .padding(.vertical, 2)
        #endif
        .shadow(color: .black.opacity(0.35), radius: 12, y: 6)
    }

    private func rankLabelsColumn(cell: CGFloat) -> some View {
        VStack(spacing: 0) {
            ForEach(0 ..< 8, id: \.self) { vis in
                let r = flipped ? vis : (7 - vis)
                Text("\(r + 1)")
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .frame(width: 20, height: cell)
            }
        }
    }

    @ViewBuilder
    private func fileLabelsRow(cell: CGFloat) -> some View {
        HStack(spacing: 0) {
            ForEach(0 ..< 8, id: \.self) { vis in
                let fileIndex = flipped ? (7 - vis) : vis
                let scalar = UnicodeScalar(97 + fileIndex)!
                Text(String(Character(scalar)))
                    .font(.caption2.weight(.medium))
                    .foregroundStyle(.secondary)
                    .frame(width: cell, height: 16)
            }
        }
    }

    private func squareName(row: Int, col: Int) -> String {
        let fileChar = Character(UnicodeScalar(97 + col)!)
        let rank = 8 - row
        return "\(fileChar)\(rank)".lowercased()
    }

    private func pieceIdAt(row: Int, col: Int) -> String {
        guard row >= 0, row < 8, col >= 0, col < 8,
              row < pieceIdGrid.count, col < pieceIdGrid[row].count else { return "" }
        return pieceIdGrid[row][col]
    }

    @ViewBuilder
    private func pieceLayer(piece: Character, cell: CGFloat, isLight: Bool, stableId: String) -> some View {
        let t = ChessPieceArtView(fenChar: piece, size: cell * 0.54, isLightSquare: isLight)
            .minimumScaleFactor(0.5)
        if piece != " " && !stableId.isEmpty {
            t
                .id(stableId)
                .matchedGeometryEffect(id: stableId, in: pieceSpace)
                .optionalPieceAnimation(animatePieces, value: piece)
        } else {
            t
                .optionalPieceAnimation(animatePieces, value: piece)
        }
    }

    private func squareView(row: Int, col: Int, cell: CGFloat) -> some View {
        let rankNumber = 8 - row
        let isLight = (col + rankNumber) % 2 == 0
        let piece = pieceChar(row: row, col: col)
        let pieceInstanceId = pieceIdAt(row: row, col: col)
        let isFrom = fromIndex.map { $0.row == row && $0.col == col } ?? false
        let isTo = toIndex.map { $0.row == row && $0.col == col } ?? false
        let isLastFrom = lastFromIndex.map { $0.row == row && $0.col == col } ?? false
        let isLastTo = lastToIndex.map { $0.row == row && $0.col == col } ?? false
        let showLastMove = (isLastFrom || isLastTo) && !isFrom && !isTo
        let isRemoteSel = remoteSelIndex.map { $0.row == row && $0.col == col } ?? false
        let isErrInvalid = errorInvalidIndex.map { $0.row == row && $0.col == col } ?? false
        let isErrOrig = errorOriginalIndex.map { $0.row == row && $0.col == col } ?? false
        let isInvalidFlash =
            (invalidFlashFromIndex.map { $0.row == row && $0.col == col } ?? false)
            || (invalidFlashToIndex.map { $0.row == row && $0.col == col } ?? false)

        let fieldLight = sandboxFieldColors
            ? Color(red: 0.93, green: 0.89, blue: 0.83)
            : boardStyle.squareLight
        let fieldDark = sandboxFieldColors
            ? Color(red: 0.36, green: 0.29, blue: 0.43)
            : boardStyle.squareDark

        return ZStack {
            Rectangle()
                .fill(isLight ? fieldLight : fieldDark)

            if isErrInvalid {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.red.opacity(0.42))
            } else if isErrOrig {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.orange.opacity(0.32))
            }

            if isInvalidFlash {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.red.opacity(0.55))
                    .transition(.opacity)
            }

            if isRemoteSel {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.blue.opacity(0.28))
            }

            if showLastMove {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.yellow.opacity(0.22))
            }

            if isFrom {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.green.opacity(0.42))
            }
            if isTo {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(Color.orange.opacity(0.38))
            }

            pieceLayer(
                piece: piece,
                cell: cell,
                isLight: isLight,
                stableId: pieceInstanceId
            )
        }
        .frame(width: cell, height: cell)
        .accessibilityLabel(squareAccessibilityLabel(row: row, col: col, piece: piece))
        .accessibilityAddTraits(onRemoteSquareTap != nil ? [.isButton] : [])
        .contentShape(Rectangle())
        .onTapGesture {
            if let onRemoteSquareTap {
                onRemoteSquareTap(squareName(row: row, col: col))
            }
        }
        .allowsHitTesting(onRemoteSquareTap != nil)
    }

    private func pieceChar(row: Int, col: Int) -> Character {
        guard row >= 0, row < 8, col >= 0, col < 8, row < board.count, col < board[row].count else {
            return " "
        }
        let s = board[row][col]
        return s.first ?? " "
    }

    private func squareAccessibilityLabel(row: Int, col: Int, piece: Character) -> String {
        let fileChar = Character(UnicodeScalar(97 + col)!)
        let sq = "\(fileChar)\(8 - row)"
        let name = pieceVoiceName(piece)
        if name.isEmpty {
            return "Pole \(sq), prázdné"
        }
        return "Pole \(sq), \(name)"
    }

    private func pieceVoiceName(_ ch: Character) -> String {
        switch ch {
        case "K": return "bílý král"
        case "Q": return "bílá dáma"
        case "R": return "bílá věž"
        case "B": return "bílý střelec"
        case "N": return "bílý jezdec"
        case "P": return "bílý pěšec"
        case "k": return "černý král"
        case "q": return "černá dáma"
        case "r": return "černá věž"
        case "b": return "černý střelec"
        case "n": return "černý jezdec"
        case "p": return "černý pěšec"
        default: return ""
        }
    }

}

extension View {
    @ViewBuilder
    fileprivate func optionalPieceAnimation(_ animate: Bool, value: Character) -> some View {
        if animate {
            self.animation(.spring(response: 0.38, dampingFraction: 0.82), value: value)
        } else {
            self
        }
    }
}

#Preview {
    ChessBoardView(
        board: Array(repeating: Array(repeating: " ", count: 8), count: 8),
        highlightFrom: "e2",
        highlightTo: "e4",
        lastMoveFrom: "e2",
        lastMoveTo: "e4",
        zoom: .constant(1.0)
    )
    .padding()
}
