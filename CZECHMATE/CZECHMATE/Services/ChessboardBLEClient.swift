//
//  ChessboardBLEClient.swift
//  CZECHMATE — CoreBluetooth centrála: CZECHMATE GATT (viz ESP ble_task.c).
//

import Foundation

enum BLETransportError: Error, LocalizedError {
    case bluetoothUnavailable
    case notConnected
    case characteristicMissing
    case notifySubscriptionFailed
    case decodeFailed
    case unsupported

    var errorDescription: String? {
        switch self {
        case .bluetoothUnavailable: return "Bluetooth není k dispozici nebo je vypnutý."
        case .notConnected: return "Šachovnice přes Bluetooth není připojená."
        case .characteristicMissing: return "Na desce chybí očekávaná BLE služba."
        case .notifySubscriptionFailed: return "Nepodařilo se zapnout notifikace stavu hry přes Bluetooth."
        case .decodeFailed: return "Nepodařilo se přečíst data ze šachovnice (BLE)."
        case .unsupported: return "BLE není na této platformě podporováno."
        }
    }
}

#if canImport(CoreBluetooth)
@preconcurrency import CoreBluetooth

/// Centrální role: scan → connect → GATT (služba CZECHMATE).
/// Sdílená instance — aby výběr ze `BoardDiscoveryView` a připojení v `BoardConnectionStore` viděly stejné nalezené periférie.
final class ChessboardBLEClient: NSObject, @unchecked Sendable {
    static let shared = ChessboardBLEClient()

    private var central: CBCentralManager?
    private let queue = DispatchQueue(label: "czechmate.ble")

    private var scanHandler: (@MainActor (DiscoveredBoardDevice) -> Void)?
    private var activePeripheral: CBPeripheral?
    private var snapshotCharacteristic: CBCharacteristic?
    private var commandCharacteristic: CBCharacteristic?

    private var snapshotNotifyHandler: (@Sendable (Data) -> Void)?
    /// Voláno při BLE odpojení — signalizuje ukončení AsyncStream v BLEBoardTransport.
    private var snapshotDisconnectHandler: (@Sendable () -> Void)?
    private var jsonReassembly = Data()
    private var jsonChunkPart: Int = 0
    private var jsonChunkTotal: Int = 0

    private var readDataContinuation: CheckedContinuation<Data, Error>?
    /// Čekání na dokončení `setNotifyValue(true)` pro snapshot (jinak může UI „viset“ bez dat).
    private var notifyEnableContinuation: CheckedContinuation<Void, Error>?
    /// Po `discoverServices([UUID])` bez výsledku jednou zkusíme `discoverServices(nil)` (iOS / GATT cache).
    private var pendingDiscoverAllServices = false
    /// Opakované pokusy při 0 službách (ESP32 Wi‑Fi + BLE na jednom rádiu — ATT často odpovídá až po prodlevě).
    private var serviceDiscoveryEmptyAttempts = 0
    private let serviceDiscoveryDelaysSec: [TimeInterval] = [1.25, 0.6, 1.2, 1.8]
    private var discoveredPeripherals: [UUID: CBPeripheral] = [:]

    override init() {
        super.init()
    }

    func prepareCentral() {
        if central == nil {
            central = CBCentralManager(delegate: self, queue: queue)
        }
    }

    var isBluetoothReady: Bool {
        central?.state == .poweredOn
    }

    /// Spustí sken (povol duplicity pro RSSI).
    /// Pozn.: Po vytvoření `CBCentralManager` je stav nejdřív `.unknown` — sken se spustí až v `centralManagerDidUpdateState` při `.poweredOn` (viz `restartBLEScanIfNeeded`).
    func startScanning(onFound: @escaping @MainActor (DiscoveredBoardDevice) -> Void) {
        prepareCentral()
        scanHandler = onFound
        restartBLEScanIfNeeded()
    }

