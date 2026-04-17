//
//  AppDebugLog.swift
//  CZECHMATE — jednotné [staging] výpisy do Xcode konzole (jen DEBUG).
//

import Foundation

enum AppDebugLog {
    /// `UserDefaults` — v Release zapni v Nastavení → Vývojář → „Podrobné logy (trenér / model)“.
    static let coachTraceLogsDefaultsKey = "czechmate.coach.traceLogs"

    /// Obecný řádek; v Release se nevyhodnotí string (autoclosure).
    static func staging(_ message: @autoclosure () -> String) {
        #if DEBUG
        print("[staging] \(message())")
        #endif
    }

    /// Chyba s domain/code (typicky NSURLError).
    static func stagingError(_ label: String, _ error: Error) {
        #if DEBUG
        let ns = error as NSError
        print("[staging] [\(label)] \(error.localizedDescription) — \(ns.domain) \(ns.code)")
        #endif
    }

    /// Odpovědi desky na BLE příkazy (`cmd_ack`) — v Release se tiskne jen při chybě (konzole / zařízení).
    static func boardBleCommandAck(_ message: @autoclosure () -> String, ok: Bool) {
        let text = message()
        #if DEBUG
        print("[staging] [BLE cmd_ack] \(text)")
        #else
        if !ok {
            print("[CZECHMATE][BLE] \(text)")
        }
        #endif
    }

    /// Trenér, stažení Gemmy, MediaPipe — vždy v DEBUG (`[staging:coach]`); v Release jen při zapnutém přepínači (`[coach]`).
    static func coachTrace(_ message: @autoclosure () -> String) {
        let text = message()
        #if DEBUG
        print("[staging:coach] \(text)")
        #endif
        if UserDefaults.standard.bool(forKey: coachTraceLogsDefaultsKey) {
            print("[coach] \(text)")
        }
    }
}
