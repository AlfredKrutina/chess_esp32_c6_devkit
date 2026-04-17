//
//  GameContainerView.swift
//  CZECHMATE — Game view coordinator and router
//
//  Replaces monolithic GameView with modular architecture
//

import CZECHMATEShared
import SwiftUI

/// Main game container that coordinates between different layout modes
struct GameContainerView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass
    @EnvironmentObject private var learningModeManager: LearningModeManager
    @EnvironmentObject private var chessCoachManager: ChessCoachManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager

    @AppStorage("czechmate.showBoardCoordinates") private var storedShowCoordinates = true
    @AppStorage("czechmate.moveAnimationsEnabled") private var storedMoveAnimations = true
    @AppStorage("czechmate.remoteMovesEnabled") private var storedRemoteMoves = false
    @AppStorage("czechmate.boardFlipped") private var storedBoardFlipped = false

    @AppStorage("czechmate.game.layoutMode") private var layoutModeRaw: String = GameLayoutMode.standard.rawValue
    @AppStorage("czechmate.game.boardZoomStorage") private var boardZoomStorage = 1.0
    @AppStorage("czechmate.game.regularLayoutDefaultDone") private var regularLayoutDefaultDone = false
    @AppStorage("czechmate.endgameReportAutoOpen") private var endgameReportAutoOpen = true
    @AppStorage("czechmate.coach.userLevel") private var storedCoachUserLevel = 4

    @State private var viewState = GameViewState()
    @State private var lastSeenMoveCount: UInt32 = 0
    @State private var isHintBusy: Bool = false
    /// Po zavření souhrnu pro daný konec partie znovu neotevírat automaticky (stejný `state_version` + počet tahů).
    @State private var endgameReportDismissedAutoKey: String?

    var body: some View {
        gameNavStackWithSheets
            .modifier(GameContainerLifecycleModifier(
                viewState: viewState,
                lastSeenMoveCount: $lastSeenMoveCount,
                storedShowCoordinates: $storedShowCoordinates,
                storedMoveAnimations: $storedMoveAnimations,
                storedRemoteMoves: $storedRemoteMoves,
                storedBoardFlipped: $storedBoardFlipped,
                layoutModeRaw: $layoutModeRaw,
                boardZoomStorage: $boardZoomStorage,
                regularLayoutDefaultDone: $regularLayoutDefaultDone,
                storedCoachUserLevel: $storedCoachUserLevel,
                learningModeManager: learningModeManager,
                chessCoachManager: chessCoachManager,
                modelDownloadManager: modelDownloadManager,
                store: store,
                syncLearningModeFromSessionToggle: syncLearningModeFromSessionToggle,
                applyBoardPrefsFromStorage: applyBoardPrefsFromStorage,
                applyLayoutAndZoomFromStorage: applyLayoutAndZoomFromStorage,
                applyRegularWidthLayoutDefaultIfNeeded: applyRegularWidthLayoutDefaultIfNeeded
            ))
    }

    /// Odděleno kvůli type-check výkonu kompilátoru (dlouhý řetězec `.sheet` / `.onChange`).
    private var gameNavStackWithSheets: some View {
        gameNavStack
            .sheet(isPresented: $viewState.showConnection) {
                BoardDiscoveryView()
                    .environment(store)
            }
            .sheet(isPresented: $viewState.showNewGameSetup) {
                NewGameSetupSheet()
                    .environment(store)
            }
            .sheet(isPresented: $viewState.showGameControls) {
                GameControlsSheet(viewState: viewState)
                    .environment(store)
                    .environmentObject(learningModeManager)
                    .environmentObject(modelDownloadManager)
                    .environmentObject(chessCoachManager)
            }
            .sheet(isPresented: $viewState.showCoachOnboarding) {
                CoachOnboardingView(
                    modelDownload: modelDownloadManager,
                    onFinished: { viewState.showCoachOnboarding = false }
                )
            }
            .sheet(isPresented: $viewState.showPositionCoach) {
                PositionCoachSheet(coachUserLevel: viewState.sessionConfig.coachUserLevel)
            }
            .sheet(isPresented: $viewState.showCoachChat) {
                CoachChatSheet(coachUserLevel: viewState.sessionConfig.coachUserLevel)
            }
            .sheet(isPresented: $viewState.showEndgameReport) {
                GameEndReportSheet()
                    .environment(store)
            }
            .onChange(of: endgameReportAutoOpenKey) { _, newKey in
                if newKey == nil {
                    // Při chybějícím snímku neresetovat otevřený souhrn (pollování / výpadek).
                    guard let snap = store.snapshot else { return }
                    if !snap.status.isGameFinished {
                        endgameReportDismissedAutoKey = nil
                        viewState.showEndgameReport = false
                    }
                    return
                }
                if let k = newKey, k != endgameReportDismissedAutoKey, endgameReportAutoOpen {
                    viewState.showEndgameReport = true
                    AppDebugLog.staging("EndgameReport: auto-open key=\(k)")
                }
            }
            .onChange(of: viewState.showEndgameReport) { _, show in
                if !show, let k = endgameReportAutoOpenKey {
                    endgameReportDismissedAutoKey = k
                }
            }
    }

    /// Klíč konce partie — `GameEndReportSessionKey` (volitelně `game_id` z JSON).
    private var endgameReportAutoOpenKey: String? {
        guard let s = store.snapshot else { return nil }
        return GameEndReportSessionKey.fingerprint(for: s)
    }

    private var gameNavStack: some View {
        NavigationStack {
            Group {
                switch viewState.layoutMode {
                case .boardOnly:
                    GameBoardOnlyView(viewState: viewState)
                case .standard:
                    GameStandardView(
                        viewState: viewState,
                        isHintBusy: $isHintBusy
                    )
                case .wide:
                    GameWideLayoutView(
                        viewState: viewState,
                        isHintBusy: $isHintBusy
                    )
                }
            }
            .refreshable {
                await store.refreshSnapshotFromServer()
            }
            .background(groupedBackground)
            .navigationTitle("")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                GameToolbar(
                    viewState: viewState,
                    isHintBusy: $isHintBusy
                )
            }
        }
        .overlay {
            if let mgr = store.boardSetupManager {
                let _ = mgr.currentIndex
                let _ = mgr.phase
                SetupModeView(manager: mgr)
                    .id("\(mgr.currentIndex)-\(String(describing: mgr.phase))")
            }
        }
    }

    private func syncLearningModeFromSessionToggle(_ wantsOn: Bool) {
        if wantsOn {
            guard modelDownloadManager.isModelInstalled else {
                learningModeManager.pendingActivateAfterModel = true
                var cfg = viewState.sessionConfig
                cfg.learningMode = false
                viewState.sessionConfig = cfg
                viewState.showCoachOnboarding = true
                return
            }
            if !learningModeManager.isLearningModeActive {
                learningModeManager.isLearningModeActive = true
            }
        } else if learningModeManager.isLearningModeActive {
            learningModeManager.isLearningModeActive = false
        }
    }

    private func applyBoardPrefsFromStorage() {
        viewState.displayOptions.showCoordinates = storedShowCoordinates
        viewState.displayOptions.moveAnimations = storedMoveAnimations
        viewState.displayOptions.remoteMoves = storedRemoteMoves
        viewState.displayOptions.boardFlipped = storedBoardFlipped
        let coachRaw = UserDefaults.standard.integer(forKey: "czechmate.coach.userLevel")
        let coachClamped = (coachRaw >= 1 && coachRaw <= 4) ? coachRaw : 4
        if viewState.sessionConfig.coachUserLevel != coachClamped {
            var cfg = viewState.sessionConfig
            cfg.coachUserLevel = coachClamped
            viewState.sessionConfig = cfg
        }
    }

    private func applyLayoutAndZoomFromStorage() {
        if let mode = GameLayoutMode(rawValue: layoutModeRaw) {
            viewState.layoutMode = mode
        }
        viewState.boardZoom = CGFloat(boardZoomStorage)
    }

    /// První vstup do regular width: jednorázově nabídnout široké rozložení, pokud je uloženo výchozí „standard“.
    private func applyRegularWidthLayoutDefaultIfNeeded() {
        guard horizontalSizeClass == .regular else { return }
        if regularLayoutDefaultDone { return }
        regularLayoutDefaultDone = true
        if layoutModeRaw == GameLayoutMode.standard.rawValue {
            viewState.layoutMode = .wide
            layoutModeRaw = GameLayoutMode.wide.rawValue
        }
    }

    private var groupedBackground: some View {
        Color(uiColor: .systemGroupedBackground)
            .ignoresSafeArea()
    }
}