    /// Volat z delegate při přechodu na `poweredOn` — obnoví sken, pokud už máme handler (dříve selhalo tiše, když uživatel otevřel seznam dřív než BT naběhlo).
    private func restartBLEScanIfNeeded() {
        guard let c = central, c.state == .poweredOn, scanHandler != nil else { return }
        c.scanForPeripherals(
            withServices: [CZECHMATEBLEUUIDs.service],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: true]
        )
        AppDebugLog.staging("BLE: scanForPeripherals (CZECHMATE service UUID)")
    }

    func stopScanning() {
        central?.stopScan()
        scanHandler = nil
    }

    func connect(to device: DiscoveredBoardDevice) async throws {
        prepareCentral()
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            queue.async { [weak self] in
                guard let self else {
                    cont.resume(throwing: BLETransportError.bluetoothUnavailable)
                    return
                }
                guard let c = self.central else {
                    cont.resume(throwing: BLETransportError.bluetoothUnavailable)
                    return
                }
                guard c.state == .poweredOn else {
                    AppDebugLog.staging("BLE connect: central state=\(c.state.rawValue) (očekáváno 5=poweredOn)")
                    cont.resume(throwing: BLETransportError.bluetoothUnavailable)
                    return
                }
                guard let u = UUID(uuidString: device.id) else {
                    cont.resume(throwing: BLETransportError.notConnected)
                    return
                }
                if let stale = self.connectContinuation {
                    self.connectContinuation = nil
                    stale.resume(throwing: BLETransportError.notConnected)
                }
                let p: CBPeripheral
                if let known = self.discoveredPeripherals[u] {
                    p = known
                } else if let r = c.retrievePeripherals(withIdentifiers: [u]).first {
                    p = r
                } else {
                    cont.resume(throwing: BLETransportError.notConnected)
                    return
                }
                p.delegate = self
                self.activePeripheral = p
                self.pendingDiscoverAllServices = false
                self.serviceDiscoveryEmptyAttempts = 0
                self.connectContinuation = cont
                c.connect(p, options: nil)
            }
        }
    }

    private var connectContinuation: CheckedContinuation<Void, Error>?

    func startSnapshotNotifications(handler: @escaping @Sendable (Data) -> Void, disconnectHandler: @escaping @Sendable () -> Void) async throws {
        snapshotNotifyHandler = handler
        snapshotDisconnectHandler = disconnectHandler
        guard activePeripheral != nil, snapshotCharacteristic != nil else {
            throw BLETransportError.notConnected
        }
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            queue.async { [weak self] in
                self?.enqueueSnapshotNotifyEnable(continuation: cont)
            }
        }
        queue.async { [weak self] in
            guard let self, let p = self.activePeripheral, self.snapshotCharacteristic != nil else { return }
            let w = p.maximumWriteValueLength(for: .withResponse)
            let wo = p.maximumWriteValueLength(for: .withoutResponse)
            AppDebugLog.staging(
                "BLE: maxWrite withResponse=\(w) B, withoutResponse=\(wo) B (odvozeno od ATT MTU po subscribe)"
            )
        }
    }

    func stopSnapshotNotifications() {
        if let c = notifyEnableContinuation {
            notifyEnableContinuation = nil
            c.resume(throwing: BLETransportError.notConnected)
        }
        if let p = activePeripheral, let ch = snapshotCharacteristic {
            p.setNotifyValue(false, for: ch)
        }
        snapshotNotifyHandler = nil
        snapshotDisconnectHandler = nil
    }

    func disconnect() async {
        stopSnapshotNotifications()
        if let p = activePeripheral, let c = central {
            c.cancelPeripheralConnection(p)
        }
        activePeripheral = nil
        snapshotCharacteristic = nil
        commandCharacteristic = nil
    }

    func writeCommandJSON(_ obj: [String: Any]) async throws {
        guard commandCharacteristic != nil, activePeripheral != nil else {
            throw BLETransportError.notConnected
        }
        let data = try JSONSerialization.data(withJSONObject: obj)
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            queue.async { [weak self] in
                self?.enqueueWriteCommand(data: data, continuation: cont)
            }
        }
    }

    private func enqueueWriteCommand(data: Data, continuation: CheckedContinuation<Void, Error>) {
        guard let ch = commandCharacteristic, let p = activePeripheral else {
            continuation.resume(throwing: BLETransportError.notConnected)
            return
        }
        if let stale = writeContinuation {
            writeContinuation = nil
            stale.resume(throwing: BLETransportError.notConnected)
        }
        writeContinuation = continuation
        p.writeValue(data, for: ch, type: .withResponse)
    }

    private func enqueueSnapshotNotifyEnable(continuation: CheckedContinuation<Void, Error>) {
        guard let p = activePeripheral, let ch = snapshotCharacteristic else {
            continuation.resume(throwing: BLETransportError.notConnected)
            return
        }
        if let stale = notifyEnableContinuation {
            notifyEnableContinuation = nil
            stale.resume(throwing: BLETransportError.notConnected)
        }
        notifyEnableContinuation = continuation
        p.setNotifyValue(true, for: ch)
    }

    private func enqueueReadSnapshot(continuation: CheckedContinuation<Data, Error>) {
        guard let ch = snapshotCharacteristic, let p = activePeripheral, ch.properties.contains(.read) else {
            continuation.resume(throwing: BLETransportError.notConnected)
            return
        }
        if let stale = readDataContinuation {
            readDataContinuation = nil
            stale.resume(throwing: BLETransportError.notConnected)
        }
        readDataContinuation = continuation
        p.readValue(for: ch)
    }

    private var writeContinuation: CheckedContinuation<Void, Error>?

    /// BLE GATT read nemá ETag; parametr je kvůli shodě s `BoardTransport` / Wi‑Fi.
    func readSnapshotFull(ifNoneMatch _: String?) async throws -> GameSnapshot? {
        guard let ch = snapshotCharacteristic, ch.properties.contains(.read), activePeripheral != nil else {
            return nil
        }
        let data = try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Data, Error>) in
            queue.async { [weak self] in
                self?.enqueueReadSnapshot(continuation: cont)
            }
        }
        return try decodeSnapshot(from: data)
    }

    private func appendChunk(_ data: Data) {
        guard data.count >= 4, data[0] == 0x43, data[1] == 0x4D else {
            snapshotNotifyHandler?(data)
            return
        }
        let part = Int(data[2])
        let of = Int(data[3])
        let payload = data.dropFirst(4)
        if part == 1 {
            jsonReassembly = Data(payload)
            jsonChunkPart = 1
            jsonChunkTotal = of
        } else {
            jsonReassembly.append(payload)
            jsonChunkPart = part
            jsonChunkTotal = of
        }
        if part == of, part > 0 {
            snapshotNotifyHandler?(jsonReassembly)
            jsonReassembly = Data()
        }
    }
}

