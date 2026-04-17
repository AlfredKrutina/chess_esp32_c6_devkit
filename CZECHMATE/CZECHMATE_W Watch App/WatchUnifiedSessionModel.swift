//
//  WatchUnifiedSessionModel.swift
//  CZECHMATE_W Watch App
//
//  Přímé BLE k ESP (primární) + volitelný WatchConnectivity k iPhonu.
//  Bez automatického přepnutí na iPhone — BLE k desce stačí pro hru a příkazy.
//

import CZECHMATEShared
import CoreBluetooth
import Foundation
import Observation
import WatchConnectivity
import WatchKit

// Re-export for ContentView compatibility
public typealias WatchBLEConnectionState = BLEConnectionState

@Observable
@MainActor
final class WatchUnifiedSessionModel: NSObject {
    // MARK: - Connection Mode
    
    enum ConnectionMode: Equatable {
        case determining
        case bleDirect
        case watchConnectivity
        
        var description: String {
            switch self {
            case .determining:
                return "Zjišťuji..."
            case .bleDirect:
                return "Přímé BLE"
            case .watchConnectivity:
                return "Přes iPhone"
            }
        }
    }
    
    private(set) var connectionMode: ConnectionMode = .determining
    
    /// Živý kanál přes iPhone (`sendMessage`) — potřeba pro příkazy v režimu WC, pokud companion neběží v popředí.
    var isPhoneBridgeReachable: Bool {
        guard WCSession.isSupported() else { return false }
        return WCSession.default.isReachable
    }
    
    // MARK: - Game State (unified across modes)
    
    var gameState: UnifiedGameState = UnifiedGameState()
    
    /// Computed properties for UI compatibility
    var moveCount: Int { gameState.moveCount }
    var currentPlayer: String { gameState.currentPlayer }
    var fen: String { gameState.fen }
    var gameStateString: String { gameState.gameState }
    var isTimerRunning: Bool { gameState.isTimerRunning }
    var whiteTime: UInt32? { gameState.whiteTime }
    var blackTime: UInt32? { gameState.blackTime }
    var lastFrom: String? { gameState.lastFrom }
    var lastTo: String? { gameState.lastTo }
    var inCheck: Bool { gameState.inCheck }
    var gameEnded: Bool { gameState.gameEnded }
    var lastUpdate: Date? { gameState.lastUpdate }
    var advantageSummary: String { gameState.advantageSummary }
    var clockSyncAt: Date? { gameState.clockSyncAt }
    var watchEndgameHeadline: String? { gameState.watchEndgameHeadline }
    var watchEndgameScoreLine: String? { gameState.watchEndgameScoreLine }
    var watchEndgameReason: String? { gameState.watchEndgameReason }
    var watchEndgameStats: String? { gameState.watchEndgameStats }
    
    // MARK: - BLE Connection State
    
    var bleConnectionState: BLEConnectionState = .idle
    
    /// Computed for UI
    /// Pozn.: `isReachable` je true jen když je iPhone app v popředí — pro WC režim stačí aktivní session + párování;
    /// živé příkazy (`sendMessage`) stejně kontrolují `isReachable` zvlášť.
    var connectionLost: Bool {
        switch connectionMode {
        case .determining:
            return true
        case .bleDirect:
            return !(bleStateMachine?.connectionState.isConnected ?? false)
        case .watchConnectivity:
            guard WCSession.isSupported() else { return true }
            let s = WCSession.default
            // Na watchOS není `isPaired` — stačí aktivovaná session.
            return s.activationState != .activated
        }
    }
    
    // MARK: - User Preferences
    
    var hapticEnabled: Bool = false {
        didSet {
            UserDefaults.standard.set(hapticEnabled, forKey: "hapticEnabled")
        }
    }
    
    // MARK: - Internal State
    
    private var bleStateMachine: BLEStateMachine?
    private var wcSession: WCSession?
    private var lastHapticMoveCount: Int = -1
    /// Odložený první / záložní sken — musí se zrušit při `deactivate`, jinak běží pro `nil` nebo nový SM.
    private var bleDelayedInitialScan: DispatchWorkItem?
    private var bleBackupIdleScan: DispatchWorkItem?
    
    // Conflict resolution for iPhone priority
    private var pendingMove: (move: ChessMove, completion: ((Bool) -> Void))?
    private var iPhonePriorityTimer: Timer?

    /// Čekání na `cmd_ack` z desky po zápisu na GATT CMD (jen režim přímého BLE).
    private var bleCommandAckPending: (expectedCmd: String, completion: (Bool) -> Void, timeout: DispatchWorkItem)?
    
