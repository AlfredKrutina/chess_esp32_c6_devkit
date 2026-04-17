//
//  BLEStateMachine.swift
//  CZECHMATEShared
//
//  Unified BLE connection state management for iPhone and Watch.
//

import Foundation
import CoreBluetooth
import Observation

#if DEBUG
/// Staging: sledování průběhu na Watch/iPhone v konzoli (viz `context/WATCH_BLE_DEEP_ANALYSIS_AND_PLAN.md` §6).
private func bleStateMachineTrace(_ message: String) {
    print("[BLEStateMachine][staging] \(message)")
}
#else
@inline(__always)
private func bleStateMachineTrace(_ message: String) {}
#endif

/// BLE connection states
public enum BLEConnectionState: Equatable, Sendable {
    case idle
    case bluetoothOff
    case unauthorized
    case scanning
    case discovered(peripheralName: String?)
    case connecting
    case discoveringServices
    case connected
    case reconnecting
    case error(String)
    
    public var description: String {
        switch self {
        case .idle:
            return "Připraveno"
        case .bluetoothOff:
            return "Bluetooth vypnutý"
        case .unauthorized:
            return "Chybí oprávnění"
        case .scanning:
            return "Hledám šachovnici..."
        case .discovered(let name):
            return "Nalezeno: \(name ?? "CZECHMATE")"
        case .connecting:
            return "Připojování..."
        case .discoveringServices:
            return "Zjišťuji služby..."
        case .connected:
            return "Připojeno"
        case .reconnecting:
            return "Obnovuji připojení..."
        case .error(let msg):
            return "Chyba: \(msg)"
        }
    }
    
    public var isConnected: Bool {
        if case .connected = self { return true }
        return false
    }
    
    public var isConnecting: Bool {
        switch self {
        case .scanning, .discovered, .connecting, .discoveringServices, .reconnecting:
            return true
        default:
            return false
        }
    }
}

/// Odpověď desky na GATT zápis příkazu (`channel` == `cmd_ack` ve firmware).
public struct BLECommandAck: Sendable, Equatable {
    public let ok: Bool
    public let code: String
    public let cmd: String
    public let message: String

    public init(ok: Bool, code: String, cmd: String, message: String) {
        self.ok = ok
        self.code = code
        self.cmd = cmd
        self.message = message
    }

    /// Parsuje tělo notifikace z charakteristiky `A0B40005…`.
    public static func parse(data: Data) -> BLECommandAck? {
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let ch = obj["channel"] as? String, ch == "cmd_ack" else { return nil }
        let ok = (obj["ok"] as? Bool) ?? false
        let code = (obj["code"] as? String) ?? ""
        let cmd = (obj["cmd"] as? String) ?? ""
        let message = (obj["message"] as? String) ?? ""
        return BLECommandAck(ok: ok, code: code, cmd: cmd, message: message)
    }
}

/// Connection mode for Watch (BLE direct vs iPhone bridge)
public enum WatchConnectionMode: Equatable, Sendable {
    case determining
    case bleDirect  // Watch connects directly to ESP32
    case watchConnectivity  // Watch connects through iPhone
    
