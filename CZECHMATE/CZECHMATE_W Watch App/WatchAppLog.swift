//
//  WatchAppLog.swift
//  CZECHMATE_W — logy do Console / Xcode (subsystem = bundle ID).
//

import Foundation
import OSLog

enum WatchAppLog {
    private static let subsystem = Bundle.main.bundleIdentifier ?? "CZECHMATE.watch"
    private static let appLog = Logger(subsystem: subsystem, category: "app")
    private static let wcLog = Logger(subsystem: subsystem, category: "watchconnectivity")
    private static let bleLog = Logger(subsystem: subsystem, category: "ble")

    static func staging(_ message: String) {
        appLog.info("\(message, privacy: .public)")
        #if DEBUG
        print("[Watch][STAGING] \(message)")
        #endif
    }

    static func wc(_ message: String) {
        wcLog.info("\(message, privacy: .public)")
        #if DEBUG
        print("[Watch][WC] \(message)")
        #endif
    }

    static func ble(_ message: String) {
        bleLog.info("\(message, privacy: .public)")
        #if DEBUG
        print("[Watch][BLE] \(message)")
        #endif
    }
}
