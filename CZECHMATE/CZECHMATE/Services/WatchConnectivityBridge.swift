//
//  WatchConnectivityBridge.swift
//  iOS: kompaktní stav na Apple Watch (watchOS companion).
//

import Foundation
import CZECHMATEShared

#if os(iOS) && canImport(WatchConnectivity)
import WatchConnectivity

/// Posílá na hodinky rozšířený kontext (FEN, časy, …) přes `BoardCompanionSync` throttling.
@MainActor
final class WatchConnectivityBridge: NSObject, WCSessionDelegate {
    static let shared = WatchConnectivityBridge()

    /// Volat při startu aplikace (např. z `ContentView.onAppear`).
    static func activateShared() {
        #if targetEnvironment(simulator)
        #else
        _ = shared
        #endif
    }

    private override init() {
        super.init()
        if WCSession.isSupported() {
            WCSession.default.delegate = self
            WCSession.default.activate()
        }
    }

    static func pushCompanionContext(snapshot: GameSnapshot, timer: BoardTimerHTTPState?, fen: String) {
        #if targetEnvironment(simulator)
        return
        #else
        guard WCSession.isSupported(), WCSession.default.activationState == .activated else { return }
        let session = WCSession.default
        guard session.isPaired, session.isWatchAppInstalled else { return }

        let last = snapshot.history.moves.last
        let gameEnded =
            snapshot.status.gameEnd?.ended == true
            || snapshot.status.checkmate == true
            || snapshot.status.stalemate == true

        let advantage = MaterialEvaluator.advantageSummary(board: snapshot.board)
        let syncAt = Date().timeIntervalSince1970
        var ctx: [String: Any] = [
            "moveCount": Int(snapshot.status.moveCount),
            "currentPlayer": snapshot.status.currentPlayer,
            "gameState": snapshot.status.gameState,
            "isTimerRunning": snapshot.status.isTimerRunning,
            "inCheck": snapshot.status.inCheck,
            "gameEnded": gameEnded,
            "fen": fen,
            "ts": syncAt,
            "advantageSummary": advantage,
            "clockSyncAt": syncAt,
        ]
        if let t = timer {
            ctx["whiteTime"] = UInt32(t.whiteTimeMs / 1000)
            ctx["blackTime"] = UInt32(t.blackTimeMs / 1000)
            ctx["isTimerRunning"] = t.timerRunning && !t.gamePaused
        } else {
            if let w = snapshot.status.whiteTime { ctx["whiteTime"] = w }
            if let b = snapshot.status.blackTime { ctx["blackTime"] = b }
        }
        if let v = snapshot.stateVersion { ctx["stateVersion"] = v }
        if let f = last?.from { ctx["lastMoveFrom"] = f }
        if let t = last?.to { ctx["lastMoveTo"] = t }

        do {
            try session.updateApplicationContext(ctx)
            #if DEBUG
            print("[staging] WatchConnectivity push (throttled) moveCount=\(ctx["moveCount"] ?? 0) fen.len=\(fen.count)")
            #endif
        } catch {
            #if DEBUG
            print("[staging] WatchConnectivity failed: \(error.localizedDescription)")
            #endif
        }
        #endif
    }

    /// Po zastavení pollingu — hodinky zobrazí jednotnou hlášku o výpadku spojení.
    static func pushConnectionLost() {
        #if targetEnvironment(simulator)
        return
        #else
        guard WCSession.isSupported(), WCSession.default.activationState == .activated else { return }
        let session = WCSession.default
        guard session.isPaired, session.isWatchAppInstalled else { return }

        let ctx: [String: Any] = [
            "connectionLost": true,
            "companionMessage": CompanionStrings.connectionLostTitle,
            "advantageSummary": "",
            "moveCount": 0,
            "currentPlayer": "—",
            "fen": "",
            "ts": Date().timeIntervalSince1970,
            "gameEnded": false,
            "isTimerRunning": false,
        ]
        do {
            try session.updateApplicationContext(ctx)
            #if DEBUG
            print("[staging] WatchConnectivity push connectionLost")
            #endif
        } catch {
            #if DEBUG
            print("[staging] WatchConnectivity connectionLost failed: \(error.localizedDescription)")
            #endif
        }
        #endif
    }

    nonisolated func session(
        _ session: WCSession,
        activationDidCompleteWith activationState: WCSessionActivationState,
        error: Error?
    ) {
        #if DEBUG
        if let error {
            print("[staging] WC activation error: \(error.localizedDescription)")
        } else if activationState == .activated {
            // „Application context data is nil“ / „counterpart app not installed“ často vypisuje systém — tady je skutečný stav.
            print(
                "[staging] WC ready paired=\(session.isPaired) watchAppInstalled=\(session.isWatchAppInstalled)"
            )
        }
        #endif
    }

    nonisolated func sessionDidBecomeInactive(_: WCSession) {}
    nonisolated func sessionDidDeactivate(_: WCSession) {
        WCSession.default.activate()
    }
}

#else

@MainActor
enum WatchConnectivityBridge {
    static func activateShared() {}
    static func pushCompanionContext(snapshot _: GameSnapshot, fen _: String) {}
    static func pushConnectionLost() {}
}

#endif