    // MARK: - Lifecycle
    
    override init() {
        super.init()
        self.hapticEnabled = UserDefaults.standard.bool(forKey: "hapticEnabled")
    }
    
    func activate() {
        WatchAppLog.staging("WatchUnifiedSessionModel.activate()")
        // WCSession musí být activate() hned při startu — jinak systém při čtení default session loguje
        // „WCSession has not been activated“ (BLE režim WC stejně nepoužívá ke hře).
        ensureWatchConnectivitySession()
        // Režim přes iPhone — BLE záměrně vypnutý (`switchToWatchConnectivity`), neobnovovat při každém scenePhase.active.
        if case .watchConnectivity = connectionMode {
            WatchAppLog.staging("activate: watchConnectivity — přeskočeno start BLE")
            return
        }
        // Opakované activate() (onAppear + scenePhase) nesmí vytvářet nový BLEStateMachine — zdvojení a úniky.
        guard bleStateMachine == nil else {
            WatchAppLog.staging("activate: BLE state machine už běží — přeskočeno")
            return
        }
        attemptBLEConnection()
    }
    
    func deactivate() {
        WatchAppLog.staging("WatchUnifiedSessionModel.deactivate()")
        bleDelayedInitialScan?.cancel()
        bleDelayedInitialScan = nil
        bleBackupIdleScan?.cancel()
        bleBackupIdleScan = nil
        cancelBLECommandAckPending(notifyFailure: true)
        bleStateMachine?.deactivate()
        bleStateMachine = nil
        // Nemažeme WCSession.delegate — session má zůstat aktivní; mazání způsobovalo opakované chyby po návratu do appky.
        iPhonePriorityTimer?.invalidate()
        // connectionMode neresetovat — po návratu z pozadí zachovat volbu BLE vs WC (stejná instance modelu).
    }
    
    // MARK: - Connection Mode Switching
    
    /// Jednorázově nastaví delegáta a zavolá `activate()` — idempotentní, bezpečné volat opakovaně.
    private func ensureWatchConnectivitySession() {
        guard WCSession.isSupported() else {
            WatchAppLog.wc("WCSession není podporováno — přeskočeno")
            return
        }
        let session = WCSession.default
        wcSession = session
        session.delegate = self
        WatchAppLog.wc("WCSession.activate() (stav před: \(session.activationState.rawValue))")
        session.activate()
    }
    
