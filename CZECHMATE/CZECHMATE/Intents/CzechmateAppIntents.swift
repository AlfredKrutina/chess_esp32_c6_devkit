//
//  CzechmateAppIntents.swift
//  Siri Shortcuts — otevření záložky Hra.
//

import AppIntents
#if canImport(UIKit)
import UIKit
#endif

struct OpenCzechmateGameIntent: AppIntent {
    static var title: LocalizedStringResource = "Otevřít CZECHMATE — Hra"
    static var description = IntentDescription("Přepne aplikaci na záložku Hra (deep link).")
    static var openAppWhenRun: Bool = true

    func perform() async throws -> some IntentResult {
        #if canImport(UIKit)
        await MainActor.run {
            if let url = URL(string: "czechmate://game") {
                UIApplication.shared.open(url, options: [:], completionHandler: nil)
            }
        }
        #endif
        return .result()
    }
}