// MARK: - Lifecycle (odděleno kvůli type-checku `GameContainerView.body`)

private struct GameContainerLifecycleModifier: ViewModifier {
    var viewState: GameViewState
    @Binding var lastSeenMoveCount: UInt32
    @Binding var storedShowCoordinates: Bool
    @Binding var storedMoveAnimations: Bool
    @Binding var storedRemoteMoves: Bool
    @Binding var storedBoardFlipped: Bool
    @Binding var layoutModeRaw: String
    @Binding var boardZoomStorage: Double
    @Binding var regularLayoutDefaultDone: Bool
    @Binding var storedCoachUserLevel: Int

    var learningModeManager: LearningModeManager
    var chessCoachManager: ChessCoachManager
    var modelDownloadManager: ModelDownloadManager
    var store: BoardConnectionStore

    var syncLearningModeFromSessionToggle: (Bool) -> Void
    var applyBoardPrefsFromStorage: () -> Void
    var applyLayoutAndZoomFromStorage: () -> Void
    var applyRegularWidthLayoutDefaultIfNeeded: () -> Void

    func body(content: Content) -> some View {
        content
            .modifier(
                GameContainerCoachLifecycleModifier(
                    viewState: viewState,
                    learningModeManager: learningModeManager,
                    chessCoachManager: chessCoachManager,
                    modelDownloadManager: modelDownloadManager,
                    regularLayoutDefaultDone: $regularLayoutDefaultDone,
                    syncLearningModeFromSessionToggle: syncLearningModeFromSessionToggle,
                    applyBoardPrefsFromStorage: applyBoardPrefsFromStorage,
                    applyLayoutAndZoomFromStorage: applyLayoutAndZoomFromStorage,
                    applyRegularWidthLayoutDefaultIfNeeded: applyRegularWidthLayoutDefaultIfNeeded
                )
            )
            .modifier(
                GameContainerPrefsSyncModifier(
                    viewState: viewState,
                    storedShowCoordinates: $storedShowCoordinates,
                    storedMoveAnimations: $storedMoveAnimations,
                    storedRemoteMoves: $storedRemoteMoves,
                    storedBoardFlipped: $storedBoardFlipped,
                    storedCoachUserLevel: $storedCoachUserLevel,
                    layoutModeRaw: $layoutModeRaw,
                    boardZoomStorage: $boardZoomStorage
                )
            )
            .modifier(
                GameContainerGameEventsModifier(
                    viewState: viewState,
                    lastSeenMoveCount: $lastSeenMoveCount,
                    learningModeManager: learningModeManager,
                    store: store
                )
            )
    }
}

