//
//  AnalysisView.swift
//

import Charts
import CZECHMATEShared
import SwiftUI

// MARK: - Graf evaluace (po tazích ze Stockfish)

private struct AnalysisEvalChartPoint: Identifiable {
    let id: Int
    let moveIndex: Int
    let evalWhitePawns: Double
}

// MARK: - Tabulka historie pro Analýzu (čitelnější než horizontální strip)

private struct AnalysisMoveHistoryTable: View {
    let moves: [GameHistoryMove]
    /// `globalIndex` 0…n−1 v `moves`.
    let gradeForGlobalMoveIndex: (Int) -> MoveGrade?
    /// Klepnutí na tabulku — otevře chronologický průběh partie.
    var onTapShowSequentialFlow: (() -> Void)? = nil

    private var rows: [MoveHistoryPlyRow] { MoveHistoryPlyRow.build(from: moves) }

    var body: some View {
        if moves.isEmpty {
            Text("Zatím žádné tahy.")
                .font(Theme.Typography.body())
                .foregroundStyle(.secondary)
        } else {
            VStack(alignment: .leading, spacing: 8) {
                if onTapShowSequentialFlow != nil {
                    HStack(spacing: 6) {
                        Image(systemName: "hand.tap.fill")
                            .font(.caption)
                            .foregroundStyle(Theme.accent)
                        Text("Klepnutím zobrazíš celou historii tahů v pořadí (jak šly po sobě).")
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .padding(.bottom, 4)
                }

                VStack(alignment: .leading, spacing: 10) {
                    ForEach(rows) { row in
                        HStack(alignment: .firstTextBaseline, spacing: 12) {
                            Text("\(row.moveNumber).")
                                .font(Theme.Typography.caption())
                                .foregroundStyle(.secondary)
                                .monospacedDigit()
                                .frame(width: 28, alignment: .trailing)

                            Text(row.whiteText)
                                .font(Theme.Typography.body())
                                .foregroundStyle(gradeForeground(row.whiteIndex))

                            if let bi = row.blackIndex, let bt = row.blackText {
                                Spacer(minLength: 8)
                                Text(bt)
                                    .font(Theme.Typography.body())
                                    .foregroundStyle(gradeForeground(bi))
                            }
                        }
                        .padding(.vertical, 4)
                        .padding(.horizontal, 8)
                        .background(
                            RoundedRectangle(cornerRadius: 8, style: .continuous)
                                .fill(Color.primary.opacity(0.04))
                        )
                    }
                }
                .contentShape(Rectangle())
                .onTapGesture {
                    onTapShowSequentialFlow?()
                }
                .accessibilityAddTraits(onTapShowSequentialFlow != nil ? .isButton : [])
                .accessibilityHint(onTapShowSequentialFlow != nil ? "Otevře průběh partie po jednotlivých tazích" : "")
            }
        }
    }

    private func gradeForeground(_ globalIndex: Int) -> Color {
        AnalysisMoveGradePalette.color(for: gradeForGlobalMoveIndex(globalIndex))
    }
}

// MARK: - Chronologický průběh partie (sheet)

private enum AnalysisMoveGradePalette {
    static func color(for grade: MoveGrade?) -> Color {
        switch grade {
        case .none, .some(.error):
            return Color.primary
        case .some(.best), .some(.good):
            return Theme.Semantic.success
        case .some(.inaccuracy):
            return Color.orange
        case .some(.mistake):
            return Theme.Semantic.warningForeground
        case .some(.blunder):
            return Theme.Semantic.errorForeground
        }
    }
}

/// Jednotlivé tahy v pořadí: 1 = první tah bílého, 2 = první tah černého, …
private struct AnalysisSequentialGameFlowSheet: View {
    let moves: [GameHistoryMove]
    let gradeForGlobalMoveIndex: (Int) -> MoveGrade?
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            Group {
                if moves.isEmpty {
                    ContentUnavailableView(
                        "Žádné tahy",
                        systemImage: "list.bullet",
                        description: Text("Historie je prázdná.")
                    )
                } else {
                    List {
                        Section {
                            Text(
                                "Každý řádek je jeden tah v pořadí, v jakém proběhl na desce — nejdřív bílý, pak černý, střídavě."
                            )
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                            .listRowBackground(Color.clear)
                        }

                        Section("Průběh") {
                            ForEach(moves.indices, id: \.self) { i in
                                HStack(alignment: .firstTextBaseline, spacing: 10) {
                                    Text("\(i + 1).")
                                        .font(Theme.Typography.caption().weight(.semibold))
                                        .foregroundStyle(.tertiary)
                                        .monospacedDigit()
                                        .frame(width: 28, alignment: .trailing)

                                    Text(i % 2 == 0 ? "Bílý" : "Černý")
                                        .font(Theme.Typography.caption())
                                        .foregroundStyle(.secondary)
                                        .frame(width: 52, alignment: .leading)

                                    Text(MoveHistoryNotation.displayString(for: moves[i]))
                                        .font(Theme.Typography.body())
                                        .foregroundStyle(AnalysisMoveGradePalette.color(for: gradeForGlobalMoveIndex(i)))

                                    Spacer(minLength: 0)
                                }
                                .accessibilityElement(children: .combine)
                                .accessibilityLabel(
                                    "Tah \(i + 1), \(i % 2 == 0 ? "bílý" : "černý"), \(MoveHistoryNotation.displayString(for: moves[i]))"
                                )
                            }
                        }
                    }
                    .listStyle(.insetGrouped)
                }
            }
            .navigationTitle("Průběh partie")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") { dismiss() }
                }
            }
        }
    }
}

