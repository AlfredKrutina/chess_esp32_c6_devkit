//
//  MoveHistoryView.swift
//  Horizontální historie tahů ve stylu chess.com — dvojice Bílý/Černý, zvýraznění posledního tahu.
//

import CZECHMATEShared
import SwiftUI

// MARK: - Parsování a zobrazení notace

enum MoveHistoryNotation {
    /// Zobrazitelný text tahu (SAN nebo odvozený z údajů z desky).
    static func displayString(for move: HistoryMove) -> String {
        if let s = move.san?.trimmingCharacters(in: .whitespacesAndNewlines), !s.isEmpty {
            return prettifySAN(s)
        }
        return engineCompact(move)
    }

    /// Kompaktní notace z from/to/piece (bez plného SAN enginu).
    private static func engineCompact(_ m: HistoryMove) -> String {
        let from = m.from.lowercased()
        let to = m.to.lowercased()
        let pc = m.piece.first ?? " "
        if let cs = castlingNotation(from: from, to: to, piece: pc) {
            return cs
        }
        if pc == "P" || pc == "p" {
            return to
        }
        let sym = ChessPieceGlyph.symbol(for: pc)
        if sym.isEmpty { return to }
        return sym + to
    }

    private static func castlingNotation(from: String, to: String, piece: Character) -> String? {
        guard piece == "K" || piece == "k" else { return nil }
        let f = from.lowercased()
        let t = to.lowercased()
        if f == "e1", t == "g1" { return "O-O" }
        if f == "e1", t == "c1" { return "O-O-O" }
        if f == "e8", t == "g8" { return "O-O" }
        if f == "e8", t == "c8" { return "O-O-O" }
        return nil
    }

    /// Nahradí úvodní písmena figur v SAN Unicode symboly; rošádu a en passant nechá v ASCII kde je to bezpečné.
    static func prettifySAN(_ raw: String) -> String {
        var s = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if s.hasPrefix("O-O-O") { return s }
        if s.hasPrefix("O-O") { return s }
        guard let first = s.first else { return s }
        let pieceMap: [Character: String] = [
            "N": ChessPieceGlyph.symbol(for: "N"),
            "B": ChessPieceGlyph.symbol(for: "B"),
            "R": ChessPieceGlyph.symbol(for: "R"),
            "Q": ChessPieceGlyph.symbol(for: "Q"),
            "K": ChessPieceGlyph.symbol(for: "K"),
        ]
        if let rep = pieceMap[first] {
            s = rep + String(s.dropFirst())
        }
        if s.hasSuffix("=Q") {
            s = String(s.dropLast(2)) + "=" + ChessPieceGlyph.symbol(for: "Q")
        } else if s.hasSuffix("=R") {
            s = String(s.dropLast(2)) + "=" + ChessPieceGlyph.symbol(for: "R")
        } else if s.hasSuffix("=B") {
            s = String(s.dropLast(2)) + "=" + ChessPieceGlyph.symbol(for: "B")
        } else if s.hasSuffix("=N") {
            s = String(s.dropLast(2)) + "=" + ChessPieceGlyph.symbol(for: "N")
        }
        return s
    }
}

// MARK: - Řádky (1. bílý / černý)

private struct MoveHistoryPlyRow: Identifiable {
    var id: Int { moveNumber }
    let moveNumber: Int
    let whiteIndex: Int
    let whiteText: String
    let blackIndex: Int?
    let blackText: String?

    static func build(from moves: [HistoryMove]) -> [MoveHistoryPlyRow] {
        guard !moves.isEmpty else { return [] }
        var rows: [MoveHistoryPlyRow] = []
        var i = 0
        var fullMove = 1
        while i < moves.count {
            let w = moves[i]
            let wText = MoveHistoryNotation.displayString(for: w)
            if i + 1 < moves.count {
                let b = moves[i + 1]
                let bText = MoveHistoryNotation.displayString(for: b)
                rows.append(
                    MoveHistoryPlyRow(
                        moveNumber: fullMove,
                        whiteIndex: i,
                        whiteText: wText,
                        blackIndex: i + 1,
                        blackText: bText
                    )
                )
                i += 2
            } else {
                rows.append(
                    MoveHistoryPlyRow(
                        moveNumber: fullMove,
                        whiteIndex: i,
                        whiteText: wText,
                        blackIndex: nil,
                        blackText: nil
                    )
                )
                i += 1
            }
            fullMove += 1
        }
        return rows
    }
}

