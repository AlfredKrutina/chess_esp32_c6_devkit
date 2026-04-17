//
//  ChessTimeControlPresets.swift
//  Odpovídá `time_control_type_t` v `components/timer_system/include/timer_system.h` (typ 0…14).
//

import Foundation

/// Výběr času pro `POST /api/timer/config` (stejné jako webové ESP UI).
enum ChessTimeControlChoice: Equatable, Sendable {
    /// Přednastavený režim z firmware (1…13).
    case firmwareType(Int)
    /// `TIME_CONTROL_CUSTOM` — minuty 1…180, inkrement 0…60 sekund.
    case custom(minutes: Int, incrementSeconds: Int)

    var apiType: Int {
        switch self {
        case .firmwareType(let t): return t
        case .custom: return 14
        }
    }

    var customMinutes: Int? {
        if case .custom(let m, _) = self { return m }
        return nil
    }

    var customIncrement: Int? {
        if case .custom(_, let inc) = self { return inc }
        return nil
    }
}

/// Nabídka pro UI (chess.com styl).
enum ChessTimeControlPreset: String, CaseIterable, Identifiable, Codable {
    /// `TIME_CONTROL_NONE` — hra bez odpočtu (viz `timer_system.h`).
    case no_time_limit
    case bullet1_0
    case bullet2_1
    case blitz3_0
    case blitz3_2
    case blitz5_0
    case rapid10_0
    case rapid15_10

    var id: String { rawValue }

    var title: String {
        switch self {
        case .no_time_limit: return "Bez čas. limitu"
        case .bullet1_0: return "1 min"
        case .bullet2_1: return "2 | 1"
        case .blitz3_0: return "3 min"
        case .blitz3_2: return "3 | 2"
        case .blitz5_0: return "5 min"
        case .rapid10_0: return "10 min"
        case .rapid15_10: return "15 | 10"
        }
    }

    var subtitle: String {
        switch self {
        case .no_time_limit: return "Bez časomíry"
        case .bullet1_0: return "Bullet"
        case .bullet2_1: return "Bullet"
        case .blitz3_0, .blitz3_2, .blitz5_0: return "Blitz"
        case .rapid10_0, .rapid15_10: return "Rapid"
        }
    }

    var systemImage: String {
        switch self {
        case .no_time_limit: return "infinity"
        case .bullet1_0, .bullet2_1: return "bolt.fill"
        case .blitz3_0, .blitz3_2, .blitz5_0: return "hare.fill"
        case .rapid10_0, .rapid15_10: return "tortoise.fill"
        }
    }

    /// Index do `TIME_CONTROLS[]` ve firmware.
    var firmwareType: Int {
        switch self {
        case .no_time_limit: return 0 // TIME_CONTROL_NONE
        case .bullet1_0: return 1 // TIME_CONTROL_BULLET_1_0
        case .bullet2_1: return 3 // BULLET_2_1
        case .blitz3_0: return 4
        case .blitz3_2: return 5
        case .blitz5_0: return 6
        case .rapid10_0: return 8
        case .rapid15_10: return 10
        }
    }

    var choice: ChessTimeControlChoice {
        .firmwareType(firmwareType)
    }
}

// MARK: - Poslední volba (sheet „Nová hra“)

/// Uložení posledního času před novou hrou; při chybějících datech UI začíná na `no_time_limit`.
enum LastNewGameTimeSelection: Equatable, Codable {
    case presetChoice(ChessTimeControlPreset)
    case customChoice(minutes: Int, incrementSeconds: Int)

    private static let udKey = "czechmate.lastNewGameTimeSelection.v1"

    static func load() -> LastNewGameTimeSelection? {
        guard let data = UserDefaults.standard.data(forKey: udKey) else { return nil }
        return try? JSONDecoder().decode(Self.self, from: data)
    }

    func save() {
        if let data = try? JSONEncoder().encode(self) {
            UserDefaults.standard.set(data, forKey: Self.udKey)
        }
    }

    static func persist(from choice: ChessTimeControlChoice) {
        guard let sel = Self.from(choice) else { return }
        sel.save()
    }

    static func from(_ choice: ChessTimeControlChoice) -> LastNewGameTimeSelection? {
        switch choice {
        case .firmwareType(let t):
            guard let preset = ChessTimeControlPreset.allCases.first(where: { $0.firmwareType == t }) else {
                return nil
            }
            return .presetChoice(preset)
        case .custom(let m, let inc):
            return .customChoice(minutes: m, incrementSeconds: inc)
        }
    }
}
