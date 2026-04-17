//
//  BLEBoardTransport.swift
//  GATT snapshot + příkazy přes rozšířený ChessboardBLEClient.
//
//  GATT: snapshot notify, hint, jas, tah, nová hra, undo, promoce, časovač (`timer_*`), virtual_action.
//  WiFi konfigurace a velké NVS `/api/settings/ui` zůstávají u `WiFiBoardTransport`; demo/MQTT/auto lampa/LED guidance i nová hra+FEN také přes BLE.
//

import Foundation
import CZECHMATEShared

@MainActor
final class BLEBoardTransport: BoardTransport, BLEBoardTransportProtocol {
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
            },
            cmdReplyHandler: { data in
                NotificationCenter.default.post(
                    name: .czechmateBleCommandAck,
                    object: nil,
                    userInfo: ["jsonData": data]
                )
            }
        )

        for await data in stream {
            if Task.isCancelled { break }
            do {
                let snap = try GameSnapshot.decodeFromBoardDataRepairingAndNormalizing(data)
                await MainActor.run { onSnapshot(snap) }
            } catch {
                let head = String(decoding: data.prefix(360), as: UTF8.self)
                    .replacingOccurrences(of: "\n", with: "\\n")
                AppDebugLog.staging(
                    "BLE snapshot decode failed: \(error) utf8_prefix_360=\(head)"
                )
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

    /// Stejné tělo jako `POST /api/game/hint_highlight` jen s `to` — jen cílové pole na LED.
    func postHintHighlightDestinationOnly(to square: String) async throws {
        try await ble.writeCommandJSON(["cmd": "hint_highlight", "to": square.lowercased()])
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

    /// Přečte network info z BLE (IP adresa, SSID, online status).
    /// Vrací JSON: {"sta_connected":true,"sta_ip":"192.168.1.45","sta_ssid":"WiFi","ap_ip":"192.168.4.1","online":true}
    func fetchNetworkInfo() async -> (staIP: String?, apIP: String?, online: Bool)? {
        do {
            guard let data = try await ble.readNetworkInfo() else { return nil }
            guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }
            let staIP = json["sta_ip"] as? String
            let apIP = json["ap_ip"] as? String
            let online = json["online"] as? Bool ?? false
            return (staIP, apIP, online)
        } catch {
            AppDebugLog.staging("BLE fetchNetworkInfo failed: \(error)")
            return nil
        }
    }

    /// Send raw command to chessboard (for WatchConnectivityBridgeEnhanced compatibility)
    func sendCommand(_ command: [String: Any]) -> Bool {
        Task {
            do {
                try await ble.writeCommandJSON(command)
            } catch {
                AppDebugLog.staging("BLE sendCommand failed: \(error)")
            }
        }
        return true // Optimistic return - actual result in async Task
    }

    // MARK: - BLE-Only Game Commands

    /// Send chess move via BLE
    /// Format: {"cmd": "move", "from": "e2", "to": "e4", "promotion": "q"}
    func postMove(from: String, to: String, promotion: String? = nil) async throws {
        var cmd: [String: Any] = [
            "cmd": "move",
            "from": from.lowercased(),
            "to": to.lowercased()
        ]
        if let p = promotion {
            cmd["promotion"] = p.lowercased()
        }
        try await ble.writeCommandJSON(cmd)
    }

    /// Nová hra přes BLE. Volitelný `fen` — stejná logika jako `POST /api/game/new` s JSON `{"fen":...}`.
    func postNewGame(fen: String? = nil) async throws {
        var payload: [String: Any] = ["cmd": "new_game"]
        if let fen, !fen.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            payload["fen"] = fen.trimmingCharacters(in: .whitespacesAndNewlines)
        }
        try await ble.writeCommandJSON(payload)
    }

    /// Undo last move via BLE
    /// Format: {"cmd": "undo"}
    func postUndo() async throws {
        try await ble.writeCommandJSON(["cmd": "undo"])
    }

    /// Send promotion choice via BLE
    /// Format: {"cmd": "promotion", "choice": "q"}
    func postPromotion(choice: String) async throws {
        try await ble.writeCommandJSON([
            "cmd": "promotion",
            "choice": choice.lowercased()
        ])
    }

    /// Stejné tělo jako `POST /api/timer/config` — přes BLE JSON `cmd: timer_config`.
    func postTimerConfig(type: Int, customMinutes: Int?, customIncrement: Int?) async throws {
        var cmd: [String: Any] = ["cmd": "timer_config", "type": type]
        if type == 14 {
            if let m = customMinutes { cmd["custom_minutes"] = m }
            if let i = customIncrement { cmd["custom_increment"] = i }
        }
        try await ble.writeCommandJSON(cmd)
    }

    func postTimerPause() async throws {
        try await ble.writeCommandJSON(["cmd": "timer_pause"])
    }

    func postTimerResume() async throws {
        try await ble.writeCommandJSON(["cmd": "timer_resume"])
    }

    func postTimerReset() async throws {
        try await ble.writeCommandJSON(["cmd": "timer_reset"])
    }

    /// Stejné chování jako `POST /api/game/virtual_action`.
    func postVirtualAction(action: String, square: String?, choice: String?) async throws {
        var cmd: [String: Any] = ["cmd": "virtual_action", "action": action]
        if let s = square?.trimmingCharacters(in: .whitespacesAndNewlines), !s.isEmpty {
            cmd["square"] = s.lowercased()
        }
        if let c = choice?.trimmingCharacters(in: .whitespacesAndNewlines), !c.isEmpty {
            cmd["choice"] = c.uppercased()
        }
        try await ble.writeCommandJSON(cmd)
    }

    /// Parita s `POST /api/light/command` — na ESP stejná cesta jako HTTP (`ha_light_request_web_lamp`).
    func postLightCommand(state: Bool, r: Int, g: Int, b: Int) async throws {
        let rs = min(255, max(0, r)), gs = min(255, max(0, g)), bs = min(255, max(0, b))
        try await ble.writeCommandJSON([
            "cmd": "light_command",
            "state": state,
            "r": rs,
            "g": gs,
            "b": bs
        ])
    }

    /// Parita s `POST /api/light/game_mode`.
    func postLightGameMode() async throws {
        try await ble.writeCommandJSON(["cmd": "light_game_mode"])
    }

    /// Uloží SSID/heslo do NVS na desce a spustí STA připojení (viz firmware `wifi_sta_config` — probíhá na pozadí až ~30 s).
    func postWiFiSTAConfig(ssid: String, password: String) async throws {
        var payload: [String: Any] = [
            "cmd": "wifi_sta_config",
            "ssid": ssid
        ]
        payload["password"] = password
        try await ble.writeCommandJSON(payload)
    }

    /// Parita s `POST /api/system/factory_reset` — vymaže celý NVS oddíl a restart desky.
    func postFactoryReset() async throws {
        let ack = try await waitForBleCommandAck(expectedCmd: "factory_reset", timeoutSeconds: 4) {
            try await self.ble.writeCommandJSON([
                "cmd": "factory_reset",
                "confirm": "erase_all_nvs",
            ])
        }
        let ok = (ack["ok"] as? Bool) ?? false
        if ok { return }
        let message = (ack["message"] as? String).flatMap { $0.isEmpty ? nil : $0 }
        throw ChessboardAPIError.httpStatus(400, message)
    }

    // MARK: - BLE parita s HTTP (NVS / demo / MQTT) — bez GET odpovědi na GATT write

    func postAutoLampTimeout(seconds: Int) async throws {
        let v = min(7200, max(5, seconds))
        try await ble.writeCommandJSON(["cmd": "settings_auto_lamp_timeout", "seconds": v])
    }

    func postGuidedHints(enabled: Bool) async throws {
        try await ble.writeCommandJSON(["cmd": "settings_guided_hints", "enabled": enabled])
    }

    func postLedGuidance(level: Int) async throws {
        let v = min(5, max(1, level))
        try await ble.writeCommandJSON(["cmd": "settings_led_guidance", "level": v])
    }

    func postDemoConfig(enabled: Bool, speedMs: UInt32?) async throws {
        var cmd: [String: Any] = ["cmd": "demo_config", "enabled": enabled]
        if let speedMs {
            cmd["speed_ms"] = Int(speedMs)
        }
        try await ble.writeCommandJSON(cmd)
    }

    func postDemoStart() async throws {
        try await ble.writeCommandJSON(["cmd": "demo_start"])
    }

    func postMQTTConfig(host: String, port: Int, username: String?, password: String?) async throws {
        var cmd: [String: Any] = ["cmd": "mqtt_config", "host": host, "port": port]
        if let u = username, !u.isEmpty { cmd["username"] = u }
        if let p = password, !p.isEmpty { cmd["password"] = p }
        try await ble.writeCommandJSON(cmd)
    }

    /// Parita s `POST /api/game/setup_tutorial` — čeká na `cmd_ack` (409 = `tutorial_finish_conflict`).
    func postSetupTutorial(action: SetupTutorialAPIAction) async throws {
        let ack = try await waitForBleCommandAck(expectedCmd: "setup_tutorial", timeoutSeconds: 6) {
            try await self.ble.writeCommandJSON([
                "cmd": "setup_tutorial",
                "action": action.rawValue
            ])
        }
        let ok = (ack["ok"] as? Bool) ?? false
        if ok { return }
        let code = (ack["code"] as? String) ?? ""
        let message = (ack["message"] as? String).flatMap { $0.isEmpty ? nil : $0 }
        if code == "tutorial_finish_conflict" {
            throw ChessboardAPIError.httpStatus(409, message)
        }
        throw ChessboardAPIError.httpStatus(400, message)
    }

    private func waitForBleCommandAck(
        expectedCmd: String,
        timeoutSeconds: TimeInterval,
        write: @escaping () async throws -> Void
    ) async throws -> [String: Any] {
        try await withCheckedThrowingContinuation { cont in
            var observer: NSObjectProtocol?
            var completed = false
            func finish(_ result: Result<[String: Any], Error>) {
                if completed { return }
                completed = true
                if let o = observer {
                    NotificationCenter.default.removeObserver(o)
                }
                cont.resume(with: result)
            }
            observer = NotificationCenter.default.addObserver(
                forName: .czechmateBleCommandAck,
                object: nil,
                queue: .main
            ) { note in
                guard let data = note.userInfo?["jsonData"] as? Data,
                      let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                      (obj["channel"] as? String) == "cmd_ack"
                else { return }
                let cmd = (obj["cmd"] as? String)?.lowercased() ?? ""
                guard cmd == expectedCmd.lowercased() else { return }
                finish(.success(obj))
            }
            Task { @MainActor in
                do {
                    try await write()
                } catch {
                    finish(.failure(error))
                    return
                }
                try? await Task.sleep(nanoseconds: UInt64(timeoutSeconds * 1_000_000_000))
                guard !completed else { return }
                finish(.failure(ChessboardAPIError.invalidResponse))
            }
        }
    }
}
