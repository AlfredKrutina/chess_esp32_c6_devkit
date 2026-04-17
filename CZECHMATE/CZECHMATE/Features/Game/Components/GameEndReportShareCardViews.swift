//
//  GameEndReportShareCardViews.swift
//  CZECHMATE — vizuální karta pro export obrázku (gradient / průhledné pozadí, volitelné bloky, Charts).
//

import Charts
import SwiftUI

// MARK: - Data pro export

struct ShareEvalPoint: Identifiable, Equatable {
    var id: Int { ply }
    let ply: Int
    let eval: Double
    let grade: MoveGrade
}

struct GameEndReportSharePayload {
    let model: GameEndReportModel
    let thinkPoints: [EndgameThinkPlyPoint]
    let cumulative: [EndgameCumulativePoint]
    let barPoints: [EndgameThinkPlyPoint]
    let evalPoints: [ShareEvalPoint]
    let clock: BoardTimerHTTPState?
    let avgWhite: Double?
    let avgBlack: Double?
    let avgOverall: Double?

    static func build(
        snapshot: GameSnapshot,
        model: GameEndReportModel,
        evalHistory: [MoveEvalHistoryEntry]
    ) -> GameEndReportSharePayload {
        let think = GameEndReportTiming.thinkPlyPoints(from: snapshot.history.moves)
        let cum = GameEndReportTiming.cumulativePoints(from: think)
        let bar = think.filter { $0.secondsFromPrevious != nil }
        let avgs = GameEndReportTiming.averageSecondsPerSide(from: think)
        let overall = GameEndReportTiming.overallAverageSeconds(from: think)
        let n = snapshot.history.moves.count
        let evalPts: [ShareEvalPoint] = evalHistory
            .filter { $0.moveIndex1Based <= n && $0.evalWhitePawns != nil }
            .map { ShareEvalPoint(ply: $0.moveIndex1Based, eval: $0.evalWhitePawns!, grade: $0.grade) }
            .sorted { $0.ply < $1.ply }
        let clock = snapshot.clock
        return GameEndReportSharePayload(
            model: model,
            thinkPoints: think,
            cumulative: cum,
            barPoints: bar,
            evalPoints: evalPts,
            clock: clock,
            avgWhite: avgs.white,
            avgBlack: avgs.black,
            avgOverall: overall
        )
    }
}

// MARK: - Panel (sklo na gradientu / neprůhledný na alfa pozadí)

private struct ShareExportPanel<Content: View>: View {
    /// `true` = `.ultraThinMaterial` jako dřív; `false` = tmavý panel čitelný na průhledném PNG.
    var usesGlassMaterial: Bool
    var cornerRadius: CGFloat = 22
    @ViewBuilder var content: () -> Content

    var body: some View {
        content()
            .padding(22)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background {
                Group {
                    if usesGlassMaterial {
                        RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                            .fill(.ultraThinMaterial)
                            .overlay {
                                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                                    .stroke(
                                        LinearGradient(
                                            colors: [
                                                Color.white.opacity(0.35),
                                                Color.white.opacity(0.08),
                                            ],
                                            startPoint: .topLeading,
                                            endPoint: .bottomTrailing
                                        ),
                                        lineWidth: 1.2
                                    )
                            }
                    } else {
                        RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                            .fill(Color.black.opacity(0.56))
                            .overlay {
                                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                                    .stroke(Color.white.opacity(0.2), lineWidth: 1)
                            }
                    }
                }
            }
    }
}

// MARK: - Hlavní karta

struct GameEndReportShareCardView: View {
    let layout: GameEndReportShareLayout
    let payload: GameEndReportSharePayload
    let options: GameEndReportShareExportOptions

    private var model: GameEndReportModel { payload.model }
    private var resolved: GameEndReportShareExportOptions { options.resolved(for: payload) }
    private var useGlassPanels: Bool { !resolved.transparentBackground }

