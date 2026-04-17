//
//  BotStrengthCopy.swift
//  Popisky síly bota (NVS) pro uživatele — orientační Elo z čísla hloubky/strength.
//

import Foundation

enum BotStrengthCopy {
    /// Heuristický odhad pro UI; nejde o kalibrovaný Elo test.
    static func approximateEloDescription(strengthDigits: String) -> String {
        guard let d = Int(strengthDigits.trimmingCharacters(in: .whitespaces)), d >= 6, d <= 18 else {
            return "Číslo síly odpovídá nastavení na desce; větší hodnota obvykle znamená silnější návrhy tahů."
        }
        let approx = 480 + d * 95
        return "Orientačně jako hráč kolem \(approx) Elo — jen přibližná představa, ne měření."
    }
}
