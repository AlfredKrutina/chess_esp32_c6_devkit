//
//  WatchRootView.swift
//  Přehled | Deska | Tahy — swipe, velké tap cíle, deska na celý displej.
//

import CZECHMATEShared
import SwiftUI

struct WatchRootView: View {
    @State private var model = WatchSessionModel()
    @State private var boardFlipped = false
    @State private var fullScreenBoard = false
    @State private var pageIndex = 0

    var body: some View {
        TabView(selection: $pageIndex) {
            glancePage
                .tag(0)
            boardPage
                .tag(1)
            movesPage
                .tag(2)
        }
        .tabViewStyle(.page(indexDisplayMode: .automatic))
        .onAppear { model.activate() }
        .fullScreenCover(isPresented: $fullScreenBoard) {
            fullScreenBoardView
        }
    }

    private var glancePage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                if model.connectionLost {
                    VStack(alignment: .leading, spacing: 4) {
                        Label(model.companionMessage.isEmpty ? CompanionStrings.connectionLostTitle : model.companionMessage, systemImage: "iphone.slash")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.orange)
                        Text(CompanionStrings.connectionLostDetail)
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 10).fill(Color.orange.opacity(0.18)))
                }
                Text("CZECHMATE")
                    .font(.headline)
                if !model.advantageSummary.isEmpty {
                    Text(model.advantageSummary)
                        .font(.system(.caption, design: .rounded).weight(.semibold))
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                }
                Text("Na tahu")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                Text(displayPlayerName(model.currentPlayer))
                    .font(.title2.weight(.semibold))
                    .minimumScaleFactor(0.8)
                watchTimerRow
                Text("Tahy: \(model.moveCount)")
                    .font(.caption)
                if model.inCheck {
                    Label("Šach", systemImage: "exclamationmark.shield.fill")
                        .font(.caption)
                        .foregroundStyle(.red)
                }
                if model.gameEnded {
                    Text("Konec hry")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                boardPreviewTap
            }
            .padding(.horizontal, 6)
        }
    }

    private var watchTimerRow: some View {
        TimelineView(.periodic(from: .now, by: 0.5)) { context in
            let (w, b) = clocks(at: context.date)
            HStack(spacing: 6) {
                timeChip(label: "Bílý", sec: w, highlight: isWhiteTurn(model.currentPlayer))
                timeChip(label: "Černý", sec: b, highlight: !isWhiteTurn(model.currentPlayer))
            }
        }
    }

    private func isWhiteTurn(_ raw: String) -> Bool {
        let l = raw.lowercased()
        return l == "white" || l == "bílý"
    }

    private func displayPlayerName(_ raw: String) -> String {
        let l = raw.lowercased()
        if l == "white" || l == "bílý" { return "Bílý" }
        if l == "black" || l == "černý" { return "Černý" }
        return raw
    }

    /// Odpočet u hráče na tahu, pokud běží hodiny a máme sync čas.
    private func clocks(at now: Date) -> (UInt32?, UInt32?) {
        guard model.isTimerRunning,
              let sync = model.clockSyncAt ?? model.lastUpdate
        else {
            return (model.whiteTime, model.blackTime)
        }
        let elapsed = UInt32(min(UInt64(Int.max), UInt64(max(0, now.timeIntervalSince(sync)))))
        let wTurn = isWhiteTurn(model.currentPlayer)
        if wTurn {
            let w = model.whiteTime.map { $0 > elapsed ? $0 - elapsed : 0 }
            return (w, model.blackTime)
        } else {
            let b = model.blackTime.map { $0 > elapsed ? $0 - elapsed : 0 }
            return (model.whiteTime, b)
        }
    }

    private func timeChip(label: String, sec: UInt32?, highlight: Bool) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
            Text(formatClock(sec))
                .font(.caption.monospacedDigit())
                .fontWeight(highlight ? .semibold : .regular)
                .foregroundStyle(highlight ? Color.primary : Color.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(6)
        .background(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(highlight ? Color.green.opacity(0.22) : Color.secondary.opacity(0.12))
        )
    }

    private var boardPreviewTap: some View {
        Button {
            fullScreenBoard = true
        } label: {
            Group {
                if let b = boardFromFen(model.fen) {
                    SharedChessBoardView(
                        board: b,
                        flipped: boardFlipped,
                        highlightFrom: model.lastFrom,
                        highlightTo: model.lastTo,
                        rankLabelWidth: 12,
                        cellSize: nil
                    )
                    .frame(height: 100)
                } else {
                    Text("Čekám na data z iPhonu…")
                        .font(.caption2)
                        .frame(height: 80)
                }
            }
            .frame(maxWidth: .infinity)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Otevřít šachovnici na celý displej")
    }

    private var boardPage: some View {
        ScrollView {
            VStack(spacing: 8) {
                Toggle("Pohled černého", isOn: $boardFlipped)
                    .font(.caption)
                if let b = boardFromFen(model.fen) {
                    SharedChessBoardView(
                        board: b,
                        flipped: boardFlipped,
                        highlightFrom: model.lastFrom,
                        highlightTo: model.lastTo,
                        rankLabelWidth: 16,
                        cellSize: nil
                    )
                    .frame(height: 140)
                }
                Button {
                    fullScreenBoard = true
                } label: {
                    Text("Celá obrazovka")
                        .frame(maxWidth: .infinity, minHeight: 40)
                }
                .buttonStyle(.borderedProminent)
            }
            .padding(.horizontal, 4)
        }
    }

    private var fullScreenBoardView: some View {
        NavigationStack {
            Group {
                if let b = boardFromFen(model.fen) {
                    SharedChessBoardView(
                        board: b,
                        flipped: boardFlipped,
                        highlightFrom: model.lastFrom,
                        highlightTo: model.lastTo,
                        rankLabelWidth: 18,
                        cellSize: nil
                    )
                } else {
                    ContentUnavailableView("Žádná deska", systemImage: "checkerboard.rectangle")
                }
            }
            .navigationTitle("Partie")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") {
                        fullScreenBoard = false
                    }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        boardFlipped.toggle()
                    } label: {
                        Image(systemName: "arrow.up.arrow.down.circle")
                    }
                    .accessibilityLabel("Otočit desku")
                }
            }
        }
    }

    private var movesPage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 6) {
                Text("Poslední tah")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                if let f = model.lastFrom, let t = model.lastTo {
                    Text("\(f) → \(t)")
                        .font(.title3.monospaced())
                } else {
                    Text("—")
                        .foregroundStyle(.secondary)
                }
                Text("Stav: \(model.gameState.isEmpty ? "—" : model.gameState)")
                    .font(.caption2)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding()
        }
    }

    private func formatClock(_ sec: UInt32?) -> String {
        guard let s = sec else { return "—" }
        let m = Int(s) / 60
        let r = Int(s) % 60
        return String(format: "%d:%02d", m, r)
    }

    private func boardFromFen(_ fen: String) -> [[String]]? {
        let t = fen.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return nil }
        return FENPlacementParser.boardFromFullFEN(t)
    }
}

#Preview {
    WatchRootView()
}
