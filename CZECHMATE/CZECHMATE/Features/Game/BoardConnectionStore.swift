//
//  BoardConnectionStore.swift
//  CZECHMATE — orchestrátor BoardTransport (Wi‑Fi / BLE / mock).
//

import Foundation
import Observation

@MainActor
@Observable
final class BoardConnectionStore {
    private static let defaultsKey = "czechmate.boardBaseURL"
    private static let depthKey = "czechmate.hintDepth"
    private static let moveEvalKey = "czechmate.moveEvaluationEnabled"
    private static let mockKey = "czechmate.useMockBoard"

    var baseURLString: String
    var hintDepth: Int
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
    /// Zbývající čas z `GET /api/timer` (`white_time_ms` / `black_time_ms`). Snapshot `white_time`/`black_time` jsou v firmware jiné údaje (kumul. časy tahů).
    private(set) var lastTimerState: BoardTimerHTTPState?
    private(set) var timerClockReceivedAt: Date?
    private var lastTimerHttpFetchAt: Date = .distantPast
    /// Počet tahů v historii při posledním úspěšném `GET /api/timer` — při novém tahu vynutit stáhnutí (jinak throttle může nechat zastaralé ms).
    private var lastTimerFetchMoveCount: Int = -1
    /// Po každém `applySnapshot` se zvýší; dokončený async refresh timeru zapíše stav jen pokud `refreshId` je stále poslední (ochrana před přepsáním starší HTTP odpovědí).
    private var timerRefreshLatestId: UInt64 = 0

    #if DEBUG
    private var stagingTimerHttpFetchCount: Int = 0
    private var stagingTimerFromSnapshotCount: Int = 0
    private var stagingTimerMetricsWindowStart: Date = Date()

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

    var snapshot: GameSnapshot? {
        didSet {
            if let s = snapshot {
                snapshotReceivedAt = Date()
                StatsRecorder.observeSnapshot(s)
            }
            boardSetupManager?.consumeSnapshot(snapshot)
        }
    }

    var lastError: String?
    var hintSquareFrom: String?
    var hintSquareTo: String?
    var lastHintSummary: String?
    var lastMoveEvaluation: MoveEvaluationResult?

    /// Statická pozice z puzzle (FEN). Když je nastaveno, Hra zobrazí tuto desku místo živého snímku.
    var puzzleBoardPreview: [[String]]?

    /// Průvodce postavením (LED nápověda, krok za krokem).
    private(set) var boardSetupManager: BoardSetupManager?

    /// Poslední úspěšný `GET /api/wifi/status` (domácí síť šachovnice, AP, …).
    private(set) var espWiFiStatusCache: ESPWiFiStatusJSON?
    private(set) var espWiFiStatusUpdatedAt: Date?

    var isPolling = false
    var useWebSocket = true
    private(set) var isWebSocketLive = false
    private(set) var pollingStartedAt: Date?
    private(set) var lastRestFetchAt: Date?
    private(set) var lastRestRoundTripMs: Int?
    #if DEBUG
    private(set) var webSocketFrameCount = 0
    #endif

    var webSocketFramesForDiagnostics: Int {
        #if DEBUG
        webSocketFrameCount
        #else
        0
        #endif
    }

    /// Jak je uživatel aktuálně spojen s deskou (štítek v UI).
    private(set) var activeLinkKind: BoardLinkKind = .wifiLAN

