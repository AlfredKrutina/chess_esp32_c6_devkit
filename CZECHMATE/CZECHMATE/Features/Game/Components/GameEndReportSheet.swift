//
//  GameEndReportSheet.swift
//  CZECHMATE — Souhrn skončené partie (výsledek, důvod, statistiky, tahy).
//

import Charts
import CZECHMATEShared
import SwiftUI

// MARK: - Model

enum GameEndWinnerSide: Equatable {
    case white
    case black
    case draw
}

struct GameEndReportModel: Equatable {
    let winner: GameEndWinnerSide
    let headline: String
    let scoreLine: String
    let whiteScoreSymbol: String
    let blackScoreSymbol: String
    let terminationLine: String
    let moveCount: Int
    let durationSummary: String?
    /// PGN (tagy + tahy) pro sdílení / soubory.
    let pgnExport: String
    let sharePlainText: String

    init(snapshot: GameSnapshot) {
        let status = snapshot.status
        winner = Self.resolveWinner(status: status)
        headline = Self.headline(for: winner)
        switch winner {
        case .white:
            scoreLine = "1 – 0"
            whiteScoreSymbol = "1"
            blackScoreSymbol = "0"
        case .black:
            scoreLine = "0 – 1"
            whiteScoreSymbol = "0"
            blackScoreSymbol = "1"
        case .draw:
            scoreLine = "½ – ½"
            whiteScoreSymbol = "½"
            blackScoreSymbol = "½"
        }
        terminationLine = Self.terminationDescription(status: status)
        moveCount = Int(snapshot.status.moveCount)
        durationSummary = Self.durationSummary(moves: snapshot.history.moves)
        pgnExport = GameHistoryPGN.build(moves: snapshot.history.moves, result: winner)
        sharePlainText = Self.buildShareText(
            snapshot: snapshot,
            headline: headline,
            scoreLine: scoreLine,
            terminationLine: terminationLine,
            moveCount: moveCount,
            durationSummary: durationSummary,
            pgn: pgnExport
        )
    }

    private static func resolveWinner(status: GameStatus) -> GameEndWinnerSide {
        if status.stalemate == true { return .draw }

        if let ge = status.gameEnd, ge.ended {
            if let w = ge.winner?.trimmingCharacters(in: .whitespacesAndNewlines), !w.isEmpty {
                if playerStringIsWhite(w) { return .white }
                if playerStringIsBlack(w) { return .black }
            }
        }

        if status.checkmate == true {
            if playerStringIsWhite(status.currentPlayer) { return .black }
            if playerStringIsBlack(status.currentPlayer) { return .white }
        }

        if let ge = status.gameEnd, ge.ended {
            if let l = ge.loser?.trimmingCharacters(in: .whitespacesAndNewlines), !l.isEmpty {
                if playerStringIsWhite(l) { return .black }
                if playerStringIsBlack(l) { return .white }
            }
        }

        return .draw
    }

    private static func playerStringIsWhite(_ s: String) -> Bool {
        let l = s.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        return l == "white" || l == "bílý" || l == "bily" || l == "w"
    }

    private static func playerStringIsBlack(_ s: String) -> Bool {
        let l = s.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        return l == "black" || l == "černý" || l == "cerny" || l == "b"
    }

    private static func headline(for side: GameEndWinnerSide) -> String {
        switch side {
        case .white: return "Bílý vyhrál"
        case .black: return "Černý vyhrál"
        case .draw: return "Remíza"
        }
    }

    private static func terminationDescription(status: GameStatus) -> String {
        if status.stalemate == true {
            return "Pat — hráč na tahu nemá legální tah"
        }
        if status.checkmate == true {
            return "Šach mat"
        }
        if let r = status.gameEnd?.reason?.trimmingCharacters(in: .whitespacesAndNewlines), !r.isEmpty {
            return mapReasonToCzech(r)
        }
        if status.isGameFinished {
            return "Partie ukončena"
        }
        return ""
    }

