//
//  AppAppearance.swift
//  Vzhled aplikace: systém / světlý / tmavý / OLED (černé pozadí).
//

import SwiftUI

enum AppAppearance: String, CaseIterable, Identifiable {
    case system
    case light
    case dark
    case oled

    var id: String { rawValue }

    var title: String {
        switch self {
        case .system: return "Podle systému"
        case .light: return "Světlé"
        case .dark: return "Tmavé"
        case .oled: return "OLED (černé)"
        }
    }

    var colorScheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .light: return .light
        case .dark, .oled: return .dark
        }
    }

    var isOLED: Bool { self == .oled }
}

extension EnvironmentValues {
    @Entry var czechmateOLEDBackground: Bool = false
}

// MARK: - Styl šachovnice (barvy polí v `ChessBoardView` / `MiniBoardView`)

enum ChessBoardStyle: String, CaseIterable, Identifiable {
    case wooden
    case modernDark
    case iceBlue
    case forestGreen
    case marbleGray
    case midnight
    case coral
    case slate

    var id: String { rawValue }

    var title: String {
        switch self {
        case .wooden: return "Dřevěná klasik"
        case .modernDark: return "Tmavý turnaj"
        case .iceBlue: return "Ledově modrá"
        case .forestGreen: return "Lesní zeleň"
        case .marbleGray: return "Mramor šedá"
        case .midnight: return "Půlnoční modř"
        case .coral: return "Korál a terakota"
        case .slate: return "Břidlicová"
        }
    }

    /// Světlé / tmavé pole (řádek+sloupec sudý/lichý).
    var squareLight: Color {
        switch self {
        case .wooden:
            Color(red: 0.96, green: 0.88, blue: 0.72)
        case .modernDark:
            Color(red: 0.48, green: 0.48, blue: 0.50)
        case .iceBlue:
            Color(red: 0.88, green: 0.93, blue: 0.98)
        case .forestGreen:
            Color(red: 0.86, green: 0.93, blue: 0.86)
        case .marbleGray:
            Color(red: 0.97, green: 0.97, blue: 0.96)
        case .midnight:
            Color(red: 0.36, green: 0.40, blue: 0.52)
        case .coral:
            Color(red: 0.98, green: 0.91, blue: 0.84)
        case .slate:
            Color(red: 0.78, green: 0.81, blue: 0.85)
        }
    }

    var squareDark: Color {
        switch self {
        case .wooden:
            Color(red: 0.55, green: 0.40, blue: 0.28)
        case .modernDark:
            Color(red: 0.20, green: 0.20, blue: 0.22)
        case .iceBlue:
            Color(red: 0.42, green: 0.58, blue: 0.76)
        case .forestGreen:
            Color(red: 0.18, green: 0.42, blue: 0.28)
        case .marbleGray:
            Color(red: 0.72, green: 0.70, blue: 0.68)
        case .midnight:
            Color(red: 0.11, green: 0.13, blue: 0.22)
        case .coral:
            Color(red: 0.74, green: 0.40, blue: 0.32)
        case .slate:
            Color(red: 0.40, green: 0.44, blue: 0.52)
        }
    }
}