    private func attemptBLEConnection() {
        WatchAppLog.ble("attemptBLEConnection — start BLE state machine (watch)")
        bleDelayedInitialScan?.cancel()
        bleBackupIdleScan?.cancel()
        
        let bleSM = BLEStateMachine(deviceType: .watch)
        bleSM.delegate = self
        bleSM.autoReconnect = true
        self.bleStateMachine = bleSM
        
        connectionMode = .bleDirect
        bleSM.activate()
        WatchAppLog.ble("BLEStateMachine.activate() zavoláno, režim=bleDirect")

        // Start scanning after a brief delay to allow Bluetooth to power on
        let initialScan = DispatchWorkItem { [weak self] in
            WatchAppLog.ble("odložené startScanning (+0.5s)")
            self?.bleStateMachine?.startScanning()
        }
        bleDelayedInitialScan = initialScan
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5, execute: initialScan)
        // Záloha: výjimečně se `poweredOn`/defer neprojeví včas — druhý pokus jen při `.idle`.
        let backupScan = DispatchWorkItem { [weak self] in
            guard let self, case .bleDirect = self.connectionMode, let sm = self.bleStateMachine else { return }
            guard case .idle = sm.connectionState else { return }
            WatchAppLog.ble("záložní startScanning (+2s), stav stále idle")
            sm.startScanning()
        }
        bleBackupIdleScan = backupScan
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0, execute: backupScan)

        // Bez automatického přepnutí na iPhone: stačí přímé BLE k ESP; iPhone režim jen z UI („Přes iPhone“).
        WatchAppLog.staging("BLE režim: bez auto-fallback na WatchConnectivity — spojení jen přes šachovnici")
    }
    
    private func switchToWatchConnectivity() {
        guard WCSession.isSupported() else {
            WatchAppLog.wc("switchToWatchConnectivity — WCSession nepodporováno")
            return
        }

        cancelBLECommandAckPending(notifyFailure: true)
        bleDelayedInitialScan?.cancel()
        bleDelayedInitialScan = nil
        bleBackupIdleScan?.cancel()
        bleBackupIdleScan = nil
        
        bleStateMachine?.deactivate()
        bleStateMachine = nil
        
        ensureWatchConnectivitySession()
        connectionMode = .watchConnectivity
        WatchAppLog.staging("connectionMode = watchConnectivity (reachable=\(WCSession.default.isReachable))")
    }
    
    func switchToBLE() {
        cancelBLECommandAckPending(notifyFailure: true)
        bleDelayedInitialScan?.cancel()
        bleDelayedInitialScan = nil
        bleBackupIdleScan?.cancel()
        bleBackupIdleScan = nil
        bleStateMachine?.deactivate()
        bleStateMachine = nil
        ensureWatchConnectivitySession()
        attemptBLEConnection()
    }
    
    // MARK: - Actions
    
    func startScanning() {
        if case .bleDirect = connectionMode {
            bleStateMachine?.startScanning()
        }
    }
    
    func disconnect() {
        switch connectionMode {
        case .bleDirect:
            bleStateMachine?.disconnect()
        case .watchConnectivity:
            // Cannot disconnect from WatchConnectivity, just switch modes
            switchToBLE()
        default:
            break
        }
    }
    
    func sendCommand(_ json: [String: Any]) -> Bool {
        switch connectionMode {
        case .bleDirect:
            return bleStateMachine?.sendCommand(json) ?? false
        case .watchConnectivity:
            guard let session = wcSession, session.isReachable else { return false }
            session.sendMessage(
                ["action": "boardCommand", "ble": json],
                replyHandler: nil,
                errorHandler: { error in
                    print("[WC] boardCommand send failed: \(error)")
                }
            )
            return true
        default:
            return false
        }
    }
    
    /// Odeslání příkazu na desku s potvrzením (BLE: `cmd_ack` z ESP; iPhone: odpověď z companion).
    func sendBoardCommand(_ json: [String: Any], completion: @escaping (Bool) -> Void) {
        switch connectionMode {
        case .bleDirect:
            sendBLECommandWithAck(json, completion: completion)
        case .watchConnectivity:
            guard let session = wcSession, session.isReachable else {
                completion(false)
                return
            }
            session.sendMessage(
                ["action": "boardCommand", "ble": json],
                replyHandler: { reply in
                    completion(reply["success"] as? Bool ?? false)
                },
                errorHandler: { _ in
                    completion(false)
                }
            )
        default:
            completion(false)
        }
    }
    
    func postNewGame(completion: @escaping (Bool) -> Void) {
        sendBoardCommand(["cmd": "new_game"]) { ok in
            if ok {
                MoveConflictResolver.shared.reset()
            }
            completion(ok)
        }
    }
    
    func postUndo(completion: @escaping (Bool) -> Void) {
        sendBoardCommand(["cmd": "undo"], completion: completion)
    }
    
    func postHintClear(completion: @escaping (Bool) -> Void) {
        sendBoardCommand(["cmd": "hint_clear"], completion: completion)
    }
    
    func postPromotion(choice: String, completion: @escaping (Bool) -> Void) {
        sendBoardCommand([
            "cmd": "promotion",
            "choice": choice.lowercased()
        ], completion: completion)
    }
    
    func postBrightness(percent: Int, completion: @escaping (Bool) -> Void) {
        let p = min(100, max(0, percent))
        sendBoardCommand([
            "cmd": "brightness",
            "percent": p
        ], completion: completion)
    }
    
    /// Režim lampa (RGB) — stejné jako POST `/api/light/command` (přes BLE i přes iPhone bridge).
    func postLightCommand(state: Bool, r: Int, g: Int, b: Int, completion: @escaping (Bool) -> Void) {
        sendBoardCommand([
            "cmd": "light_command",
            "state": state,
            "r": min(255, max(0, r)),
            "g": min(255, max(0, g)),
            "b": min(255, max(0, b)),
        ], completion: completion)
    }
    
    /// Návrat LED do herního režimu — jako POST `/api/light/game_mode`.
    func postLightGameMode(completion: @escaping (Bool) -> Void) {
        sendBoardCommand(["cmd": "light_game_mode"], completion: completion)
    }
    
    /// Execute a move with iPhone priority conflict resolution
    func executeMove(_ move: ChessMove, completion: @escaping (Bool) -> Void) {
        switch connectionMode {
        case .bleDirect:
            // Apply 500ms iPhone priority window
            let ticket = MoveConflictResolver.shared.queueMove(move, from: .watch)
            
            DispatchQueue.main.asyncAfter(deadline: .now() + MoveConflictResolver.iPhonePriorityWindow) { [weak self] in
                guard let self = self else {
                    completion(false)
                    return
                }
                
                if MoveConflictResolver.shared.isValid(ticket) {
                    self.sendBLECommandWithAck([
                        "cmd": "move",
                        "from": move.from,
                        "to": move.to,
                        "promotion": move.promotion as Any,
                    ]) { success in
                        MoveConflictResolver.shared.markExecuted(ticket)
                        completion(success)
                    }
                } else {
                    completion(false)
                    self.triggerHaptic(.failure)
                }
            }
            
        case .watchConnectivity:
            // Route through iPhone - let iPhone handle priority
            let message: [String: Any] = [
                "action": "executeMove",
                "from": move.from,
                "to": move.to,
                "promotion": move.promotion as Any
            ]
            
            if let session = wcSession, session.isReachable {
                session.sendMessage(message, replyHandler: { response in
                    let success = response["success"] as? Bool ?? false
                    completion(success)
                }) { error in
                    print("[WC] Move send failed: \(error)")
                    completion(false)
                }
            } else {
                completion(false)
            }
            
        default:
            completion(false)
        }
    }
    
    // MARK: - BLE cmd_ack (ESP)

    private func cancelBLECommandAckPending(notifyFailure: Bool) {
        guard let p = bleCommandAckPending else { return }
        p.timeout.cancel()
        bleCommandAckPending = nil
        if notifyFailure {
            p.completion(false)
        }
    }

    private func completeBLECommandAckPending(success: Bool) {
        guard let p = bleCommandAckPending else { return }
        p.timeout.cancel()
        bleCommandAckPending = nil
        p.completion(success)
    }

    /// Zápis na GATT CMD; výsledek podle notifikace `cmd_ack` (viz firmware), jinak timeout.
    private func sendBLECommandWithAck(_ json: [String: Any], completion: @escaping (Bool) -> Void) {
        guard let sm = bleStateMachine else {
            completion(false)
            return
        }
        cancelBLECommandAckPending(notifyFailure: true)
        let cmdRaw = json["cmd"] as? String ?? ""
        let expected = cmdRaw.lowercased()
        let written = sm.sendCommand(json)
        guard written else {
            completion(false)
            return
        }
        let timeout = DispatchWorkItem { [weak self] in
            guard let self else { return }
            WatchAppLog.ble("cmd_ack timeout — očekáváno cmd=\"\(expected)\"")
            self.cancelBLECommandAckPending(notifyFailure: true)
        }
        bleCommandAckPending = (expected, completion, timeout)
        DispatchQueue.main.asyncAfter(deadline: .now() + 5.0, execute: timeout)
    }

    /// Request hint from Gemini agent (via iPhone)
    func requestHint(completion: @escaping (String?) -> Void) {
        guard case .watchConnectivity = connectionMode,
              let session = wcSession,
              session.isReachable else {
            completion(nil)
            return
        }
        
        let message = ["action": "requestHint"]
        session.sendMessage(message, replyHandler: { response in
            completion(response["hint"] as? String)
        }) { _ in
            completion(nil)
        }
    }
    
    // MARK: - Haptic Feedback
    
    func triggerHaptic(_ type: WKHapticType) {
        guard hapticEnabled else { return }
        WKInterfaceDevice.current().play(type)
    }
    
    private func checkForMoveHaptic() {
        guard moveCount != lastHapticMoveCount, lastHapticMoveCount >= 0 else {
            lastHapticMoveCount = moveCount
            return
        }
        triggerHaptic(.click)
        lastHapticMoveCount = moveCount
    }
    
    // MARK: - Game State Updates
    
    private func updateGameState(_ state: UnifiedGameState) {
        self.gameState = state
        checkForMoveHaptic()
    }
}