    /// Tahy z aplikace, časovač a WiFi konfigurace na ESP — jen přes HTTP (Wi‑Fi), ne přes BLE/mock.
    var supportsWiFiRemoteCommands: Bool {
        wifiTransport != nil && !useMockBoard
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

    private var wifiTransport: WiFiBoardTransport?
    private var bleTransport: BLEBoardTransport?
    private var mockTransport: MockBoardTransport?

    private let stockfish = StockfishAPIClient()

    private var currentTransport: (any BoardTransport)? {
        mockTransport ?? bleTransport ?? wifiTransport
    }

    /// Je aktivní některý transport (Wi‑Fi / BLE / mock) — lze posílat např. jas.
    var hasActiveBoardTransport: Bool { currentTransport != nil }

    init() {
        let saved = UserDefaults.standard.string(forKey: Self.defaultsKey) ?? "http://192.168.4.1"
        baseURLString = saved
        useMockBoard = UserDefaults.standard.bool(forKey: Self.mockKey)
        let d = UserDefaults.standard.integer(forKey: Self.depthKey)
        hintDepth = d == 0 ? 10 : min(18, max(1, d))
        if UserDefaults.standard.object(forKey: Self.moveEvalKey) == nil {
            moveEvaluationEnabled = true
        } else {
            moveEvaluationEnabled = UserDefaults.standard.bool(forKey: Self.moveEvalKey)
        }
    }

    func prepareBluetoothForPermissions() {
        let b = BLEBoardTransport()
        b.preparePermissions()
    }

    func loadPuzzlePosition(fromFEN fen: String) {
        puzzleBoardPreview = FENParser.parseBoard(fromFEN: fen)
    }

    func clearPuzzlePosition() {
        puzzleBoardPreview = nil
    }

    func saveSettings() {
        UserDefaults.standard.set(baseURLString, forKey: Self.defaultsKey)
        UserDefaults.standard.set(hintDepth, forKey: Self.depthKey)
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
        runTransport(m)
    }

    func startWiFiTransport(url: URL) {
        stopPolling()
        UserDefaults.standard.set(url.absoluteString, forKey: Self.defaultsKey)
        baseURLString = url.absoluteString
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
        runTransport(w)
        Task { await syncBoardUIPrefsFromDevice() }
    }

    func startBLETransport(device: DiscoveredBoardDevice) async {
        stopPolling()
        let b = BLEBoardTransport()
        bleTransport = b
        activeLinkKind = .bluetooth
        isPolling = true
        pollingStartedAt = Date()
        lastError = nil
        do {
            try await b.connect(to: device)
            AppDebugLog.staging("BLE: připojeno \(device.displayName) id=\(device.id)")
            runTransport(b)
        } catch {
            AppDebugLog.stagingError("BLE connect", error)
            lastError = error.localizedDescription
            isPolling = false
            bleTransport = nil
        }
    }

    private func runTransport(_ transport: any BoardTransport) {
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
            await MainActor.run {
                self.isPolling = false
            }
        }
    }

    private func applySnapshot(_ snap: GameSnapshot) {
        if let w = wifiTransport {
            isWebSocketLive = w.isWebSocketLive
        }
        let previous = snapshot
        if let prev = previous, !Self.isSnapshotAcceptable(snap, versus: prev) {
            AppDebugLog.staging(
                "Snapshot přeskočen (starší verze/čas): ver \(String(describing: snap.stateVersion))→\(String(describing: prev.stateVersion)) ts \(snap.timestamp)/\(prev.timestamp)"
            )
            return
        }
        #if DEBUG
        if wifiTransport != nil, isWebSocketLive {
            webSocketFrameCount += 1
        }
        #endif
        snapshot = snap
        lastError = nil
        #if os(iOS)
        if wifiTransport != nil {
            timerRefreshLatestId += 1
            let rid = timerRefreshLatestId
            Task { await self.refreshTimerThrottledThenCompanionSync(snapshot: snap, refreshId: rid) }
        } else {
            BoardCompanionSync.onSnapshotUpdated(snap, timer: nil)
        }
        #endif
        if let prev = previous {
            resetHintPoolIfNewGame(previous: prev, current: snap)
            scheduleMoveEvaluation(previous: prev, current: snap)
        }
        maybeSuggestBotMove(current: snap)
    }

