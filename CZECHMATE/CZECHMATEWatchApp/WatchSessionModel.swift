//
//  WatchSessionModel.swift
//  Stav z iPhonu přes WatchConnectivity.
//

import CZECHMATEShared
import Foundation
import Observation
import WatchConnectivity
import WatchKit

@Observable
@MainActor
final class WatchSessionModel: NSObject {
    /// True po `pushConnectionLost` z iPhonu (polling zastaven).
    var connectionLost: Bool = false
    var companionMessage: String = ""
    var moveCount: Int = 0
    var currentPlayer: String = "—"
    var fen: String = ""
    var gameState: String = ""
    var isTimerRunning: Bool = false
    var whiteTime: UInt32?
    var blackTime: UInt32?
    var lastFrom: String?
    var lastTo: String?
    var inCheck: Bool = false
    var gameEnded: Bool = false
    var lastUpdate: Date?
    var advantageSummary: String = ""
    /// Čas iPhonu při posledním přenosu časů (pro odpočet aktivního hráče).
    var clockSyncAt: Date?

    private var lastHapticMoveCount: Int = -1

    func activate() {
        guard WCSession.isSupported() else { return }
        let s = WCSession.default
        s.delegate = self
        s.activate()
        applyContext(s.receivedApplicationContext)
    }

    private func applyContext(_ ctx: [String: Any]) {
        if let lost = ctx["connectionLost"] as? Bool, lost {
            connectionLost = true
            companionMessage = (ctx["companionMessage"] as? String) ?? CompanionStrings.connectionLostTitle
        } else {
            connectionLost = false
            companionMessage = ""
        }
        if let mc = ctx["moveCount"] as? Int {
            moveCount = mc
        } else if let mc = ctx["moveCount"] as? Int32 {
            moveCount = Int(mc)
        }
        if let cp = ctx["currentPlayer"] as? String { currentPlayer = cp }
        if let f = ctx["fen"] as? String { fen = f }
        if let gs = ctx["gameState"] as? String { gameState = gs }
        if let tr = ctx["isTimerRunning"] as? Bool { isTimerRunning = tr }
        if let w = ctx["whiteTime"] as? UInt32 { whiteTime = w }
        if let b = ctx["blackTime"] as? UInt32 { blackTime = b }
        if let w = ctx["whiteTime"] as? Int { whiteTime = UInt32(clamping: w) }
        if let b = ctx["blackTime"] as? Int { blackTime = UInt32(clamping: b) }
        lastFrom = ctx["lastMoveFrom"] as? String
        lastTo = ctx["lastMoveTo"] as? String
        if let ic = ctx["inCheck"] as? Bool { inCheck = ic }
        if let ge = ctx["gameEnded"] as? Bool { gameEnded = ge }
        if let ts = ctx["ts"] as? TimeInterval {
            lastUpdate = Date(timeIntervalSince1970: ts)
        }
        if let adv = ctx["advantageSummary"] as? String {
            advantageSummary = adv
        }
        if let c = ctx["clockSyncAt"] as? TimeInterval {
            clockSyncAt = Date(timeIntervalSince1970: c)
        } else if let ts = ctx["ts"] as? TimeInterval {
            clockSyncAt = Date(timeIntervalSince1970: ts)
        }
        if moveCount != lastHapticMoveCount, lastHapticMoveCount >= 0 {
            WKInterfaceDevice.current().play(.click)
        }
        lastHapticMoveCount = moveCount
    }
}

extension WatchSessionModel: WCSessionDelegate {
    nonisolated func session(
        _: WCSession,
        activationDidCompleteWith _: WCSessionActivationState,
        error _: Error?
    ) {}

    nonisolated func session(_: WCSession, didReceiveApplicationContext applicationContext: [String: Any]) {
        Task { @MainActor in
            self.applyContext(applicationContext)
            #if DEBUG
            print("[staging] Watch received context moveCount=\(applicationContext["moveCount"] ?? "?")")
            #endif
        }
    }
}
