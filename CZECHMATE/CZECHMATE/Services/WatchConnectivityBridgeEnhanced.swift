//
//  WatchConnectivityBridgeEnhanced.swift
//  CZECHMATE
//
//  Bidirectional WatchConnectivity with Gemini Agent support.
//  Handles commands from Watch and routes through iPhone priority system.
//

import Foundation
import CZECHMATEShared

#if os(iOS) && canImport(WatchConnectivity)
import WatchConnectivity

/// Enhanced bridge with bidirectional communication and Gemini Agent support
@MainActor
final class WatchConnectivityBridgeEnhanced: NSObject, WCSessionDelegate {
    static let shared = WatchConnectivityBridgeEnhanced()
    
    // MARK: - Dependencies
    
    /// BLE transport for sending commands to chessboard
    weak var bleTransport: BLEBoardTransportProtocol?
    
    /// Gemini agent for hints
    weak var geminiAgent: GeminiAgent?
    
    /// Move conflict resolver for iPhone priority
    private var conflictResolver: MoveConflictResolver { .shared }
    
    // MARK: - Initialization
    
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
    
    // MARK: - Context Push (iPhone → Watch)
    
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
        let boardClock = timer ?? snapshot.clock
        if let t = boardClock {
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

        let st = snapshot.status
        if let br = st.brightness { ctx["brightness"] = br }
        if let m = st.lightMode { ctx["lightMode"] = m }
        if let ls = st.lightState { ctx["lightState"] = ls }
        if let lr = st.lightR { ctx["lightR"] = lr }
        if let lg = st.lightG { ctx["lightG"] = lg }
        if let lb = st.lightB { ctx["lightB"] = lb }
        if let at = st.autoLampTimeoutSec { ctx["autoLampTimeoutSec"] = at }

        if gameEnded {
            let endFields = WatchCompanionEndgameSummary.contextFields(
                snapshot: snapshot,
                boardClock: boardClock
            )
            for (k, v) in endFields {
                ctx[k] = v
            }
        }

        do {
            try session.updateApplicationContext(ctx)
            #if DEBUG
            print("[WC Enhanced] Context push moveCount=\(ctx["moveCount"] ?? 0)")
            #endif
        } catch {
            #if DEBUG
            print("[WC Enhanced] Context push failed: \(error.localizedDescription)")
            #endif
        }
        #endif
    }
    
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
        } catch {
            #if DEBUG
            print("[WC Enhanced] Connection lost push failed: \(error)")
            #endif
        }
        #endif
    }
    
    // MARK: - WCSessionDelegate (Receiving from Watch)
    
    nonisolated func session(
        _ session: WCSession,
        activationDidCompleteWith activationState: WCSessionActivationState,
        error: Error?
    ) {
        #if DEBUG
        if let error {
            print("[WC Enhanced] Activation error: \(error.localizedDescription)")
        } else if activationState == .activated {
            // „Application context data is nil“ / „counterpart app not installed“ často vypisuje systém — tady je skutečný stav.
            print(
                "[WC Enhanced] Ready paired=\(session.isPaired) watchAppInstalled=\(session.isWatchAppInstalled)"
            )
        }
        #endif
    }
    
    nonisolated func sessionDidBecomeInactive(_: WCSession) {}
    
    nonisolated func sessionDidDeactivate(_: WCSession) {
        WCSession.default.activate()
    }
    
    nonisolated func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        Task { @MainActor in
            self.handleWatchMessage(message)
        }
    }
    
    nonisolated func session(
        _ session: WCSession,
        didReceiveMessage message: [String: Any],
        replyHandler: @escaping ([String: Any]) -> Void
    ) {
        Task { @MainActor in
            let response = self.handleWatchMessageWithResponse(message)
            replyHandler(response)
        }
    }
    
    // MARK: - Message Handling
    
    @MainActor
    private func handleWatchMessage(_ message: [String: Any]) {
        _ = handleWatchMessageWithResponse(message)
    }
    
    @MainActor
    private func handleWatchMessageWithResponse(_ message: [String: Any]) -> [String: Any] {
        guard let action = message["action"] as? String else {
            return ["error": "No action specified"]
        }
        
        switch action {
        case "executeMove":
            return handleExecuteMove(message)
        case "requestHint":
            return handleRequestHint(message)
        case "boardCommand":
            return handleBoardCommand(message)
        default:
            return ["error": "Unknown action: \(action)"]
        }
    }
    
    /// Obecný BLE příkaz z hodinek (nová hra, zpět, jas, hint_clear, …) — stejný JSON jako přímé BLE.
    @MainActor
    private func handleBoardCommand(_ message: [String: Any]) -> [String: Any] {
        guard let payload = message["ble"] as? [String: Any] else {
            return ["success": false, "error": "Missing ble payload"]
        }
        let sent = bleTransport?.sendCommand(payload) ?? false
        return ["success": sent]
    }
    
    @MainActor
    private func handleExecuteMove(_ message: [String: Any]) -> [String: Any] {
        guard let from = message["from"] as? String,
              let to = message["to"] as? String else {
            return ["success": false, "error": "Missing from/to"]
        }
        
        let move = ChessMove(
            from: from,
            to: to,
            promotion: message["promotion"] as? String
        )
        
        // iPhone priority: queue and validate
        let ticket = conflictResolver.queueMove(move, from: .watch)
        
        // Immediate check (iPhone might have just moved)
        if !conflictResolver.isValid(ticket) {
            return ["success": false, "error": "iPhone priority: move invalidated"]
        }
        
        // Forward to BLE transport
        let command: [String: Any] = [
            "cmd": "move",
            "from": from,
            "to": to,
            "promotion": move.promotion as Any
        ]
        
        let sent = bleTransport?.sendCommand(command) ?? false
        if sent {
            conflictResolver.markExecuted(ticket)
            return ["success": true]
        } else {
            conflictResolver.cancelMove(ticket)
            return ["success": false, "error": "Failed to send to board"]
        }
    }
    
    @MainActor
    private func handleRequestHint(_ message: [String: Any]) -> [String: Any] {
        guard let agent = geminiAgent else {
            return ["hint": "Gemini agent not available"]
        }
        
        // Get current game state from BLE transport or cache
        // This is a simplified version - actual implementation would need current FEN
        let fen = message["fen"] as? String ?? ""
        
        // Async hint request
        Task {
            let hint = await agent.requestHint(forFEN: fen)
            // Send hint back via context update or direct message
            self.sendHintToWatch(hint)
        }
        
        return ["success": true, "hint": "Analyzing..."]
    }
    
    @MainActor
    private func sendHintToWatch(_ hint: String) {
        #if !targetEnvironment(simulator)
        guard WCSession.default.isReachable else { return }
        
        let message = ["hint": hint]
        WCSession.default.sendMessage(message, replyHandler: nil) { error in
            #if DEBUG
            print("[WC Enhanced] Hint send failed: \(error)")
            #endif
        }
        #endif
    }
    
    nonisolated func sessionReachabilityDidChange(_ session: WCSession) {
        #if DEBUG
        print("[WC Enhanced] Reachability changed: \(session.isReachable)")
        #endif
    }
}

// MARK: - Gemini Agent Protocol

@MainActor
protocol GeminiAgent: AnyObject {
    func requestHint(forFEN fen: String) async -> String
}

// MARK: - BLE Transport Protocol

protocol BLEBoardTransportProtocol: AnyObject {
    func sendCommand(_ command: [String: Any]) -> Bool
}

#else

@MainActor
enum WatchConnectivityBridgeEnhanced {
    static func activateShared() {}
    static func pushCompanionContext(snapshot: GameSnapshot, timer: BoardTimerHTTPState?, fen: String) {}
    static func pushConnectionLost() {}
}

#endif