    #if os(iOS)
    /// Clock ze snapshotu (nový firmware) nebo `/api/timer` (throttle + nový tah vynutí fetch).
    /// Po každém **novém tahu** v historii vždy synchronizuje čas — jinak krátký throttle mohl přeskočit aktualizaci a UI ukazovalo zastaralé časy (hodiny „nežily“).
    private func refreshTimerThrottledThenCompanionSync(snapshot: GameSnapshot, refreshId: UInt64) async {
        guard let w = wifiTransport else {
            BoardCompanionSync.onSnapshotUpdated(snapshot, timer: nil)
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
            BoardCompanionSync.onSnapshotUpdated(snapshot, timer: lastTimerState)
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
        BoardCompanionSync.onSnapshotUpdated(snapshot, timer: lastTimerState)
    }
    #endif

    /// Ochrana před „cukáním“ UI při zrychleném střídání WS a REST nebo mimořadém pořadí rámců.
    /// Pozn.: `timestamp` z desky nemusí být monotónní (nová hra, reboot, rozdíl WS vs REST) — při **stejném**
    /// `state_version` proto neodmítáme snímek jen kvůli nižšímu timestampu (jinak UI zamrzne na staré pozici).
    private static func isSnapshotAcceptable(_ new: GameSnapshot, versus old: GameSnapshot) -> Bool {
        switch (new.stateVersion, old.stateVersion) {
        case let (n?, o?) where n < o:
            return false
        case let (n?, o?) where n > o:
            return true
        case let (n?, o?) where n == o:
            return true
        default:
            break
        }
        if new.timestamp < old.timestamp {
            return false
        }
        return true
    }

    /// - Parameter notifyWatchDisconnect: `true` jen když uživatel explicitně zastaví připojení (záložka Nastavení). Při přepnutí transportu / `startPolling` nechat `false`, aby Watch nedostával falešné „connection lost“.
    func stopPolling(notifyWatchDisconnect: Bool = false) {
        pollTask?.cancel()
        pollTask = nil
        evalTask?.cancel()
        evalTask = nil
        botSuggestionTask?.cancel()
        botSuggestionTask = nil
        isPolling = false
        pollingStartedAt = nil
        isWebSocketLive = false
        #if DEBUG
        webSocketFrameCount = 0
        #endif
        snapshot = nil
        snapshotReceivedAt = nil
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
        lastHintSummary = nil
        lastMoveEvaluation = nil
        historyReviewMoveIndex = nil
        lastBotSuggestionKey = nil
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
            lastError = "K nápovědě Stockfish je potřeba internet (chess-api)."
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
            if let ev = best.evalPawns {
                EvalHistoryRecorder.append(evalPawns: ev)
            }
            guard let t = currentTransport else {
                lastError = "Není aktivní připojení k desce."
                return
            }
            try await t.postHintHighlight(from: best.from, to: best.to)
            lastError = nil
            if limit > 0 {
                if snap.status.currentPlayer.lowercased() == "white" {
                    hintsRemainingWhite = max(0, hintsRemainingWhite - 1)
                } else {
                    hintsRemainingBlack = max(0, hintsRemainingBlack - 1)
                }
            }
            hintSquareFrom = best.from.lowercased()
            hintSquareTo = best.to.lowercased()
            lastHintSummary = "\(best.from) → \(best.to)" + (best.evalPawns.map { String(format: " (eval %.2f)", $0) } ?? "")
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
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
            if wifiTransport != nil {
                await refreshSnapshotWiFi()
            }
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
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
            let bestLine = try await stockfish.fetchBestMove(fen: fenBefore, depth: depth)
            let played = MoveEvaluation.normalizeUci(from: last.from, to: last.to)
            let bestUci = MoveEvaluation.normalizeUci(from: bestLine.from, to: bestLine.to)
            if played == bestUci {
                lastMoveEvaluation = MoveEvaluationResult(
                    grade: .best,
                    message: "Výborný tah! Byl to nejlepší tah."
                )
                return
            }
            guard !Task.isCancelled else { return }
            let afterProbe = try await stockfish.fetchBestMove(fen: fenAfter, depth: depth)
            let eb = bestLine.evalPawns.map { MoveEvaluation.evalToWhitePerspective(fen: fenBefore, evalRaw: $0) }
            let ea = afterProbe.evalPawns.map { MoveEvaluation.evalToWhitePerspective(fen: fenAfter, evalRaw: $0) }
            let bestFmt = "\(bestLine.from)-\(bestLine.to)"
            let idx = current.history.moves.count
            lastMoveEvaluation = MoveEvaluation.classify(
                playedFrom: last.from,
                playedTo: last.to,
                bestFrom: bestLine.from,
                bestTo: bestLine.to,
                evalBeforeWhite: eb,
                evalAfterWhite: ea,
                moveIndex1Based: idx,
                bestMoveFormatted: bestFmt
            )
            if let ev = lastMoveEvaluation {
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
        guard let idx = FirmwareSquareNotation.indices(from: move.to) else { return false }
        let cell = previous.board[idx.row][idx.col].trimmingCharacters(in: .whitespaces)
        guard !cell.isEmpty, cell != " " else { return false }
        let moving = move.piece.trimmingCharacters(in: .whitespaces)
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
        }
    }

    private func reseedHintPool() {
        let lim = boardUIPrefsPayload.chessHintLimit
        let n = lim > 0 ? lim : 999
        hintsRemainingWhite = n
        hintsRemainingBlack = n
    }

    private func maybeSuggestBotMove(current: GameSnapshot) {
        guard let w = wifiTransport else { return }
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
                try await w.postHintHighlight(from: best.from, to: best.to)
                await MainActor.run {
                    self.hintSquareFrom = best.from.lowercased()
                    self.hintSquareTo = best.to.lowercased()
                    self.lastHintSummary = "Bot návrh: \(best.from) → \(best.to)"
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
        "Dostupné jen přes Wi‑Fi k desce (HTTP). Bluetooth a ukázka nepodporují tyto příkazy."
    }

    private func refreshSnapshotWiFi() async {
        guard let w = wifiTransport else { return }
        do {
            let s = try await w.fetchSnapshotNow()
            applySnapshot(s)
            lastError = nil
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func fetchTimerStateFromBoard() async -> BoardTimerHTTPState? {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return nil
        }
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

    func postTimerConfigToBoard(type: Int, customMinutes: Int?, customIncrement: Int?) async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postTimerConfig(type: type, customMinutes: customMinutes, customIncrement: customIncrement)
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postTimerPauseToBoard() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postTimerPause()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postTimerResumeToBoard() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postTimerResume()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postTimerResetToBoard() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postTimerReset()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postNewGameOnBoard() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postNewGame()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    /// Nastaví čas podle firmware typu (`/api/timer/config`) a spustí novou hru na desce.
    @discardableResult
    func startNewGameWithTimeControl(_ choice: ChessTimeControlChoice) async -> Bool {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return false
        }
        do {
            try await w.postTimerConfig(
                type: choice.apiType,
                customMinutes: choice.customMinutes,
                customIncrement: choice.customIncrement
            )
            try await w.postNewGame()
            await refreshSnapshotWiFi()
            lastError = nil
            return true
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
            return false
        }
    }

    /// POST `/api/game/new` s FEN (záleží na firmwaru). `true` při HTTP úspěchu.
    @discardableResult
    func postPuzzleFenToBoard(_ fen: String) async -> Bool {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return false
        }
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

    @discardableResult
    func postRemoteMove(from: String, to: String, promotion: String?) async -> Bool {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return false
        }
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

    func postVirtualActionOnBoard(action: String, square: String?, choice: String?) async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postVirtualAction(action: action, square: square, choice: choice)
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
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
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postWiFiConfig(ssid: ssid, password: password)
            try await w.postWiFiConnect()
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
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

    // MARK: - NVS UI prefs, MQTT, demo, lampa (HTTP)

    func syncBoardUIPrefsFromDevice() async {
        guard let w = wifiTransport else { return }
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
            lastError = wifiOnlyMessage()
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
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postMQTTConfig(host: host, port: port, username: username, password: password)
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
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
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postDemoConfig(enabled: enabled, speedMs: speedMs)
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postDemoStartOnBoard() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postDemoStart()
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postAutoLampTimeoutToBoard(seconds: Int) async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postAutoLampTimeout(seconds: seconds)
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postGuidedHintsToBoard(enabled: Bool) async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postGuidedHints(enabled: enabled)
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postLedGuidanceToBoard(level: Int) async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postLedGuidance(level: level)
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postLightCommandToBoard(state: Bool, r: Int, g: Int, b: Int) async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postLightCommand(state: state, r: r, g: g, b: b)
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    func postLightGameModeOnBoard() async {
        guard let w = wifiTransport else {
            lastError = wifiOnlyMessage()
            return
        }
        do {
            try await w.postLightGameMode()
            lastError = nil
            await refreshSnapshotWiFi()
        } catch {
            lastError = NetworkErrorFormatter.userMessage(for: error)
        }
    }

    // MARK: - Průvodce postavením (tutoriál / puzzle FEN)

    func clearBoardSetupManagerIfMatches(_ manager: BoardSetupManager) {
        if boardSetupManager === manager {
            boardSetupManager = nil
        }
    }

    func postSetupHintDestinationOnly(to square: String) async throws {
        guard let w = wifiTransport else {
            throw BoardSetupStoreError.wifiRequired
        }
        try await w.postHintHighlightDestinationOnly(to: square)
    }

    func postSetupHintClearOnly() async throws {
        guard let t = currentTransport else { return }
        try await t.postHintClear()
    }

    func fetchESPGameStatus() async throws -> ESPGameStatusJSON? {
        guard let w = wifiTransport else { return nil }
        return try await w.fetchGameStatus()
    }

    func postSetupTutorialAPI(_ action: SetupTutorialAPIAction) async throws {
        guard let w = wifiTransport else {
            throw BoardSetupStoreError.wifiRequired
        }
        try await w.postSetupTutorial(action: action)
    }

    /// Základní rozestavení — jako web „Postav šachovnici“ (`setup_tutorial` na ESP).
    func beginStandardBoardSetupWizard() async -> Bool {
        clearPuzzlePosition()
        guard supportsWiFiRemoteCommands else {
            lastError = "Průvodce vyžaduje Wi‑Fi připojení k desce."
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

    /// Postupné postavení podle libovolného FEN (např. denní puzzle). Volitelně nejdřív `POST /api/game/new` s FEN.
    func beginFenBoardSetupWizard(fen: String, postNewGameWithFEN: Bool) async -> Bool {
        clearPuzzlePosition()
        guard supportsWiFiRemoteCommands else {
            lastError = "Průvodce vyžaduje Wi‑Fi připojení k desce."
            return false
        }
        if let existing = boardSetupManager {
            await existing.cancel()
        }
        do {
            if postNewGameWithFEN {
                guard let w = wifiTransport else {
                    throw BoardSetupStoreError.wifiRequired
                }
                try await w.postNewGame(fen: fen)
                await refreshSnapshotWiFi()
            }
            let mgr = BoardSetupManager(mode: .fenGuidedPlacement(postNewGameWithFEN: postNewGameWithFEN), targetFEN: fen, store: self)
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
}

enum BoardSetupStoreError: LocalizedError {
    case wifiRequired

    var errorDescription: String? {
        switch self {
        case .wifiRequired:
            return "Tato akce vyžaduje Wi‑Fi připojení k šachovnici."
        }
    }
}