    private static func mapReasonToCzech(_ r: String) -> String {
        let lower = r.lowercased()
        if lower.contains("resign") { return "Vzdání partie" }
        if lower.contains("time") || lower.contains("clock") { return "Vypršení času" }
        if lower.contains("stalemate") { return "Pat" }
        if lower.contains("checkmate") || lower == "mate" || lower.hasSuffix(" mate") { return "Šach mat" }
        if lower.contains("agreement") || lower.contains("mutual") { return "Remíza dohodou" }
        if lower.contains("draw") && (lower.contains("claim") || lower.contains("claimed")) { return "Uplatněná remíza" }
        if lower.contains("insufficient") { return "Nedostatek materiálu" }
        if lower.contains("repetition") || lower.contains("threefold") { return "Trojí opakování pozice" }
        if lower.contains("fifty") { return "Pravidlo padesáti tahů" }
        return r
    }

    private static func durationSummary(moves: [GameHistoryMove]) -> String? {
        let stamps = moves.compactMap(\.timestamp)
        guard stamps.count >= 2,
              let first = stamps.first,
              let last = stamps.last,
              last >= first else { return nil }
        let seconds = last - first
        return Self.formatDuration(seconds)
    }

    private static func formatDuration(_ seconds: UInt64) -> String {
        if seconds < 60 {
            return "cca \(seconds) s"
        }
        let m = seconds / 60
        if m < 60 {
            return "cca \(m) min"
        }
        let h = m / 60
        let rem = m % 60
        if rem == 0 {
            return "cca \(h) h"
        }
        return "cca \(h) h \(rem) min"
    }

    private static func buildShareText(
        snapshot: GameSnapshot,
        headline: String,
        scoreLine: String,
        terminationLine: String,
        moveCount: Int,
        durationSummary: String?,
        pgn: String
    ) -> String {
        var lines: [String] = [
            "CZECHMATE — souhrn partie",
            headline,
            "Výsledek: \(scoreLine)",
            terminationLine,
            "Počet tahů: \(moveCount)",
        ]
        if let d = durationSummary {
            lines.append("Délka (odhad z desky): \(d)")
        }
        let timingExtra = GameEndReportTiming.shareTimingLines(from: snapshot.history.moves)
        if !timingExtra.isEmpty {
            lines.append("")
            lines.append(contentsOf: timingExtra)
        }
        lines.append("")
        lines.append("Notace:")
        let rows = MoveHistoryPlyRow.build(from: snapshot.history.moves)
        for row in rows {
            if let bt = row.blackText {
                lines.append("\(row.moveNumber). \(row.whiteText)  \(bt)")
            } else {
                lines.append("\(row.moveNumber). \(row.whiteText)")
            }
        }
        lines.append("")
        lines.append("PGN:")
        lines.append(pgn)
        return lines.joined(separator: "\n")
    }
}

// MARK: - Sheet