    public var description: String {
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

/// Unified game state container (shared across devices)
public struct UnifiedGameState: Equatable, Sendable {
    public var moveCount: Int = 0
    public var currentPlayer: String = "—"
    public var fen: String = ""
    public var gameState: String = ""
    public var isTimerRunning: Bool = false
    public var whiteTime: UInt32?
    public var blackTime: UInt32?
    public var lastFrom: String?
    public var lastTo: String?
    public var inCheck: Bool = false
    public var gameEnded: Bool = false
    public var lastUpdate: Date?
    public var advantageSummary: String = ""
    public var clockSyncAt: Date?
    
    /// Jas LED podsvětlení (0–100), ze snapshot `status.brightness`.
    public var brightness: Int?
    /// `game` | `lamp` (stejné jako ve firmware `light_mode`).
    public var lightMode: String?
    public var lightState: Bool?
    public var lightR: Int?
    public var lightG: Int?
    public var lightB: Int?
    public var autoLampTimeoutSec: Int?
    
    /// Kompaktní souhrn konce partie (iPhone → Watch / volitelně z kontextu).
    public var watchEndgameHeadline: String?
    public var watchEndgameScoreLine: String?
    public var watchEndgameReason: String?
    public var watchEndgameStats: String?
    
    public init() {}
    
    public init(from json: [String: Any]) {
        self.init()
        if let st = json["status"] as? [String: Any] {
            mergeFromFirmwareStatus(st)
        }
        
        if let mc = json["moveCount"] as? Int {
            moveCount = mc
        } else if let mc = json["moveCount"] as? Int32 {
            moveCount = Int(mc)
        }
        
        if let cp = json["currentPlayer"] as? String { currentPlayer = cp }
        if let f = json["fen"] as? String { fen = f }
        if let gs = json["gameState"] as? String { gameState = gs }
        if let tr = json["isTimerRunning"] as? Bool { isTimerRunning = tr }
        if let w = json["whiteTime"] as? UInt32 { whiteTime = w }
        if let b = json["blackTime"] as? UInt32 { blackTime = b }
        if let w = json["whiteTime"] as? Int { whiteTime = UInt32(clamping: w) }
        if let b = json["blackTime"] as? Int { blackTime = UInt32(clamping: b) }
        
        lastFrom = json["lastMoveFrom"] as? String
        lastTo = json["lastMoveTo"] as? String
        
        if let ic = json["inCheck"] as? Bool { inCheck = ic }
        if let ge = json["gameEnded"] as? Bool { gameEnded = ge }
        
        if let ts = json["ts"] as? TimeInterval {
            lastUpdate = Date(timeIntervalSince1970: ts)
        }
        
        if let adv = json["advantageSummary"] as? String {
            advantageSummary = adv
        }
        
        if let c = json["clockSyncAt"] as? TimeInterval {
            clockSyncAt = Date(timeIntervalSince1970: c)
        } else if let ts = json["ts"] as? TimeInterval {
            clockSyncAt = Date(timeIntervalSince1970: ts)
        }
        
        if let b = json["brightness"] as? Int { brightness = b }
        if let m = json["lightMode"] as? String { lightMode = m }
        if let m = json["light_mode"] as? String, lightMode == nil { lightMode = m }
        if let s = json["lightState"] as? Bool { lightState = s }
        if let s = json["light_state"] as? Bool, lightState == nil { lightState = s }
        if let v = json["lightR"] as? Int { lightR = v }
        if let v = json["light_r"] as? Int, lightR == nil { lightR = v }
        if let v = json["lightG"] as? Int { lightG = v }
        if let v = json["light_g"] as? Int, lightG == nil { lightG = v }
        if let v = json["lightB"] as? Int { lightB = v }
        if let v = json["light_b"] as? Int, lightB == nil { lightB = v }
        if let v = json["autoLampTimeoutSec"] as? Int { autoLampTimeoutSec = v }
        if let v = json["auto_lamp_timeout_sec"] as? Int, autoLampTimeoutSec == nil { autoLampTimeoutSec = v }
        
        watchEndgameHeadline = json["watchEndgameHeadline"] as? String
        watchEndgameScoreLine = json["watchEndgameScoreLine"] as? String
        watchEndgameReason = json["watchEndgameReason"] as? String
        watchEndgameStats = json["watchEndgameStats"] as? String
    }
    
    /// Vnořený `status` z BLE / HTTP snapshotu (snake_case jako ve firmware).
    private mutating func mergeFromFirmwareStatus(_ st: [String: Any]) {
        if let v = st["move_count"] as? Int {
            moveCount = v
        } else if let v = st["move_count"] as? UInt32 {
            moveCount = Int(v)
        }
        if let v = st["current_player"] as? String { currentPlayer = v }
        if let v = st["game_state"] as? String {
            gameState = v
            let s = v.lowercased()
            isTimerRunning = s == "active" || s == "playing"
        }
        if let v = st["white_time"] as? UInt32 { whiteTime = v }
        if let v = st["black_time"] as? UInt32 { blackTime = v }
        if let v = st["white_time"] as? Int { whiteTime = UInt32(clamping: v) }
        if let v = st["black_time"] as? Int { blackTime = UInt32(clamping: v) }
        if let v = st["in_check"] as? Bool { inCheck = v }
        if let ge = st["game_end"] as? [String: Any], let ended = ge["ended"] as? Bool {
            gameEnded = ended
        }
        if let v = st["brightness"] as? Int { brightness = v }
        if let v = st["light_mode"] as? String { lightMode = v }
        if let v = st["light_state"] as? Bool { lightState = v }
        if let v = st["light_r"] as? Int { lightR = v }
        if let v = st["light_g"] as? Int { lightG = v }
        if let v = st["light_b"] as? Int { lightB = v }
        if let v = st["auto_lamp_timeout_sec"] as? Int { autoLampTimeoutSec = v }
    }
}

/// Protocol for BLE state machine delegate
public protocol BLEStateMachineDelegate: AnyObject {
    func stateMachine(_ stateMachine: BLEStateMachine, didUpdateState state: BLEConnectionState)
    func stateMachine(_ stateMachine: BLEStateMachine, didReceiveGameState gameState: UnifiedGameState)
    func stateMachine(_ stateMachine: BLEStateMachine, didEncounterError error: Error)
    /// Potvrzení příkazu z ESP (`cmd_ack` notify) — výchozí implementace je prázdná.
    func stateMachine(_ stateMachine: BLEStateMachine, didReceiveCommandAck ack: BLECommandAck)
}

public extension BLEStateMachineDelegate {
    func stateMachine(_ stateMachine: BLEStateMachine, didReceiveCommandAck ack: BLECommandAck) {}
}

/// Unified BLE state machine for both iPhone and Watch
@Observable
@MainActor
public final class BLEStateMachine: NSObject {
    /// Current connection state
    public private(set) var connectionState: BLEConnectionState = .idle
    
    /// Current game state (updated from BLE notifications)
    public private(set) var gameState: UnifiedGameState = UnifiedGameState()
    
    /// Delegate for events
    public weak var delegate: BLEStateMachineDelegate?
    
    /// BLE UUIDs (must match ESP32 GATT) — `nonisolated`: delegate CoreBluetooth běží na `bleCallbackQueue`.
    public nonisolated let serviceUUID: CBUUID
    public nonisolated let snapshotCharUUID: CBUUID
    public nonisolated let commandCharUUID: CBUUID
    public nonisolated let networkCharUUID: CBUUID
    public nonisolated let commandAckCharUUID: CBUUID
    
    /// CoreBluetooth reference — dotýkat se jen z `bleCallbackQueue` (kromě `nil` při `deactivate` uvnitř sync).
    nonisolated(unsafe) internal var centralManager: CBCentralManager?
    nonisolated(unsafe) internal var peripheral: CBPeripheral?
    nonisolated(unsafe) internal var chessService: CBService?
    nonisolated(unsafe) internal var snapshotCharacteristic: CBCharacteristic?
    nonisolated(unsafe) internal var commandCharacteristic: CBCharacteristic?
    nonisolated(unsafe) internal var networkCharacteristic: CBCharacteristic?
    nonisolated(unsafe) internal var commandAckCharacteristic: CBCharacteristic?
    
    /// Continuation for network info read (nastavení + resume na `bleCallbackQueue`)
    nonisolated(unsafe) internal var readNetworkContinuation: CheckedContinuation<Data?, Error>?
    
    /// Reconnection settings
    public var autoReconnect: Bool = true
    public var maxReconnectionAttempts: Int = 3
    private var reconnectionAttempts: Int = 0
    private var lastConnectedPeripheralIdentifier: UUID?
    
    /// Device type identification
    public let deviceType: DeviceType
    
    /// Všechna volání `CBCentralManager` / `CBPeripheral` musí běžet na této frontě — **ne** na `.main`,
    /// jinak watchOS (a často iPhone) při skenu zablokuje UI a SwiftUI se „nenačte“.
    private let bleCallbackQueue = DispatchQueue(label: "com.czechmate.ble-cb.\(UUID().uuidString)", qos: .userInitiated)
    
    /// `startScanning` narazil na `.unknown` / `.resetting` — po `centralManagerDidUpdateState(.poweredOn)` znovu spustit sken (jen z `bleCallbackQueue`).
    nonisolated(unsafe) private var deferStartScanUntilPoweredOn: Bool = false
    
    public init(
        deviceType: DeviceType,
        serviceUUID: CBUUID = CBUUID(string: "A0B40001-9267-4AB6-BDCC-E8336F8A8D9E"),
        snapshotCharUUID: CBUUID = CBUUID(string: "A0B40002-9267-4AB6-BDCC-E8336F8A8D9E"),
        commandCharUUID: CBUUID = CBUUID(string: "A0B40003-9267-4AB6-BDCC-E8336F8A8D9E"),
        networkCharUUID: CBUUID = CBUUID(string: "A0B40004-9267-4AB6-BDCC-E8336F8A8D9E"),
        commandAckCharUUID: CBUUID = CBUUID(string: "A0B40005-9267-4AB6-BDCC-E8336F8A8D9E")
    ) {
        self.deviceType = deviceType
        self.serviceUUID = serviceUUID
        self.snapshotCharUUID = snapshotCharUUID
        self.commandCharUUID = commandCharUUID
        self.networkCharUUID = networkCharUUID
        self.commandAckCharUUID = commandAckCharUUID
        super.init()
    }
    
    // MARK: - Connection Lifecycle
    
    public func activate() {
        // Sync: `WatchUnifiedSessionModel` volá `startScanning()` ~0,5 s po `activate()` — při čistém `async`
        // by manager ještě neexistoval → „Manager not initialized“ / prázdný stav.
        bleCallbackQueue.sync { [weak self] in
            guard let self else { return }
            guard self.centralManager == nil else { return }
            let cm = CBCentralManager(delegate: self, queue: self.bleCallbackQueue)
            self.centralManager = cm
            bleStateMachineTrace("activate: manager vytvořen, device=\(self.deviceType.rawValue), CBState=\(cm.state.rawValue)")
        }
    }
    
    public func deactivate() {
        bleCallbackQueue.sync { [weak self] in
            guard let self else { return }
            self.centralManager?.stopScan()
            if let p = self.peripheral {
                self.centralManager?.cancelPeripheralConnection(p)
            }
            self.clearBleReferences()
            self.centralManager = nil
            self.deferStartScanUntilPoweredOn = false
            bleStateMachineTrace("deactivate: manager uvolněn")
        }
        connectionState = .idle
    }
    
    public func disconnect() {
        bleCallbackQueue.async { [weak self] in
            guard let self else { return }
            if let p = self.peripheral {
                self.centralManager?.cancelPeripheralConnection(p)
            }
            self.clearBleReferences()
            Task { @MainActor [weak self] in
                guard let self else { return }
                // Po zrušení spojení / skenu vrať UI do idle (ne jen .connected / .connecting).
                if self.connectionState != .bluetoothOff, self.connectionState != .unauthorized {
                    self.connectionState = .idle
                }
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
        }
    }
    
    /// Pouze reference na GATT — volat z `bleCallbackQueue`.
    private nonisolated func clearBleReferences() {
        if let cont = readNetworkContinuation {
            readNetworkContinuation = nil
            cont.resume(throwing: CancellationError())
        }
        peripheral = nil
        chessService = nil
        snapshotCharacteristic = nil
        commandCharacteristic = nil
        networkCharacteristic = nil
        commandAckCharacteristic = nil
    }
    
    private func cleanupConnection() {
        bleCallbackQueue.sync { [weak self] in
            self?.clearBleReferences()
        }
        if connectionState.isConnected || connectionState.isConnecting {
            connectionState = .idle
        }
    }
    
    // MARK: - Scanning & Connection
    
    public func startScanning() {
        bleCallbackQueue.async { [weak self] in
            guard let self else { return }
            bleStateMachineTrace("startScanning: vstup (device=\(self.deviceType.rawValue))")
            guard let cm = self.centralManager else {
                bleStateMachineTrace("startScanning: ABORT — centralManager nil")
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .error("Manager not initialized")
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                return
            }
            let power = cm.state
            bleStateMachineTrace("startScanning: CBState=\(power.rawValue) (0=unknown … 5=poweredOn)")
            // Po `CBCentralManager(...)` je stav často `.unknown` / `.resetting` — to NENÍ „BT vypnutý“.
            // Původní mapování na `.bluetoothOff` ničilo první sken (+0,5 s) a UI ukazovalo falešnou chybu.
            switch power {
            case .poweredOn:
                break
            case .unauthorized:
                bleStateMachineTrace("startScanning: konec — unauthorized")
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .unauthorized
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                return
            case .poweredOff:
                bleStateMachineTrace("startScanning: konec — poweredOff")
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .bluetoothOff
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                return
            case .unsupported:
                bleStateMachineTrace("startScanning: konec — unsupported")
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .error("Bluetooth není na tomto zařízení k dispozici")
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                return
            case .unknown, .resetting:
                self.deferStartScanUntilPoweredOn = true
                bleStateMachineTrace("startScanning: odloženo do poweredOn (deferStartScanUntilPoweredOn=true)")
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .idle
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                return
            @unknown default:
                bleStateMachineTrace("startScanning: neznámý CBState — UI idle, return")
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .idle
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                return
            }
            bleStateMachineTrace("startScanning: scanForPeripherals(serviceUUID)")
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.connectionState = .scanning
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
            cm.scanForPeripherals(withServices: [self.serviceUUID], options: [
                CBCentralManagerScanOptionAllowDuplicatesKey: false,
            ])
        }
    }
    
    public func stopScanning() {
        bleCallbackQueue.async { [weak self] in
            guard let self else { return }
            // Uživatel / UI sken ukončil — neobnovovat automaticky po náhodném `poweredOn`.
            self.deferStartScanUntilPoweredOn = false
            self.centralManager?.stopScan()
            Task { @MainActor [weak self] in
                guard let self else { return }
                if case .scanning = self.connectionState {
                    self.connectionState = .idle
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
            }
        }
    }
    
    public func connectToPeripheral(_ peripheral: CBPeripheral) {
        bleCallbackQueue.async { [weak self] in
            guard let self else { return }
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.connectionState = .connecting
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
            self.centralManager?.connect(peripheral, options: nil)
        }
    }
    
    // MARK: - Data Operations
    
    public func sendCommand(_ json: [String: Any]) -> Bool {
        var ok = false
        var caught: Error?
        bleCallbackQueue.sync { [weak self] in
            guard let self else { return }
            guard let peripheral = self.peripheral,
                  let characteristic = self.commandCharacteristic,
                  peripheral.state == .connected else {
                return
            }
            do {
                let data = try JSONSerialization.data(withJSONObject: json)
                peripheral.writeValue(data, for: characteristic, type: .withResponse)
                ok = true
            } catch {
                caught = error
            }
        }
        if let caught {
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.delegate?.stateMachine(self, didEncounterError: caught)
            }
        }
        return ok
    }
    
    // MARK: - Reconnection
    
    internal func attemptReconnection() {
        guard autoReconnect,
              reconnectionAttempts < maxReconnectionAttempts,
              let identifier = lastConnectedPeripheralIdentifier else {
            connectionState = .idle
            return
        }
        
        reconnectionAttempts += 1
        connectionState = .reconnecting
        delegate?.stateMachine(self, didUpdateState: connectionState)
        
        let uuid = identifier
        bleCallbackQueue.async { [weak self] in
            guard let self else { return }
            let peripherals = self.centralManager?.retrievePeripherals(withIdentifiers: [uuid])
            if let p = peripherals?.first {
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.connectionState = .connecting
                    self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
                }
                self.centralManager?.connect(p, options: nil)
            } else {
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    self.startScanning()
                }
            }
        }
    }
    
    // MARK: - Game State Processing
    
    internal func processSnapshotData(_ data: Data) {
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return
        }
        
        let newState = UnifiedGameState(from: json)
        gameState = newState
        delegate?.stateMachine(self, didReceiveGameState: newState)
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEStateMachine: CBCentralManagerDelegate {
    nonisolated public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOff, .unauthorized, .unsupported:
            deferStartScanUntilPoweredOn = false
        default:
            break
        }
        let runDeferredScan: Bool
        if central.state == .poweredOn {
            runDeferredScan = deferStartScanUntilPoweredOn
            deferStartScanUntilPoweredOn = false
        } else {
            runDeferredScan = false
        }
        bleStateMachineTrace("centralDidUpdateState: CBState=\(central.state.rawValue), runDeferredScan=\(runDeferredScan)")
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                if self.connectionState == .bluetoothOff || self.connectionState == .unauthorized {
                    self.connectionState = .idle
                }
            case .unauthorized:
                self.connectionState = .unauthorized
            case .poweredOff:
                self.connectionState = .bluetoothOff
                self.cleanupConnection()
            default:
                break
            }
            self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
        }
        if runDeferredScan {
            bleStateMachineTrace("centralDidUpdateState: poweredOn → opakuji startScanning (defer)")
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.startScanning()
            }
        }
    }
    
    nonisolated public func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        central.stopScan()
        self.peripheral = peripheral
        let name = peripheral.name
        bleStateMachineTrace("didDiscover: \(name ?? "(bez jména)") id=\(peripheral.identifier.uuidString.prefix(8))…")
        Task { @MainActor in
            self.connectionState = .discovered(peripheralName: name)
            self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            self.connectionState = .connecting
            self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
        }
        central.connect(peripheral, options: nil)
    }
    
    nonisolated public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        let savedId = peripheral.identifier
        Task { @MainActor in
            self.connectionState = .discoveringServices
            self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            self.reconnectionAttempts = 0
            self.lastConnectedPeripheralIdentifier = savedId
        }
        peripheral.discoverServices([serviceUUID])
    }
    
    nonisolated public func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            self.connectionState = .error("Connection failed")
            self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            self.attemptReconnection()
        }
    }
    
    nonisolated public func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            self.cleanupConnection()
            if self.autoReconnect {
                self.attemptReconnection()
            } else {
                self.connectionState = .idle
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEStateMachine: CBPeripheralDelegate {
    nonisolated public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            Task { @MainActor in
                self.connectionState = .error("Service discovery failed")
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
            return
        }
        guard let service = peripheral.services?.first(where: { $0.uuid == self.serviceUUID }) else {
            Task { @MainActor in
                self.connectionState = .error("CZECHMATE service not found")
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
            return
        }
        chessService = service
        peripheral.discoverCharacteristics(
            [snapshotCharUUID, commandCharUUID, networkCharUUID, commandAckCharUUID],
            for: service
        )
    }
    
    nonisolated public func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        if error != nil {
            Task { @MainActor in
                self.connectionState = .error("Characteristic discovery failed")
                self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
            }
            return
        }
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == snapshotCharUUID {
                snapshotCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            } else if characteristic.uuid == commandCharUUID {
                commandCharacteristic = characteristic
            } else if characteristic.uuid == networkCharUUID {
                networkCharacteristic = characteristic
            } else if characteristic.uuid == commandAckCharUUID {
                commandAckCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                bleStateMachineTrace("cmd_ack: notify zapnuto (potvrzení příkazů z desky)")
            }
        }
        Task { @MainActor in
            self.connectionState = .connected
            self.delegate?.stateMachine(self, didUpdateState: self.connectionState)
        }
    }
    
    nonisolated public func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if characteristic.uuid == networkCharUUID {
            if let cont = readNetworkContinuation {
                readNetworkContinuation = nil
                if let err = error {
                    cont.resume(throwing: err)
                } else {
                    cont.resume(returning: characteristic.value)
                }
            }
            return
        }
        if characteristic.uuid == commandAckCharUUID, error == nil, let data = characteristic.value,
           let ack = BLECommandAck.parse(data: data) {
            Task { @MainActor in
                self.delegate?.stateMachine(self, didReceiveCommandAck: ack)
            }
            return
        }
        guard error == nil,
              characteristic.uuid == snapshotCharUUID,
              let data = characteristic.value else { return }
        Task { @MainActor in
            self.processSnapshotData(data)
        }
    }
    
    nonisolated public func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if let error = error {
            Task { @MainActor in
                self.delegate?.stateMachine(self, didEncounterError: error)
            }
        }
    }
}

