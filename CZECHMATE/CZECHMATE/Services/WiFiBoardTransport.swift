//
//  WiFiBoardTransport.swift
//  REST + WebSocket — stejná logika jako dříve v BoardConnectionStore.
//

import Foundation

@MainActor
final class WiFiBoardTransport: BoardTransport {
    let linkKind: BoardLinkKind = .wifiLAN
    var connectionLabel: String { "Wi‑Fi (domácí síť)" }

    private let boardClient: ChessboardAPIClient
    private let wsClient: ChessboardWebSocketClient
    private var onSnapshot: (@MainActor (GameSnapshot) -> Void)?
    private(set) var isWebSocketLive = false
    private var lastStateTag: String?

    /// Volitelně: RTT a čas pro diagnostiku v UI.
    var onDiagnostics: ((Int, Date) -> Void)?

    var useWebSocket: Bool

    init(baseURL: URL, useWebSocket: Bool = true) {
        let session = BoardLANURLSessionFactory.makeSession()
        boardClient = ChessboardAPIClient(baseURL: baseURL, session: session)
        wsClient = ChessboardWebSocketClient(session: session)
        self.useWebSocket = useWebSocket
    }

    func setBaseURL(_ url: URL) async {
        await boardClient.setBaseURL(url)
    }

    func startSnapshotUpdates(onSnapshot: @escaping @MainActor (GameSnapshot) -> Void) async throws {
        await stopSnapshotUpdates()
        self.onSnapshot = onSnapshot
        if useWebSocket {
            let url = await boardClient.currentBaseURL()
            await wsClient.connect(baseURL: url) { [weak self] snap in
                Task { @MainActor [weak self] in
                    self?.isWebSocketLive = true
                    self?.lastStateTag = snap.stateVersion.map(String.init)
                    onSnapshot(snap)
                }
            } onDisconnect: { [weak self] in
                Task { @MainActor [weak self] in
                    self?.isWebSocketLive = false
                }
            }
        }
        await pollLoop(onSnapshot: onSnapshot)
    }

    func stopSnapshotUpdates() async {
        onSnapshot = nil
        isWebSocketLive = false
        await wsClient.disconnect()
    }

    @MainActor
    private func pollLoop(onSnapshot: @escaping @MainActor (GameSnapshot) -> Void) async {
        var backoff: TimeInterval = 1.0
        let successInterval: TimeInterval = 1.0
        let restWatchdogSeconds: TimeInterval = 25
        while !Task.isCancelled {
            let interval: TimeInterval
            do {
                if useWebSocket, isWebSocketLive {
                    let tag = lastStateTag
                    let t0 = Date()
                    let fetched = try await boardClient.fetchSnapshot(ifNoneMatch: tag)
                    let ms = Int(Date().timeIntervalSince(t0) * 1000)
                    onDiagnostics?(ms, Date())
                    if let snap = fetched {
                        lastStateTag = snap.stateVersion.map(String.init)
                        onSnapshot(snap)
                    }
                    interval = restWatchdogSeconds
                } else {
                    let t0 = Date()
                    let snap = try await boardClient.fetchSnapshot()
                    let ms = Int(Date().timeIntervalSince(t0) * 1000)
                    onDiagnostics?(ms, Date())
                    lastStateTag = snap.stateVersion.map(String.init)
                    onSnapshot(snap)
                    backoff = successInterval
                    interval = successInterval
                }
            } catch {
                isWebSocketLive = false
                backoff = min(backoff * 1.5, 10.0)
                interval = backoff
                AppDebugLog.stagingError("WiFiBoardTransport poll", error)
                AppDebugLog.staging(
                    "WiFi poll další pokus za \(String(format: "%.1f", backoff)) s (useWebSocket=\(useWebSocket))"
                )
            }
            try? await Task.sleep(nanoseconds: UInt64(interval * 1_000_000_000))
        }
    }

    func postHintHighlight(from: String, to: String) async throws {
        try await boardClient.postHintHighlight(from: from, to: to)
    }

    func postHintClear() async throws {
        try await boardClient.postHintClear()
    }

    func postBrightness(percent: Int) async throws {
        try await boardClient.postBrightness(percent: percent)
    }

    func fetchSnapshot(ifNoneMatch: String?) async throws -> GameSnapshot? {
        try await boardClient.fetchSnapshot(ifNoneMatch: ifNoneMatch)
    }

