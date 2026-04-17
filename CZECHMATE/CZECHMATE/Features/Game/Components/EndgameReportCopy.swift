//
//  EndgameReportCopy.swift
//  CZECHMATE — texty souhrnu partie (CS + automaticky EN podle preferovaného jazyka).
//

import Foundation

enum EndgameReportCopy {
    private static var useEnglish: Bool {
        Locale.preferredLanguages.first?.hasPrefix("en") == true
    }

    static var navTitle: String { useEnglish ? "Game summary" : "Souhrn partie" }
    static var done: String { useEnglish ? "Done" : "Hotovo" }
    static var share: String { useEnglish ? "Share" : "Sdílet" }
    static var sharePgn: String { useEnglish ? "Share PGN" : "Sdílet PGN" }
    static var shareSummary: String { useEnglish ? "Share text summary" : "Sdílet text souhrnu" }
    static var notAvailableTitle: String { useEnglish ? "Summary unavailable" : "Souhrn není k dispozici" }
    static var notAvailableDetail: String {
        useEnglish
            ? "The game has not ended yet, or board data is missing."
            : "Partie ještě neskončila, nebo chybí data ze desky."
    }

    static var timeAxis: String { useEnglish ? "Timeline" : "Časová osa" }
    static var playedTime: String { useEnglish ? "Elapsed time" : "Odehraný čas" }
    static var playedTimeSub: String {
        useEnglish
            ? "Cumulative from move timestamps (min)"
            : "Součet od začátku (min) — z razítek tahů na desce"
    }
    static var timePerMove: String { useEnglish ? "Time per move" : "Čas na tah" }
    static var timePerMoveSub: String {
        useEnglish
            ? "Seconds since previous move — scroll horizontally for long games"
            : "Sekundy od předchozího tahu — posuň vodorovně u dlouhých partií"
    }
    static var white: String { useEnglish ? "White" : "Bílý" }
    static var black: String { useEnglish ? "Black" : "Černý" }
    static var noMoveTimestamps: String {
        useEnglish
            ? "No per-move timestamps from the board — time charts unavailable."
            : "Deska neposílá razítka u tahů — grafy času nejsou k dispozici."
    }

    static var clockEndTitle: String {
        useEnglish ? "Clock at end of game" : "Časomíra na konci partie"
    }
    static var clockEndSub: String {
        useEnglish ? "Remaining time from the board (minutes)" : "Zbývající čas ze snímku (minuty)"
    }

    static var evalTitle: String { useEnglish ? "Position (Stockfish)" : "Pozice (Stockfish)" }
    static var evalSub: String {
        useEnglish
            ? "Eval after each move (white’s perspective, pawns). Requires move evaluation enabled during the game."
            : "Evaluace po tahu z perspektivy bílého (pány). Vyžaduje zapnuté vyhodnocování tahů během partie."
    }
    static var noEvalData: String {
        useEnglish
            ? "No evaluation history for this game — turn on move evaluation in developer settings before playing."
            : "Pro tuto partii nejsou uložené evaluace — zapni vyhodnocování tahů (vývojář) před hrou."
    }

    static var progressSection: String { useEnglish ? "Game moves" : "Průběh partie" }
    static var noMovesInHistory: String { useEnglish ? "No moves in history." : "Žádné tahy v historii." }
    static var statMoves: String { useEnglish ? "Moves" : "Tahy" }
    static var statDuration: String { useEnglish ? "Duration" : "Délka" }

    /// Stejný text jako na Apple Watch (časomíra + tahy v jedné kartě).
    static var compactWatchSummaryTitle: String {
        useEnglish ? "Compact summary (same as Watch)" : "Kompaktní přehled (stejný jako na Watch)"
    }

    static func previousGame(_ headline: String) -> String {
        useEnglish ? "Previous game: \(headline)" : "Předchozí partie: \(headline)"
    }

    static var axisHalfMove: String { useEnglish ? "Half-move" : "Půltah" }
    static var axisPawnsEval: String { useEnglish ? "pawns (White+)" : "pány (bílý+)" }

    // MARK: - Sdílení obrázku souhrnu

    static var imageShareTitle: String { useEnglish ? "Share as image" : "Sdílet jako obrázek" }
    static var imageShareLayoutSection: String { useEnglish ? "Layout" : "Rozvržení" }
    static var imageShareQuality: String { useEnglish ? "Export quality" : "Kvalita exportu" }
    static var imageShareQualityStandard: String { useEnglish ? "Standard (2×)" : "Standard (2×)" }
    static var imageShareQualityHigh: String { useEnglish ? "High (3×)" : "Vysoká (3×)" }
    static var imageShareQualityHint: String {
        useEnglish ? "Higher scale is sharper for social posts and printing." : "Vyšší měřítko = ostřejší výstup pro sociální sítě nebo tisk."
    }
    static var imageSharePreview: String { useEnglish ? "Preview" : "Náhled" }
    static var imageShareRendering: String { useEnglish ? "Rendering…" : "Vykresluji…" }
    static var imageShareNoPreview: String { useEnglish ? "No preview" : "Žádný náhled" }
    static var imageShareNoPreviewDetail: String {
        useEnglish ? "Try another layout or wait for rendering to finish." : "Zkus jiné rozvržení nebo počkej na dokončení vykreslení."
    }
    static var imageShareShareImage: String { useEnglish ? "Share image" : "Sdílet obrázek" }
    static var imageShareCopyClipboard: String { useEnglish ? "Copy to clipboard" : "Kopírovat do schránky" }
    static var imageShareCopied: String { useEnglish ? "Copied" : "Zkopírováno" }