// MARK: - Network Info

extension BLEStateMachine {
    /// Přečte network info z BLE (IP adresa, SSID, online status).
    /// Vrací JSON data: {"sta_connected":true,"sta_ip":"192.168.1.45","sta_ssid":"WiFi","ap_ip":"192.168.4.1","online":true}
    public func readNetworkInfo() async throws -> Data? {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data?, Error>) in
            bleCallbackQueue.async { [weak self] in
                guard let self else {
                    continuation.resume(throwing: CancellationError())
                    return
                }
                guard let peripheral = self.peripheral,
                      let characteristic = self.networkCharacteristic,
                      characteristic.properties.contains(.read) else {
                    continuation.resume(returning: nil)
                    return
                }
                self.readNetworkContinuation = continuation
                peripheral.readValue(for: characteristic)
            }
        }
    }
}

// MARK: - BLE-Only Game Commands (Watch)
extension BLEStateMachine {
    /// Send chess move via BLE
    public func sendMove(from: String, to: String, promotion: String? = nil) -> Bool {
        var json: [String: Any] = [
            "cmd": "move",
            "from": from.lowercased(),
            "to": to.lowercased(),
        ]
        if let promo = promotion {
            json["promotion"] = promo.lowercased()
        }
        return writeCommandJSON(json)
    }

    /// Start new game via BLE
    public func sendNewGame() -> Bool {
        writeCommandJSON(["cmd": "new_game"])
    }

    /// Undo last move via BLE
    public func sendUndo() -> Bool {
        writeCommandJSON(["cmd": "undo"])
    }

    /// Send promotion choice via BLE
    public func sendPromotion(choice: String) -> Bool {
        writeCommandJSON([
            "cmd": "promotion",
            "choice": choice.lowercased(),
        ])
    }

    private func writeCommandJSON(_ json: [String: Any]) -> Bool {
        var ok = false
        var caught: Error?
        bleCallbackQueue.sync { [weak self] in
            guard let self else { return }
            guard let peripheral = self.peripheral,
                  let characteristic = self.commandCharacteristic,
                  peripheral.state == .connected else {
                return
            }
            do {
                let data = try JSONSerialization.data(withJSONObject: json)
                peripheral.writeValue(data, for: characteristic, type: .withResponse)
                ok = true
            } catch {
                caught = error
            }
        }
        if let caught {
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.delegate?.stateMachine(self, didEncounterError: caught)
            }
        }
        return ok
    }
}