extension ChessboardBLEClient: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        AppDebugLog.staging("BLE central state=\(central.state.rawValue) (5=poweredOn)")
        switch central.state {
        case .poweredOn:
            restartBLEScanIfNeeded()
        case .poweredOff, .unauthorized, .unsupported:
            failPendingContinuations(with: BLETransportError.bluetoothUnavailable)
        default:
            break
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        discoveredPeripherals[peripheral.identifier] = peripheral
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? "CZECHMATE"
        let dev = DiscoveredBoardDevice(
            id: peripheral.identifier.uuidString,
            displayName: name,
            transport: .bluetooth,
            resolvedBaseURL: nil,
            signalDescription: "\(RSSI.intValue) dBm"
        )
        Task { @MainActor in
            self.scanHandler?(dev)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        activePeripheral = peripheral
        pendingDiscoverAllServices = false
        serviceDiscoveryEmptyAttempts = 0
        let nm = peripheral.name ?? "nil"
        AppDebugLog.staging(
            "BLE didConnect name=\(nm) id=\(peripheral.identifier.uuidString.prefix(8))… — discoverServices za \(serviceDiscoveryDelaysSec[0])s (Wi‑Fi+BLE coexistence)"
        )
        queue.asyncAfter(deadline: .now() + serviceDiscoveryDelaysSec[0]) { [weak self] in
            guard let self, self.activePeripheral?.identifier == peripheral.identifier else { return }
            peripheral.discoverServices([CZECHMATEBLEUUIDs.service])
        }
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        if let error {
            AppDebugLog.stagingError("BLE didFailToConnect", error)
        } else {
            AppDebugLog.staging("BLE didFailToConnect (bez NSError)")
        }
        connectContinuation?.resume(throwing: error ?? BLETransportError.notConnected)
        connectContinuation = nil
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        if peripheral.identifier == activePeripheral?.identifier {
            failPendingContinuations(with: error ?? BLETransportError.notConnected)
            snapshotCharacteristic = nil
            commandCharacteristic = nil
            snapshotNotifyHandler = nil
            snapshotDisconnectHandler?()
            snapshotDisconnectHandler = nil
        }
        activePeripheral = nil
    }

    /// Zruší čekání na připojení / read / write při pádu spojení nebo vypnutí BT (jinak může `connect()` viset navěky).
    private func failPendingContinuations(with error: Error) {
        if let c = connectContinuation {
            connectContinuation = nil
            c.resume(throwing: error)
        }
        if let c = readDataContinuation {
            readDataContinuation = nil
            c.resume(throwing: error)
        }
        if let c = writeContinuation {
            writeContinuation = nil
            c.resume(throwing: error)
        }
        if let c = notifyEnableContinuation {
            notifyEnableContinuation = nil
            c.resume(throwing: error)
        }
    }
}