// MARK: - BLEStateMachineDelegate

extension WatchUnifiedSessionModel: BLEStateMachineDelegate {
    func stateMachine(_ stateMachine: BLEStateMachine, didUpdateState state: BLEConnectionState) {
        self.bleConnectionState = state
        WatchAppLog.ble("stav BLE: \(state.description)")

        if !state.isConnected {
            cancelBLECommandAckPending(notifyFailure: true)
        }

        // When BLE connects, try to read network info for IP discovery
        if state.isConnected {
            Task {
                await readNetworkInfoFromBLE(stateMachine)
            }
        }
    }
    
    /// Reads network info from BLE to get IP address for potential WiFi switch
    private func readNetworkInfoFromBLE(_ stateMachine: BLEStateMachine) async {
        do {
            guard let data = try await stateMachine.readNetworkInfo() else {
                print("[BLE] Network info not available")
                return
            }
            
            guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                print("[BLE] Failed to parse network info JSON")
                return
            }
            
            let staIP = json["sta_ip"] as? String
            let apIP = json["ap_ip"] as? String
            let online = json["online"] as? Bool ?? false
            
            print("[BLE] Network info: staIP=\(staIP ?? "nil") apIP=\(apIP ?? "nil") online=\(online)")
            
            // If we have STA IP and are online, we could potentially switch to WiFi
            // via iPhone bridge (future enhancement)
            // For now, just log the available IP addresses
            if let staIP = staIP, !staIP.isEmpty, online {
                print("[BLE] Board available at WiFi: http://\(staIP)")
            }
            if let apIP = apIP, !apIP.isEmpty {
                print("[BLE] Board AP IP: \(apIP)")
            }
        } catch {
            print("[BLE] Failed to read network info: \(error)")
        }
    }
    
    func stateMachine(_ stateMachine: BLEStateMachine, didReceiveCommandAck ack: BLECommandAck) {
        guard let pending = bleCommandAckPending else {
            WatchAppLog.ble("cmd_ack bez čekajícího příkazu ok=\(ack.ok) code=\(ack.code) cmd=\(ack.cmd)")
            return
        }
        let ackCmd = ack.cmd.lowercased()
        let matches = ackCmd.isEmpty || ackCmd == pending.expectedCmd
        guard matches else {
            WatchAppLog.ble("cmd_ack jiný příkaz: přišlo cmd=\"\(ack.cmd)\" očekáno=\"\(pending.expectedCmd)\"")
            return
        }
        if !ack.ok {
            WatchAppLog.ble("cmd_ack selhání code=\(ack.code) msg=\(ack.message)")
        } else {
            WatchAppLog.ble("cmd_ack OK cmd=\(ack.cmd)")
        }
        completeBLECommandAckPending(success: ack.ok)
    }

    func stateMachine(_ stateMachine: BLEStateMachine, didReceiveGameState gameState: UnifiedGameState) {
        updateGameState(gameState)
    }
    
    func stateMachine(_ stateMachine: BLEStateMachine, didEncounterError error: Error) {
        print("[BLE] Error: \(error)")
        // Optionally trigger error haptic if enabled
        if hapticEnabled {
            triggerHaptic(.notification)
        }
    }
}