struct GameEndReportSheet: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.dismiss) private var dismiss
    @Environment(\.colorScheme) private var colorScheme

    /// Zmrazený snímek při otevření — při pollování / výpadku sítě nezmizí text ani grafy.
    @State private var frozenSnapshot: GameSnapshot?
    /// Krátké srovnání s minulou uloženou partií (UserDefaults).
    @State private var previousGameBanner: String?
    @State private var showImageShareSheet = false

    private var effectiveSnapshot: GameSnapshot? {
        frozenSnapshot ?? store.snapshot
    }

    private func model(for snapshot: GameSnapshot) -> GameEndReportModel? {
        guard snapshot.status.isGameFinished else { return nil }
        return GameEndReportModel(snapshot: snapshot)
    }

    private var plyRows: [MoveHistoryPlyRow] {
        guard let moves = effectiveSnapshot?.history.moves else { return [] }
        return MoveHistoryPlyRow.build(from: moves)
    }

    private var thinkTimingPoints: [EndgameThinkPlyPoint] {
        guard let moves = effectiveSnapshot?.history.moves else { return [] }
        return GameEndReportTiming.thinkPlyPoints(from: moves)
    }

    private var cumulativeTimingPoints: [EndgameCumulativePoint] {
        GameEndReportTiming.cumulativePoints(from: thinkTimingPoints)
    }

    private var barTimingPoints: [EndgameThinkPlyPoint] {
        thinkTimingPoints.filter { $0.secondsFromPrevious != nil }
    }

    @ViewBuilder
    private var clockEndSection: some View {
        if let snap = effectiveSnapshot,
           let c = snap.clock,
           c.isTimeControlEnabled {
            VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                Text(EndgameReportCopy.clockEndTitle)
                    .font(Theme.Typography.subsection())
                Text(EndgameReportCopy.clockEndSub)
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.secondary)
                Chart {
                    BarMark(
                        x: .value("Strana", EndgameReportCopy.white),
                        y: .value("min", Double(c.whiteTimeMs) / 60_000)
                    )
                    .foregroundStyle(Theme.accent.opacity(0.72))
                    BarMark(
                        x: .value("Strana", EndgameReportCopy.black),
                        y: .value("min", Double(c.blackTimeMs) / 60_000)
                    )
                    .foregroundStyle(Color.purple.opacity(0.52))
                }
                .chartYAxisLabel("min", position: .leading, alignment: .center)
                .frame(height: 200)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .themeCard()
        }
    }

    @ViewBuilder
    private var evalQualitySection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            Text(EndgameReportCopy.evalTitle)
                .font(Theme.Typography.subsection())
            if evalPlotPoints.isEmpty {
                Text(EndgameReportCopy.noEvalData)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            } else {
                Text(EndgameReportCopy.evalSub)
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.secondary)
                Chart(evalPlotPoints) { p in
                    LineMark(
                        x: .value("Půltah", p.ply),
                        y: .value("p", p.eval)
                    )
                    .interpolationMethod(.catmullRom)
                    .lineStyle(StrokeStyle(lineWidth: 2))
                    .foregroundStyle(Theme.accent.opacity(0.85))
                    PointMark(
                        x: .value("Půltah", p.ply),
                        y: .value("p", p.eval)
                    )
                    .symbolSize(endgameEvalSymbolSize(for: p.grade))
                    .foregroundStyle(endgameEvalGradeColor(p.grade))
                }
                .chartXAxisLabel(EndgameReportCopy.axisHalfMove, position: .bottom, alignment: .center)
                .chartYAxisLabel(EndgameReportCopy.axisPawnsEval, position: .leading, alignment: .center)
                .frame(height: 220)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private var evalPlotPoints: [EndgameEvalPlotPoint] {
        guard let n = effectiveSnapshot?.history.moves.count else { return [] }
        return store.moveEvalHistory
            .filter { $0.moveIndex1Based <= n && $0.evalWhitePawns != nil }
            .map { e in
                EndgameEvalPlotPoint(
                    id: e.moveIndex1Based,
                    ply: e.moveIndex1Based,
                    eval: e.evalWhitePawns!,
                    grade: e.grade
                )
            }
            .sorted { $0.ply < $1.ply }
    }

    var body: some View {
        NavigationStack {
            Group {
                if let snap = effectiveSnapshot, let m = model(for: snap) {
                    reportScrollContent(snapshot: snap, model: m)
                } else {
                    ContentUnavailableView(
                        EndgameReportCopy.notAvailableTitle,
                        systemImage: "doc.text.magnifyingglass",
                        description: Text(EndgameReportCopy.notAvailableDetail)
                    )
                }
            }
            .navigationTitle(EndgameReportCopy.navTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(EndgameReportCopy.done) { dismiss() }
                }
                if let snap = effectiveSnapshot, let m = model(for: snap) {
                    ToolbarItem(placement: .primaryAction) {
                        Menu {
                            ShareLink(
                                item: m.sharePlainText,
                                subject: Text(EndgameReportCopy.navTitle),
                                message: Text(m.sharePlainText)
                            ) {
                                Label(EndgameReportCopy.shareSummary, systemImage: "doc.plaintext")
                            }
                            ShareLink(
                                item: m.pgnExport,
                                subject: Text("game.pgn"),
                                message: Text(m.pgnExport)
                            ) {
                                Label(EndgameReportCopy.sharePgn, systemImage: "doc.richtext")
                            }
                            Button {
                                showImageShareSheet = true
                            } label: {
                                Label(EndgameReportCopy.imageShareTitle, systemImage: "photo.on.rectangle.angled")
                            }
                        } label: {
                            Label(EndgameReportCopy.share, systemImage: "square.and.arrow.up")
                        }
                    }
                }
            }
            .presentationDetents([.large, .medium])
            .presentationDragIndicator(.visible)
        }
        .onAppear {
            if frozenSnapshot == nil,
               let s = store.snapshot,
               s.status.isGameFinished {
                frozenSnapshot = s
                AppDebugLog.staging("EndgameReport: latched snapshot moveCount=\(s.status.moveCount)")
            }
            refreshPreviousGameBanner()
        }
        .onDisappear {
            if let s = frozenSnapshot,
               let fp = GameEndReportSessionKey.fingerprint(for: s),
               let m = model(for: s) {
                UserDefaults.standard.set(fp, forKey: EndgameReportStorageKeys.lastFingerprint)
                UserDefaults.standard.set(m.headline, forKey: EndgameReportStorageKeys.lastHeadline)
            }
            frozenSnapshot = nil
        }
        .sheet(isPresented: $showImageShareSheet) {
            if let snap = effectiveSnapshot, let m = model(for: snap) {
                GameEndReportImageShareSheet(
                    snapshot: snap,
                    model: m,
                    evalHistory: store.moveEvalHistory
                )
            } else {
                ContentUnavailableView(
                    EndgameReportCopy.notAvailableTitle,
                    systemImage: "photo",
                    description: Text(EndgameReportCopy.notAvailableDetail)
                )
            }
        }
    }

    private func refreshPreviousGameBanner() {
        guard let s = frozenSnapshot ?? store.snapshot,
              let fp = GameEndReportSessionKey.fingerprint(for: s) else {
            previousGameBanner = nil
            return
        }
        let def = UserDefaults.standard
        guard let lastFp = def.string(forKey: EndgameReportStorageKeys.lastFingerprint),
              lastFp != fp,
              let h = def.string(forKey: EndgameReportStorageKeys.lastHeadline) else {
            previousGameBanner = nil
            return
        }
        previousGameBanner = EndgameReportCopy.previousGame(h)
    }

    @ViewBuilder
    private func compactWatchSummaryCard(snapshot: GameSnapshot) -> some View {
        let f = WatchCompanionEndgameSummary.contextFields(snapshot: snapshot, boardClock: snapshot.clock)
        if let headline = f["watchEndgameHeadline"] as? String, !headline.isEmpty {
            VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                Text(EndgameReportCopy.compactWatchSummaryTitle)
                    .font(Theme.Typography.caption().weight(.semibold))
                    .foregroundStyle(.secondary)
                Text(headline)
                    .font(Theme.Typography.subsection())
                if let score = f["watchEndgameScoreLine"] as? String, !score.isEmpty {
                    Text(score)
                        .font(.title2.monospaced())
                        .fontWeight(.semibold)
                }
                if let reason = f["watchEndgameReason"] as? String, !reason.isEmpty {
                    Text(reason)
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                }
                if let stats = f["watchEndgameStats"] as? String, !stats.isEmpty {
                    Text(stats)
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.tertiary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .themeCard()
        }
    }

    private func reportScrollContent(snapshot: GameSnapshot, model: GameEndReportModel) -> some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                if let line = previousGameBanner {
                    ThemedBanner(style: .neutral, cornerRadius: 12) {
                        Text(line)
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                    }
                }
                heroBlock(model: model)
                compactWatchSummaryCard(snapshot: snapshot)
                playerResultsCard(model: model)
                statsRow(model: model)
                clockEndSection
                evalQualitySection
                timingChartsSection
                movesCard()
            }
            .padding(.horizontal, Theme.Spacing.l)
            .padding(.bottom, Theme.Spacing.xxl)
        }
    }

    @ViewBuilder
    private var timingChartsSection: some View {
        let hasBars = !barTimingPoints.isEmpty
        if hasBars || !cumulativeTimingPoints.isEmpty {
            VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                Text(EndgameReportCopy.timeAxis)
                    .font(Theme.Typography.subsection())
                if !cumulativeTimingPoints.isEmpty {
                    timingChartCard(
                        title: EndgameReportCopy.playedTime,
                        subtitle: EndgameReportCopy.playedTimeSub
                    ) {
                        Chart(cumulativeTimingPoints) { p in
                            AreaMark(
                                x: .value("Půltah", p.plyIndex),
                                y: .value("Min", p.totalSeconds / 60)
                            )
                            .foregroundStyle(Theme.accent.opacity(0.18))
                            LineMark(
                                x: .value("Půltah", p.plyIndex),
                                y: .value("Min", p.totalSeconds / 60)
                            )
                            .interpolationMethod(.catmullRom)
                            .lineStyle(StrokeStyle(lineWidth: 2.5))
                            .foregroundStyle(Theme.accent)
                        }
                        .chartXAxisLabel("půltah", position: .bottom, alignment: .center)
                        .chartYAxisLabel("min", position: .leading, alignment: .center)
                        .frame(height: 200)
                    }
                }
                if hasBars {
                    timingChartCard(
                        title: EndgameReportCopy.timePerMove,
                        subtitle: EndgameReportCopy.timePerMoveSub
                    ) {
                        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                            ScrollView(.horizontal, showsIndicators: true) {
                                Chart {
                                    ForEach(barTimingPoints) { p in
                                        BarMark(
                                            x: .value("Půltah", p.plyIndex),
                                            y: .value("s", p.secondsFromPrevious ?? 0)
                                        )
                                        .foregroundStyle(p.isWhite ? Theme.accent.opacity(0.75) : Color.purple.opacity(0.55))
                                    }
                                }
                                .chartXAxisLabel("půltah", position: .bottom, alignment: .center)
                                .chartYAxisLabel("s", position: .leading, alignment: .center)
                                .frame(width: max(CGFloat(barTimingPoints.count) * 12 + 40, 320), height: 200)
                            }
                            HStack(spacing: Theme.Spacing.l) {
                                chartLegendDot(color: Theme.accent.opacity(0.75), text: EndgameReportCopy.white)
                                chartLegendDot(color: Color.purple.opacity(0.55), text: EndgameReportCopy.black)
                            }
                            .font(Theme.Typography.caption2())
                            .foregroundStyle(.secondary)
                        }
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .themeCard()
        } else {
            VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                Text(EndgameReportCopy.timeAxis)
                    .font(Theme.Typography.subsection())
                Text(EndgameReportCopy.noMoveTimestamps)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .themeCard()
        }
    }

    private func endgameEvalGradeColor(_ g: MoveGrade) -> Color {
        switch g {
        case .best: Theme.accent
        case .good: Color.green.opacity(0.85)
        case .inaccuracy: Color.yellow.opacity(0.95)
        case .mistake: Color.orange
        case .blunder: Color.red
        case .error: Color.gray
        }
    }

    private func endgameEvalSymbolSize(for g: MoveGrade) -> CGFloat {
        switch g {
        case .blunder: return 100
        case .mistake: return 72
        case .inaccuracy: return 56
        default: return 44
        }
    }

    private func chartLegendDot(color: Color, text: String) -> some View {
        HStack(spacing: 6) {
            Circle()
                .fill(color)
                .frame(width: 8, height: 8)
            Text(text)
        }
    }

    private func timingChartCard<Content: View>(
        title: String,
        subtitle: String,
        @ViewBuilder chart: () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text(title)
                .font(Theme.Typography.cardTitle())
            Text(subtitle)
                .font(Theme.Typography.caption2())
                .foregroundStyle(.secondary)
            chart()
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private func heroBlock(model: GameEndReportModel) -> some View {
        VStack(spacing: Theme.Spacing.m) {
            Text(model.headline)
                .font(Theme.Typography.sectionTitle())
                .multilineTextAlignment(.center)
                .foregroundStyle(.primary)

            Text(model.scoreLine)
                .font(.system(size: 44, weight: .bold, design: .rounded))
                .monospacedDigit()
                .foregroundStyle(.primary)

            Text(model.terminationLine)
                .font(Theme.Typography.body())
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, Theme.Spacing.xl)
        .padding(.horizontal, Theme.Spacing.m)
        .background {
            RoundedRectangle(cornerRadius: 20, style: .continuous)
                .fill(
                    LinearGradient(
                        colors: [
                            Theme.accent.opacity(0.28),
                            Theme.accent.opacity(0.06),
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
                .overlay {
                    RoundedRectangle(cornerRadius: 20, style: .continuous)
                        .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
                }
        }
    }

    private func playerResultsCard(model: GameEndReportModel) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            resultSideRow(
                label: EndgameReportCopy.white,
                dotColor: Color.white,
                dotStroke: Color.black.opacity(0.2),
                score: model.whiteScoreSymbol,
                emphasized: model.winner == .white
            )
            Divider()
            resultSideRow(
                label: EndgameReportCopy.black,
                dotColor: Color(red: 0.12, green: 0.12, blue: 0.14),
                dotStroke: Color.white.opacity(0.15),
                score: model.blackScoreSymbol,
                emphasized: model.winner == .black
            )
        }
        .themeCard()
    }

    private func resultSideRow(
        label: String,
        dotColor: Color,
        dotStroke: Color,
        score: String,
        emphasized: Bool
    ) -> some View {
        HStack(spacing: Theme.Spacing.m) {
            Circle()
                .fill(dotColor)
                .frame(width: 12, height: 12)
                .overlay {
                    Circle().stroke(dotStroke, lineWidth: 0.5)
                }
            Text(label)
                .font(Theme.Typography.cardTitle())
            Spacer()
            Text(score)
                .font(.system(.title2, design: .rounded).weight(emphasized ? .bold : .medium))
                .foregroundStyle(emphasized ? Theme.accent : .secondary)
        }
    }

    private func statsRow(model: GameEndReportModel) -> some View {
        HStack(spacing: Theme.Spacing.m) {
            statChip(title: EndgameReportCopy.statMoves, value: "\(model.moveCount)", icon: "list.number")
            if let d = model.durationSummary {
                statChip(title: EndgameReportCopy.statDuration, value: d, icon: "clock")
            }
        }
    }

    private func statChip(title: String, value: String, icon: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Label(title, systemImage: icon)
                .font(Theme.Typography.caption2())
                .foregroundStyle(.secondary)
            Text(value)
                .font(Theme.Typography.subsection())
                .foregroundStyle(.primary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(Theme.Spacing.m)
        .themeCard(cornerRadius: 12)
    }

    private func movesCard() -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            Text(EndgameReportCopy.progressSection)
                .font(Theme.Typography.subsection())
            if plyRows.isEmpty {
                Text(EndgameReportCopy.noMovesInHistory)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            } else {
                LazyVStack(alignment: .leading, spacing: 8) {
                    ForEach(plyRows) { row in
                        HStack(alignment: .firstTextBaseline, spacing: Theme.Spacing.m) {
                            Text("\(row.moveNumber).")
                                .font(.system(.subheadline, design: .rounded).monospacedDigit())
                                .foregroundStyle(.secondary)
                                .frame(width: 28, alignment: .leading)
                            Text(row.whiteText)
                                .font(.system(.body, design: .rounded))
                            if let bt = row.blackText {
                                Text(bt)
                                    .font(.system(.body, design: .rounded))
                                    .padding(.leading, Theme.Spacing.s)
                            }
                            Spacer(minLength: 0)
                        }
                    }
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }
}

private struct EndgameEvalPlotPoint: Identifiable {
    let id: Int
    let ply: Int
    let eval: Double
    let grade: MoveGrade
}

#Preview {
    GameEndReportSheet()
        .environment(BoardConnectionStore())
}
