//
//  GameView.swift
//  CZECHMATE — Hra: šachovnice jako hlavní prvek, časy podél desky, bez vývojářských údajů.
//

import CZECHMATEShared
import SwiftUI
#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

struct GameView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass
    @EnvironmentObject private var learningModeManager: LearningModeManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager
    @EnvironmentObject private var chessCoachManager: ChessCoachManager
    @State private var isHintBusy = false
    @State private var bleQuickReconnectBusy = false
    @State private var showConnection = false
    @State private var showHelp = false
    @State private var pollingWaitStarted: Date?
    @AppStorage("czechmate.boardFlipped") private var boardFlipped = false
    @AppStorage("czechmate.remoteMovesEnabled") private var remoteMovesEnabled = false
    @AppStorage("czechmate.gameBoardOnlyMode") private var boardOnlyMode = false
    @AppStorage("czechmate.moveAnimationsEnabled") private var moveAnimationsEnabled = true
    @AppStorage("czechmate.showBoardCoordinates") private var showBoardCoordinates = true
    @AppStorage("czechmate.boardStyleRaw") private var boardStyleRaw = ChessBoardStyle.wooden.rawValue
    @State private var boardZoom: CGFloat = 1.0
    @State private var remoteMoveFrom: String?
    @State private var showPromotionPick = false
    @State private var promotionPair: (from: String, to: String)?
    @State private var showNewGameSetup = false
    @State private var timerDetail: BoardTimerHTTPState?
    @State private var isTimerRefreshBusy = false
    @State private var virtualPickupSquare = ""
    @State private var virtualDropSquare = ""
    @State private var showGameControlsSheet = false
    @State private var invalidFlashFrom: String?
    @State private var invalidFlashTo: String?
    /// Poslední snapshot pro detekci nového tahu (haptika + zvuk šach/braní).
    @State private var lastFeedbackSnapshot: GameSnapshot?
    @State private var showCoachOnboardingSheet = false
    @State private var showPositionCoachSheet = false
    @State private var lastSeenMoveCount: UInt32 = 0
    @AppStorage("czechmate.coach.userLevel") private var coachUserLevel = 4
    @State private var transientBoardMessage: String?
    @State private var transientBoardMessageDismissID = UUID()

    var body: some View {
        NavigationStack {
            Group {
                if boardOnlyMode, activeBoard != nil {
                    boardOnlyScroll
                } else if useWideBoardLayout {
                    wideBoardLayout
                } else {
                    phoneScrollLayout
                }
            }
            .refreshable {
                await store.refreshSnapshotFromServer()
            }
            .background(groupedBackground)
            .navigationTitle("")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    ConnectionStatusBadge(onTap: { showConnection = true })
                }
                ToolbarItem(placement: .principal) {
                    HStack(spacing: 8) {
                        Image("AppLogo")
                            .resizable()
                            .scaledToFit()
                            .frame(width: 26, height: 26)
                            .clipShape(RoundedRectangle(cornerRadius: 5, style: .continuous))
                        Text("Hra")
                            .font(.system(.headline, design: .rounded))
                    }
                }
                ToolbarItemGroup(placement: .primaryAction) {
                    Toggle(isOn: learningModeToggleBinding) {
                        Image(systemName: "graduationcap.fill")
                    }
                    .tint(Theme.accent)
                    .accessibilityLabel("Učící mód")
                    Button {
                        showGameControlsSheet = true
                    } label: {
                        Image(systemName: "gearshape")
                    }
                    .accessibilityLabel("Časovač a ovládání desky")
                    Toggle(isOn: $boardOnlyMode) {
                        Image(systemName: boardOnlyMode ? "rectangle.dashed" : "rectangle.fill")
                    }
                    .accessibilityLabel("Jen deska a hodiny")
                    ThemeToolbarIconChip(
                        systemName: "book.pages.fill",
                        accessibilityLabel: "Nápověda a funkce"
                    ) {
                        showHelp = true
                    }
                    ThemeToolbarIconChip(
                        systemName: "link.circle.fill",
                        accessibilityLabel: "Najít šachovnici",
                        tint: Color(red: 0.22, green: 0.42, blue: 0.92),
                        background: Color(red: 0.22, green: 0.42, blue: 0.92).opacity(0.16)
                    ) {
                        showConnection = true
                    }
                }
            }
            .sheet(isPresented: $showConnection) {
                BoardDiscoveryView()
                    .environment(store)
            }
            .sheet(isPresented: $showHelp) {
                NavigationStack {
                    HelpView()
                }
                .tint(Theme.accent)
            }
            .sheet(isPresented: $showGameControlsSheet) {
                NavigationStack {
                    ScrollView {
                        VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                            advancedControlsContent
                        }
                        .padding(Theme.Spacing.l)
                    }
                    .background(groupedBackground)
                    .navigationTitle("Deska a časovač")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Hotovo") { showGameControlsSheet = false }
                        }
                    }
                }
                .tint(Theme.accent)
            }
            .confirmationDialog("Vyber promoci figury", isPresented: $showPromotionPick, titleVisibility: .visible) {
                Button("Dáma") { confirmPromotion("Q") }
                Button("Věž") { confirmPromotion("R") }
                Button("Střelec") { confirmPromotion("B") }
                Button("Jezdec") { confirmPromotion("N") }
                Button("Zrušit", role: .cancel) {
                    promotionPair = nil
                    remoteMoveFrom = nil
                }
            }
            .sheet(isPresented: $showNewGameSetup) {
                NewGameSetupSheet()
                    .environment(store)
            }
            .sheet(isPresented: $showCoachOnboardingSheet) {
                CoachOnboardingView(
                    modelDownload: modelDownloadManager,
                    onFinished: {
                        learningModeManager.completePendingActivationIfNeeded(modelReady: modelDownloadManager.isModelInstalled)
                        showCoachOnboardingSheet = false
                    },
                    onSkipForNow: {
                        learningModeManager.cancelPendingActivation()
                        showCoachOnboardingSheet = false
                    }
                )
            }
            .sheet(isPresented: $showPositionCoachSheet, onDismiss: {
                chessCoachManager.advice = ""
            }) {
                NavigationStack {
                    ScrollView {
                        CoachBubbleView(
                            isThinking: chessCoachManager.isThinking && chessCoachManager.advice.isEmpty,
                            fullText: chessCoachManager.advice.isEmpty ? " " : chessCoachManager.advice,
                            typewriterEnabled: !chessCoachManager.isStreamingLLM
                        )
                        .padding(Theme.Spacing.l)
                    }
                    .navigationTitle("Plán pozice")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Zavřít") { showPositionCoachSheet = false }
                        }
                    }
                }
                .tint(Theme.accent)
            }
            .fullScreenCover(isPresented: blunderBrakeCoverBinding) {
                CoachBlunderInterventionView(
                    message: learningModeManager.blunderBrake?.coachMessage ?? "",
                    onContinue: { learningModeManager.dismissBlunderBrake() }
                )
            }
            .onAppear {
                modelDownloadManager.reconcileWithDisk()
                if !store.isPolling {
                    store.startPolling()
                }
                if store.snapshot == nil {
                    pollingWaitStarted = Date()
                }
                lastFeedbackSnapshot = store.snapshot
                lastSeenMoveCount = store.snapshot?.status.moveCount ?? 0
                chessCoachManager.learningMode = learningModeManager
                chessCoachManager.syncLearningModeAndModel(
                    learningModeActive: learningModeManager.isLearningModeActive,
                    modelDownload: modelDownloadManager
                )
            }
            .onChange(of: learningModeManager.isLearningModeActive) { _, active in
                chessCoachManager.syncLearningModeAndModel(
                    learningModeActive: active,
                    modelDownload: modelDownloadManager
                )
            }
            .onChange(of: modelDownloadManager.isModelInstalled) { _, _ in
                chessCoachManager.syncLearningModeAndModel(
                    learningModeActive: learningModeManager.isLearningModeActive,
                    modelDownload: modelDownloadManager
                )
            }
            .onChange(of: coachEvaluationChangeToken) { _, _ in
                guard let ev = store.lastMoveEvaluation, let s = store.snapshot else { return }
                learningModeManager.consumeMoveEvaluationForBlunderBrake(
                    evaluation: ev,
                    currentMoveCount: Int(s.status.moveCount)
                )
            }
            .onChange(of: store.snapshot?.status.moveCount) { _, newCount in
                guard let n = newCount else { return }
                if n < lastSeenMoveCount {
                    learningModeManager.resetBlunderTrackingForNewGame()
                }
                lastSeenMoveCount = n
            }
            .onChange(of: store.useMockBoard) { _, _ in
                store.startPolling()
            }
            .onChange(of: store.isPolling) { _, polling in
                if polling, store.snapshot == nil {
                    pollingWaitStarted = Date()
                } else if !polling {
                    pollingWaitStarted = nil
                }
            }
            .onChange(of: store.snapshot?.timestamp) { _, _ in
                if store.snapshot != nil {
                    pollingWaitStarted = nil
                }
                guard let next = store.snapshot else {
                    lastFeedbackSnapshot = nil
                    return
                }
                if let prev = lastFeedbackSnapshot, next.history.moves.count > prev.history.moves.count {
                    feedbackAfterRecordedMove(prev: prev, next: next)
                    runImmediateEvalBlunderBrakeIfNeeded(prev: prev, next: next)
                }
                lastFeedbackSnapshot = next
            }
            .tint(Theme.accent)
        }
    }

    /// Zvuk + haptika po novém tahu z desky (braní, rošáda, promoce, šach, mat, klidný tah).
    private func feedbackAfterRecordedMove(prev: GameSnapshot, next: GameSnapshot) {
        SoundManager.shared.playFeedbackAfterBoardMove(previous: prev, current: next)
    }

    /// V učícím módu: paralelní eval před/po tahu — záchranná brzda bez čekání na `lastMoveEvaluation`.
    private func runImmediateEvalBlunderBrakeIfNeeded(prev: GameSnapshot, next: GameSnapshot) {
        guard learningModeManager.isLearningModeActive else { return }
        guard next.history.moves.count == prev.history.moves.count + 1 else { return }
        guard network.isInternetLikelyForStockfish else { return }
        guard store.puzzleBoardPreview == nil else { return }

        Task { @MainActor in
            guard let fenBefore = FENBuilder.boardAndStatusToFEN(board: prev.board, status: prev.status, history: prev.history),
                  let fenAfter = FENBuilder.boardAndStatusToFEN(board: next.board, status: next.status, history: next.history) else { return }
            do {
                async let eb = store.fetchEvalWhiteForFEN(fenBefore)
                async let ea = store.fetchEvalWhiteForFEN(fenAfter)
                let (beforeW, afterW) = try await (eb, ea)
                learningModeManager.applyEvalDropBlunderBrake(
                    evalBeforeWhite: beforeW,
                    evalAfterWhite: afterW,
                    moveIndex1Based: next.history.moves.count,
                    moveCount: Int(next.status.moveCount)
                )
            } catch {
                #if DEBUG
                AppDebugLog.staging("Immediate eval blunder check: \(error.localizedDescription)")
                #endif
            }
        }
    }

    private var activeBoard: [[String]]? {
        if let p = store.puzzleBoardPreview { return p }
        if let idx = store.historyReviewMoveIndex, let snap = store.snapshot {
            let n = idx + 1
            if let replay = HistoryBoardReplay.board(afterApplyingFirst: n, moves: snap.history.moves) {
                return replay
            }
        }
        return store.snapshot?.board
    }

    private var useWideBoardLayout: Bool {
        horizontalSizeClass == .regular && activeBoard != nil && !boardOnlyMode
    }

    private var learningModeToggleBinding: Binding<Bool> {
        Binding(
            get: { learningModeManager.isLearningModeActive },
            set: { new in
                if new {
                    guard modelDownloadManager.isModelInstalled else {
                        learningModeManager.pendingActivateAfterModel = true
                        showCoachOnboardingSheet = true
                        return
                    }
                    learningModeManager.isLearningModeActive = true
                } else {
                    learningModeManager.isLearningModeActive = false
                }
            }
        )
    }

    private var blunderBrakeCoverBinding: Binding<Bool> {
        Binding(
            get: { learningModeManager.blunderBrake != nil },
            set: { if !$0 { learningModeManager.dismissBlunderBrake() } }
        )
    }

    /// Token změny vyhodnocení tahu (Stockfish) — spouští záchrannou brzdu při blunderu.
    private var coachEvaluationChangeToken: String {
        guard let ev = store.lastMoveEvaluation, let s = store.snapshot else { return "-" }
        return "\(s.status.moveCount)-\(ev.grade.rawValue)-\(ev.message)"
    }

    private var mainContentStack: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.l) {
            compactAlerts

            if store.isPolling, activeBoard == nil, !store.useMockBoard {
                loadingOrTimeoutBlock
                pollingHintIfStale
            } else if store.isPolling, activeBoard == nil, store.useMockBoard {
                loadingBlock
            }

            if let board = activeBoard {
                boardSection(board: board)
            } else if !store.isPolling {
                emptyStateBlock
            }

            if store.snapshot != nil, store.puzzleBoardPreview == nil {
                if let ev = store.lastMoveEvaluation {
                    evaluationBlock(ev)
                }
                hintSection
            }
        }
        .padding(.horizontal, Theme.Spacing.l)
        .padding(.vertical, Theme.Spacing.m)
    }

    private var phoneScrollLayout: some View {
        ScrollView {
            mainContentStack
        }
    }

    private var boardOnlyScroll: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                PrePuzzleRestoreBanner(onFailureMessage: { showLegacyTransientBoardMessage($0) })
                if let board = activeBoard {
                    boardSection(board: board)
                }
            }
            .padding(.horizontal, Theme.Spacing.l)
            .padding(.vertical, Theme.Spacing.m)
        }
    }

    private var wideBoardLayout: some View {
        HStack(alignment: .top, spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                    if let board = activeBoard {
                        boardSection(board: board)
                    }
                }
                .padding(.leading, Theme.Spacing.l)
                .padding(.vertical, Theme.Spacing.m)
            }
            .frame(maxWidth: 440)

            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    compactAlerts
                    if store.isPolling, activeBoard == nil, !store.useMockBoard {
                        loadingOrTimeoutBlock
                        pollingHintIfStale
                    } else if store.isPolling, activeBoard == nil, store.useMockBoard {
                        loadingBlock
                    }
                    if activeBoard == nil, !store.isPolling {
                        emptyStateBlock
                    }
                    if store.snapshot != nil, store.puzzleBoardPreview == nil {
                        if let ev = store.lastMoveEvaluation {
                            evaluationBlock(ev)
                        }
                        hintSection
                    }
                }
                .padding(.trailing, Theme.Spacing.l)
                .padding(.vertical, Theme.Spacing.m)
            }
        }
    }

    @ViewBuilder
    private var compactAlerts: some View {
        if let err = store.lastError {
            ThemedBanner(style: .error) {
                Text(err)
                    .font(Theme.Typography.body())
                    .foregroundStyle(Theme.Semantic.errorForeground)
            }
        }

        if store.useMockBoard, store.snapshot != nil, store.puzzleBoardPreview == nil {
            ThemedBanner(style: .mock, cornerRadius: 10) {
                Label("Ukázkový režim (bez ESP)", systemImage: "square.grid.3x3.square")
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            }
        }

        if store.historyReviewMoveIndex != nil, store.puzzleBoardPreview == nil {
            ThemedBanner(style: .neutral, cornerRadius: 10) {
                HStack {
                    Label("Náhled pozice z historie", systemImage: "clock.arrow.circlepath")
                        .font(Theme.Typography.caption())
                    Spacer()
                    Button("Zpět na živou hru") {
                        store.historyReviewMoveIndex = nil
                    }
                    .buttonStyle(.themeSecondary)
                }
            }
        }

        if store.boardUIPrefsPayload.botSettings.isBotMode, store.snapshot != nil {
            ThemedBanner(style: .accent) {
                Text("Režim bot: návrhy tahů na LED (Stockfish). Hraj fyzicky za stranu bota.")
                    .font(Theme.Typography.caption())
            }
        }

        if store.snapshot?.status.webLocked == true {
            ThemedBanner(style: .warning, cornerRadius: 10) {
                Text("Web na ESP je uzamčen — vzdálené tahy a časovač z aplikace jsou blokované.")
                    .font(Theme.Typography.caption())
                    .foregroundStyle(Theme.Semantic.warningForeground)
            }
        }

        if learningModeManager.isLearningModeActive, store.puzzleBoardPreview == nil {
            ThemedBanner(style: .accent, cornerRadius: 12) {
                VStack(alignment: .leading, spacing: 6) {
                    Label("Učící mód", systemImage: "graduationcap.fill")
                        .font(Theme.Typography.subsection())
                    Text("Časovače jsou skryté — soustředíš se na pozici. Komentáře trenéra běží lokálně po stažení modelu; odhad hrubek a nápovědy tahu stále spoléhají na Stockfish přes internet.")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                }
            }
        }

        firmwareStateBanners

        if let phase = store.pairingPhaseMessage, !phase.isEmpty {
            ThemedBanner(style: .accent, cornerRadius: 10) {
                HStack(alignment: .center, spacing: 10) {
                    ProgressView()
                    Text(phase)
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.primary)
                        .multilineTextAlignment(.leading)
                }
            }
        }

        if let msg = store.connectionStallMessage {
            ThemedBanner(style: .error, cornerRadius: 10) {
                Text(msg)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(Theme.Semantic.errorForeground)
            }
        }

        if !network.isInternetLikelyForStockfish {
            ThemedBanner(style: .warning, cornerRadius: 8) {
                Label(
                    store.activeLinkKind == .bluetooth
                        ? "Pro nápovědu, analýzu a zhodnocení tahů (Stockfish) zapni mobilní data nebo Wi‑Fi v telefonu. Deska může zůstat připojená jen přes Bluetooth."
                        : "Pro nápovědu tahu, analýzu a automatické zhodnocení tahů (Stockfish) je potřeba internet (Wi‑Fi nebo mobilní data). Samotné čtení pozice z desky může jít jen přes Bluetooth.",
                    systemImage: "antenna.radiowaves.left.and.right.slash"
                )
                .font(Theme.Typography.caption2())
                .foregroundStyle(Theme.Semantic.warningForeground)
            }
        } else if network.isConstrained, store.activeLinkKind == .bluetooth {
            ThemedBanner(style: .neutral, cornerRadius: 8) {
                Label(
                    "Úsporný síťový režim (např. nízká data) — výpočet Stockfish může být pomalejší. Deska zůstává na Bluetooth.",
                    systemImage: "leaf.fill"
                )
                .font(Theme.Typography.caption2())
                .foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder
    private func boardSection(board: [[String]]) -> some View {
        PuzzlePreviewModeBanner()

        if let s = store.snapshot, store.puzzleBoardPreview == nil {
            statusLine(s)
        }

        if learningModeManager.isLearningModeActive, store.snapshot != nil, store.puzzleBoardPreview == nil {
            learningModeCoachChrome
        }

        VStack(spacing: 8) {
            if store.snapshot != nil, store.puzzleBoardPreview == nil, !learningModeManager.isLearningModeActive {
                GameBoardTimerLineIfNeeded(board: board, isBlackRow: true)
                GameBoardTimerPauseStripIfNeeded()
            }

            ZStack(alignment: .topTrailing) {
                ChessBoardView(
                    board: board,
                    flipped: boardFlipped,
                    highlightFrom: store.boardHintHighlightFrom,
                    highlightTo: store.boardHintHighlightTo,
                    lastMoveFrom: store.snapshot?.history.moves.last?.from,
                    lastMoveTo: store.snapshot?.history.moves.last?.to,
                    zoom: $boardZoom,
                    remoteSelectionSquare: remoteMovesEnabled && store.supportsRemoteChessCommands && store.puzzleBoardPreview == nil
                        ? remoteMoveFrom
                        : nil,
                    errorInvalidSquare: store.snapshot?.status.errorState?.invalidPos,
                    errorOriginalSquare: store.snapshot?.status.errorState?.originalPos,
                    invalidMoveFlashFrom: invalidFlashFrom,
                    invalidMoveFlashTo: invalidFlashTo,
                    onRemoteSquareTap: remoteMovesEnabled && store.supportsRemoteChessCommands && store.puzzleBoardPreview == nil
                        && store.snapshot?.status.webLocked != true
                        && store.snapshot?.status.isErrorRecoveryActive != true
                        && store.snapshot?.status.castlingInProgress != true
                        ? { sq in
                            if let snap = store.snapshot {
                                handleRemoteSquareTap(sq, snap: snap)
                            }
                        }
                        : nil,
                    animatePieces: moveAnimationsEnabled,
                    showCoordinates: showBoardCoordinates,
                    boardStyle: ChessBoardStyle(rawValue: boardStyleRaw) ?? .wooden
                )
                Button {
                    boardFlipped.toggle()
                    HapticSettings.lightImpactIfEnabled()
                } label: {
                    Image(systemName: "arrow.up.arrow.down.circle.fill")
                        .font(.title3)
                        .foregroundStyle(.secondary)
                        .padding(8)
                        .background(.ultraThinMaterial, in: Circle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Otočit pohled — bílý nebo černý dole")
                .padding(6)
            }
            .padding(10)
            .background(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(cardFillColor)
                    .shadow(color: .black.opacity(0.06), radius: 10, y: 3)
            )

            if let msg = transientBoardMessage {
                TransientBoardMessageBanner(
                    message: msg,
                    onDismiss: {
                        transientBoardMessage = nil
                    }
                )
                .task(id: transientBoardMessageDismissID) {
                    try? await Task.sleep(nanoseconds: 4_000_000_000)
                    await MainActor.run {
                        transientBoardMessage = nil
                    }
                }
            }

            if remoteMovesEnabled {
                RemoteMovesHintBanner(remoteMovesEnabled: true)
            }

            if store.snapshot != nil, store.puzzleBoardPreview == nil, !learningModeManager.isLearningModeActive {
                GameBoardTimerLineIfNeeded(board: board, isBlackRow: false)
            }
            if let s = store.snapshot, store.puzzleBoardPreview == nil {
                MoveHistoryView(
                    moves: s.history.moves,
                    selectedMoveIndex: Bindable(store).historyReviewMoveIndex
                )
            }
        }
    }

    private func triggerInvalidMoveFlash(from: String, to: String) {
        #if os(iOS)
        HapticSettings.notificationErrorIfEnabled()
        #endif
        SoundManager.shared.playInvalidMoveSound()
        Task { @MainActor in
            for _ in 0 ..< 2 {
                invalidFlashFrom = from
                invalidFlashTo = to
                try? await Task.sleep(nanoseconds: 140_000_000)
                invalidFlashFrom = nil
                invalidFlashTo = nil
                try? await Task.sleep(nanoseconds: 90_000_000)
            }
            invalidFlashFrom = from
            invalidFlashTo = to
            try? await Task.sleep(nanoseconds: 280_000_000)
            invalidFlashFrom = nil
            invalidFlashTo = nil
        }
    }

    private func statusLine(_ s: GameSnapshot) -> some View {
        HStack {
            gamePhaseCaption(s.status)
            Spacer()
            Text("Na tahu: \(s.status.currentPlayer == "White" || s.status.currentPlayer.lowercased() == "white" ? "Bílý" : "Černý")")
                .font(.system(.caption, design: .rounded))
                .foregroundStyle(.secondary)
            Text("·")
                .foregroundStyle(.tertiary)
            Text("Tahy \(s.status.moveCount)")
                .font(.system(.caption, design: .rounded))
                .foregroundStyle(.secondary)
        }
        .padding(.horizontal, 4)
    }

    @ViewBuilder
    private func gamePhaseCaption(_ st: GameStatus) -> some View {
        let gs = st.gameState.lowercased()
        if st.isGameFinished {
            if let ge = st.gameEnd, ge.ended, let r = ge.reason, !r.isEmpty {
                Text(r)
                    .font(.system(.caption, design: .rounded).weight(.semibold))
                    .foregroundStyle(.purple)
                    .lineLimit(2)
            } else {
                Text("Konec hry")
                    .font(.system(.caption, design: .rounded).weight(.semibold))
                    .foregroundStyle(.purple)
            }
        } else if gs == "promotion" {
            Text("Promoce")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .foregroundStyle(.orange)
        } else if gs == "paused" {
            Text("Pauza")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .foregroundStyle(.secondary)
        } else if st.checkmate == true {
            Text("Mat")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .foregroundStyle(.red)
        } else if st.stalemate == true {
            Text("Pat")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .foregroundStyle(.orange)
        } else if st.inCheck {
            Text("Šach")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .foregroundStyle(.red)
        } else {
            Text(" ")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .foregroundStyle(.clear)
        }
    }

    /// Stav z MCU / webu — parita s webovým panelem (chyby, puzzle, matice, konec partie).
    @ViewBuilder
    private var firmwareStateBanners: some View {
        PrePuzzleRestoreBanner(onFailureMessage: { showLegacyTransientBoardMessage($0) })

        if let st = store.snapshot?.status {
            if st.isErrorRecoveryActive {
                ThemedBanner(style: .error) {
                    VStack(alignment: .leading, spacing: Theme.Spacing.xs) {
                        Label("Oprava neplatného tahu", systemImage: "exclamationmark.triangle.fill")
                            .font(Theme.Typography.subsection())
                        if let e = st.errorState {
                            let inv = e.invalidPos ?? "?"
                            let orig = e.originalPos ?? "?"
                            Text("Problém na poli \(inv) — vrať figurku na \(orig).")
                                .font(Theme.Typography.caption())
                        }
                        Text("Stejné LED zvýraznění jako na desce.")
                            .font(Theme.Typography.caption2())
                            .foregroundStyle(.secondary)
                    }
                    .foregroundStyle(Theme.Semantic.errorForeground)
                }
            }

            if let rs = st.restoreState, rs.resyncRequired == true {
                ThemedBanner(style: .warning, cornerRadius: 10) {
                    Label(
                        "Deska žádá resynchronizaci uloženého stavu — zkontroluj pozici na šachovnici.",
                        systemImage: "arrow.triangle.2.circlepath"
                    )
                    .font(Theme.Typography.caption())
                    .foregroundStyle(Theme.Semantic.warningForeground)
                }
            }

            if st.matrixGuardActive == true {
                let n = st.matrixGuardConflicts ?? 0
                ThemedBanner(style: .warning, cornerRadius: 10) {
                    Label(
                        n > 0
                            ? "Reed matice: konflikt snímačů (\(n)) — zkontroluj figury na polích."
                            : "Reed matice: hlídání zapnuto.",
                        systemImage: "square.grid.3x3.topleft.filled"
                    )
                    .font(Theme.Typography.caption())
                    .foregroundStyle(Theme.Semantic.warningForeground)
                }
            }

            if let p = st.puzzle, p.active == true || p.setupActive == true {
                ThemedBanner(style: .accent) {
                    VStack(alignment: .leading, spacing: Theme.Spacing.xs) {
                        Label("Puzzle na desce", systemImage: "puzzlepiece.extension")
                            .font(Theme.Typography.subsection())
                            .foregroundStyle(.primary)
                        if let t = p.title, !t.isEmpty {
                            Text(t)
                                .font(Theme.Typography.caption())
                        }
                        if let m = p.message, !m.isEmpty {
                            Text(m)
                                .font(Theme.Typography.caption2())
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }

            if st.boardSetupTutorial == true {
                ThemedBanner(style: .neutral, cornerRadius: 10) {
                    Label("Režim výuky postavení figurek — web / deska.", systemImage: "book.fill")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                }
            }

            if st.isGameFinished, let ge = st.gameEnd, ge.ended {
                ThemedBanner(style: .purple) {
                    VStack(alignment: .leading, spacing: Theme.Spacing.xs) {
                        Label("Partie ukončena", systemImage: "flag.checkered")
                            .font(Theme.Typography.subsection())
                            .foregroundStyle(.primary)
                        if let w = ge.winner, !w.isEmpty {
                            Text("Vítěz: \(w)")
                                .font(Theme.Typography.caption())
                        }
                        if let r = ge.reason, !r.isEmpty {
                            Text(r)
                                .font(Theme.Typography.caption2())
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }
        }
    }

    @ViewBuilder
    private var advancedControlsContent: some View {
        VStack(alignment: .leading, spacing: 14) {
            BoardWiFiLampSection()
            timerControlCard
            gameControlCard
        }
        .padding(.top, 8)
    }

    private var loadingBlock: some View {
        VStack(spacing: Theme.Spacing.m) {
            ProgressView()
            Text("Načítám pozici…")
                .font(Theme.Typography.body())
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(Theme.Spacing.xxl)
        .themeCard()
    }

    /// Po 3 s bez snapshotu zobrazíme obsah místo „nekonečného“ načítání.
    @ViewBuilder
    private var loadingOrTimeoutBlock: some View {
        TimelineView(.periodic(from: .now, by: 0.4)) { context in
            let elapsed = pollingWaitStarted.map { context.date.timeIntervalSince($0) } ?? 0
            if elapsed > 3 {
                loadingTimeoutContent
            } else {
                loadingBlock
            }
        }
    }

    private var loadingTimeoutContent: some View {
        VStack(spacing: Theme.Spacing.m) {
            Image(systemName: "antenna.radiowaves.left.and.right.slash")
                .font(.system(size: 36))
                .foregroundStyle(.secondary)
            Text("Šachovnice nenalezena")
                .font(.system(.headline, design: .rounded))
            Text("Nepodařilo se načíst pozici včas. Zkontroluj Wi‑Fi nebo Bluetooth a zkuste znovu, případně hrajte v ukázkovém režimu.")
                .font(Theme.Typography.caption())
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
            VStack(spacing: 10) {
                Button {
                    Task {
                        await store.refreshSnapshotFromServer()
                        store.startPolling()
                        pollingWaitStarted = Date()
                    }
                } label: {
                    Text("Zkusit znovu")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.themePrimary)
                if let dev = store.lastSavedBLEBoardForReconnect {
                    Button {
                        Task {
                            bleQuickReconnectBusy = true
                            await store.reconnectLastSavedBLEBoard()
                            bleQuickReconnectBusy = false
                            pollingWaitStarted = Date()
                        }
                    } label: {
                        if bleQuickReconnectBusy {
                            ProgressView()
                                .frame(maxWidth: .infinity)
                        } else {
                            Text("Znovu „\(dev.displayName)“ (Bluetooth)")
                                .frame(maxWidth: .infinity)
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Theme.accent)
                    .disabled(bleQuickReconnectBusy)
                }
                Button {
                    store.useMockBoard = true
                    store.startPolling()
                } label: {
                    Text("Ukázková deska (offline)")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.themeSecondary)
                Button {
                    showConnection = true
                } label: {
                    Text("Najít šachovnici")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(Theme.Spacing.l)
        .themeCard()
    }

    private var emptyStateBlock: some View {
        VStack(spacing: Theme.Spacing.m) {
            AppEmptyState(
                systemImage: "antenna.radiowaves.left.and.right.slash",
                title: "Žádná data z desky",
                message: "Připoj šachovnici v Najít šachovnici, nebo v Nastavení obnov připojení. S mobilními daty můžeš mít desku na Bluetooth a nápovědu Stockfish přes internet — v Nastavení zapni „Nepřepínat na Wi‑Fi po Bluetooth“.",
                primaryButtonTitle: "Otevřít připojení",
                primaryAction: { showConnection = true }
            )
            if let dev = store.lastSavedBLEBoardForReconnect {
                Button {
                    Task {
                        bleQuickReconnectBusy = true
                        await store.reconnectLastSavedBLEBoard()
                        bleQuickReconnectBusy = false
                    }
                } label: {
                    if bleQuickReconnectBusy {
                        ProgressView()
                            .frame(maxWidth: .infinity)
                    } else {
                        Label("Znovu „\(dev.displayName)“ přes Bluetooth", systemImage: "dot.radiowaves.left.and.right")
                            .frame(maxWidth: .infinity)
                    }
                }
                .buttonStyle(.bordered)
                .disabled(bleQuickReconnectBusy)
            }
        }
        .themeCard()
    }

    @ViewBuilder
    private var timerControlCard: some View {
        if store.supportsTimerRemoteControls {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("Časovač na desce")
                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                    Spacer()
                    Button {
                        Task {
                            isTimerRefreshBusy = true
                            timerDetail = await store.fetchTimerStateFromBoard()
                            isTimerRefreshBusy = false
                        }
                    } label: {
                        if isTimerRefreshBusy {
                            ProgressView()
                        } else {
                            Text("Stav")
                                .font(.system(.caption, design: .rounded))
                        }
                    }
                }
                if let t = timerDetail {
                    Text("\(t.config.name) — \(t.config.description)")
                        .font(.system(.caption, design: .rounded))
                        .foregroundStyle(.secondary)
                    HStack(spacing: 8) {
                        Button("Pauza") { Task { await store.postTimerPauseToBoard() } }
                        Button("Pokračovat") { Task { await store.postTimerResumeToBoard() } }
                        Button("Reset") { Task { await store.postTimerResetToBoard() } }
                    }
                    .buttonStyle(.bordered)
                    .font(.system(.caption, design: .rounded))
                } else {
                    Text("Stáhni detail časovače z desky.")
                        .font(.system(.caption, design: .rounded))
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    @ViewBuilder
    private var gameControlCard: some View {
        if store.snapshot != nil {
            VStack(alignment: .leading, spacing: 12) {
                Text("Ovládání hry")
                    .font(.system(.subheadline, design: .rounded).weight(.semibold))
                if store.supportsRemoteChessCommands {
                    Toggle(GameRemoteMovesCopy.toggleTitle, isOn: $remoteMovesEnabled)
                        .font(.system(.body, design: .rounded))
                        .accessibilityHint(legacyRemoteMovesAccessibilityHint)
                    Text("Posílá tahy na desku přes Wi‑Fi (`/api/move`) nebo Bluetooth (GATT `move`).")
                        .font(.system(.caption2, design: .rounded))
                        .foregroundStyle(.tertiary)
                    Button("Nová hra na desce") {
                        showNewGameSetup = true
                    }
                    .buttonStyle(.themePrimary)
                }
                if store.supportsRemoteChessCommands {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Virtuální pickup / drop")
                            .font(.system(.caption, design: .rounded).weight(.semibold))
                        HStack {
                            TextField("e2", text: $virtualPickupSquare)
                                .textFieldStyle(.roundedBorder)
                                .autocorrectionDisabled()
                            Button("Pickup") {
                                Task {
                                    await store.postVirtualActionOnBoard(
                                        action: "pickup",
                                        square: virtualPickupSquare.trimmingCharacters(in: .whitespacesAndNewlines),
                                        choice: nil
                                    )
                                }
                            }
                            .disabled(virtualPickupSquare.count < 2)
                        }
                        HStack {
                            TextField("e4", text: $virtualDropSquare)
                                .textFieldStyle(.roundedBorder)
                                .autocorrectionDisabled()
                            Button("Drop") {
                                Task {
                                    await store.postVirtualActionOnBoard(
                                        action: "drop",
                                        square: virtualDropSquare.trimmingCharacters(in: .whitespacesAndNewlines),
                                        choice: nil
                                    )
                                }
                            }
                            .disabled(virtualDropSquare.count < 2)
                        }
                    }
                    .font(.system(.caption, design: .rounded))
                }
                if !store.supportsRemoteChessCommands {
                    Text("Nová hra a odeslání tahů z aplikace vyžadují připojení k desce (Bluetooth nebo Wi‑Fi).")
                        .font(.system(.caption, design: .rounded))
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private func handleRemoteSquareTap(_ sq: String, snap: GameSnapshot) {
        let board = snap.board
        guard store.supportsRemoteChessCommands, store.snapshot?.status.webLocked != true else { return }
        if let from = remoteMoveFrom {
            if from == sq {
                remoteMoveFrom = nil
                #if os(iOS)
                HapticSettings.lightImpactIfEnabled()
                #endif
                return
            }
            if RemoteChessMoveLegality.moveNeedsPromotionPrompt(board: board, from: from, to: sq) {
                promotionPair = (from, sq)
                showPromotionPick = true
            } else {
                Task {
                    let ok = await store.postRemoteMove(from: from, to: sq, promotion: nil)
                    remoteMoveFrom = nil
                    if ok {
                        #if os(iOS)
                        HapticSettings.mediumImpactIfEnabled()
                        #endif
                        SoundManager.shared.playMoveSound()
                    } else {
                        triggerInvalidMoveFlash(from: from, to: sq)
                        let msg = (store.lastError?.trimmingCharacters(in: .whitespacesAndNewlines)).flatMap { $0.isEmpty ? nil : $0 }
                            ?? "Deska tah ne přijala. Zkontroluj pravidla, zámek z webu nebo spojení."
                        showLegacyTransientBoardMessage(msg)
                    }
                }
            }
        } else {
            if let ch = RemoteChessMoveLegality.pieceChar(board: board, square: sq),
               RemoteChessMoveLegality.isOwnPiece(ch, currentPlayer: snap.status.currentPlayer) {
                remoteMoveFrom = sq
                #if os(iOS)
                HapticSettings.lightImpactIfEnabled()
                #endif
                SoundManager.shared.playPieceSelectSound()
            } else {
                #if os(iOS)
                HapticSettings.notificationErrorIfEnabled()
                #endif
            }
        }
    }

    private func confirmPromotion(_ letter: String) {
        guard let p = promotionPair else { return }
        Task {
            let gs = store.snapshot?.status.gameState.lowercased() ?? ""
            let ok: Bool
            if gs == "promotion" {
                ok = await store.postRemotePromotion(choice: letter)
            } else {
                ok = await store.postRemoteMove(from: p.from, to: p.to, promotion: letter)
            }
            promotionPair = nil
            remoteMoveFrom = nil
            if ok {
                #if os(iOS)
                HapticSettings.mediumImpactIfEnabled()
                #endif
                SoundManager.shared.playMoveSound()
            } else {
                triggerInvalidMoveFlash(from: p.from, to: p.to)
                let msg = (store.lastError?.trimmingCharacters(in: .whitespacesAndNewlines)).flatMap { $0.isEmpty ? nil : $0 }
                    ?? "Promoce nebo tah nebyl přijat."
                showLegacyTransientBoardMessage(msg)
            }
        }
    }

    private func showLegacyTransientBoardMessage(_ text: String) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        transientBoardMessage = trimmed
        transientBoardMessageDismissID = UUID()
    }

    private var legacyRemoteMovesAccessibilityHint: String {
        var s = GameRemoteMovesCopy.usageHint
        if remoteMovesEnabled,
           let r = store.remoteChessFromAppBlockedReason?.trimmingCharacters(in: .whitespacesAndNewlines),
           !r.isEmpty {
            s += " " + r
        }
        return s
    }

    private var groupedBackground: Color {
        #if os(iOS)
        Color(uiColor: .systemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .windowBackgroundColor)
        #else
        Color.gray.opacity(0.08)
        #endif
    }

    private var cardFillColor: Color {
        #if os(iOS)
        Color(uiColor: .secondarySystemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .controlBackgroundColor)
        #else
        Color.gray.opacity(0.12)
        #endif
    }

    private var tertiaryFill: Color {
        #if os(iOS)
        Color(uiColor: .tertiarySystemFill)
        #elseif os(macOS)
        Color(nsColor: .tertiarySystemFill)
        #else
        Color.gray.opacity(0.18)
        #endif
    }

    @ViewBuilder
    private var pollingHintIfStale: some View {
        if let start = pollingWaitStarted, store.isPolling, store.snapshot == nil, !store.useMockBoard {
            TimelineView(.periodic(from: .now, by: 1)) { context in
                if context.date.timeIntervalSince(start) > 8 {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("Stále žádná data?")
                            .font(.system(.subheadline, design: .rounded).weight(.semibold))
                        Text("Zkontroluj připojení v Nastavení a že telefon je ve stejné síti jako šachovnice.")
                            .font(.system(.footnote, design: .rounded))
                            .foregroundStyle(.secondary)
                    }
                    .padding(.horizontal, 4)
                }
            }
        }
    }

    private func evaluationBlock(_ ev: MoveEvaluationResult) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            AppSectionHeader(title: "Zhodnocení tahu")
            Text(ev.message)
                .font(Theme.Typography.body())
                .foregroundStyle(gradeColor(ev.grade))
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private var hintSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Label("Nápověda (Stockfish + LED)", systemImage: "sparkles")
                .font(Theme.Typography.subsection())
                .foregroundStyle(.secondary)
            Picker("Úroveň nápovědy", selection: Bindable(store).moveHintTier) {
                ForEach(MoveHintTier.allCases) { tier in
                    Text(tier.shortTitle).tag(tier)
                }
            }
            .pickerStyle(.segmented)
            Text(store.moveHintTier.detailDescription)
                .font(Theme.Typography.caption2())
                .foregroundStyle(.tertiary)
            if store.activeLinkKind == .bluetooth, !network.isInternetLikelyForStockfish {
                Text("Zapni mobilní data nebo Wi‑Fi — výpočet nápovědy běží přes internet, deska zůstane na Bluetooth.")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(Theme.Semantic.warningForeground)
            }
            HStack(spacing: 10) {
                Button {
                    Task {
                        isHintBusy = true
                        await store.requestHint(internetPathSatisfied: network.isInternetLikelyForStockfish)
                        isHintBusy = false
                    }
                } label: {
                    Group {
                        if isHintBusy {
                            ProgressView()
                                .controlSize(.small)
                        } else {
                            Text("Navrhnout tah")
                        }
                    }
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(.themePrimary)
                .disabled(isHintBusy || store.snapshot == nil || store.snapshot?.status.webLocked == true)
                Button("Zrušit LED") {
                    Task { await store.clearHintLED() }
                }
                .buttonStyle(.themeSecondary)
                .disabled(store.snapshot?.status.webLocked == true)
            }
            if let h = store.lastHintSummary {
                Text(h)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
                    .lineLimit(3)
            }
            if store.boardUIPrefsPayload.chessHintLimit > 0 {
                Text("Zbývá nápověd — bílý: \(store.hintsRemainingWhite) · černý: \(store.hintsRemainingBlack)")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.tertiary)
            }
        }
        .padding(Theme.Spacing.m)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(cardFillColor)
        )
    }

    private func gradeColor(_ grade: MoveGrade) -> Color {
        switch grade {
        case .best, .good: return Theme.Semantic.success
        case .inaccuracy: return .yellow
        case .mistake: return Theme.Semantic.warningForeground
        case .blunder: return Theme.Semantic.errorForeground
        case .error: return .secondary
        }
    }

    @ViewBuilder
    private var learningModeCoachChrome: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .center, spacing: 14) {
                ZStack {
                    Circle()
                        .fill(
                            LinearGradient(
                                colors: [Theme.accent, Color(red: 0.15, green: 0.38, blue: 0.52)],
                                startPoint: .topLeading,
                                endPoint: .bottomTrailing
                            )
                        )
                        .frame(width: 56, height: 56)
                    Image(systemName: "crown.fill")
                        .font(.system(size: 24, weight: .semibold))
                        .foregroundStyle(.white)
                }
                VStack(alignment: .leading, spacing: 4) {
                    Text("AI Trenér")
                        .font(Theme.Typography.subsection())
                    Text("Zeptej se na plán celé pozice — ne jen na jeden tah.")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
            }
            Button {
                chessCoachManager.advice = ""
                showPositionCoachSheet = true
                Task { await requestPositionCoachEvaluation() }
            } label: {
                Label("Zhodnoť moji pozici", systemImage: "scope")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.themePrimary)
            .disabled(store.snapshot == nil)
        }
        .padding(Theme.Spacing.m)
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func requestPositionCoachEvaluation() async {
        guard store.snapshot != nil else {
            chessCoachManager.advice = "Nepodařilo se načíst pozici z desky."
            return
        }
        guard network.isInternetLikelyForStockfish else {
            chessCoachManager.advice = "Pro analýzu Stockfish je potřeba internet."
            return
        }
        await chessCoachManager.streamPositionPlanSummary(
            store: store,
            modelDownload: modelDownloadManager,
            userLevel: coachUserLevel
        )
    }
}

// MARK: - Záchranná brzda (celoobrazovka)

private struct CoachBlunderInterventionView: View {
    let message: String
    let onContinue: () -> Void

    var body: some View {
        ZStack {
            Color.black.opacity(0.52)
                .ignoresSafeArea()
            VStack(spacing: 22) {
                Text("Záchranná brzda")
                    .font(.system(.title2, design: .rounded).weight(.bold))
                    .foregroundStyle(.white)
                CoachBubbleView(isThinking: false, fullText: message, typewriterEnabled: true)
                    .padding(.horizontal, 8)
                Button("Rozumím, pokračovat", action: onContinue)
                    .buttonStyle(.themePrimary)
            }
            .padding(24)
        }
    }
}
