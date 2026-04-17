//
//  BoardConnectionStore.swift
//  CZECHMATE — orchestrátor BoardTransport (Wi‑Fi / BLE / mock).
//

import Foundation
import Observation

extension Notification.Name {
    /// JSON výsledek BLE příkazu (`channel` == `cmd_ack`) z `ChessboardBLEClient`.
    static let czechmateBleCommandAck = Notification.Name("czechmate.ble.commandAck")
    /// Každý rámec snapshot NOTIFY (i neplatný chunk) — indikátor připojení při BLE, když JSON chvíli nejde dekódovat.
    static let czechmateBleSnapshotNotifyTraffic = Notification.Name("czechmate.ble.snapshotNotifyTraffic")
}

@MainActor
@Observable
final class BoardConnectionStore {
    /// Stav tečky / štítku připojení: zelená jen při ověřeném kontaktu s deskou.
    enum BoardLinkIndicatorTier: Sendable {
        case live
        case connecting
        case offline
    }

    /// Pro rozhodnutí „BLE → HTTP na STA IP“: bez aktivního Wi‑Fi na telefonu zůstaň na GATT.
    weak var networkHandoffMonitor: NetworkStatusMonitor?

    /// Poslední známá STA IP desky (po úspěšném HTTP handoffu / provisioningu).
    private(set) var lastKnownBoardStaIP: String?

    /// Po připojení přes BLE nabídka listu: zapsat Wi‑Fi telefonu na desku a přejít na HTTP.
    var offerWiFiProvisionSheet = false

    /// Aktuální fáze párování / navazování spojení (Bluetooth GATT, Wi‑Fi handshake) — pro list „Najít šachovnici“ a záložku Hra.
    var pairingPhaseMessage: String?

    @ObservationIgnored
    private var wifiPathHandoffHooked = false

    private static let defaultsKey = "czechmate.boardBaseURL"
    private static let depthKey = "czechmate.hintDepth"
    private static let hintTierKey = "czechmate.moveHintTier"
    private static let moveEvalKey = "czechmate.moveEvaluationEnabled"
    private static let mockKey = "czechmate.useMockBoard"
    /// Vývojářská volba: po BLE připojení neprovést automatický přechod na HTTP přes STA IP.
    static let preferBluetoothOnlyKey = "czechmate.preferBluetoothOnly"
    /// Poslední úspěšný typ spojení (pro obnovení při startu aplikace).
    private static let lastBoardLinkKindKey = "czechmate.lastBoardLinkKind"
    private static let lastBLEDeviceIdKey = "czechmate.lastBLEDeviceUUID"
    private static let lastBLEDeviceNameKey = "czechmate.lastBLEDeviceDisplayName"
    private static let linkKindWiFi = "wifi"
    private static let linkKindBluetooth = "bluetooth"
    private static let linkKindMock = "mock"

    var baseURLString: String
    var hintDepth: Int
    /// H1–H3: kolik detailů poslat na LED / do překryvu desky (jen aplikace, ne NVS).
    var moveHintTier: MoveHintTier = .fullMove {
        didSet { UserDefaults.standard.set(moveHintTier.rawValue, forKey: Self.hintTierKey) }
    }

    var moveEvaluationEnabled: Bool

    /// Preferencí z `/api/settings/ui` (NVS na desce), sloučeno s místním `hintDepth` / eval při POST.
    var boardUIPrefsPayload: BoardUIPrefsPayload = .init()
    private(set) var boardUISettingsVersion: Int = 1
    /// Počet nápověd na stranu za partii (999 = neomezeno), jako web při `chessHintLimit`.
    var hintsRemainingWhite: Int = 999
    var hintsRemainingBlack: Int = 999
    /// Historie: `nil` = živá pozice; jinak index posledního zahrnutého tahu (0…n-1) pro náhled.
    var historyReviewMoveIndex: Int?
    private var lastBotSuggestionKey: String?
    /// Ukázková šachovnice bez ESP (staging / vývoj UI).
    /// Změna režimu musí volat `startPolling()` (např. z `GameView` / výběru desky) — ne v `didSet`,
    /// aby nevznikaly dvojí restarty při `pickWiFi` / přepnutí transportu.
    var useMockBoard = false {
        didSet { UserDefaults.standard.set(useMockBoard, forKey: Self.mockKey) }
    }

    private(set) var snapshotReceivedAt: Date?
    /// Poslední BLE GATT aktivita (raw snapshot notify nebo `cmd_ack`) — doplnění k `snapshotReceivedAt` při rozbitém chunkovaném JSON.
    private(set) var lastBleGattActivityAt: Date?
    /// Zbývající čas z `GET /api/timer` (`white_time_ms` / `black_time_ms`). Snapshot `white_time`/`black_time` jsou v firmware jiné údaje (kumul. časy tahů).
    private(set) var lastTimerState: BoardTimerHTTPState?
    private(set) var timerClockReceivedAt: Date?
    private var lastTimerHttpFetchAt: Date = .distantPast
    /// Počet tahů v historii při posledním úspěšném `GET /api/timer` — při novém tahu vynutit stáhnutí (jinak throttle může nechat zastaralé ms).
    private var lastTimerFetchMoveCount: Int = -1
    /// Po každém `applySnapshot` se zvýší; dokončený async refresh timeru zapíše stav jen pokud `refreshId` je stále poslední (ochrana před přepsáním starší HTTP odpovědí).
    private var timerRefreshLatestId: UInt64 = 0

    /// Po změně jasu/barvy ignorovat krátké `light_state: false` ve snapshotu (race HTTP vs. lampa).
    private var lampStabilizationDeadline: Date?
    private var deferredSnapshotRefreshAfterLampTask: Task<Void, Never>?

    #if DEBUG
    private var stagingTimerHttpFetchCount: Int = 0
    private var stagingTimerFromSnapshotCount: Int = 0
    private var stagingTimerMetricsWindowStart: Date = Date()
    private var webSocketFrameCount: Int = 0
    var webSocketFramesForDiagnostics: Int { webSocketFrameCount }

    private func recordStagingTimerHttpFetch() {
        stagingTimerHttpFetchCount += 1
        logStagingTimerMetricsIfNeeded()
    }

    private func recordStagingTimerFromSnapshot() {
        stagingTimerFromSnapshotCount += 1
        logStagingTimerMetricsIfNeeded()
    }

    private func logStagingTimerMetricsIfNeeded() {
        let now = Date()
        guard now.timeIntervalSince(stagingTimerMetricsWindowStart) >= 60 else { return }
        AppDebugLog.staging(
            "[timer-metrics] za 60s: GET /api/timer=\(stagingTimerHttpFetchCount), clock ve snapshotu=\(stagingTimerFromSnapshotCount)"
        )
        stagingTimerHttpFetchCount = 0
        stagingTimerFromSnapshotCount = 0
        stagingTimerMetricsWindowStart = now
    }
    #endif

    private(set) var snapshot: GameSnapshot?
    
    /// Alias for compatibility with new UI components
    var activeSnapshot: GameSnapshot? { snapshot }
    
    /// Transport existuje a buď běží snapshot smyčka, nebo je BLE perifér stále ve stavu `connected` (GATT).
    var hasActiveConnection: Bool {
        guard currentTransport != nil else { return false }
        if isPolling { return true }
        if bleTransport != nil, ChessboardBLEClient.shared.isGattPeripheralConnectedForLinkUI() {
            return true
        }
        return false
    }

    /// Čas posledního důkazu, že deska odpovídá: nový snapshot, nebo u Wi‑Fi úspěšný GET (REST watchdog).
    /// Při aktivním BLE je autoritativní jen BLE — `wifiTransport` může zůstat z fallbacku / staré session a
    /// `lastRestFetchAt` by pak kazilo indikátor i když GATT žije.
    private var lastVerifiedBoardInteractionAt: Date? {
        if bleTransport != nil {
            switch (snapshotReceivedAt, lastBleGattActivityAt) {
            case let (s?, a?): return max(s, a)
            case let (s?, nil): return s
            case let (nil, a?): return a
            case (nil, nil): return nil
            }
        }
        if wifiTransport != nil {
            switch (snapshotReceivedAt, lastRestFetchAt) {
            case let (s?, r?): return max(s, r)
            case let (s?, nil): return s
            case let (nil, r?): return r
            case (nil, nil): return nil
            }
        }
        return snapshotReceivedAt
    }

    var boardLinkIndicatorTier: BoardLinkIndicatorTier {
        let _ = linkHealthTick
        guard currentTransport != nil else { return .offline }
        let transportSessionAlive =
            isPolling
            || (bleTransport != nil && ChessboardBLEClient.shared.isGattPeripheralConnectedForLinkUI())
        guard transportSessionAlive else { return .offline }
        if useMockBoard { return .live }
        let staleLimit: TimeInterval =
            bleTransport != nil ? Self.boardLinkStaleIntervalBLE : Self.boardLinkStaleInterval
        let now = Date()
        if let last = lastVerifiedBoardInteractionAt {
            if now.timeIntervalSince(last) <= staleLimit {
                return .live
            }
            // BLE GATT stále spojený — neoznačovat „offline“ jen kvůli prodlevě JSON (chunk / MCU).
            if bleTransport != nil, ChessboardBLEClient.shared.isGattPeripheralConnectedForLinkUI() {
                return .live
            }
            return .offline
        }
        if let started = pollingStartedAt,
           now.timeIntervalSince(started) <= Self.boardFirstConnectGraceInterval {
            return .connecting
        }
        if bleTransport != nil, ChessboardBLEClient.shared.isGattPeripheralConnectedForLinkUI() {
            return .live
        }
        return .offline
    }

    var isScanning: Bool = false
    
    var lastError: String?
    var hintSquareFrom: String?
    var hintSquareTo: String?
    /// Cílové pole průvodce postavením — stejné jako LED na desce; zvýraznění na šachovnici v aplikaci (vč. BLE).
    private(set) var setupWizardTargetSquare: String?

    /// Zvýraznění polí na `ChessBoardView`: průvodce postavením má přednost před nápovědou tahu.
    var boardHintHighlightFrom: String? {
        if setupWizardTargetSquare != nil { return nil }
        return hintSquareFrom
    }

    var boardHintHighlightTo: String? {
        if let s = setupWizardTargetSquare { return s }
        return hintSquareTo
    }
    var lastHintSummary: String?
    var lastMoveEvaluation: MoveEvaluationResult?

    /// Evaluace po jednotlivých tazích (Stockfish) — graf v souhrnu partie; maže se u nové partie.
    private(set) var moveEvalHistory: [MoveEvalHistoryEntry] = []

    /// Statická pozice z puzzle (FEN). Když je nastaveno, Hra zobrazí tuto desku místo živého snímku.
    var puzzleBoardPreview: [[String]]?

    /// Volitelný konec časovače režimu „Trénink“ (záložka Puzzle). `nil` = bez limitu.
    var puzzleTrainingDeadline: Date?

    /// Průvodce postavením (LED nápověda, krok za krokem).
    private(set) var boardSetupManager: BoardSetupManager?