    var body: some View {
        ZStack {
            shareBackdrop
            VStack(alignment: .leading, spacing: sectionSpacing) {
                if resolved.includeBrandHeader {
                    headerBrand
                }
                if resolved.includeHero {
                    heroBlock
                }
                composedSections
                if resolved.includeFooterBrand {
                    footerBrand
                }
            }
            .padding(.horizontal, 44)
            .padding(.top, 52)
            .padding(.bottom, 44)
        }
        .frame(width: GameEndReportShareLayout.exportWidth)
        .fixedSize(horizontal: false, vertical: true)
        .environment(\.colorScheme, .dark)
    }

    private var sectionSpacing: CGFloat { 22 }

    private var shareBackdrop: some View {
        Group {
            if resolved.transparentBackground {
                Color.clear
            } else {
                ZStack {
                    LinearGradient(
                        colors: [
                            Color(red: 0.06, green: 0.08, blue: 0.09),
                            Color(red: 0.04, green: 0.12, blue: 0.08),
                            Color(red: 0.03, green: 0.06, blue: 0.07),
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                    RadialGradient(
                        colors: [Theme.accent.opacity(0.22), Color.clear],
                        center: .topTrailing,
                        startRadius: 40,
                        endRadius: 520
                    )
                }
            }
        }
    }

    @ViewBuilder
    private var composedSections: some View {
        if resolved.includeAveragesStrip {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                averagesBigRow
            }
        }
        if resolved.includeBigStatBlock {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                bigStatBlockContent
            }
        }
        if resolved.includeStatChips {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                statsChipsRow
            }
        }
        if resolved.includeCumulativeChart, !payload.cumulative.isEmpty {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                chartTitle(EndgameReportCopy.playedTime)
                cumulativeChart(height: cumulativeChartHeight)
            }
        }
        if resolved.includeBarChart, !payload.barPoints.isEmpty {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                chartTitle(EndgameReportCopy.timePerMove)
                barChart(height: barChartHeight)
            }
        }
        if resolved.includeEvalChart {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                if payload.evalPoints.isEmpty {
                    Text(EndgameReportCopy.noEvalData)
                        .font(.system(size: 24, design: .rounded))
                        .foregroundStyle(.white.opacity(0.72))
                        .fixedSize(horizontal: false, vertical: true)
                } else {
                    chartTitle(EndgameReportCopy.evalTitle)
                    evalChart(height: evalChartHeight)
                }
            }
        }
        if resolved.includeClockChart, let c = payload.clock, c.isTimeControlEnabled {
            ShareExportPanel(usesGlassMaterial: useGlassPanels) {
                chartTitle(EndgameReportCopy.clockEndTitle)
                clockChart(c, height: 200)
            }
        }
    }

    private var cumulativeChartHeight: CGFloat {
        switch layout {
        case .timeFocused: return 340
        default: return 260
        }
    }

    private var barChartHeight: CGFloat {
        switch layout {
        case .timeFocused: return 340
        case .statsAverages: return 300
        default: return 260
        }
    }

    private var evalChartHeight: CGFloat {
        let onlyEval = resolved.includeEvalChart
            && !resolved.includeCumulativeChart
            && !resolved.includeBarChart
        if layout == .evalFocused || onlyEval {
            return 620
        }
        return 280
    }

    private var headerBrand: some View {
        HStack(spacing: 12) {
            Image(systemName: "checkerboard.rectangle")
                .font(.system(size: 36, weight: .semibold))
                .foregroundStyle(Theme.accent)
            Text("CZECHMATE")
                .font(.system(size: 28, design: .rounded).weight(.heavy))
                .foregroundStyle(.white.opacity(0.92))
            Spacer()
        }
        .padding(.bottom, resolved.transparentBackground ? 4 : 0)
        .shadow(color: headerFooterShadow, radius: resolved.transparentBackground ? 6 : 0, y: 2)
    }

    private var headerFooterShadow: Color {
        resolved.transparentBackground ? Color.black.opacity(0.55) : .clear
    }

    private var heroBlock: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text(model.headline.uppercased())
                .font(.system(size: 32, design: .rounded).weight(.bold))
                .foregroundStyle(.white.opacity(0.75))
            Text(model.scoreLine)
                .font(.system(size: 112, weight: .heavy, design: .rounded))
                .minimumScaleFactor(0.5)
                .lineLimit(1)
                .foregroundStyle(.white)
            Text(model.terminationLine)
                .font(.system(size: 26, design: .rounded).weight(.medium))
                .foregroundStyle(.white.opacity(0.72))
                .lineLimit(3)
        }
        .shadow(color: heroShadow, radius: resolved.transparentBackground ? 8 : 0, y: 3)
    }

    private var heroShadow: Color {
        resolved.transparentBackground ? Color.black.opacity(0.65) : .clear
    }

    private var bigStatBlockContent: some View {
        VStack(alignment: .leading, spacing: 28) {
            bigStatTile(
                title: "Průměr — bílý",
                value: payload.avgWhite.map { GameEndReportTiming.formatSeconds($0) } ?? "—"
            )
            bigStatTile(
                title: "Průměr — černý",
                value: payload.avgBlack.map { GameEndReportTiming.formatSeconds($0) } ?? "—"
            )
            bigStatTile(
                title: "Průměr — celkem",
                value: payload.avgOverall.map { GameEndReportTiming.formatSeconds($0) } ?? "—"
            )
            Divider().opacity(0.35)
            HStack(spacing: 20) {
                bigStatTile(title: "Tahy", value: "\(model.moveCount)", compact: true)
                if let d = model.durationSummary {
                    bigStatTile(title: "Délka partie", value: d, compact: true)
                }
            }
        }
    }

    private func bigStatTile(title: String, value: String, compact: Bool = false) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(.system(size: compact ? 22 : 26, design: .rounded).weight(.semibold))
                .foregroundStyle(.white.opacity(0.55))
            Text(value)
                .font(.system(size: compact ? 36 : 52, weight: .bold, design: .rounded))
                .foregroundStyle(.white)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var averagesBigRow: some View {
        HStack(spacing: 16) {
            avgPill(title: "Ø bílý", seconds: payload.avgWhite)
            avgPill(title: "Ø černý", seconds: payload.avgBlack)
            avgPill(title: "Ø tah", seconds: payload.avgOverall)
        }
    }

    private func avgPill(title: String, seconds: Double?) -> some View {
        VStack(spacing: 6) {
            Text(title)
                .font(.system(size: 20, design: .rounded).weight(.semibold))
                .foregroundStyle(.white.opacity(0.55))
            Text(seconds.map { GameEndReportTiming.formatSeconds($0) } ?? "—")
                .font(.system(size: 28, design: .rounded).weight(.heavy))
                .foregroundStyle(.white)
                .minimumScaleFactor(0.6)
                .lineLimit(1)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 14)
        .background {
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(Color.white.opacity(0.08))
        }
    }

    private var statsChipsRow: some View {
        HStack(spacing: 14) {
            chip(icon: "list.number", text: "\(model.moveCount) tahů")
            if let d = model.durationSummary {
                chip(icon: "clock", text: d)
            }
            if payload.clock?.isTimeControlEnabled == true {
                chip(icon: "timer", text: "Časomíra")
            }
        }
    }

    private func chip(icon: String, text: String) -> some View {
        Label(text, systemImage: icon)
            .font(.system(size: 22, design: .rounded).weight(.medium))
            .foregroundStyle(.white.opacity(0.9))
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
            .background {
                Capsule().fill(Color.white.opacity(0.12))
            }
    }

    private func chartTitle(_ s: String) -> some View {
        Text(s)
            .font(.system(size: 24, design: .rounded).weight(.bold))
            .foregroundStyle(.white.opacity(0.85))
            .padding(.bottom, 8)
    }

    private func cumulativeChart(height: CGFloat) -> some View {
        Chart(payload.cumulative) { p in
            AreaMark(
                x: .value("p", p.plyIndex),
                y: .value("m", p.totalSeconds / 60)
            )
            .foregroundStyle(Theme.accent.opacity(0.25))
            LineMark(
                x: .value("p", p.plyIndex),
                y: .value("m", p.totalSeconds / 60)
            )
            .interpolationMethod(.catmullRom)
            .lineStyle(StrokeStyle(lineWidth: 3))
            .foregroundStyle(Theme.accent)
        }
        .chartXAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.15))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.55))
            }
        }
        .chartYAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.12))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.55))
            }
        }
        .frame(height: height)
    }

    private func barChart(height: CGFloat) -> some View {
        Chart {
            ForEach(payload.barPoints) { p in
                BarMark(
                    x: .value("p", p.plyIndex),
                    y: .value("s", p.secondsFromPrevious ?? 0)
                )
                .foregroundStyle(p.isWhite ? Theme.accent.opacity(0.85) : Color.purple.opacity(0.65))
            }
        }
        .chartXAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.12))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.5))
            }
        }
        .chartYAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.1))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.5))
            }
        }
        .frame(height: height)
    }

    private func evalChart(height: CGFloat) -> some View {
        Chart(payload.evalPoints) { p in
            LineMark(
                x: .value("ply", p.ply),
                y: .value("ev", p.eval)
            )
            .interpolationMethod(.catmullRom)
            .lineStyle(StrokeStyle(lineWidth: 3))
            .foregroundStyle(Theme.accent)
            PointMark(
                x: .value("ply", p.ply),
                y: .value("ev", p.eval)
            )
            .symbolSize(evalSymbolSize(for: p.grade))
            .foregroundStyle(evalColor(p.grade))
        }
        .chartXAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.12))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.5))
            }
        }
        .chartYAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.1))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.5))
            }
        }
        .frame(height: height)
    }

    private func clockChart(_ c: BoardTimerHTTPState, height: CGFloat) -> some View {
        Chart {
            BarMark(
                x: .value("s", "Bílý"),
                y: .value("m", Double(c.whiteTimeMs) / 60_000)
            )
            .foregroundStyle(Theme.accent.opacity(0.8))
            BarMark(
                x: .value("s", "Černý"),
                y: .value("m", Double(c.blackTimeMs) / 60_000)
            )
            .foregroundStyle(Color.purple.opacity(0.65))
        }
        .chartXAxis {
            AxisMarks { _ in
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.6))
            }
        }
        .chartYAxis {
            AxisMarks(values: .automatic) { _ in
                AxisGridLine(stroke: StrokeStyle(lineWidth: 0.5))
                    .foregroundStyle(.white.opacity(0.1))
                AxisValueLabel()
                    .foregroundStyle(.white.opacity(0.5))
            }
        }
        .frame(height: height)
    }

    private func evalColor(_ g: MoveGrade) -> Color {
        switch g {
        case .best: Theme.accent
        case .good: Color.green.opacity(0.9)
        case .inaccuracy: Color.yellow.opacity(0.95)
        case .mistake: Color.orange
        case .blunder: Color.red
        case .error: Color.gray
        }
    }

    private func evalSymbolSize(for g: MoveGrade) -> CGFloat {
        switch g {
        case .blunder: return 120
        case .mistake: return 90
        case .inaccuracy: return 70
        default: return 55
        }
    }

    private var footerBrand: some View {
        HStack {
            Spacer()
            Text("czechmate.app")
                .font(.system(size: 22, design: .rounded).weight(.semibold))
                .foregroundStyle(.white.opacity(resolved.transparentBackground ? 0.55 : 0.35))
        }
        .padding(.top, 6)
        .shadow(color: headerFooterShadow, radius: resolved.transparentBackground ? 5 : 0, y: 2)
    }
}
