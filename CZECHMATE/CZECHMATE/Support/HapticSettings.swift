//
//  HapticSettings.swift
//  Haptika — volitelně v Nastavení; generátory držíme kvůli nižší latenci.
//

import Foundation
#if os(iOS)
import UIKit
#endif

enum HapticSettings {
    /// Musí odpovídat `@AppStorage("czechmate.hapticsEnabled")` v Nastavení.
    private static let key = "czechmate.hapticsEnabled"

    static var isEnabled: Bool {
        if UserDefaults.standard.object(forKey: key) == nil { return true }
        return UserDefaults.standard.bool(forKey: key)
    }

    #if os(iOS)
    private static let lightGen = UIImpactFeedbackGenerator(style: .light)
    private static let mediumGen = UIImpactFeedbackGenerator(style: .medium)
    private static let heavyGen = UIImpactFeedbackGenerator(style: .heavy)
    private static let notificationGen = UINotificationFeedbackGenerator()

    /// Výběr pole / figurky.
    static func lightImpactIfEnabled() {
        guard isEnabled else { return }
        lightGen.prepare()
        lightGen.impactOccurred()
    }

    /// Platný tah (položení).
    static func mediumImpactIfEnabled() {
        guard isEnabled else { return }
        mediumGen.prepare()
        mediumGen.impactOccurred()
    }

    /// Šach nebo braní.
    static func heavyImpactIfEnabled() {
        guard isEnabled else { return }
        heavyGen.prepare()
        heavyGen.impactOccurred()
    }

    /// Neplatný tah.
    static func notificationErrorIfEnabled() {
        guard isEnabled else { return }
        notificationGen.prepare()
        notificationGen.notificationOccurred(.error)
    }
    #else
    static func lightImpactIfEnabled() {}
    static func mediumImpactIfEnabled() {}
    static func heavyImpactIfEnabled() {}
    static func notificationErrorIfEnabled() {}
    #endif
}