// MARK: - Lifecycle dílčí modifikátory (type-check)

private struct GameContainerCoachLifecycleModifier: ViewModifier {
    @Environment(\.scenePhase) private var scenePhase

    var viewState: GameViewState
    var learningModeManager: LearningModeManager
    var chessCoachManager: ChessCoachManager
    var modelDownloadManager: ModelDownloadManager
    @Binding var regularLayoutDefaultDone: Bool
    var syncLearningModeFromSessionToggle: (Bool) -> Void
    var applyBoardPrefsFromStorage: () -> Void
    var applyLayoutAndZoomFromStorage: () -> Void
    var applyRegularWidthLayoutDefaultIfNeeded: () -> Void

    func body(content: Content) -> some View {
        content
            .onAppear {
                applyBoardPrefsFromStorage()
                applyLayoutAndZoomFromStorage()
                applyRegularWidthLayoutDefaultIfNeeded()
                var cfgLearning = viewState.sessionConfig
                cfgLearning.learningMode = learningModeManager.isLearningModeActive
                viewState.sessionConfig = cfgLearning
                chessCoachManager.learningMode = learningModeManager
                AppDebugLog.coachTrace(
                    "GameContainer learning sync onAppear learningMode=\(learningModeManager.isLearningModeActive) modelInstalled=\(modelDownloadManager.isModelInstalled)"
                )
                chessCoachManager.syncLearningModeAndModel(
                    learningModeActive: learningModeManager.isLearningModeActive,
                    modelDownload: modelDownloadManager
                )
            }
            .onChange(of: viewState.sessionConfig.learningMode) { _, wantsOn in
                syncLearningModeFromSessionToggle(wantsOn)
            }
            .onChange(of: learningModeManager.isLearningModeActive) { _, active in
                AppDebugLog.coachTrace("GameContainer learningMode toggled → \(active)")
                if viewState.sessionConfig.learningMode != active {
                    var cfg = viewState.sessionConfig
                    cfg.learningMode = active
                    viewState.sessionConfig = cfg
                }
                chessCoachManager.syncLearningModeAndModel(
                    learningModeActive: active,
                    modelDownload: modelDownloadManager
                )
            }
            .onChange(of: modelDownloadManager.isModelInstalled) { _, installed in
                AppDebugLog.coachTrace("GameContainer model isModelInstalled → \(installed), re-sync coach")
                chessCoachManager.syncLearningModeAndModel(
                    learningModeActive: learningModeManager.isLearningModeActive,
                    modelDownload: modelDownloadManager
                )
            }
            .onChange(of: scenePhase) { _, phase in
                if phase == .active {
                    applyBoardPrefsFromStorage()
                    applyLayoutAndZoomFromStorage()
                }
            }
    }
}