    /// Pozice z živého snímku před nahráním puzzle na desku (FEN) — pro návrat z puzzle režimu.
    struct PrePuzzleGameBookmark: Equatable, Sendable {
        let capturedAt: Date
        let restoreFEN: String?
        let previousMoveCount: Int

        var moveCountDescription: String {
            switch previousMoveCount {
            case 0: return "startovní pozice (0 tahů)"
            case 1: return "1 tah"
            case 2 ... 4: return "\(previousMoveCount) tahy"
            default: return "\(previousMoveCount) tahů"
            }
        }
    }

    private(set) var prePuzzleBookmark: PrePuzzleGameBookmark?

    /// Je uložená pozice k obnovení po `Odeslat na desku` / LED průvodci s novou hrou.
    var canRestorePrePuzzleGame: Bool { prePuzzleBookmark != nil }

    var prePuzzleRestoreSubtitle: String {
        guard let b = prePuzzleBookmark else { return "" }
        return "Stav před puzzle: \(b.moveCountDescription)."
    }

    /// Poslední úspěšný `GET /api/wifi/status` (domácí síť šachovnice, AP, …).
    private(set) var espWiFiStatusCache: ESPWiFiStatusJSON?
    private(set) var espWiFiStatusUpdatedAt: Date?

    var useWebSocket = true
    private(set) var isWebSocketLive = false
    private(set) var pollingStartedAt: Date?
    private(set) var lastRestFetchAt: Date?
    private(set) var lastRestRoundTripMs: Int?

    private var currentTransport: (any BoardTransport)? {
        mockTransport ?? bleTransport ?? wifiTransport
    }

    /// Je aktivní některý transport (Wi‑Fi / BLE / mock) — lze posílat např. jas.
    var transport: BoardTransport? { currentTransport }
    
    /// Alias for compatibility
    var activeTransport: BoardTransport? { transport }
    
    /// Connection label
    var connectionLabel: String? { currentTransport?.connectionLabel }

    /// Poslední zvolený typ spojení (pro štítky a diagnostiku).
    private(set) var activeLinkKind: BoardLinkKind = .wifiLAN

    /// Bez nového snapshotu ani úspěšného HTTP (200/304) považuj spojení za mrtvé (Wi‑Fi watchdog ~25 s).
    private static let boardLinkStaleInterval: TimeInterval = 55
    /// BLE nemusí posílat NOTIFY při nehybné pozici tak často — delší tolerance, aby neblikala falešná odpojení.
    private static let boardLinkStaleIntervalBLE: TimeInterval = 300
    /// První odpověď z LAN může trvat až ~35 s (`timeoutIntervalForRequest`).
    private static let boardFirstConnectGraceInterval: TimeInterval = 55

    @ObservationIgnored
    private var linkHealthMonitorTask: Task<Void, Never>?
    /// Nárazové přepočty UI (SwiftUI jinak nevidí uplynulý čas u stavu „stale“).
    private(set) var linkHealthTick: UInt64 = 0

    var isPolling = false
    var isPollingActive: Bool { isPolling }

    /// HTTP k desce: WiFi/NVS, MQTT, některé pokročilé API — jen se `WiFiBoardTransport`.
    var supportsWiFiRemoteCommands: Bool {
        wifiTransport != nil && !useMockBoard
    }

    /// Uložení SSID/hesla na desku: přes HTTP (`/api/wifi/config`) nebo přes BLE (`wifi_sta_config`).
    var canConfigureBoardWiFiCredentials: Bool {
        (wifiTransport != nil || bleTransport != nil) && !useMockBoard
    }

    /// Pauza / pokračování / reset časomíry a čtení stavu — Wi‑Fi (`/api/timer`) **nebo** BLE (`timer_*` na firmwaru).
    var supportsTimerRemoteControls: Bool {
        (wifiTransport != nil || bleTransport != nil) && !useMockBoard
    }

    /// Jas + RGB lampa + návrat do herního LED režimu — Wi‑Fi **nebo** BLE (`brightness`, `light_command`, `light_game_mode` na firmwaru).
    var supportsBoardLightingControls: Bool {
        (wifiTransport != nil || bleTransport != nil) && !useMockBoard
    }

    /// Tahy z aplikace na desku a základní hra (vč. nové hry přes GATT) — Wi‑Fi nebo Bluetooth.
    var supportsRemoteChessCommands: Bool {
        (wifiTransport != nil || bleTransport != nil) && !useMockBoard
    }

    /// Zápis demo, MQTT do NVS, auto lampa, LED guidance — přes HTTP nebo stejné BLE `cmd` na firmwaru.
    var supportsOnDeviceSettingsWrite: Bool {
        (wifiTransport != nil || bleTransport != nil) && !useMockBoard
    }

    /// Tovární vymazání celého NVS oddílu — Wi‑Fi nebo BLE (`factory_reset`).
    var supportsFactoryReset: Bool {
        (wifiTransport != nil || bleTransport != nil) && !useMockBoard
    }

    /// Důvod, proč nelze posílat tahy z aplikace, i když je přepínač zapnutý (`nil` = můžeš klepat na pole).
    var remoteChessFromAppBlockedReason: String? {
        if useMockBoard {
            return "U ukázkové šachovnice nelze posílat tahy na fyzickou desku."
        }
        if wifiTransport == nil && bleTransport == nil {
            return "Nejprve připoj šachovnici (Hra → Najít šachovnici)."
        }
        if puzzleBoardPreview != nil {
            return "V náhledu puzzle z aplikace se tahy na desku neposílají."
        }
        if snapshot?.status.webLocked == true {
            return "Partie je uzamčena z webu — tahy z aplikace jsou vypnuté, dokud se deska neodemkne."
        }
        return nil
    }

    /// Čistě BLE cesta (bez přepnutí na STA HTTP po `startBLETransport`).
    var isBluetoothOnlyBoardPath: Bool {
        bleTransport != nil && wifiTransport == nil && !useMockBoard
    }

    var connectionStallMessage: String? {
        guard activeLinkKind != .mock else { return nil }
        guard isPolling, snapshot == nil, let t = pollingStartedAt else { return nil }
        if Date().timeIntervalSince(t) > 18 {
            return "Deska neodpovídá déle než 18 s. Zkus znovu Najít šachovnici nebo zkontroluj síť."
        }
        return nil
    }

    private var evalTask: Task<Void, Never>?
    private var botSuggestionTask: Task<Void, Never>?
    var pollIntervalSuccess: TimeInterval = 1.0
    private var pollTask: Task<Void, Never>?
    /// Zrušený Wi‑Fi `pollLoop` / mock smyčka se vrátí bez `CancellationError` — dokončený starý `pollTask` by jinak přepsal `isPolling = false` i při už běžícím novém transportu (BLE po fallbacku).
    @ObservationIgnored
    private var pollTransportGeneration: UInt64 = 0

    private var wifiTransport: WiFiBoardTransport?
    private var bleTransport: BLEBoardTransport?
    private var mockTransport: MockBoardTransport?

    private let stockfish = StockfishAPIClient()

    /// Je aktivní některý transport (Wi‑Fi / BLE / mock) — lze posílat např. jas.
    var hasActiveBoardTransport: Bool { currentTransport != nil }

    /// Token pro `removeObserver` v `deinit`; není součástí `@Observable` stavu.
    @ObservationIgnored
    nonisolated(unsafe) private var bleCmdAckObserver: NSObjectProtocol?
    @ObservationIgnored
    nonisolated(unsafe) private var bleGattTrafficObserver: NSObjectProtocol?
    /// Omezí záplavu `Task` z NOTIFY snapshotů (jinak hlavní vlákno přetíží a UI zamrzne).
    @ObservationIgnored
    private var bleGattNotifyThrottleAnchor: Date = .distantPast

    init() {
        let saved = UserDefaults.standard.string(forKey: Self.defaultsKey) ?? "http://192.168.4.1"
        baseURLString = saved
        useMockBoard = UserDefaults.standard.bool(forKey: Self.mockKey)
        let d = UserDefaults.standard.integer(forKey: Self.depthKey)
        hintDepth = d == 0 ? 10 : min(18, max(1, d))
        let tierRaw = UserDefaults.standard.integer(forKey: Self.hintTierKey)
        moveHintTier = MoveHintTier(rawValue: tierRaw) ?? .fullMove
        if UserDefaults.standard.object(forKey: Self.moveEvalKey) == nil {
            moveEvaluationEnabled = true
        } else {
            moveEvaluationEnabled = UserDefaults.standard.bool(forKey: Self.moveEvalKey)
        }
        bleCmdAckObserver = NotificationCenter.default.addObserver(
            forName: .czechmateBleCommandAck,
            object: nil,
            queue: .main
        ) { [weak self] note in
            let data = note.userInfo?["jsonData"] as? Data
            Task { @MainActor [weak self] in
                self?.handleBleCommandAckPayload(data)
            }
        }
        bleGattTrafficObserver = NotificationCenter.default.addObserver(
            forName: .czechmateBleSnapshotNotifyTraffic,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            guard let self else { return }
            let now = Date()
            guard now.timeIntervalSince(self.bleGattNotifyThrottleAnchor) >= 0.2 else { return }
            self.bleGattNotifyThrottleAnchor = now
            self.recordBleGattActivityForTransportLink()
        }
    }

    deinit {
        if let o = bleCmdAckObserver {
            NotificationCenter.default.removeObserver(o)
        }
        if let o = bleGattTrafficObserver {
            NotificationCenter.default.removeObserver(o)
        }
    }

    private func recordBleGattActivityForTransportLink() {
        guard bleTransport != nil else { return }
        lastBleGattActivityAt = Date()
    }

