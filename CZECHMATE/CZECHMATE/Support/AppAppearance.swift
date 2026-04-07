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

// MARK: - Styl šachovnice (budoucí barvy polí)

enum ChessBoardStyle: String, CaseIterable, Identifiable {
    case wooden
    case modernDark

    var id: String { rawValue }

    var title: String {
        switch self {
        case .wooden: return "Základní dřevěná"
        case .modernDark: return "Moderní tmavá"
        }
    }
}
