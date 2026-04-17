//
//  GameEndReportShareLayout.swift
//  CZECHMATE — vizuální šablony exportu souhrnu jako obrázek (inspirace sportovními „activity cards“).
//

import Foundation

/// Šablona obrázku pro sdílení (Strava-like: jedna scéna, silná typografie, přehledné bloky).
enum GameEndReportShareLayout: String, CaseIterable, Identifiable, Sendable {
    /// Výsledek, statistiky, křivka času a případně eval — univerzální karta.
    case classic
    /// Dominuje časová osa (součet + sloupce), průměry na tah.
    case timeFocused
    /// Graf pozice (Stockfish) a výsledek.
    case evalFocused
    /// Velké čísla: průměry B/Č, celkový průměr, tahy, délka — minimum grafů.
    case statsAverages
    /// Kompaktní: jen výsledek a značka — rychlý export.
    case minimal

    var id: String { rawValue }

    /// Exportní šířka (výška je dynamická podle zvolených bloků — viz `GameEndReportShareImageExporter`).
    static let exportWidth: CGFloat = 1080
    /// Historická referenční výška (9:16); náhled už používá skutečný poměr exportovaného obrázku.
    static let exportHeightLegacy: CGFloat = 1920
}