extension ChessboardBLEClient: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error {
            AppDebugLog.stagingError("BLE didDiscoverServices", error)
            connectContinuation?.resume(throwing: error)
            connectContinuation = nil
            return
        }
        let list = peripheral.services ?? []
        let uuidList = list.map(\.uuid.uuidString).joined(separator: ", ")
        AppDebugLog.staging("BLE didDiscoverServices: \(list.count) služeb [\(uuidList)]")

        if let s = list.first(where: { $0.uuid == CZECHMATEBLEUUIDs.service }) {
            pendingDiscoverAllServices = false
            serviceDiscoveryEmptyAttempts = 0
            peripheral.discoverCharacteristics(
                [CZECHMATEBLEUUIDs.snapshotCharacteristic, CZECHMATEBLEUUIDs.commandCharacteristic],
                for: s
            )
            return
        }

        if list.isEmpty {
            serviceDiscoveryEmptyAttempts += 1
            let maxEmpty = serviceDiscoveryDelaysSec.count + 1
            if serviceDiscoveryEmptyAttempts < maxEmpty {
                let idx = min(serviceDiscoveryEmptyAttempts - 1, serviceDiscoveryDelaysSec.count - 1)
                let delay = serviceDiscoveryDelaysSec[idx]
                if serviceDiscoveryEmptyAttempts <= 2 {
                    AppDebugLog.staging(
                        "BLE: 0 služeb — pokus \(serviceDiscoveryEmptyAttempts)/\(maxEmpty), znovu CZECHMATE UUID za \(delay)s"
                    )
                    queue.asyncAfter(deadline: .now() + delay) { [weak self] in
                        guard let self, self.activePeripheral?.identifier == peripheral.identifier else { return }
                        peripheral.discoverServices([CZECHMATEBLEUUIDs.service])
                    }
                    return
                }
                if serviceDiscoveryEmptyAttempts == 3 {
                    pendingDiscoverAllServices = true
                    AppDebugLog.staging(
                        "BLE: stále 0 služeb — discoverServices(nil) za \(delay)s (celý GATT)"
                    )
                    queue.asyncAfter(deadline: .now() + delay) { [weak self] in
                        guard let self, self.activePeripheral?.identifier == peripheral.identifier else { return }
                        peripheral.discoverServices(nil)
                    }
                    return
                }
                AppDebugLog.staging(
                    "BLE: poslední pokus discover(nil) za \(delay)s"
                )
                queue.asyncAfter(deadline: .now() + delay) { [weak self] in
                    guard let self, self.activePeripheral?.identifier == peripheral.identifier else { return }
                    peripheral.discoverServices(nil)
                }
                return
            }
        }

        if !pendingDiscoverAllServices {
            pendingDiscoverAllServices = true
            AppDebugLog.staging(
                "BLE: služba CZECHMATE v seznamu není — zkouším discoverServices(nil) (GATT cache / iOS)"
            )
            peripheral.discoverServices(nil)
            return
        }
        AppDebugLog.staging(
            "BLE: 0 služeb i po opakování — iOS nedostalo ATT odpověď (Wi‑Fi+BLE na C6). Zkus: v Nastavení → Bluetooth zapomenout zařízení CZECHMATE; Wi‑Fi na telefonu vypnout; v nRF Connect ověřit služby. Na UART má být „coexistence: PREFER_BT“ a po spojení „MTU“/„GATT registered“; pokud MTU nepřijde, link je mrtvý na vyšší vrstvě."
        )
        connectContinuation?.resume(throwing: BLETransportError.characteristicMissing)
        connectContinuation = nil
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard error == nil else {
            AppDebugLog.stagingError("BLE didDiscoverCharacteristics", error!)
            connectContinuation?.resume(throwing: error!)
            connectContinuation = nil
            return
        }
        for c in service.characteristics ?? [] {
            if c.uuid == CZECHMATEBLEUUIDs.snapshotCharacteristic {
                snapshotCharacteristic = c
            }
            if c.uuid == CZECHMATEBLEUUIDs.commandCharacteristic {
                commandCharacteristic = c
            }
        }
        if snapshotCharacteristic != nil, commandCharacteristic != nil {
            AppDebugLog.staging("BLE GATT: snapshot + command char OK")
            connectContinuation?.resume()
            connectContinuation = nil
        } else {
            AppDebugLog.staging("BLE GATT: chybí char (snap=\(snapshotCharacteristic != nil) cmd=\(commandCharacteristic != nil))")
            connectContinuation?.resume(throwing: BLETransportError.characteristicMissing)
            connectContinuation = nil
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic == snapshotCharacteristic else { return }

        // Čeká `readSnapshotFull` — musíme vždy resume (i při nil value / jen chybě), jinak
        // „SWIFT TASK CONTINUATION MISUSE: readSnapshotFull leaked its continuation“.
        if let cont = readDataContinuation {
            readDataContinuation = nil
            if let err = error {
                AppDebugLog.stagingError("BLE read snapshot", err)
                cont.resume(throwing: err)
            } else {
                cont.resume(returning: characteristic.value ?? Data())
            }
            return
        }

        if error == nil, let data = characteristic.value {
            appendChunk(data)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic == commandCharacteristic else { return }
        if let err = error {
            AppDebugLog.stagingError("BLE write command", err)
            writeContinuation?.resume(throwing: err)
        } else {
            writeContinuation?.resume()
        }
        writeContinuation = nil
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == CZECHMATEBLEUUIDs.snapshotCharacteristic else { return }
        if let error {
            AppDebugLog.stagingError("BLE snapshot notify subscription", error)
            notifyEnableContinuation?.resume(throwing: error)
            notifyEnableContinuation = nil
            return
        }
        if notifyEnableContinuation != nil {
            guard characteristic.isNotifying else {
                AppDebugLog.staging("BLE snapshot notify: isNotifying=false po úspěšném callbacku — neočekáváno")
                notifyEnableContinuation?.resume(throwing: BLETransportError.notifySubscriptionFailed)
                notifyEnableContinuation = nil
                return
            }
            notifyEnableContinuation?.resume()
            notifyEnableContinuation = nil
            AppDebugLog.staging("BLE snapshot notify isNotifying=true (ESP posílá JSON jen po zapnutí notify)")
            return
        }
        AppDebugLog.staging("BLE snapshot notify isNotifying=\(characteristic.isNotifying) (změna mimo subscribe)")
    }
}