private struct GameContainerPrefsSyncModifier: ViewModifier {
    var viewState: GameViewState
    @Binding var storedShowCoordinates: Bool
    @Binding var storedMoveAnimations: Bool
    @Binding var storedRemoteMoves: Bool
    @Binding var storedBoardFlipped: Bool
    @Binding var storedCoachUserLevel: Int
    @Binding var layoutModeRaw: String
    @Binding var boardZoomStorage: Double

    func body(content: Content) -> some View {
        content
            .onChange(of: storedShowCoordinates) { _, v in
                if viewState.displayOptions.showCoordinates != v {
                    viewState.displayOptions.showCoordinates = v
                }
            }
            .onChange(of: storedMoveAnimations) { _, v in
                if viewState.displayOptions.moveAnimations != v {
                    viewState.displayOptions.moveAnimations = v
                }
            }
            .onChange(of: storedRemoteMoves) { _, v in
                if viewState.displayOptions.remoteMoves != v {
                    viewState.displayOptions.remoteMoves = v
                }
            }
            .onChange(of: storedBoardFlipped) { _, v in
                if viewState.displayOptions.boardFlipped != v {
                    viewState.displayOptions.boardFlipped = v
                }
            }
            .onChange(of: storedCoachUserLevel) { _, v in
                let clamped = (v >= 1 && v <= 4) ? v : 4
                if viewState.sessionConfig.coachUserLevel != clamped {
                    var cfg = viewState.sessionConfig
                    cfg.coachUserLevel = clamped
                    viewState.sessionConfig = cfg
                }
            }
            .onChange(of: viewState.displayOptions) { _, newValue in
                if storedShowCoordinates != newValue.showCoordinates {
                    storedShowCoordinates = newValue.showCoordinates
                }
                if storedMoveAnimations != newValue.moveAnimations {
                    storedMoveAnimations = newValue.moveAnimations
                }
                if storedRemoteMoves != newValue.remoteMoves {
                    storedRemoteMoves = newValue.remoteMoves
                }
                if storedBoardFlipped != newValue.boardFlipped {
                    storedBoardFlipped = newValue.boardFlipped
                }
            }
            .onChange(of: viewState.sessionConfig.coachUserLevel) { _, v in
                let clamped = (v >= 1 && v <= 4) ? v : 4
                if storedCoachUserLevel != clamped {
                    storedCoachUserLevel = clamped
                }
            }
            .onChange(of: viewState.layoutMode) { _, mode in
                if layoutModeRaw != mode.rawValue {
                    layoutModeRaw = mode.rawValue
                }
            }
            .onChange(of: layoutModeRaw) { _, raw in
                let mode = GameLayoutMode(rawValue: raw) ?? .standard
                if viewState.layoutMode != mode {
                    viewState.layoutMode = mode
                }
            }
            .onChange(of: viewState.boardZoom) { _, z in
                let d = Double(z)
                if abs(boardZoomStorage - d) > 0.000_1 {
                    boardZoomStorage = d
                }
            }
            .onChange(of: boardZoomStorage) { _, z in
                let cg = CGFloat(z)
                if abs(viewState.boardZoom - cg) > 0.000_1 {
                    viewState.boardZoom = cg
                }
            }
    }
}

private struct GameContainerGameEventsModifier: ViewModifier {
    var viewState: GameViewState
    @Binding var lastSeenMoveCount: UInt32
    var learningModeManager: LearningModeManager
    var store: BoardConnectionStore

    func body(content: Content) -> some View {
        content
            .onChange(of: store.snapshot?.status.moveCount) { _, newValue in
                guard let n = newValue else { return }
                if n < lastSeenMoveCount {
                    learningModeManager.resetBlunderTrackingForNewGame()
                }
                if n != lastSeenMoveCount {
                    lastSeenMoveCount = n
                    store.historyReviewMoveIndex = nil
                    viewState.resetUIState()
                }
            }
            .onChange(of: store.lastMoveEvaluation) { _, evaluation in
                guard let ev = evaluation, let s = store.snapshot else { return }
                learningModeManager.consumeMoveEvaluationForBlunderBrake(
                    evaluation: ev,
                    currentMoveCount: Int(s.status.moveCount)
                )
            }
    }
}

// MARK: - Preview

#Preview {
    GameContainerView()
        .environment(BoardConnectionStore())
        .environment(NetworkStatusMonitor())
        .environmentObject(LearningModeManager())
        .environmentObject(ModelDownloadManager())
        .environmentObject(ChessCoachManager())
}