// MARK: - View

struct MoveHistoryView: View {
    let moves: [HistoryMove]
    @Binding var selectedMoveIndex: Int?
    /// Když false, jen zobrazení (např. Analýza).
    var interactive: Bool

    init(moves: [HistoryMove], selectedMoveIndex: Binding<Int?> = .constant(nil), interactive: Bool = true) {
        self.moves = moves
        _selectedMoveIndex = selectedMoveIndex
        self.interactive = interactive
    }

    private var rows: [MoveHistoryPlyRow] { MoveHistoryPlyRow.build(from: moves) }

    private var lastGlobalIndex: Int? {
        guard !moves.isEmpty else { return nil }
        return moves.count - 1
    }

    private func isEmphasized(globalIndex: Int) -> Bool {
        if let sel = selectedMoveIndex {
            return sel == globalIndex
        }
        return globalIndex == lastGlobalIndex
    }

    var body: some View {
        if moves.isEmpty {
            EmptyView()
        } else {
            ScrollViewReader { proxy in
                ScrollView(.horizontal, showsIndicators: false) {
                    // Jedna logická „řada“ tahů jako na chess.com — LazyHStack = jeden vizuální řádek vodorovně.
                    LazyHStack(alignment: .firstTextBaseline, spacing: 0) {
                        ForEach(Array(rows.enumerated()), id: \.element.id) { rowIdx, row in
                            plyGroup(row)
                            if rowIdx < rows.count - 1 {
                                Text("|")
                                    .font(.system(.caption2, design: .rounded))
                                    .foregroundStyle(.tertiary)
                                    .padding(.horizontal, 6)
                            }
                        }
                        Color.clear.frame(width: 1).id("moveHistoryEnd")
                    }
                    .padding(.vertical, 4)
                }
                .onChange(of: moves.count) { _, _ in
                    scrollToEnd(proxy: proxy)
                }
                .onAppear {
                    scrollToEnd(proxy: proxy)
                }
            }
        }
    }

    private func scrollToEnd(proxy: ScrollViewProxy) {
        DispatchQueue.main.async {
            withAnimation(.easeOut(duration: 0.25)) {
                proxy.scrollTo("moveHistoryEnd", anchor: .trailing)
            }
        }
    }

    @ViewBuilder
    private func plyGroup(_ row: MoveHistoryPlyRow) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 6) {
            Text("\(row.moveNumber).")
                .font(.system(.caption, design: .rounded))
                .foregroundStyle(.secondary)
                .monospacedDigit()

            plyButton(globalIndex: row.whiteIndex, text: row.whiteText)

            if let bi = row.blackIndex, let bt = row.blackText {
                plyButton(globalIndex: bi, text: bt)
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 5)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(rowContainsLastMove(row) ? Theme.accent.opacity(0.12) : Color.clear)
        )
        .accessibilityElement(children: .combine)
        .accessibilityLabel(accessibilityLabel(for: row))
    }

    private func rowContainsLastMove(_ row: MoveHistoryPlyRow) -> Bool {
        guard let li = lastGlobalIndex else { return false }
        return row.whiteIndex == li || row.blackIndex == li
    }

    private func accessibilityLabel(for row: MoveHistoryPlyRow) -> String {
        if let b = row.blackText {
            return "Tah \(row.moveNumber), bílý \(row.whiteText), černý \(b)"
        }
        return "Tah \(row.moveNumber), bílý \(row.whiteText)"
    }

    @ViewBuilder
    private func plyButton(globalIndex: Int, text: String) -> some View {
        let on = isEmphasized(globalIndex: globalIndex)
        if interactive {
            Button {
                selectedMoveIndex = globalIndex
            } label: {
                plyText(text, emphasized: on)
            }
            .buttonStyle(.plain)
        } else {
            plyText(text, emphasized: on)
        }
    }

    private func plyText(_ text: String, emphasized: Bool) -> some View {
        Text(text)
            .font(.system(.caption, design: .rounded))
            .fontWeight(emphasized ? .semibold : .regular)
            .foregroundStyle(emphasized ? Theme.accent : Color.primary)
    }
}