#else

final class ChessboardBLEClient: @unchecked Sendable {
    static let shared = ChessboardBLEClient()
    func prepareCentral() {}
    var isBluetoothReady: Bool { false }
    func startScanning(onFound: @escaping @MainActor (DiscoveredBoardDevice) -> Void) {}
    func stopScanning() {}
    func connect(to device: DiscoveredBoardDevice) async throws { throw BLETransportError.unsupported }
    func startSnapshotNotifications(handler: @escaping @Sendable (Data) -> Void, disconnectHandler: @escaping @Sendable () -> Void) async throws { throw BLETransportError.unsupported }
    func stopSnapshotNotifications() {}
    func disconnect() async {}
    func writeCommandJSON(_ obj: [String: Any]) async throws { throw BLETransportError.unsupported }
    func readSnapshotFull(ifNoneMatch: String?) async throws -> GameSnapshot? { nil }
}

#endif

#if canImport(CoreBluetooth)
extension ChessboardBLEClient {
    func decodeSnapshot(from data: Data) throws -> GameSnapshot {
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(data)
        let dec = JSONDecoder.forGameSnapshot()
        do {
            return try dec.decode(GameSnapshot.self, from: fixed)
        } catch {
            let preview = String(data: fixed.prefix(200), encoding: .utf8) ?? ""
            AppDebugLog.staging("BLE decodeSnapshot: \(error) preview=\(preview)")
            throw error
        }
    }
}
#endif
