//
//  GameEndReportShareExportOptions.swift
//  CZECHMATE — volby exportu souhrnu (průhlednost, které bloky, výška podle obsahu).
//

import Foundation

/// Co zahrnout do rasterizované karty; šířka exportu zůstává `GameEndReportShareLayout.exportWidth`.
struct GameEndReportShareExportOptions: Equatable, Sendable {
    var transparentBackground: Bool

    var includeBrandHeader: Bool
    var includeHero: Bool
    /// Řádek pilulek Ø bílý / Ø černý / Ø tah (jako time-focused).
    var includeAveragesStrip: Bool
    /// Velké dlaždice průměrů + tahy/délka (šablona „průměry“).
    var includeBigStatBlock: Bool
    var includeStatChips: Bool
    var includeCumulativeChart: Bool
    var includeBarChart: Bool
    var includeEvalChart: Bool
    var includeClockChart: Bool
    var includeFooterBrand: Bool

    /// Výchozí stav podle předvolené šablony (uživatel může dál upravit přepínače).
    static func preset(_ layout: GameEndReportShareLayout) -> GameEndReportShareExportOptions {
        switch layout {
        case .classic:
            GameEndReportShareExportOptions(
                transparentBackground: false,
                includeBrandHeader: true,
                includeHero: true,
                includeAveragesStrip: false,
                includeBigStatBlock: false,
                includeStatChips: true,
                includeCumulativeChart: true,
                includeBarChart: true,
                includeEvalChart: true,
                includeClockChart: true,
                includeFooterBrand: true
            )
        case .timeFocused:
            GameEndReportShareExportOptions(
                transparentBackground: false,
                includeBrandHeader: true,
                includeHero: true,
                includeAveragesStrip: true,
                includeBigStatBlock: false,
                includeStatChips: true,
                includeCumulativeChart: true,
                includeBarChart: true,
                includeEvalChart: false,
                includeClockChart: false,
                includeFooterBrand: true
            )
        case .evalFocused:
            GameEndReportShareExportOptions(
                transparentBackground: false,
                includeBrandHeader: true,
                includeHero: true,
                includeAveragesStrip: false,
                includeBigStatBlock: false,
                includeStatChips: true,
                includeCumulativeChart: false,
                includeBarChart: false,
                includeEvalChart: true,
                includeClockChart: false,
                includeFooterBrand: true
            )
        case .statsAverages:
            GameEndReportShareExportOptions(
                transparentBackground: false,
                includeBrandHeader: true,
                includeHero: true,
                includeAveragesStrip: false,
                includeBigStatBlock: true,
                includeStatChips: false,
                includeCumulativeChart: false,
                includeBarChart: true,
                includeEvalChart: false,
                includeClockChart: false,
                includeFooterBrand: true
            )
        case .minimal:
            GameEndReportShareExportOptions(
                transparentBackground: false,
                includeBrandHeader: true,
                includeHero: true,
                includeAveragesStrip: false,
                includeBigStatBlock: false,
                includeStatChips: false,
                includeCumulativeChart: false,
                includeBarChart: false,
                includeEvalChart: false,
                includeClockChart: false,
                includeFooterBrand: true
            )
        }
    }

    /// Vypne sekce, pro které nejsou data (grafy / časomíra).
    func resolved(for payload: GameEndReportSharePayload) -> GameEndReportShareExportOptions {
        var o = self
        let hasTiming = !payload.cumulative.isEmpty || !payload.barPoints.isEmpty
        if !hasTiming || payload.cumulative.isEmpty { o.includeCumulativeChart = false }
        if !hasTiming || payload.barPoints.isEmpty { o.includeBarChart = false }
        if payload.clock?.isTimeControlEnabled != true { o.includeClockChart = false }
        let hasAvg = payload.avgWhite != nil || payload.avgBlack != nil || payload.avgOverall != nil
        if !hasAvg {
            o.includeAveragesStrip = false
            o.includeBigStatBlock = false
        }
        return o
    }

    /// Alespoň jeden viditelný blok kromě hlavičky/patičky.
    var hasAnyContentBlock: Bool {
        includeHero || includeAveragesStrip || includeBigStatBlock || includeStatChips
            || includeCumulativeChart || includeBarChart || includeEvalChart || includeClockChart
    }
}