    private func handleBleCommandAckPayload(_ data: Data?) {
        guard let data else { return }
        guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            AppDebugLog.staging("BLE cmd_ack: neplatný JSON")
            return
        }
        guard (obj["channel"] as? String) == "cmd_ack" else { return }
        let ok = (obj["ok"] as? Bool) ?? false
        let code = (obj["code"] as? String) ?? "?"
        let cmd = (obj["cmd"] as? String).flatMap { $0.isEmpty ? nil : $0 }
        let message = (obj["message"] as? String) ?? ""
        let cmdPart = cmd.map { " cmd=\($0)" } ?? ""
        let espVal = obj["esp"]
        let espPart: String = {
            if let i = espVal as? Int { return " esp=\(i)" }
            if let d = espVal as? Double { return " esp=\(Int(d))" }
            return ""
        }()
        let logLine = "ok=\(ok) code=\(code)\(cmdPart)\(espPart) — \(message)"
        AppDebugLog.boardBleCommandAck(logLine, ok: ok)
        recordBleGattActivityForTransportLink()
        if !ok {
            let ackCmd = (obj["cmd"] as? String) ?? ""
            if ackCmd != "setup_tutorial" {
                lastError = message.isEmpty ? "Deska odmítla příkaz (kód \(code))." : message
            }
        }
    }

    func prepareBluetoothForPermissions() {
        let b = BLEBoardTransport()
        b.preparePermissions()
    }

    /// Nastaví sledování změny Wi‑Fi rozhraní na telefonu (handoff HTTP ↔ BLE).
    func attachWifiPathHandoff(using monitor: NetworkStatusMonitor) {
        networkHandoffMonitor = monitor
        guard !wifiPathHandoffHooked else { return }
        wifiPathHandoffHooked = true
        monitor.onWiFiInterfaceForHandoffChanged = { [weak self] active in
            Task { @MainActor [weak self] in
                await self?.handlePhoneWiFiHandoffPathChange(isWiFiActive: active)
            }
        }
    }

    func dismissWiFiProvisionOffer() {
        offerWiFiProvisionSheet = false
    }

    /// Zapíše SSID/heslo na desku přes BLE, počká na STA a přepne transport na HTTP (telefon musí být na stejné LAN Wi‑Fi).
    func provisionBoardWiFiOverBLE(ssid: String, password: String) async -> String? {
        guard let b = bleTransport else {
            return "Nejprve připoj šachovnici přes Bluetooth."
        }
        guard let dev = Self.loadRestoredBLEDevice() else {
            return "Chybí uložené BLE zařízení — znovu připoj přes Najít šachovnici."
        }
        do {
            try await b.postWiFiSTAConfig(ssid: ssid, password: password)
        } catch {
            return error.localizedDescription
        }

        var gotIP: String?
        for _ in 0 ..< 40 {
            try? await Task.sleep(nanoseconds: 1_000_000_000)
            if let net = await b.fetchNetworkInfo(), net.online, let ip = net.staIP, !ip.isEmpty {
                gotIP = ip
                break
            }
        }
        guard let staIP = gotIP else {
            return "Deska se nepřipojila k Wi‑Fi v časovém limitu. Zkus síť 2,4 GHz (ne jen 5 GHz), heslo a dosah k routeru."
        }
        guard networkHandoffMonitor?.isWiFiInterfaceActive == true else {
            return "Na telefonu není aktivní Wi‑Fi — k IP \(staIP) teď HTTP nedojde. Zůstaň na Bluetooth nebo zapni Wi‑Fi."
        }

        let wifiURL = URL(string: "http://\(staIP)")!
        persistSuccessfulBluetoothSession(device: dev)
        lastKnownBoardStaIP = staIP
        baseURLString = wifiURL.absoluteString
        UserDefaults.standard.set(baseURLString, forKey: Self.defaultsKey)

        pollTask?.cancel()
        pollTask = nil
        linkHealthMonitorTask?.cancel()
        linkHealthMonitorTask = nil
        evalTask?.cancel()
        evalTask = nil
        botSuggestionTask?.cancel()
        botSuggestionTask = nil

        bleTransport = nil
        await b.stopSnapshotUpdates()

        AppDebugLog.staging(
            "provisionBoardWiFiOverBLE: STA OK ip=\(staIP) — start Wi‑Fi transport"
        )

        let w = WiFiBoardTransport(baseURL: wifiURL, useWebSocket: useWebSocket)
        w.onDiagnostics = { [weak self] ms, date in
            self?.lastRestRoundTripMs = ms
            self?.lastRestFetchAt = date
        }
        wifiTransport = w
        activeLinkKind = .wifiLAN
        isPolling = true
        pollingStartedAt = Date()
        persistLinkKindWiFi()
        offerWiFiProvisionSheet = false
        lastError = nil
        runTransport(w)
        Task { await syncBoardUIPrefsFromDevice() }
        return nil
    }

    @MainActor
    private func handlePhoneWiFiHandoffPathChange(isWiFiActive: Bool) async {
        guard isPolling, !useMockBoard else { return }

        if !isWiFiActive, wifiTransport != nil {
            AppDebugLog.staging("[handoff] Telefon ztratil Wi‑Fi — návrat na Bluetooth k desce")
            stopPolling(notifyWatchDisconnect: false)
            if let dev = Self.loadRestoredBLEDevice() {
                await startBLETransport(device: dev)
            } else {
                lastError = "Znovu připoj šachovnici přes Bluetooth (Najít šachovnici)."
            }
            return
        }

        if isWiFiActive, bleTransport != nil, wifiTransport == nil,
           !UserDefaults.standard.bool(forKey: Self.preferBluetoothOnlyKey) {
            await tryUpgradeBleToWifiIfPossible()
        }
    }

    @MainActor
    private func tryUpgradeBleToWifiIfPossible() async {
        guard networkHandoffMonitor?.isWiFiInterfaceActive == true else { return }
        guard let b = bleTransport else { return }
        guard let net = await b.fetchNetworkInfo(), net.online, let ip = net.staIP, !ip.isEmpty else {
            AppDebugLog.staging("[handoff] Telefon má Wi‑Fi, ale deska nehlásí STA — zůstávám na BLE")
            return
        }
        guard let dev = Self.loadRestoredBLEDevice() else { return }

        persistSuccessfulBluetoothSession(device: dev)
        lastKnownBoardStaIP = ip
        let wifiURL = URL(string: "http://\(ip)")!
        baseURLString = wifiURL.absoluteString
        UserDefaults.standard.set(baseURLString, forKey: Self.defaultsKey)

        AppDebugLog.staging("[handoff] Telefon znovu na Wi‑Fi — přepínám na HTTP \(wifiURL.absoluteString)")

        pollTask?.cancel()
        pollTask = nil
        linkHealthMonitorTask?.cancel()
        linkHealthMonitorTask = nil
        evalTask?.cancel()
        evalTask = nil
        botSuggestionTask?.cancel()
        botSuggestionTask = nil

        bleTransport = nil
        await b.stopSnapshotUpdates()

        let w = WiFiBoardTransport(baseURL: wifiURL, useWebSocket: useWebSocket)
        w.onDiagnostics = { [weak self] ms, date in
            self?.lastRestRoundTripMs = ms
            self?.lastRestFetchAt = date
        }
        wifiTransport = w
        activeLinkKind = .wifiLAN
        isPolling = true
        pollingStartedAt = Date()
        persistLinkKindWiFi()
        lastError = nil
        runTransport(w)
        Task { await syncBoardUIPrefsFromDevice() }
    }

    private func scheduleWifiProvisionOfferIfEligible() {
        guard networkHandoffMonitor?.isWiFiInterfaceActive == true else { return }
        guard !UserDefaults.standard.bool(forKey: Self.preferBluetoothOnlyKey) else { return }
        Task { @MainActor in
            try? await Task.sleep(nanoseconds: 800_000_000)
            guard self.bleTransport != nil, self.wifiTransport == nil, self.isPolling else { return }
            self.offerWiFiProvisionSheet = true
            AppDebugLog.staging("[handoff] Nabídka sheetu: Wi‑Fi provisioning přes BLE")
        }
    }

    func loadPuzzlePosition(fromFEN fen: String, trainingDeadline deadline: Date? = nil) {
        clearMoveEvaluationAndReviewStateForNewBoardContext()
        puzzleBoardPreview = FENParser.parseBoard(fromFEN: fen)
        puzzleTrainingDeadline = deadline
    }

    func clearPuzzlePosition() {
        puzzleBoardPreview = nil
        puzzleTrainingDeadline = nil
    }

    /// Před nahráním puzzle na desku uloží aktuální FEN ze snapshotu (pro „Vrátit předchozí partii“).
    func captureStateBeforePuzzleOnBoard() {
        guard let snap = snapshot else {
            prePuzzleBookmark = nil
            AppDebugLog.staging("captureStateBeforePuzzleOnBoard: žádný snapshot — záloha se nevytvoří")
            return
        }
        guard let fen = FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, history: snap.history) else {
            prePuzzleBookmark = nil
            AppDebugLog.staging("captureStateBeforePuzzleOnBoard: FEN nelze sestavit — záloha se nevytvoří")
            return
        }
        prePuzzleBookmark = PrePuzzleGameBookmark(
            capturedAt: Date(),
            restoreFEN: fen,
            previousMoveCount: snap.history.moves.count
        )
        AppDebugLog.staging("captureStateBeforePuzzleOnBoard: tahů=\(snap.history.moves.count)")
    }

    /// Vymaže lokální eval / nápovědu / replay — po nové hře s puzzle FEN na desce.
    private func clearMoveEvaluationAndReviewStateForNewBoardContext() {
        evalTask?.cancel()
        evalTask = nil
        botSuggestionTask?.cancel()
        botSuggestionTask = nil
        lastBotSuggestionKey = nil
        moveEvalHistory.removeAll()
        EvalHistoryRecorder.clear()
        lastMoveEvaluation = nil
        historyReviewMoveIndex = nil
        hintSquareFrom = nil
        hintSquareTo = nil
        setupWizardTargetSquare = nil
        lastHintSummary = nil
        AppDebugLog.staging("clearMoveEvaluationAndReviewStateForNewBoardContext")
    }

    /// Nahraje uloženou pozici před puzzle (`postNewGame` / FEN). Vymaže zálohu jen při úspěchu.
    @discardableResult
    func restorePrePuzzleGame() async -> Bool {
        guard let mark = prePuzzleBookmark else {
            lastError = "Není uložená partie před puzzle."
            return false
        }
        guard supportsRemoteChessCommands else {
            lastError = noBoardConnectionMessage()
            return false
        }
        if let existing = boardSetupManager {
            await existing.cancel()
            boardSetupManager = nil
        }
        clearPuzzlePosition()
        let ok: Bool
        if let fen = mark.restoreFEN, !fen.isEmpty {
            ok = await postNewGameWithFENToBoard(fen)
        } else {
            ok = await postNewGameWithoutFENReturningBool()
        }
        if ok {
            clearMoveEvaluationAndReviewStateForNewBoardContext()
            prePuzzleBookmark = nil
            lastError = nil
            AppDebugLog.staging("restorePrePuzzleGame: obnoveno, tahů před puzzle byl \(mark.previousMoveCount)")
        } else {
            AppDebugLog.staging("restorePrePuzzleGame: selhalo — \(lastError ?? "?")")
        }
        return ok
    }

    private func postNewGameWithFENToBoard(_ fen: String) async -> Bool {
        if let w = wifiTransport {
            do {
                try await w.postNewGame(fen: fen)
                await refreshSnapshotWiFi()
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        if let b = bleTransport {
            do {
                try await b.postNewGame(fen: fen)
                await refreshSnapshotWiFi()
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        lastError = noBoardConnectionMessage()
        return false
    }

    private func postNewGameWithoutFENReturningBool() async -> Bool {
        await cancelBoardSetupWizardIfActive()
        if let w = wifiTransport {
            do {
                try await w.postNewGame()
                await refreshSnapshotWiFi()
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        if let b = bleTransport {
            do {
                try await b.postNewGame()
                await refreshSnapshotWiFi()
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        lastError = noBoardConnectionMessage()
        return false
    }

    func saveSettings() {
        UserDefaults.standard.set(baseURLString, forKey: Self.defaultsKey)
        UserDefaults.standard.set(hintDepth, forKey: Self.depthKey)
        UserDefaults.standard.set(moveHintTier.rawValue, forKey: Self.hintTierKey)
        UserDefaults.standard.set(moveEvaluationEnabled, forKey: Self.moveEvalKey)
        boardUIPrefsPayload.applyLocalHintAndEval(hintDepth: hintDepth, moveEvaluationEnabled: moveEvaluationEnabled)
        Task {
            if let w = wifiTransport {
                guard let u = URL(string: baseURLString.trimmingCharacters(in: .whitespacesAndNewlines)) else { return }
                await w.setBaseURL(u)
            }
        }
    }

    func startPolling() {
        if shouldSkipPollingRestart() {
            return
        }
        stopPolling()
        if useMockBoard {
            startMockTransport()
            return
        }
        guard let u = URL(string: baseURLString.trimmingCharacters(in: .whitespacesAndNewlines)) else {
            lastError = ChessboardAPIError.invalidBaseURL.localizedDescription
            AppDebugLog.staging("startPolling: neplatná baseURL „\(baseURLString)“")
            return
        }
        startWiFiTransport(url: u)
    }

    /// Obnovení spojení po startu aplikace: ukázková deska, volitelně poslední Bluetooth, jinak Wi‑Fi polling podle uložené URL.
    func resumeConnectionIfNeeded() async {
        if shouldSkipPollingRestart() {
            AppDebugLog.staging("resumeConnectionIfNeeded: přeskočeno — aktivní transport už běží")
            return
        }
        if useMockBoard {
            startMockTransport()
            return
        }
        let preferBLEOnly = UserDefaults.standard.bool(forKey: Self.preferBluetoothOnlyKey)
        let savedBLE = Self.loadRestoredBLEDevice()
        if preferBLEOnly, let dev = savedBLE {
            AppDebugLog.staging("resumeConnectionIfNeeded: preferBluetoothOnly → zkouším BLE „\(dev.displayName)“ (\(dev.id))")
            await startBLETransport(device: dev)
            if bleTransport != nil, isPolling {
                AppDebugLog.staging("resumeConnectionIfNeeded: Bluetooth obnoven")
                return
            }
            AppDebugLog.staging("resumeConnectionIfNeeded: BLE se nepodařil — fallback startPolling()")
            startPolling()
            return
        }
        let kind = UserDefaults.standard.string(forKey: Self.lastBoardLinkKindKey)
        if kind == Self.linkKindBluetooth, let dev = savedBLE {
            AppDebugLog.staging("resumeConnectionIfNeeded: poslední režim Bluetooth → zkouším „\(dev.displayName)“")
            await startBLETransport(device: dev)
            if bleTransport != nil, isPolling {
                AppDebugLog.staging("resumeConnectionIfNeeded: Bluetooth obnoven")
                return
            }
            AppDebugLog.staging("resumeConnectionIfNeeded: Bluetooth neobnoven — fallback startPolling()")
        }
        startPolling()
    }

    /// Uložená deska z posledního úspěšného BLE spojení (pro tlačítko „Znovu připojit“).
    var lastSavedBLEBoardForReconnect: DiscoveredBoardDevice? {
        Self.loadRestoredBLEDevice()
    }

    /// Znovu připojit poslední BLE desku (`retrievePeripherals` — skenovat nemusíš).
    func reconnectLastSavedBLEBoard() async {
        guard let dev = Self.loadRestoredBLEDevice() else { return }
        await startBLETransport(device: dev)
    }

    private static func loadRestoredBLEDevice() -> DiscoveredBoardDevice? {
        guard let id = UserDefaults.standard.string(forKey: Self.lastBLEDeviceIdKey),
              UUID(uuidString: id) != nil
        else { return nil }
        let name = UserDefaults.standard.string(forKey: Self.lastBLEDeviceNameKey) ?? "CZECHMATE"
        return DiscoveredBoardDevice(id: id, displayName: name, transport: .bluetooth)
    }

    private func persistLinkKindMock() {
        UserDefaults.standard.set(Self.linkKindMock, forKey: Self.lastBoardLinkKindKey)
    }

    private func persistLinkKindWiFi() {
        UserDefaults.standard.set(Self.linkKindWiFi, forKey: Self.lastBoardLinkKindKey)
    }

    private func persistSuccessfulBluetoothSession(device: DiscoveredBoardDevice) {
        UserDefaults.standard.set(Self.linkKindBluetooth, forKey: Self.lastBoardLinkKindKey)
        UserDefaults.standard.set(device.id, forKey: Self.lastBLEDeviceIdKey)
        UserDefaults.standard.set(device.displayName, forKey: Self.lastBLEDeviceNameKey)
        AppDebugLog.staging("BoardConnectionStore: uložen poslední BLE \(device.displayName) id=\(device.id)")
    }

    /// Zabrání zbytečnému `stopPolling` při už souladném transportu (např. po `pickWiFi`).
    private func shouldSkipPollingRestart() -> Bool {
        guard isPolling else { return false }
        if useMockBoard {
            return mockTransport != nil && wifiTransport == nil && bleTransport == nil
        }
        if mockTransport != nil { return false }
        if wifiTransport != nil, bleTransport == nil { return true }
        if bleTransport != nil, wifiTransport == nil { return true }
        return false
    }

    func startMockTransport() {
        stopPolling()
        AppDebugLog.staging("Mock board transport (bez ESP)")
        let m = MockBoardTransport()
        mockTransport = m
        activeLinkKind = .mock
        isPolling = true
        pollingStartedAt = Date()
        persistLinkKindMock()
        runTransport(m)
    }

    func startWiFiTransport(url: URL) {
        stopPolling()
        pairingPhaseMessage = "Navazuji komunikaci s deskou přes Wi‑Fi…"
        UserDefaults.standard.set(url.absoluteString, forKey: Self.defaultsKey)
        baseURLString = url.absoluteString
        if BoardLANHostHints.looksLikeLANOnlyBoardAddress(host: url.host),
           networkHandoffMonitor?.isWiFiInterfaceActive == false {
            AppDebugLog.staging(
                "WiFi transport \(url.absoluteString): na telefonu není aktivní Wi‑Fi — HTTP k LAN IP obvykle selže (mobilní data soukromou IP nedoručí). Připoj Wi‑Fi k síti desky, nebo Bluetooth."
            )
        }
        AppDebugLog.staging("WiFi transport: \(url.absoluteString) useWebSocket=\(useWebSocket)")
        let w = WiFiBoardTransport(baseURL: url, useWebSocket: useWebSocket)
        w.onDiagnostics = { [weak self] ms, date in
            self?.lastRestRoundTripMs = ms
            self?.lastRestFetchAt = date
        }
        wifiTransport = w
        activeLinkKind = .wifiLAN
        isPolling = true
        pollingStartedAt = Date()
        persistLinkKindWiFi()
        if let host = url.host, !host.isEmpty {
            lastKnownBoardStaIP = host
        }
        runTransport(w)
        pairingPhaseMessage = "Čekám na první data z desky…"
        Task { await syncBoardUIPrefsFromDevice() }
    }

    func startBLETransport(device: DiscoveredBoardDevice) async {
        stopPolling()
        ChessboardBLEClient.shared.onPairingPhase = { [weak self] text in
            Task { @MainActor in
                self?.pairingPhaseMessage = text
            }
        }

        pairingPhaseMessage = "Připojuji k „\(device.displayName)“…"
        let b = BLEBoardTransport()
        bleTransport = b
        activeLinkKind = .bluetooth
        isPolling = true
        pollingStartedAt = Date()
        lastError = nil
        do {
            try await b.connect(to: device)
            AppDebugLog.staging("BLE: připojeno \(device.displayName) id=\(device.id)")

            pairingPhaseMessage = "Čtu síťové údaje z desky (volitelné)…"
            // Zkus získat IP adresu z BLE network charakteristiky
            if let netInfo = await b.fetchNetworkInfo() {
                AppDebugLog.staging("BLE network info: staIP=\(netInfo.staIP ?? "nil") apIP=\(netInfo.apIP ?? "nil") online=\(netInfo.online)")

                // STA IP + deska „online“ ještě neznamená, že i telefon dosáhne na LAN (bez Wi‑Fi k téže síti HTTP na 192.168.x nejde).
                let phoneWiFi = networkHandoffMonitor?.isWiFiInterfaceActive ?? false
                let preferBT = UserDefaults.standard.bool(forKey: Self.preferBluetoothOnlyKey)
                if !preferBT,
                   let staIP = netInfo.staIP, !staIP.isEmpty, netInfo.online, phoneWiFi {
                    let wifiURL = URL(string: "http://\(staIP)")!
                    AppDebugLog.staging(
                        "BLE→Wi‑Fi přepnutí: url=\(wifiURL.absoluteString) preferBluetoothOnly=\(preferBT) phoneWiFi=\(phoneWiFi) staOnline=\(netInfo.online) — důvod: STA dostupná a telefon na Wi‑Fi"
                    )
                    persistSuccessfulBluetoothSession(device: device)
                    lastKnownBoardStaIP = staIP
                    baseURLString = wifiURL.absoluteString
                    UserDefaults.standard.set(baseURLString, forKey: Self.defaultsKey)
                    let bRef = b
                    bleTransport = nil
                    pairingPhaseMessage = "Přepínám na rychlejší spojení přes domácí Wi‑Fi…"
                    await bRef.stopSnapshotUpdates()
                    startWiFiTransport(url: wifiURL)
                    return
                }
                if netInfo.online, let staIP = netInfo.staIP, !staIP.isEmpty, preferBT {
                    AppDebugLog.staging(
                        "BLE: zůstávám na GATT — preferBluetoothOnly=true (STA \(staIP) ignorována pro HTTP přepnutí)"
                    )
                }
                if netInfo.online, let staIP = netInfo.staIP, !staIP.isEmpty, !phoneWiFi {
                    AppDebugLog.staging(
                        "BLE: deska hlásí STA \(staIP) a online, ale telefon nemá aktivní Wi‑Fi — zůstávám na Bluetooth (jinak by HTTP selhalo jako offline / -1009)."
                    )
                }

                // Pokud nemáme STA IP ale máme AP IP, ulož ji pro reference
                if let apIP = netInfo.apIP, !apIP.isEmpty {
                    // Fallback: použij AP IP pokud je v budoucnu potřeba
                    AppDebugLog.staging("BLE AP IP dostupné: \(apIP) (použij při připojení k hotspotu)")
                }
            }

            // Pokud nemáme WiFi IP, pokračuj s BLE transportem
            persistSuccessfulBluetoothSession(device: device)
            runTransport(b)
            scheduleWifiProvisionOfferIfEligible()
        } catch {
            AppDebugLog.stagingError("BLE connect", error)
            lastError = error.localizedDescription
            pairingPhaseMessage = nil
            ChessboardBLEClient.shared.onPairingPhase = nil
            isPolling = false
            bleTransport = nil
        }
    }

    private func startLinkHealthMonitoring() {
        linkHealthMonitorTask?.cancel()
        linkHealthMonitorTask = Task { @MainActor [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: 5_000_000_000)
                guard let self else { break }
                guard self.isPolling, !self.useMockBoard else { continue }
                self.linkHealthTick &+= 1
            }
        }
    }

    private func runTransport(_ transport: any BoardTransport) {
        startLinkHealthMonitoring()
        pollTransportGeneration += 1
        let generation = pollTransportGeneration
        pollTask = Task {
            do {
                try await transport.startSnapshotUpdates { [weak self] snap in
                    Task { @MainActor [weak self] in
                        self?.applySnapshot(snap)
                    }
                }
            } catch is CancellationError {
                AppDebugLog.staging("Transport snapshot task zrušen (stopPolling / nové připojení)")
            } catch {
                AppDebugLog.stagingError("Transport startSnapshotUpdates", error)
                await MainActor.run {
                    self.lastError = error.localizedDescription
                }
            }
            await MainActor.run { [weak self] in
                guard let self else { return }
                guard self.pollTransportGeneration == generation else { return }
                self.isPolling = false
            }
        }
    }

    private func applySnapshot(_ snap: GameSnapshot) {
        if pairingPhaseMessage != nil {
            pairingPhaseMessage = nil
            ChessboardBLEClient.shared.onPairingPhase = nil
        }
        if let w = wifiTransport {
            isWebSocketLive = w.isWebSocketLive
        }
        let previous = snapshot
        var snap = snap
        if let deadline = lampStabilizationDeadline, Date() < deadline,
           let prev = previous, prev.status.lightState == true, snap.status.lightState == false
        {
            let merged = snap.status.coalescingTransientLampOff(previousLampOn: prev.status)
            snap = snap.replacingStatus(merged)
            #if DEBUG
            AppDebugLog.staging("[lamp] snapshot: přechodné light_state=false sloučeno (stabilizační okno)")
            #endif
        }
        if let prev = previous, !Self.isSnapshotAcceptable(snap, versus: prev) {
            AppDebugLog.staging(
                "Snapshot přeskočen (starší verze/čas): ver \(String(describing: snap.stateVersion))→\(String(describing: prev.stateVersion)) ts \(snap.timestamp)/\(prev.timestamp)"
            )
            /* BLE NOTIFY může opakovat stejný snímek — bez „heartbeat“ by nepadl `snapshotReceivedAt` a UI by
             * po pár minutách hlásilo odpojení i při živém GATT. */
            if bleTransport != nil {
                snapshotReceivedAt = Date()
            }
            return
        }
        #if DEBUG
        if wifiTransport != nil, isWebSocketLive {
            webSocketFrameCount += 1
        }
        #endif
        mergeChessHintLimitFromSnapshotStatus(snap.status)
        snapshot = snap
        snapshotReceivedAt = Date()
        if let d = lampStabilizationDeadline, Date() >= d {
            lampStabilizationDeadline = nil
        }
        lastError = nil
        #if os(iOS)
        if wifiTransport != nil {
            timerRefreshLatestId += 1
            let rid = timerRefreshLatestId
            Task { await self.refreshTimerThrottledThenCompanionSync(snapshot: snap, refreshId: rid) }
        } else {
            // BLE: stejný JSON jako z HTTP často obsahuje `clock` — použij ho pro Watch / UI místo vždy `nil`.
            if let c = snap.clock {
                lastTimerState = c
                timerClockReceivedAt = Date()
                lastTimerFetchMoveCount = snap.history.moves.count
                lastTimerHttpFetchAt = Date()
                #if DEBUG
                recordStagingTimerFromSnapshot()
                #endif
            }
            BoardCompanionSync.onSnapshotUpdated(
                snap,
                timer: GameBoardTimerDisplay.resolvedClock(lastTimer: lastTimerState, snapshot: snap)
            )
        }
        #endif
        if let prev = previous {
            resetHintPoolIfNewGame(previous: prev, current: snap)
            scheduleMoveEvaluation(previous: prev, current: snap)
        }
        boardSetupManager?.consumeSnapshot(snap)
        maybeSuggestBotMove(current: snap)
    }

    #if os(iOS)
    /// Clock ze snapshotu (nový firmware) nebo `/api/timer` (throttle + nový tah vynutí fetch).
    /// Po každém **novém tahu** v historii vždy synchronizuje čas — jinak krátký throttle mohl přeskočit aktualizaci a UI ukazovalo zastaralé časy (hodiny „nežily“).
    private func refreshTimerThrottledThenCompanionSync(snapshot: GameSnapshot, refreshId: UInt64) async {
        guard let w = wifiTransport else {
            BoardCompanionSync.onSnapshotUpdated(
                snapshot,
                timer: GameBoardTimerDisplay.resolvedClock(lastTimer: lastTimerState, snapshot: snapshot)
            )
            return
        }
        let moveCount = snapshot.history.moves.count

        if let clock = snapshot.clock {
            guard refreshId == timerRefreshLatestId else { return }
            lastTimerState = clock
            timerClockReceivedAt = Date()
            lastTimerFetchMoveCount = moveCount
            lastTimerHttpFetchAt = Date()
            lastError = nil
            #if DEBUG
            recordStagingTimerFromSnapshot()
            #endif
            BoardCompanionSync.onSnapshotUpdated(
                snapshot,
                timer: GameBoardTimerDisplay.resolvedClock(lastTimer: lastTimerState, snapshot: snapshot)
            )
            return
        }

        let newMoveOrMismatch = moveCount != lastTimerFetchMoveCount
        let now = Date()
        let throttleOk = now.timeIntervalSince(lastTimerHttpFetchAt) >= 0.35
        let shouldFetch = newMoveOrMismatch || throttleOk
        if shouldFetch {
            do {
                let t = try await w.fetchTimerState()
                guard refreshId == timerRefreshLatestId else { return }
                lastTimerHttpFetchAt = Date()
                lastTimerState = t
                timerClockReceivedAt = Date()
                lastTimerFetchMoveCount = moveCount
                lastError = nil
                #if DEBUG
                recordStagingTimerHttpFetch()
                #endif
            } catch {
                AppDebugLog.stagingError("GET /api/timer po snapshotu", error)
            }
        }
        guard refreshId == timerRefreshLatestId else { return }
        BoardCompanionSync.onSnapshotUpdated(
            snapshot,
            timer: GameBoardTimerDisplay.resolvedClock(lastTimer: lastTimerState, snapshot: snapshot)
        )
    }
    #endif

    /// Ochrana před „cukáním“ UI při zrychleném střídání WS a REST nebo mimořadém pořadí rámců.
    /// Pozn.: `timestamp` z desky nemusí být monotónní (reboot, wrap). Bez `state_version` (některé BLE notifikace)
    /// nesmíme odmítat nový stav jen kvůli nižšímu času — řídíme se obsahem (historie / deska / move_count).
    private static func isSnapshotAcceptable(_ new: GameSnapshot, versus old: GameSnapshot) -> Bool {
        switch (new.stateVersion, old.stateVersion) {
        case let (n?, o?) where n < o:
            return false
        case let (n?, o?) where n > o:
            return true
        case let (n?, o?) where n == o:
            if new.history.moves.isEmpty, old.history.moves.isEmpty,
               new.board == old.board, new.status.moveCount == old.status.moveCount {
                return true
            }
            // Stejná revize: odmítnout jen evidentně starší duplicitu (nižší čas, beze změny pozice).
            if new.timestamp < old.timestamp,
               new.history.moves.count == old.history.moves.count,
               new.board == old.board {
                return false
            }
            return true
        default:
            break
        }
        // Bez tahů a stejná pozice: povolit i snímek s nižším časem (reboot / duplicitní NOTIFY).
        if new.history.moves.isEmpty, old.history.moves.isEmpty,
           new.board == old.board, new.status.moveCount == old.status.moveCount {
            return true
        }
        if new.history.moves.count != old.history.moves.count {
            return true
        }
        if new.board != old.board {
            return true
        }
        if new.status.moveCount != old.status.moveCount {
            return true
        }
        if new.timestamp >= old.timestamp {
            return true
        }
        return false
    }

    /// - Parameter notifyWatchDisconnect: `true` jen když uživatel explicitně zastaví připojení (záložka Nastavení). Při přepnutí transportu / `startPolling` nechat `false`, aby Watch nedostával falešné „connection lost“.
    func stopPolling(notifyWatchDisconnect: Bool = false) {
        linkHealthMonitorTask?.cancel()
        linkHealthMonitorTask = nil
        pollTask?.cancel()
        pollTask = nil
        evalTask?.cancel()
        evalTask = nil
        botSuggestionTask?.cancel()
        botSuggestionTask = nil
        isPolling = false
        pollingStartedAt = nil
        isWebSocketLive = false
        lastRestFetchAt = nil
        lastRestRoundTripMs = nil
        #if DEBUG
        webSocketFrameCount = 0
        #endif
        snapshot = nil
        snapshotReceivedAt = nil
        lastBleGattActivityAt = nil
        lastTimerState = nil
        timerClockReceivedAt = nil
        lastTimerHttpFetchAt = .distantPast
        lastTimerFetchMoveCount = -1
        timerRefreshLatestId = 0
        #if DEBUG
        stagingTimerHttpFetchCount = 0
        stagingTimerFromSnapshotCount = 0
        stagingTimerMetricsWindowStart = Date()
        #endif
        lastError = nil
        hintSquareFrom = nil
        hintSquareTo = nil
        setupWizardTargetSquare = nil
        lastHintSummary = nil
        lastMoveEvaluation = nil
        historyReviewMoveIndex = nil
        lastBotSuggestionKey = nil
        pairingPhaseMessage = nil
        ChessboardBLEClient.shared.onPairingPhase = nil
        #if os(iOS)
        BoardCompanionSync.onConnectionStopped(notifyWatchConnectionLost: notifyWatchDisconnect)
        #endif
        let m = mockTransport
        let b = bleTransport
        let w = wifiTransport
        mockTransport = nil
        bleTransport = nil
        wifiTransport = nil
        Task {
            if let x = m { await x.stopSnapshotUpdates() }
            if let x = b { await x.stopSnapshotUpdates() }
            if let x = w { await x.stopSnapshotUpdates() }
        }
    }

    func requestHint(internetPathSatisfied: Bool = true) async {
        lastHintSummary = nil
        if !internetPathSatisfied {
            lastError = "K nápovědě Stockfish je potřeba internet — Wi‑Fi nebo mobilní data (chess-api)."
            return
        }
        guard let snap = snapshot else {
            lastError = "Nejdřív načti stav šachovnice (spusť připojení)."
            return
        }
        if snap.status.webLocked == true {
            lastError = ChessboardAPIError.webLocked.localizedDescription
            return
        }
        let limit = boardUIPrefsPayload.chessHintLimit
        if limit > 0 {
            let whiteTurn = snap.status.currentPlayer.lowercased() == "white"
            let rem = whiteTurn ? hintsRemainingWhite : hintsRemainingBlack
            if rem <= 0 {
                lastError = "Došly nápovědy pro tuto stranu (limit z desky)."
                return
            }
        }
        guard let fen = FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, history: snap.history) else {
            lastError = "Nepodařilo se sestavit FEN."
            return
        }
        do {
            let best = try await stockfish.fetchBestMove(fen: fen, depth: hintDepth)
            guard let t = currentTransport else {
                lastError = "Není aktivní připojení k desce."
                return
            }
            #if DEBUG
            AppDebugLog.staging("requestHint tier=\(moveHintTier.rawValue) best=\(best.from)→\(best.to)")
            #endif
            try await postHintForTier(moveHintTier, best: best, transport: t)
            lastError = nil
            if limit > 0 {
                if snap.status.currentPlayer.lowercased() == "white" {
                    hintsRemainingWhite = max(0, hintsRemainingWhite - 1)
                } else {
                    hintsRemainingBlack = max(0, hintsRemainingBlack - 1)
                }
            }
            applyHintOverlaySquares(tier: moveHintTier, best: best)
            await refreshSnapshotWiFi()
            lastHintSummary = moveHintTier.summaryLine(best: best)
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    private func postHintForTier(_ tier: MoveHintTier, best: StockfishBestMove, transport: any BoardTransport) async throws {
        switch tier {
        case .destinationOnly:
            try await transport.postHintHighlightDestinationOnly(to: best.to)
        case .originOnly:
            try await transport.postHintHighlightDestinationOnly(to: best.from)
        case .fullMove:
            try await transport.postHintHighlight(from: best.from, to: best.to)
        }
    }

    private func applyHintOverlaySquares(tier: MoveHintTier, best: StockfishBestMove) {
        switch tier {
        case .destinationOnly:
            hintSquareFrom = nil
            hintSquareTo = best.to.lowercased()
        case .originOnly:
            hintSquareFrom = best.from.lowercased()
            hintSquareTo = nil
        case .fullMove:
            hintSquareFrom = best.from.lowercased()
            hintSquareTo = best.to.lowercased()
        }
    }

    func clearHintLED() async {
        guard let t = currentTransport else {
            lastError = "Není aktivní připojení k desce."
            return
        }
        do {
            try await t.postHintClear()
            lastError = nil
            hintSquareFrom = nil
            hintSquareTo = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    /// Vrátí poslední tah na desce přes BLE (`cmd: undo`). U Wi‑Fi / mock zobrazí vysvětlení v `lastError`.
    func requestUndo() async {
        if snapshot?.status.webLocked == true {
            lastError = ChessboardAPIError.webLocked.localizedDescription
            return
        }
        guard mockTransport == nil else {
            lastError = "V ukázkovém režimu není zpět k dispozici."
            return
        }
        guard let b = bleTransport else {
            lastError = "Zpět tahu z aplikace funguje přes Bluetooth. U čistě Wi‑Fi připojení použij webové rozhraní šachovnice."
            return
        }
        guard (snapshot?.status.moveCount ?? 0) > 0 else { return }
        do {
            try await b.postUndo()
            lastError = nil
            await refreshSnapshotFromServer()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    // MARK: - AI trenér / okamžitá analýza (chess-api Stockfish)

    enum CoachStockfishError: Error, Sendable {
        case noSnapshot
        case noFEN
    }

    /// Aktuální FEN a čerstvý nejlepší tah + eval z API (stejná hloubka jako nápověda).
    func fetchCoachStockfishAnalysis() async throws -> (fen: String, best: StockfishBestMove) {
        guard let snap = snapshot else { throw CoachStockfishError.noSnapshot }
        guard let fen = FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, history: snap.history) else {
            throw CoachStockfishError.noFEN
        }
        let best = try await stockfish.fetchBestMove(fen: fen, depth: hintDepth)
        return (fen, best)
    }

    /// Eval z perspektivy bílého (jako `MoveEvaluation`) pro dané FEN — pro záchrannou brzdu.
    func fetchEvalWhiteForFEN(_ fen: String) async throws -> Double? {
        let best = try await stockfish.fetchBestMove(fen: fen, depth: hintDepth)
        guard let raw = best.evalPawns else { return nil }
        return MoveEvaluation.evalToWhitePerspective(fen: fen, evalRaw: raw)
    }

    /// Okamžitý GET snapshot (gesto „stáhnout“ / pull-to-refresh).
    func refreshSnapshotFromServer() async {
        guard isPolling else { return }
        do {
            if let w = wifiTransport {
                let snap = try await w.fetchSnapshotNow()
                applySnapshot(snap)
                lastError = nil
                return
            }
            if let b = bleTransport {
                if let snap = try await b.fetchSnapshot(ifNoneMatch: nil) {
                    applySnapshot(snap)
                }
                lastError = nil
                return
            }
            if let m = mockTransport, let snap = try await m.fetchSnapshot(ifNoneMatch: nil) {
                applySnapshot(snap)
                lastError = nil
                return
            }
        } catch {
            AppDebugLog.stagingError("refreshSnapshotFromServer", error)
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func pingBoardRTT() async -> String {
        if let w = wifiTransport {
            let ms = await w.pingRTT()
            lastError = nil
            return ms
        }
        guard let u = URL(string: baseURLString.trimmingCharacters(in: .whitespacesAndNewlines)) else {
            return "Neplatná URL"
        }
        let temp = WiFiBoardTransport(baseURL: u, useWebSocket: false)
        await temp.setBaseURL(u)
        return await temp.pingRTT()
    }

    func setBoardBrightness(percent: Int) async {
        guard let t = currentTransport else {
            lastError = "Není aktivní připojení k desce."
            return
        }
        do {
            try await t.postBrightness(percent: percent)
            lastError = nil
            markLampStabilizationWindow()
            scheduleDeferredSnapshotRefreshAfterLampCommand()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    private func markLampStabilizationWindow() {
        lampStabilizationDeadline = Date().addingTimeInterval(0.5)
    }

    private func clearLampStabilizationAndCancelDeferredRefresh() {
        lampStabilizationDeadline = nil
        deferredSnapshotRefreshAfterLampTask?.cancel()
        deferredSnapshotRefreshAfterLampTask = nil
    }

    /// Ihned po POST jasu/barvy GET snapshot často vrátí chvíli nekonzistentní `light_state` — počkáme a pak synchronizujeme.
    private func scheduleDeferredSnapshotRefreshAfterLampCommand() {
        deferredSnapshotRefreshAfterLampTask?.cancel()
        deferredSnapshotRefreshAfterLampTask = Task { @MainActor [weak self] in
            guard let self else { return }
            try? await Task.sleep(nanoseconds: 450_000_000)
            guard !Task.isCancelled else { return }
            await self.refreshSnapshotWiFi()
        }
    }

    private func scheduleMoveEvaluation(previous: GameSnapshot, current: GameSnapshot) {
        guard moveEvaluationEnabled else { return }
        evalTask?.cancel()
        evalTask = Task { @MainActor in
            await self.runMoveEvaluation(previous: previous, current: current)
        }
    }

    private func runMoveEvaluation(previous: GameSnapshot, current: GameSnapshot) async {
        guard !Task.isCancelled else { return }
        guard current.status.moveCount > previous.status.moveCount else { return }
        guard current.history.moves.count == previous.history.moves.count + 1 else { return }
        guard let last = current.history.moves.last else { return }
        guard let fenBefore = FENBuilder.boardAndStatusToFEN(
            board: previous.board,
            status: previous.status,
            history: previous.history
        ),
            let fenAfter = FENBuilder.boardAndStatusToFEN(
                board: current.board,
                status: current.status,
                history: current.history
            )
        else { return }

        let depth = min(18, max(12, hintDepth))
        do {
            guard !Task.isCancelled else { return }
            guard let playedFrom = last.from, let playedTo = last.to else { return }
            let bestLine = try await stockfish.fetchBestMove(fen: fenBefore, depth: depth)
            let played = MoveEvaluation.normalizeUci(from: playedFrom, to: playedTo)
            let bestUci = MoveEvaluation.normalizeUci(from: bestLine.from, to: bestLine.to)
            let eb = bestLine.evalPawns.map { MoveEvaluation.evalToWhitePerspective(fen: fenBefore, evalRaw: $0) }
            let idx = current.history.moves.count

            guard !Task.isCancelled else { return }
            let afterProbe = try await stockfish.fetchBestMove(fen: fenAfter, depth: depth)
            let ea = afterProbe.evalPawns.map { MoveEvaluation.evalToWhitePerspective(fen: fenAfter, evalRaw: $0) }

            if played == bestUci {
                lastMoveEvaluation = MoveEvaluationResult(
                    grade: .best,
                    message: "Výborný tah! Byl to nejlepší tah."
                )
                recordMoveEvalHistory(moveIndex1Based: idx, evalWhitePawns: ea, grade: .best, lossPawns: 0)
                return
            }

            let bestFmt = "\(bestLine.from)-\(bestLine.to)"
            lastMoveEvaluation = MoveEvaluation.classify(
                playedFrom: playedFrom,
                playedTo: playedTo,
                bestFrom: bestLine.from,
                bestTo: bestLine.to,
                evalBeforeWhite: eb,
                evalAfterWhite: ea,
                moveIndex1Based: idx,
                bestMoveFormatted: bestFmt
            )
            var lossPawns: Double?
            if let eb = eb, let ea = ea {
                let whiteJustMoved = (idx - 1) % 2 == 0
                var scoreDrop = whiteJustMoved ? (eb - ea) : (ea - eb)
                if scoreDrop < 0 { scoreDrop = 0 }
                lossPawns = scoreDrop
            }
            if let ev = lastMoveEvaluation {
                recordMoveEvalHistory(
                    moveIndex1Based: idx,
                    evalWhitePawns: ea,
                    grade: ev.grade,
                    lossPawns: lossPawns
                )
                grantHintRewardsIfNeeded(previous: previous, current: current, evaluation: ev, lastMove: last)
            }
        } catch {
            if Task.isCancelled { return }
            lastMoveEvaluation = MoveEvaluationResult(grade: .error, message: NetworkErrorFormatter.userMessage(for: error))
        }
    }

    private func grantHintRewardsIfNeeded(
        previous: GameSnapshot,
        current: GameSnapshot,
        evaluation: MoveEvaluationResult,
        lastMove: HistoryMove
    ) {
        let limit = boardUIPrefsPayload.chessHintLimit
        guard limit > 0 else { return }
        let movedWhite = (current.history.moves.count % 2) == 1
        if boardUIPrefsPayload.botSettings.isBotMode {
            let humanWhite = !boardUIPrefsPayload.botSettings.botSideIsWhite()
            if movedWhite != humanWhite { return }
        }
        var grant = false
        switch evaluation.grade {
        case .best:
            grant = boardUIPrefsPayload.chessHintAwardBest
        case .good:
            grant = boardUIPrefsPayload.chessHintAwardGood
        default:
            break
        }
        if !grant, boardUIPrefsPayload.chessHintAwardCapture, isCaptureOnBoard(previous: previous, move: lastMove) {
            grant = true
        }
        guard grant else { return }
        if movedWhite {
            hintsRemainingWhite += 1
        } else {
            hintsRemainingBlack += 1
        }
    }

    private func isCaptureOnBoard(previous: GameSnapshot, move: HistoryMove) -> Bool {
        guard let toSquare = move.to, let idx = FirmwareSquareNotation.indices(from: toSquare) else { return false }
        let cell = previous.board[idx.row][idx.col].trimmingCharacters(in: .whitespaces)
        guard !cell.isEmpty, cell != " " else { return false }
        let moving = (move.piece ?? "").trimmingCharacters(in: .whitespaces)
        let isWhiteMove = moving.first?.isUppercase == true
        let target = cell
        let targetWhite = target.first?.isUppercase == true
        return isWhiteMove != targetWhite
    }

    private func resetHintPoolIfNewGame(previous: GameSnapshot, current: GameSnapshot) {
        let emptied = current.history.moves.isEmpty && !previous.history.moves.isEmpty
        let shorter = current.history.moves.count < previous.history.moves.count
        let counterReset = current.status.moveCount == 0 && previous.status.moveCount > 0
        if emptied || shorter || counterReset {
            reseedHintPool()
            moveEvalHistory.removeAll()
        }
    }

    private func recordMoveEvalHistory(
        moveIndex1Based: Int,
        evalWhitePawns: Double?,
        grade: MoveGrade,
        lossPawns: Double?
    ) {
        moveEvalHistory.removeAll { $0.moveIndex1Based == moveIndex1Based }
        moveEvalHistory.append(
            MoveEvalHistoryEntry(
                moveIndex1Based: moveIndex1Based,
                evalWhitePawns: evalWhitePawns,
                grade: grade,
                lossPawns: lossPawns
            )
        )
        let maxN = 400
        if moveEvalHistory.count > maxN {
            moveEvalHistory.removeFirst(moveEvalHistory.count - maxN)
        }
    }

    /// Zruší lokální křivku evalu a barvy tahů v Analýze (tahová historie z Stockfish + staré body z nápověd v UserDefaults).
    func clearAnalysisEvalHistory() {
        moveEvalHistory.removeAll()
        EvalHistoryRecorder.clear()
    }

    private func reseedHintPool() {
        let lim = boardUIPrefsPayload.chessHintLimit
        let n = lim > 0 ? lim : 999
        hintsRemainingWhite = n
        hintsRemainingBlack = n
    }

    /// BLE / snapshot bez GET `/api/settings/ui`: limit z `status.chess_hint_limit` (NVS na desce).
    private func mergeChessHintLimitFromSnapshotStatus(_ status: GameStatus) {
        guard let lim = status.chessHintLimit else { return }
        let clamped = min(99, max(0, lim))
        guard boardUIPrefsPayload.chessHintLimit != clamped else { return }
        boardUIPrefsPayload.chessHintLimit = clamped
        reseedHintPool()
        #if DEBUG
        AppDebugLog.staging("mergeChessHintLimitFromSnapshot: \(clamped) (reseed pool)")
        #endif
    }

    private func maybeSuggestBotMove(current: GameSnapshot) {
        guard let t = currentTransport else { return }
        guard boardUIPrefsPayload.botSettings.isBotMode else { return }
        guard current.status.webLocked != true else { return }
        let botWhite = boardUIPrefsPayload.botSettings.botSideIsWhite()
        let curWhite = current.status.currentPlayer.lowercased() == "white"
        guard botWhite == curWhite else { return }
        if boardUIPrefsPayload.chessBotLedTargetOnlyAfterLift, current.status.pieceLifted?.lifted != true {
            return
        }
        let key = "\(current.status.moveCount)_\(current.status.currentPlayer)"
        guard lastBotSuggestionKey != key else { return }
        lastBotSuggestionKey = key
        botSuggestionTask?.cancel()
        botSuggestionTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: 450_000_000)
            guard let self, !Task.isCancelled else { return }
            guard let snap = self.snapshot else { return }
            guard let fen = FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, history: snap.history) else { return }
            do {
                let depth = self.hintDepth
                let best = try await self.stockfish.fetchBestMove(fen: fen, depth: depth)
                let tier = self.moveHintTier
                try await self.postHintForTier(tier, best: best, transport: t)
                await MainActor.run {
                    self.applyHintOverlaySquares(tier: tier, best: best)
                    self.lastHintSummary = "Bot návrh: \(tier.summaryLine(best: best))"
                }
            } catch {
                await MainActor.run {
                    AppDebugLog.stagingError("bot hint", error)
                }
            }
        }
    }

    // MARK: - Wi‑Fi remote (parita s webovým ESP)

    private func wifiOnlyMessage() -> String {
        "Dostupné jen přes Wi‑Fi k desce (HTTP)."
    }

    private func noBoardConnectionMessage() -> String {
        "Není aktivní připojení k desce (Bluetooth nebo Wi‑Fi)."
    }

    /// Po změně na desce (HTTP nebo GATT): aktualizovat snímek. Preferuje Wi‑Fi fetch, jinak BLE read.
    private func refreshSnapshotWiFi() async {
        if let w = wifiTransport {
            do {
                let s = try await w.fetchSnapshotNow()
                applySnapshot(s)
                lastError = nil
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                if let s = try await b.fetchSnapshot(ifNoneMatch: nil) {
                    applySnapshot(s)
                }
                lastError = nil
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
        }
    }

    func fetchTimerStateFromBoard() async -> BoardTimerHTTPState? {
        if let w = wifiTransport {
            do {
                let t = try await w.fetchTimerState()
                lastTimerState = t
                timerClockReceivedAt = Date()
                if let mc = snapshot?.history.moves.count {
                    lastTimerFetchMoveCount = mc
                }
                lastError = nil
                #if DEBUG
                recordStagingTimerHttpFetch()
                #endif
                return t
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return nil
            }
        }
        if let c = snapshot?.clock {
            lastTimerState = c
            timerClockReceivedAt = Date()
            if let mc = snapshot?.history.moves.count {
                lastTimerFetchMoveCount = mc
            }
            lastError = nil
            #if DEBUG
            recordStagingTimerFromSnapshot()
            #endif
            return c
        }
        lastError =
            "Detail časovače z HTTP (`/api/timer`) není bez Wi‑Fi k desce. Přes Bluetooth se časy berou ze snímku (`clock` v JSON), pokud to firmware posílá — obnov snímek nebo zkontroluj firmwar."
        return nil
    }

    func postTimerConfigToBoard(type: Int, customMinutes: Int?, customIncrement: Int?) async {
        if let w = wifiTransport {
            do {
                try await w.postTimerConfig(type: type, customMinutes: customMinutes, customIncrement: customIncrement)
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postTimerConfig(type: type, customMinutes: customMinutes, customIncrement: customIncrement)
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postTimerPauseToBoard() async {
        if let w = wifiTransport {
            do {
                try await w.postTimerPause()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postTimerPause()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postTimerResumeToBoard() async {
        if let w = wifiTransport {
            do {
                try await w.postTimerResume()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postTimerResume()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postTimerResetToBoard() async {
        if let w = wifiTransport {
            do {
                try await w.postTimerReset()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postTimerReset()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postNewGameOnBoard() async {
        await cancelBoardSetupWizardIfActive()
        if let w = wifiTransport {
            do {
                try await w.postNewGame()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postNewGame()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = "Není aktivní připojení k desce."
    }

    /// Nastaví čas podle firmware typu (`/api/timer/config`) a spustí novou hru na desce.
    @discardableResult
    func startNewGameWithTimeControl(_ choice: ChessTimeControlChoice) async -> Bool {
        await cancelBoardSetupWizardIfActive()
        if let w = wifiTransport {
            do {
                try await w.postTimerConfig(
                    type: choice.apiType,
                    customMinutes: choice.customMinutes,
                    customIncrement: choice.customIncrement
                )
                try await w.postNewGame()
                await refreshSnapshotWiFi()
                lastError = nil
                LastNewGameTimeSelection.persist(from: choice)
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        if let b = bleTransport {
            do {
                try await b.postTimerConfig(
                    type: choice.apiType,
                    customMinutes: choice.customMinutes,
                    customIncrement: choice.customIncrement
                )
                try await b.postNewGame()
                await refreshSnapshotWiFi()
                lastError = nil
                LastNewGameTimeSelection.persist(from: choice)
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        lastError = "Není aktivní připojení k desce."
        return false
    }

    /// Zruší průvodce základního postavení (tutoriál) — před novou hrou nebo při otevření sheetu „Nová hra“.
    func cancelBoardSetupWizardIfActive() async {
        guard let mgr = boardSetupManager else { return }
        await mgr.cancel()
    }

    /// POST `/api/game/new` s FEN nebo BLE `new_game` + `fen`.
    /// Před nahráním uloží zálohu aktuální partie (`captureStateBeforePuzzleOnBoard`) a po úspěchu vymaže eval / nápovědu.
    /// Při `captureBookmarkBeforeLoad == false` neukládá novou zálohu (např. obnova z `restorePrePuzzleGame`).
    @discardableResult
    func postPuzzleFenToBoard(_ fen: String, captureBookmarkBeforeLoad: Bool = true) async -> Bool {
        if captureBookmarkBeforeLoad {
            captureStateBeforePuzzleOnBoard()
        }
        let ok = await postNewGameWithFENToBoard(fen)
        if ok {
            clearMoveEvaluationAndReviewStateForNewBoardContext()
        }
        return ok
    }

    /// Po LED průvodci — jen `new_game`+FEN (záloha partie už je z `beginFenBoardSetupWizard`).
    @discardableResult
    func applyPuzzleFenOnBoardAfterGuidedSetup(_ fen: String) async -> Bool {
        let ok = await postNewGameWithFENToBoard(fen)
        if ok {
            clearMoveEvaluationAndReviewStateForNewBoardContext()
        }
        return ok
    }

    @discardableResult
    func postRemoteMove(from: String, to: String, promotion: String?) async -> Bool {
        guard let snap = snapshot else {
            lastError = RemoteMoveBlockReason.noSnapshot.userMessage
            return false
        }
        if let block = RemoteChessMoveLegality.validate(snapshot: snap, from: from, to: to, promotion: promotion) {
            lastError = block.userMessage
            AppDebugLog.staging("postRemoteMove blocked locally: \(String(describing: block))")
            return false
        }

        if let w = wifiTransport {
            do {
                try await w.postGameMove(from: from, to: to, promotion: promotion)
                await refreshSnapshotWiFi()
                hintSquareFrom = nil
                hintSquareTo = nil
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        if let b = bleTransport {
            do {
                try await b.postMove(from: from, to: to, promotion: promotion)
                await refreshSnapshotWiFi()
                hintSquareFrom = nil
                hintSquareTo = nil
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        lastError = "Není aktivní připojení k desce."
        return false
    }

    /// Dokončení promoce, když už MCU čeká (`game_state` == promotion) — BLE `promotion` / Wi‑Fi `virtual_action` promote.
    /// Při tahu z aplikace s volbou figury použij `postRemoteMove(..., promotion:)` (jeden tah s `promotion` v JSON).
    @discardableResult
    func postRemotePromotion(choice: String) async -> Bool {
        let c = choice.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard let ch = c.first, "qrnb".contains(ch) else {
            lastError = RemoteMoveBlockReason.promotionInvalid.userMessage
            return false
        }
        if let w = wifiTransport {
            do {
                try await w.postVirtualAction(action: "promote", square: nil, choice: String(ch))
                await refreshSnapshotWiFi()
                hintSquareFrom = nil
                hintSquareTo = nil
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        if let b = bleTransport {
            do {
                try await b.postPromotion(choice: String(ch))
                await refreshSnapshotWiFi()
                hintSquareFrom = nil
                hintSquareTo = nil
                lastError = nil
                return true
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
                return false
            }
        }
        lastError = "Není aktivní připojení k desce."
        return false
    }

    func postVirtualActionOnBoard(action: String, square: String?, choice: String?) async {
        if let w = wifiTransport {
            do {
                try await w.postVirtualAction(action: action, square: square, choice: choice)
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postVirtualAction(action: action, square: square, choice: choice)
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func fetchESPWiFiStatus() async -> ESPWiFiStatusJSON? {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return nil
        }
        do {
            let s = try await w.fetchWiFiStatus()
            espWiFiStatusCache = s
            espWiFiStatusUpdatedAt = Date()
            lastError = nil
            #if DEBUG
            print("[CZECHMATE][staging] fetchESPWiFiStatus STA=\(s.staSsid) ip=\(s.staIp) AP=\(s.apSsid) clients=\(s.apClients)")
            #endif
            return s
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
            return nil
        }
    }

    func saveESPWiFiCredentialsAndConnect(ssid: String, password: String) async {
        if let w = wifiTransport {
            do {
                try await w.postWiFiConfig(ssid: ssid, password: password)
                try await w.postWiFiConnect()
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if bleTransport != nil {
            if let err = await provisionBoardWiFiOverBLE(ssid: ssid, password: password) {
                lastError = err
            } else {
                lastError = nil
            }
            return
        }
        lastError = wifiOnlyMessage()
    }

    func disconnectESPWiFiSTA() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postWiFiDisconnect()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func clearESPWiFiCredentials() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postWiFiClear()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    /// Tovární reset firmwaru: smaže celý NVS oddíl a restartuje desku (HTTP i BLE).
    func factoryResetBoard() async {
        if let w = wifiTransport {
            do {
                try await w.postFactoryReset()
                lastError = nil
                AppDebugLog.staging("factory_reset HTTP přijat — deska mazání NVS + restart")
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postFactoryReset()
                lastError = nil
                AppDebugLog.staging("factory_reset BLE přijat — deska mazání NVS + restart")
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = "Nejste připojeni k desce."
    }

    // MARK: - NVS UI prefs, MQTT, demo, lampa (HTTP)

    func syncBoardUIPrefsFromDevice() async {
        guard let w = wifiTransport else {
            if bleTransport != nil {
                AppDebugLog.staging("syncBoardUIPrefsFromDevice: přeskočeno — jen Bluetooth, GET /api/settings/ui není k dispozici (použij lokální předvolby a snapshot)")
            }
            return
        }
        do {
            let env = try await w.fetchUISettingsEnvelope()
            boardUISettingsVersion = env.version
            boardUIPrefsPayload = env.prefs
            hintDepth = env.prefs.chessHintDepth
            moveEvaluationEnabled = env.prefs.chessEvaluateMove
            saveSettings()
            reseedHintPool()
            lastError = nil
            AppDebugLog.staging("NVS UI prefs sync OK ver=\(env.version)")
        } catch {
            AppDebugLog.stagingError("syncBoardUIPrefsFromDevice", error)
        }
    }

    /// Zapíše aktuální `boardUIPrefsPayload` + lokální hloubku/eval na desku (`POST /api/settings/ui`).
    func pushCurrentPrefsToBoard() async {
        guard let w = wifiTransport else {
            lastError =
                bleTransport != nil
                ? "Hromadný zápis NVS (`/api/settings/ui`) vyžaduje HTTP k desce. Přes Bluetooth použij jednotlivé přepínače v diagnostice desky nebo pošli příkazy zvlášť."
                : wifiOnlyMessage()
            return
        }
        var p = boardUIPrefsPayload
        p.applyLocalHintAndEval(hintDepth: hintDepth, moveEvaluationEnabled: moveEvaluationEnabled)
        let env = BoardUISettingsEnvelope(version: max(1, boardUISettingsVersion), prefs: p)
        do {
            try await w.postUISettingsEnvelope(env)
            boardUIPrefsPayload = p
            boardUISettingsVersion = env.version
            saveSettings()
            lastError = nil
            await refreshSnapshotWiFi()
            AppDebugLog.staging("POST /api/settings/ui OK")
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func fetchMQTTStatusFromBoard() async -> ESPMQTTStatusJSON? {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return nil
        }
        do {
            let s = try await w.fetchMQTTStatus()
            lastError = nil
            return s
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
            return nil
        }
    }

    func postMQTTConfigToBoard(host: String, port: Int, username: String?, password: String?) async {
        if let w = wifiTransport {
            do {
                try await w.postMQTTConfig(host: host, port: port, username: username, password: password)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postMQTTConfig(host: host, port: port, username: username, password: password)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func fetchDemoStatusFromBoard() async -> ESPDemoStatusJSON? {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return nil
        }
        do {
            let s = try await w.fetchDemoStatus()
            lastError = nil
            return s
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
            return nil
        }
    }

    func postDemoConfigToBoard(enabled: Bool, speedMs: UInt32?) async {
        if let w = wifiTransport {
            do {
                try await w.postDemoConfig(enabled: enabled, speedMs: speedMs)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postDemoConfig(enabled: enabled, speedMs: speedMs)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postDemoStartOnBoard() async {
        if let w = wifiTransport {
            do {
                try await w.postDemoStart()
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postDemoStart()
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postAutoLampTimeoutToBoard(seconds: Int) async {
        if let w = wifiTransport {
            do {
                try await w.postAutoLampTimeout(seconds: seconds)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postAutoLampTimeout(seconds: seconds)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postGuidedHintsToBoard(enabled: Bool) async {
        if let w = wifiTransport {
            do {
                try await w.postGuidedHints(enabled: enabled)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postGuidedHints(enabled: enabled)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postLedGuidanceToBoard(level: Int) async {
        if let w = wifiTransport {
            do {
                try await w.postLedGuidance(level: level)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let b = bleTransport {
            do {
                try await b.postLedGuidance(level: level)
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postLightCommandToBoard(state: Bool, r: Int, g: Int, b: Int) async {
        if let w = wifiTransport {
            do {
                try await w.postLightCommand(state: state, r: r, g: g, b: b)
                lastError = nil
                if state {
                    markLampStabilizationWindow()
                    scheduleDeferredSnapshotRefreshAfterLampCommand()
                } else {
                    clearLampStabilizationAndCancelDeferredRefresh()
                    await refreshSnapshotWiFi()
                }
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let bleT = bleTransport {
            do {
                try await bleT.postLightCommand(state: state, r: r, g: g, b: b)
                lastError = nil
                if state {
                    markLampStabilizationWindow()
                    scheduleDeferredSnapshotRefreshAfterLampCommand()
                } else {
                    clearLampStabilizationAndCancelDeferredRefresh()
                    await refreshSnapshotWiFi()
                }
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    func postLightGameModeOnBoard() async {
        if let w = wifiTransport {
            do {
                try await w.postLightGameMode()
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        if let bleT = bleTransport {
            do {
                try await bleT.postLightGameMode()
                lastError = nil
                await refreshSnapshotWiFi()
            } catch {
                lastError = NetworkErrorFormatter.userMessage(for: error)
            }
            return
        }
        lastError = noBoardConnectionMessage()
    }

    // MARK: - Průvodce postavením (tutoriál / puzzle FEN)

    func clearBoardSetupManagerIfMatches(_ manager: BoardSetupManager) {
        if boardSetupManager === manager {
            boardSetupManager = nil
            setupWizardTargetSquare = nil
        }
    }

    func syncSetupWizardTargetSquare(_ square: String?) {
        setupWizardTargetSquare = square.map { $0.lowercased() }
    }

    func postSetupHintDestinationOnly(to square: String) async throws {
        if let w = wifiTransport {
            try await w.postHintHighlightDestinationOnly(to: square)
            return
        }
        if let b = bleTransport {
            try await b.postHintHighlightDestinationOnly(to: square)
            return
        }
        throw BoardSetupStoreError.boardConnectionRequired
    }

    func postSetupHintClearOnly() async throws {
        guard let t = currentTransport else { return }
        try await t.postHintClear()
    }

    func fetchESPGameStatus() async throws -> ESPGameStatusJSON? {
        guard let w = wifiTransport else { return nil }
        return try await w.fetchGameStatus()
    }

    /// Reed matice pro tutoriál základního postavení: `/api/status` přes Wi‑Fi, jinak `matrix_occupied` v posledním snapshotu (BLE).
    func matrixOccupiedForBoardSetupWizard() async -> [Int]? {
        if wifiTransport != nil {
            guard let s = try? await fetchESPGameStatus() else { return nil }
            return s.matrixOccupied
        }
        return snapshot?.status.matrixOccupied
    }

    func postSetupTutorialAPI(_ action: SetupTutorialAPIAction) async throws {
        if let w = wifiTransport {
            try await w.postSetupTutorial(action: action)
            return
        }
        if let b = bleTransport {
            try await b.postSetupTutorial(action: action)
            return
        }
        throw BoardSetupStoreError.boardConnectionRequired
    }

    /// Základní rozestavení — jako web „Postav šachovnici“ (`setup_tutorial` na ESP).
    func beginStandardBoardSetupWizard() async -> Bool {
        clearPuzzlePosition()
        guard supportsRemoteChessCommands else {
            lastError = "Průvodce vyžaduje připojení k desce (Wi‑Fi nebo Bluetooth)."
            return false
        }
        if let existing = boardSetupManager {
            await existing.cancel()
        }
        let fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        do {
            try await postSetupTutorialAPI(.start)
            let mgr = BoardSetupManager(mode: .standardStartingPosition, targetFEN: fen, store: self)
            boardSetupManager = mgr
            await mgr.start()
            lastError = nil
            return true
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
            boardSetupManager = nil
            return false
        }
    }

    /// Postupné postavení podle FEN (puzzle): nejdřív `setup_tutorial` start = prázdná logická deska + matice v JSON;
    /// po poslední figurce `setup_tutorial` cancel a volitelně `new_game`+FEN (`loadFenWhenDone`).
    func beginFenBoardSetupWizard(fen: String, loadFenWhenDone: Bool) async -> Bool {
        clearPuzzlePosition()
        guard supportsRemoteChessCommands else {
            lastError = "Průvodce vyžaduje připojení k desce (Wi‑Fi nebo Bluetooth)."
            return false
        }
        if let existing = boardSetupManager {
            await existing.cancel()
        }
        if loadFenWhenDone {
            captureStateBeforePuzzleOnBoard()
        }
        do {
            try await postSetupTutorialAPI(.start)
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
            boardSetupManager = nil
            return false
        }
        let mgr = BoardSetupManager(mode: .fenGuidedPlacement(loadFenWhenDone: loadFenWhenDone), targetFEN: fen, store: self)
        boardSetupManager = mgr
        await mgr.start()
        lastError = nil
        return true
    }
}

enum BoardSetupStoreError: LocalizedError {
    case boardConnectionRequired

    var errorDescription: String? {
        switch self {
        case .boardConnectionRequired:
            return "Tato akce vyžaduje připojení k šachovnici (Wi‑Fi nebo Bluetooth)."
        }
    }
}