// MARK: - Hlavní obrazovka

struct AnalysisView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network
    @Environment(AppTabRouter.self) private var tabRouter
    @State private var customFEN: String = ""
    @State private var freeAnalysisSummary: String?
    @State private var secondaryLineText: String?
    @State private var isAnalyzingFree = false
    @State private var isSecondaryBusy = false
    @State private var showSequentialGameFlow = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    if !network.isInternetLikelyForStockfish {
                        ThemedBanner(style: .warning) {
                            Label(
                                "Stockfish (chess-api.com) vyžaduje internet — Wi‑Fi nebo mobilní data. Desku můžeš mít jen přes Bluetooth.",
                                systemImage: "antenna.radiowaves.left.and.right.slash"
                            )
                            .font(Theme.Typography.subsection())
                            .foregroundStyle(Theme.Semantic.warningForeground)
                        }
                    }

                    if let s = store.snapshot {
                        if !store.moveEvaluationEnabled {
                            ThemedBanner(style: .neutral) {
                                VStack(alignment: .leading, spacing: 6) {
                                    Text("Graf a barevné hodnocení tahů jsou vypnuté.")
                                        .font(Theme.Typography.subsection())
                                    Text("Zapni „Automatické zhodnocení tahů (Stockfish)“ v Nastavení → Diagnostika → Pokročilé a vývojář (sekce Stockfish) — po každém tahu se doplní eval a barva v historii. Totéž jde sladit v NVS desky ve stejné diagnostické sekci.")
                                        .font(Theme.Typography.caption())
                                        .foregroundStyle(.secondary)
                                }
                            }
                        }

                        gameOverviewCard(s)

                        if !store.moveEvalHistory.isEmpty {
                            moveQualitySummaryCard(
                                MoveQualityAggregator.boardSummary(from: store.moveEvalHistory)
                            )
                        }

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
            .sheet(isPresented: $showSequentialGameFlow) {
                AnalysisSequentialGameFlowSheet(
                    moves: store.snapshot?.history.moves ?? [],
                    gradeForGlobalMoveIndex: { g in
                        store.moveEvalHistory.first { $0.moveIndex1Based == g + 1 }?.grade
                    }
                )
            }
        }
    }

    private func evalChartPoints() -> [AnalysisEvalChartPoint] {
        store.moveEvalHistory
            .compactMap { e -> AnalysisEvalChartPoint? in
                guard let ev = e.evalWhitePawns else { return nil }
                return AnalysisEvalChartPoint(id: e.moveIndex1Based, moveIndex: e.moveIndex1Based, evalWhitePawns: ev)
            }
            .sorted { $0.moveIndex < $1.moveIndex }
    }

    /// Graf evaluace pozice po každém tahu (`moveEvalHistory`) — aktualizuje se přes `@Observable` store.
    @ViewBuilder
    private func gameOverviewCard(_ s: GameSnapshot) -> some View {
        let points = evalChartPoints()
        let fen = FENBuilder.boardAndStatusToFEN(board: s.board, status: s.status, history: s.history)

        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(
                title: "Přehled partie",
                subtitle: "Eval pozice po tahu (perspektiva bílého: + výhoda bílého)",
                systemImage: "chart.xyaxis.line"
            )

            if points.isEmpty {
                Text(
                    store.moveEvaluationEnabled
                        ? "Graf se naplní po pár tazích, až Stockfish vyhodnotí pozici. Hraj na kartě Hra s připojenou deskou a internetem."
                        : "Po zapnutí automatického vyhodnocení tahů uvidíš křivku zde."
                )
                .font(Theme.Typography.caption())
                .foregroundStyle(.secondary)
            } else {
                let ys = points.map(\.evalWhitePawns)
                let lo = min(ys.min() ?? -1, -0.25) - 0.4
                let hi = max(ys.max() ?? 1, 0.25) + 0.4

                Chart {
                    RuleMark(y: .value("Rovnováha", 0))
                        .foregroundStyle(.secondary.opacity(0.45))
                        .lineStyle(StrokeStyle(lineWidth: 1, dash: [5, 4]))

                    ForEach(points) { p in
                        LineMark(
                            x: .value("Tah", p.moveIndex),
                            y: .value("Eval", p.evalWhitePawns)
                        )
                        .interpolationMethod(.catmullRom)
                        .foregroundStyle(Theme.accent)
                    }

                    ForEach(points) { p in
                        PointMark(
                            x: .value("Tah", p.moveIndex),
                            y: .value("Eval", p.evalWhitePawns)
                        )
                        .foregroundStyle(Theme.accent)
                        .symbolSize(36)
                    }
                }
                .chartXAxisLabel("Pořadí tahu v partii", alignment: .center)
                .chartYAxisLabel("Pěšci (bílý +)", alignment: .leading)
                .chartYScale(domain: lo ... hi)
                .chartXScale(domain: 1 ... max(8, points.map(\.moveIndex).max() ?? 1))
                .frame(height: 180)
                .padding(.vertical, 4)

                Button("Vymazat graf a uložené evaluace tahů") {
                    store.clearAnalysisEvalHistory()
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

    private func moveQualitySummaryCard(_ summary: MoveQualityBoardSummary) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(
                title: "Kvalita tahů",
                subtitle: "Průměr podle stupně Stockfish (0–100) a průměrná ztráta v cp, kde je k dispozici",
                systemImage: "chart.line.uptrend.xyaxis"
            )

            moveQualityWindowBlock(
                title: "Poslední 3 tahy strany",
                white: summary.white.last3,
                black: summary.black.last3
            )
            moveQualityWindowBlock(
                title: "Posledních 10 tahů strany",
                white: summary.white.last10,
                black: summary.black.last10
            )
            moveQualityWindowBlock(
                title: "Celá partie",
                white: summary.white.fullGame,
                black: summary.black.fullGame
            )

            Text(
                "Každé okno počítá zvlášť tahy bílého a černého (např. „3 tahy“ = poslední tři bílé tahy v partii). "
                    + "Bez zapnutého automatického vyhodnocování se historie nedoplňuje."
            )
            .font(Theme.Typography.caption2())
            .foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func moveQualityWindowBlock(title: String, white: MoveQualityWindowStats, black: MoveQualityWindowStats) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(Theme.Typography.caption().weight(.semibold))
                .foregroundStyle(.secondary)

            HStack(alignment: .top, spacing: Theme.Spacing.m) {
                moveQualityPlayerColumn(title: "Bílý", stats: white)
                moveQualityPlayerColumn(title: "Černý", stats: black)
            }
        }
    }

    private func moveQualityPlayerColumn(title: String, stats: MoveQualityWindowStats) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .font(Theme.Typography.caption2().weight(.medium))
                .foregroundStyle(.tertiary)
            if stats.count == 0 {
                Text("—")
                    .font(Theme.Typography.body().weight(.semibold))
                    .foregroundStyle(.secondary)
            } else if let q = stats.averageQualityPercent {
                Text(String(format: "%.0f %%", q))
                    .font(Theme.Typography.body().weight(.semibold))
                    .monospacedDigit()
                if let cp = stats.averageCpLoss {
                    Text(String(format: "Ø ztráta %.0f cp", cp))
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }
                Text(stats.count == 1 ? "1 hodnocený tah" : "\(stats.count) hodnocených tahů")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.tertiary)
            } else {
                Text("—")
                    .font(Theme.Typography.body())
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
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
                + (best.evalPawns.map { String(format: " · eval %.2f (bílý +)", $0) } ?? "")
            if let eco = OpeningECO.title(forFEN: t) {
                freeAnalysisSummary = (freeAnalysisSummary ?? "") + "\n\(eco)"
            }
        } catch {
            freeAnalysisSummary = error.localizedDescription
        }
    }

    @ViewBuilder
    private func moveGradeLegendBanner() -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Význam barev tahů")
                .font(Theme.Typography.caption().weight(.semibold))
            legendRow(color: Theme.Semantic.success, text: "Nejlepší / dobrý tah — blízko nebo rovno nejlepšímu tahu Stockfish.")
            legendRow(color: .orange, text: "Nepřesnost — mírné zhoršení pozice.")
            legendRow(color: Theme.Semantic.warningForeground, text: "Chyba — výraznější ztráta.")
            legendRow(color: Theme.Semantic.errorForeground, text: "Hrubka — velké zhoršení.")
            legendRow(color: Color.primary, text: "Bez barvy — eval ještě není, nebo selhalo spojení s API.")
        }
        .font(Theme.Typography.caption())
        .foregroundStyle(.secondary)
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(Color.primary.opacity(0.05))
        )
    }

    private func legendRow(color: Color, text: String) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Circle()
                .fill(color)
                .frame(width: 10, height: 10)
                .padding(.top, 3)
            Text(text)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private func moveList(_ s: GameSnapshot) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(title: "Historie tahů", systemImage: "list.bullet")
            moveGradeLegendBanner()
            AnalysisMoveHistoryTable(
                moves: s.history.moves,
                gradeForGlobalMoveIndex: { g in
                    store.moveEvalHistory.first { $0.moveIndex1Based == g + 1 }?.grade
                },
                onTapShowSequentialFlow: {
                    showSequentialGameFlow = true
                }
            )
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
