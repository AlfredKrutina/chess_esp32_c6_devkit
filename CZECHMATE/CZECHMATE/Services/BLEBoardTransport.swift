//
//  BLEBoardTransport.swift
//  GATT snapshot + příkazy přes rozšířený ChessboardBLEClient.
//
//  Omezení: GATT příkazy zatím jen hint_highlight, hint_clear, brightness.
//  Nová hra, časovač a POST /api/move jsou jen přes Wi-Fi (`WiFiBoardTransport`).
//

import Foundation

@MainActor
final class BLEBoardTransport: BoardTransport {
    let linkKind: BoardLinkKind = .bluetooth
    let connectionLabel = "Bluetooth"

    private let ble = ChessboardBLEClient.shared

    func preparePermissions() {
        ble.prepareCentral()
    }

    func startScanningForBoards(
        onFound: @escaping @MainActor (DiscoveredBoardDevice) -> Void
    ) {
        ble.startScanning(onFound: onFound)
    }

    func stopScanning() {
        ble.stopScanning()
    }

    func connect(to device: DiscoveredBoardDevice) async throws {
        try await ble.connect(to: device)
    }

    /// Spustí GATT notify subscripci a čeká na příchozí snapshoty.
    /// Používá AsyncStream jako most mezi CoreBluetooth callbackem a Swift Concurrency.
    /// Smyčka se ukončí při:
    ///   - zrušení Task (stopPolling / nové připojení) → CancellationError propagován
    ///   - BLE odpojení → stream se ukončí přes disconnectHandler
    func startSnapshotUpdates(onSnapshot: @escaping @MainActor (GameSnapshot) -> Void) async throws {
        final class StreamContinuationBox: @unchecked Sendable {
            var continuation: AsyncStream<Data>.Continuation?
        }
        let box = StreamContinuationBox()

        let stream = AsyncStream<Data> { continuation in
            box.continuation = continuation
        }

        // Handler pro BLE GATT notify — volán z BLE queue, předává data do streamu.
        try await ble.startSnapshotNotifications(
            handler: { data in
                box.continuation?.yield(data)
            },
            disconnectHandler: {
                box.continuation?.finish()
            }
        )

        for await data in stream {
            if Task.isCancelled { break }
            do {
                let fixed = GameJSONRepair.repairStatusDataIfNeeded(data)
                let dec = JSONDecoder.forGameSnapshot()
                let snap = try dec.decode(GameSnapshot.self, from: fixed)
                await MainActor.run { onSnapshot(snap) }
            } catch {
                AppDebugLog.staging("BLE snapshot decode failed: \(error)")
            }
        }

        // Po skončení smyčky (cancel nebo disconnect) uklid streamu.
        box.continuation?.finish()
    }

    func stopSnapshotUpdates() async {
        ble.stopSnapshotNotifications()
        await ble.disconnect()
    }

    func postHintHighlight(from: String, to: String) async throws {
        try await ble.writeCommandJSON(["cmd": "hint_highlight", "from": from.lowercased(), "to": to.lowercased()])
    }

    func postHintClear() async throws {
        try await ble.writeCommandJSON(["cmd": "hint_clear"])
    }

    func postBrightness(percent: Int) async throws {
        try await ble.writeCommandJSON(["cmd": "brightness", "percent": percent])
    }

    func fetchSnapshot(ifNoneMatch: String?) async throws -> GameSnapshot? {
        do {
            return try await ble.readSnapshotFull(ifNoneMatch: ifNoneMatch)
        } catch {
            AppDebugLog.staging(
                "BLE fetchSnapshot: selhalo (typicky ATT chyba nebo MTU / neplatný JSON u jednoho read) — živý stav z NOTIFY: \(error.localizedDescription)"
            )
            return nil
        }
    }
}