// MARK: - WCSessionDelegate

extension WatchUnifiedSessionModel: WCSessionDelegate {
    nonisolated func session(
        _ session: WCSession,
        activationDidCompleteWith activationState: WCSessionActivationState,
        error: Error?
    ) {
        Task { @MainActor in
            if let error = error {
                WatchAppLog.wc("activationDidComplete chyba: \(error.localizedDescription)")
            } else if activationState == .activated {
                WatchAppLog.wc("WCSession aktivováno, isReachable=\(session.isReachable), contextKeys=\(session.receivedApplicationContext.keys.count)")
                let ctx = session.receivedApplicationContext
                if ctx.isEmpty {
                    WatchAppLog.wc(
                        "Kontext z iPhonu zatím prázdný — hláška „Application context data is nil“ v konzoli často pochází od systému, než companion pošle první updateApplicationContext. Appka na hodinkách už běží."
                    )
                } else {
                    self.updateGameState(UnifiedGameState(from: ctx))
                }
            } else {
                WatchAppLog.wc("activationDidComplete stav=\(activationState.rawValue)")
            }
        }
    }
    
    nonisolated func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        Task { @MainActor in
            // Handle messages from iPhone (e.g., game state updates)
            if message.keys.contains("fen") {
                let state = UnifiedGameState(from: message)
                self.updateGameState(state)
            }
        }
    }
    
    nonisolated func session(
        _ session: WCSession,
        didReceiveApplicationContext applicationContext: [String: Any]
    ) {
        Task { @MainActor in
            let state = UnifiedGameState(from: applicationContext)
            self.updateGameState(state)
        }
    }
    
    nonisolated func sessionReachabilityDidChange(_ session: WCSession) {
        Task { @MainActor in
            WatchAppLog.wc("Reachability změna: isReachable=\(session.isReachable)")
            // If iPhone becomes unreachable and we're in WC mode, could switch to BLE
            if !session.isReachable, case .watchConnectivity = self.connectionMode {
                // Optionally auto-switch to BLE after delay
                // self.switchToBLE()
            }
        }
    }
}

// MARK: - MoveConflictResolverDelegate

extension WatchUnifiedSessionModel: MoveConflictResolverDelegate {
    func moveTicket(_ ticket: MoveTicket, didChangeValidity isValid: Bool) {
        if !isValid {
            // Move was invalidated by iPhone priority
            triggerHaptic(.failure)
        }
    }
    
    func moveTicketDidExpire(_ ticket: MoveTicket) {
        // Handle expired move ticket
    }
}
