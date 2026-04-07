//
//  AnalysisView.swift
//

import Charts
import CZECHMATEShared
import SwiftUI

struct AnalysisView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network
    @Environment(AppTabRouter.self) private var tabRouter
    @State private var customFEN: String = ""
    @State private var freeAnalysisSummary: String?
    @State private var secondaryLineText: String?
    @State private var isAnalyzingFree = false
    @State private var isSecondaryBusy = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    if !network.isInternetLikelyForStockfish {
                        ThemedBanner(style: .warning) {
                            Label("Stockfish vyžaduje připojení k internetu.", systemImage: "wifi.slash")
                                .font(Theme.Typography.subsection())
                                .foregroundStyle(Theme.Semantic.warningForeground)
                        }
                    }

                    if let s = store.snapshot {
                        gameOverviewCard(s)

                        if let ev = store.lastMoveEvaluation {
                            VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                                AppSectionHeader(title: "Poslední zhodnocení tahu")
                                Text(ev.message)
                                    .font(Theme.Typography.body())
                                    .foregroundStyle(gradeColor(ev.grade))
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .themeCard()
                        }

                        multilineSection(fen: FENBuilder.boardAndStatusToFEN(board: s.board, status: s.status, history: s.history))

                        moveList(s)
                    } else {
                        AppEmptyState(
                            systemImage: "square.grid.3x3",
                            title: "Žádná pozice z desky",
                            message: "Na kartě Hra spusť připojení k šachovnici. Vlastní pozici můžeš nastavit v sekci níže.",
                            primaryButtonTitle: "Otevřít Hru",
                            primaryAction: { tabRouter.selectedTab = .game }
                        )
                        .themeCard()
                    }

                    freePositionSection
                }
                .padding(Theme.Spacing.l)
            }
            .navigationTitle("Analýza")
            .tint(Theme.accent)
        }
    }

    /// Jeden panel: graf evalů + minideska a lidsky čitelný stav (bez surového FEN).
    @ViewBuilder
    private func gameOverviewCard(_ s: GameSnapshot) -> some View {
        let pts = EvalHistoryRecorder.load()
        let fen = FENBuilder.boardAndStatusToFEN(board: s.board, status: s.status, history: s.history)
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(
                title: "Přehled partie",
                subtitle: "Eval z nápověd a aktuální pozice",
                systemImage: "chart.xyaxis.line"
            )

            if !pts.isEmpty {
                Chart(pts) { p in
                    LineMark(
                        x: .value("Čas", p.recordedAt),
                        y: .value("Eval", p.evalPawns)
                    )
                    .foregroundStyle(Theme.accent)
                }
                .frame(height: 140)
                Button("Vymazat historii evalů") {
                    EvalHistoryRecorder.clear()
                }
                .font(Theme.Typography.caption())
                .foregroundStyle(Theme.Semantic.errorForeground)
                Divider()
                    .padding(.vertical, 4)
            }

            if let fen, let eco = OpeningECO.title(forFEN: fen) {
                Text(eco)
                    .font(Theme.Typography.subsection())
                    .foregroundStyle(Theme.accent)
            }

            HStack(alignment: .top, spacing: Theme.Spacing.l) {
                MiniBoardView(board: s.board, cellSide: 24)
                VStack(alignment: .leading, spacing: 6) {
                    Text(localizedGameState(s.status.gameState))
                        .font(Theme.Typography.subsection())
                    Text("Na tahu: \(localizedPlayerName(s.status.currentPlayer)) · \(s.status.moveCount) tahů")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func localizedGameState(_ raw: String) -> String {
        let g = raw.lowercased()
        switch g {
        case "playing", "active": return "Probíhá hra"
        case "paused": return "Pauza"
        case "promotion": return "Promoce"
        case "finished", "ended": return "Konec partie"
        default: return "Stav: \(raw)"
        }
    }

    private func localizedPlayerName(_ raw: String) -> String {
        let l = raw.lowercased()
        if l == "white" { return "bílý" }
        if l == "black" { return "černý" }
        return raw
    }

    @ViewBuilder
    private func multilineSection(fen: String?) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(
                title: "Druhá varianta",
                subtitle: "Heuristika — nižší hloubka",
                systemImage: "arrow.left.arrow.right"
            )
            Text("Porovnání hlavní hloubky s nižší — může ukázat alternativní tah.")
                .font(Theme.Typography.caption())
                .foregroundStyle(.secondary)
            if let t = secondaryLineText {
                Text(t)
                    .font(Theme.Typography.body())
            }
            Button {
                Task { await loadSecondaryLine(fen: fen) }
            } label: {
                if isSecondaryBusy {
                    ProgressView()
                } else {
                    Text("Načíst druhou variantu")
                }
            }
            .buttonStyle(.themeSecondary)
            .disabled(isSecondaryBusy || fen == nil || !network.isInternetLikelyForStockfish)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func loadSecondaryLine(fen: String?) async {
        guard let fen, network.isInternetLikelyForStockfish else { return }
        isSecondaryBusy = true
        secondaryLineText = nil
        defer { isSecondaryBusy = false }
        let client = StockfishAPIClient()
        do {
            let pair = try await client.fetchPrimaryAndSecondary(fen: fen, depth: store.hintDepth)
            if let s = pair.secondary {
                secondaryLineText =
                    "1) \(pair.primary.from)→\(pair.primary.to) · 2) \(s.from)→\(s.to)"
                    + (pair.primary.evalPawns.map { String(format: " · eval %.2f", $0) } ?? "")
            } else {
                secondaryLineText = "Obě hloubiny dávají stejný tah \(pair.primary.from)→\(pair.primary.to)."
            }
        } catch {
            secondaryLineText = error.localizedDescription
        }
    }

    private var freePositionSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(title: "Vlastní pozice", subtitle: "Analýza bez fyzické desky", systemImage: "square.and.pencil")
            TextField("Vlož notaci pozice (FEN)", text: $customFEN, axis: .vertical)
                .font(Theme.Typography.body())
                .lineLimit(3 ... 6)
                .padding(10)
                .background(
                    RoundedRectangle(cornerRadius: 10, style: .continuous)
                        #if os(iOS)
                        .fill(Color(uiColor: .secondarySystemGroupedBackground))
                        #else
                        .fill(Color.secondary.opacity(0.12))
                        #endif
                )
            HStack {
                Button("Analyzovat pozici") {
                    Task { await analyzeCustomFEN() }
                }
                .buttonStyle(.themePrimary)
                .disabled(isAnalyzingFree || customFEN.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || !network.isInternetLikelyForStockfish)
                if isAnalyzingFree {
                    ProgressView()
                }
            }
            if let freeAnalysisSummary {
                Text(freeAnalysisSummary)
                    .font(Theme.Typography.body())
            }
            Button("Náhled pozice v Hře") {
                let t = customFEN.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !t.isEmpty else { return }
                store.loadPuzzlePosition(fromFEN: t)
            }
            .buttonStyle(.themeSecondary)
            .disabled(customFEN.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func analyzeCustomFEN() async {
        let t = customFEN.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return }
        isAnalyzingFree = true
        freeAnalysisSummary = nil
        defer { isAnalyzingFree = false }
        let client = StockfishAPIClient()
        do {
            let best = try await client.fetchBestMove(fen: t, depth: min(14, store.hintDepth))
            freeAnalysisSummary =
                "Nejlepší tah: \(best.from) → \(best.to)"
                + (best.evalPawns.map { String(format: " · eval %.2f", $0) } ?? "")
            if let eco = OpeningECO.title(forFEN: t) {
                freeAnalysisSummary = (freeAnalysisSummary ?? "") + "\n\(eco)"
            }
        } catch {
            freeAnalysisSummary = error.localizedDescription
        }
    }

    private func moveList(_ s: GameSnapshot) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(title: "Historie tahů", systemImage: "list.bullet")
            let moves = s.history.moves
            if moves.isEmpty {
                Text("Zatím žádné tahy.")
                    .font(Theme.Typography.body())
                    .foregroundStyle(.secondary)
            } else {
                MoveHistoryView(moves: moves, interactive: false)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func gradeColor(_ grade: MoveGrade) -> Color {
        switch grade {
        case .best, .good: return Theme.Semantic.success
        case .inaccuracy: return .yellow
        case .mistake: return Theme.Semantic.warningForeground
        case .blunder: return Theme.Semantic.errorForeground
        case .error: return .secondary
        }
    }
}
