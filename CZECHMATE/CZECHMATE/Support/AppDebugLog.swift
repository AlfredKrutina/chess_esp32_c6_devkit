//
//  AppDebugLog.swift
//  CZECHMATE — jednotné [staging] výpisy do Xcode konzole (jen DEBUG).
//

import Foundation

enum AppDebugLog {
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
}