    static var imageShareLayoutCustomizeHint: String {
        useEnglish
            ? "After picking a preset you can turn individual blocks on or off below."
            : "Po zvolení předvolby můžeš jednotlivé bloky níže zapínat a vypínat."
    }
    static var imageShareBackgroundSection: String { useEnglish ? "Background" : "Pozadí" }
    static var imageShareBackgroundGradient: String { useEnglish ? "Studio gradient" : "Studiový gradient" }
    static var imageShareBackgroundTransparent: String { useEnglish ? "Transparent" : "Průhledné" }
    static var imageShareBackgroundHint: String {
        useEnglish
            ? "Transparent PNG is ideal as an overlay; panels stay on semi-dark tiles so text stays readable."
            : "Průhledné PNG se hodí jako překryv; panely zůstávají na tmavých dlaždicích kvůli čitelnosti."
    }
    static var imageShareContentSection: String { useEnglish ? "Content" : "Obsah" }
    static var imageShareToggleHeader: String { useEnglish ? "App header" : "Hlavička aplikace" }
    static var imageShareToggleHero: String { useEnglish ? "Result (score & outcome)" : "Výsledek (skóre a důvod)" }
    static var imageShareToggleAveragesStrip: String {
        useEnglish ? "Average time — White / Black / move (strip)" : "Průměrný čas — bílý / černý / tah (proužek)"
    }
    static var imageShareToggleBigStats: String {
        useEnglish ? "Large average stats block" : "Velké bloky průměrů"
    }
    static var imageShareToggleChips: String { useEnglish ? "Move count & duration chips" : "Štítky tahů a délky" }
    static var imageShareToggleCumulative: String { useEnglish ? "Elapsed time chart" : "Graf odehraného času" }
    static var imageShareToggleBar: String { useEnglish ? "Time per move chart" : "Graf času na tah" }
    static var imageShareToggleEval: String { useEnglish ? "Stockfish eval (or note if missing)" : "Eval Stockfish (nebo upozornění)" }
    static var imageShareToggleClock: String { useEnglish ? "Clock at end" : "Časomíra na konci" }
    static var imageShareToggleFooter: String { useEnglish ? "Footer (czechmate.app)" : "Patička (czechmate.app)" }
    static var imageShareNoContentWarning: String {
        useEnglish
            ? "Turn on at least one content block besides header/footer."
            : "Zapni aspoň jeden obsahový blok kromě hlavičky nebo patičky."
    }

    static func imageShareLayoutTitle(_ layout: GameEndReportShareLayout) -> String {
        switch layout {
        case .classic: return useEnglish ? "Full card" : "Kompletní karta"
        case .timeFocused: return useEnglish ? "Time & charts" : "Čas a grafy"
        case .evalFocused: return useEnglish ? "Position analysis" : "Analýza pozice"
        case .statsAverages: return useEnglish ? "Stats & averages" : "Průměry a čísla"
        case .minimal: return useEnglish ? "Result only" : "Jen výsledek"
        }
    }

    static func imageShareLayoutSubtitle(_ layout: GameEndReportShareLayout) -> String {
        switch layout {
        case .classic:
            return useEnglish
                ? "Result, stats, time curve and eval (if available)."
                : "Výsledek, statistiky, časová křivka a eval (pokud je k dispozici)."
        case .timeFocused:
            return useEnglish
                ? "Elapsed time, time per move and per-side averages."
                : "Odehraný čas, čas na tah a průměry podle stran."
        case .evalFocused:
            return useEnglish
                ? "Stockfish eval curve after each move."
                : "Křivka evaluace Stockfish po tahu."
        case .statsAverages:
            return useEnglish
                ? "Average time per move (W/B), moves, game length — fewer charts."
                : "Průměrný čas na tah (bílý/černý), tahy, délka partie — minimum grafů."
        case .minimal:
            return useEnglish
                ? "Score and app name — good for stories."
                : "Skóre a název aplikace — vhodné na stories."
        }
    }
}

enum EndgameReportStorageKeys {
    static let lastFingerprint = "czechmate.endgameReport.lastFingerprint"
    static let lastHeadline = "czechmate.endgameReport.lastHeadline"
}