    /// Okamžité stažení snímku (pull-to-refresh na iPhonu).
    func fetchSnapshotNow() async throws -> GameSnapshot {
        try await boardClient.fetchSnapshot()
    }

    /// Měření RTT pro diagnostiku (jen Wi‑Fi).
    func pingRTT() async -> String {
        let t0 = Date()
        do {
            _ = try await boardClient.fetchSnapshot()
            let ms = Int(Date().timeIntervalSince(t0) * 1000)
            return "OK · \(ms) ms"
        } catch {
            return NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func fetchTimerState() async throws -> BoardTimerHTTPState {
        try await boardClient.fetchTimerState()
    }

    func postTimerConfig(type: Int, customMinutes: Int?, customIncrement: Int?) async throws {
        try await boardClient.postTimerConfig(type: type, customMinutes: customMinutes, customIncrement: customIncrement)
    }

    func postTimerPause() async throws { try await boardClient.postTimerPause() }
    func postTimerResume() async throws { try await boardClient.postTimerResume() }
    func postTimerReset() async throws { try await boardClient.postTimerReset() }

    func postNewGame(fen: String? = nil) async throws {
        try await boardClient.postNewGame(fen: fen)
    }

    func postHintHighlightDestinationOnly(to square: String) async throws {
        try await boardClient.postHintHighlightDestinationOnly(to: square)
    }

    func fetchGameStatus() async throws -> ESPGameStatusJSON {
        try await boardClient.fetchGameStatus()
    }

    func postSetupTutorial(action: SetupTutorialAPIAction) async throws {
        try await boardClient.postSetupTutorial(action: action)
    }

    func postVirtualAction(action: String, square: String?, choice: String?) async throws {
        try await boardClient.postVirtualAction(action: action, square: square, choice: choice)
    }

    func postGameMove(from: String, to: String, promotion: String?) async throws {
        try await boardClient.postGameMove(from: from, to: to, promotion: promotion)
    }

    func fetchWiFiStatus() async throws -> ESPWiFiStatusJSON {
        try await boardClient.fetchWiFiStatus()
    }

    func postWiFiConfig(ssid: String, password: String) async throws {
        try await boardClient.postWiFiConfig(ssid: ssid, password: password)
    }

    func postWiFiConnect() async throws { try await boardClient.postWiFiConnect() }
    func postWiFiDisconnect() async throws { try await boardClient.postWiFiDisconnect() }
    func postWiFiClear() async throws { try await boardClient.postWiFiClear() }

    // MARK: - Zařízení (parita s webem)

    func fetchUISettingsEnvelope() async throws -> BoardUISettingsEnvelope {
        try await boardClient.fetchUISettingsEnvelope()
    }

    func postUISettingsEnvelope(_ envelope: BoardUISettingsEnvelope) async throws {
        try await boardClient.postUISettingsEnvelope(envelope)
    }

    func postAutoLampTimeout(seconds: Int) async throws {
        try await boardClient.postAutoLampTimeout(seconds: seconds)
    }

    func postGuidedHints(enabled: Bool) async throws {
        try await boardClient.postGuidedHints(enabled: enabled)
    }

    func postLedGuidance(level: Int) async throws {
        try await boardClient.postLedGuidance(level: level)
    }

    func postLightCommand(state: Bool, r: Int, g: Int, b: Int) async throws {
        try await boardClient.postLightCommand(state: state, r: r, g: g, b: b)
    }

    func postLightGameMode() async throws {
        try await boardClient.postLightGameMode()
    }

    func fetchDemoStatus() async throws -> ESPDemoStatusJSON {
        try await boardClient.fetchDemoStatus()
    }

    func postDemoConfig(enabled: Bool, speedMs: UInt32?) async throws {
        try await boardClient.postDemoConfig(enabled: enabled, speedMs: speedMs)
    }

    func postDemoStart() async throws {
        try await boardClient.postDemoStart()
    }

    func fetchMQTTStatus() async throws -> ESPMQTTStatusJSON {
        try await boardClient.fetchMQTTStatus()
    }

    func postMQTTConfig(host: String, port: Int, username: String?, password: String?) async throws {
        try await boardClient.postMQTTConfig(host: host, port: port, username: username, password: password)
    }
}
