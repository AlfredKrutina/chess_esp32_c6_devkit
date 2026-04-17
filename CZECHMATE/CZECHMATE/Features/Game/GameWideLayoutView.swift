//
//  GameWideLayoutView.swift
//  CZECHMATE — Wide layout optimized for iPad and large screens
//
//  Two-column layout with board on left and controls on right
//

import CZECHMATEShared
import SwiftUI

struct GameWideLayoutView: View {
    @Environment(BoardConnectionStore.self) private var store

    @Bindable var viewState: GameViewState
    @Binding var isHintBusy: Bool

    private var gameBoardCardFill: Color {
        #if os(iOS)
        Color(uiColor: .secondarySystemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .controlBackgroundColor)
        #else
        Color.gray.opacity(0.14)
        #endif
    }

    var body: some View {
        GeometryReader { geo in
            let pad = Theme.Spacing.l
            let innerH = geo.size.height - pad * 2
            let innerW = geo.size.width - pad * 2
            let sideBarWidth = min(300, max(260, innerW * 0.30))

            HStack(alignment: .top, spacing: Theme.Spacing.xl) {
                ScrollView(.vertical, showsIndicators: false) {
                    wideBoardColumnContent
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .frame(maxWidth: .infinity)
                .frame(maxHeight: innerH, alignment: .top)

                sidebar
                    .frame(width: sideBarWidth)
                    .frame(maxHeight: innerH, alignment: .top)
            }
            .frame(width: geo.size.width, height: geo.size.height, alignment: .top)
            .padding(pad)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    /// Levý sloupec: deska + stav — v `ScrollView`, aby na nižším iPadu nic nelezlo přes sebe ani mimo obrazovku.
    @ViewBuilder
    private var wideBoardColumnContent: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            if let snap = store.snapshot, store.puzzleBoardPreview == nil {
                let timerBoard = viewState.sandboxHasValidBoard ? viewState.sandboxBoard : snap.board
                VStack(spacing: 6) {
                    GameBoardTimerLineIfNeeded(board: timerBoard, isBlackRow: true)
                    GameBoardTimerPauseStripIfNeeded()
                    ChessBoardContainer(
                        viewState: viewState,
                        flipped: viewState.displayOptions.boardFlipped,
                        zoom: viewState.boardZoom,
                        showCoordinates: viewState.displayOptions.showCoordinates,
                        animatePieces: viewState.displayOptions.moveAnimations,
                        invalidFlashFrom: viewState.invalidFlashFrom,
                        invalidFlashTo: viewState.invalidFlashTo,
                        remoteMovesFromAppEnabled: viewState.displayOptions.remoteMoves,
                        onInvalidRemoteMove: { from, to in
                            viewState.showInvalidMove(from: from, to: to)
                        },
                        onRemoteMoveRequestFailed: { msg in
                            viewState.showTransientBoardMessage(msg)
                        }
                    )
                    .aspectRatio(1, contentMode: .fit)
                    GameBoardTimerLineIfNeeded(board: timerBoard, isBlackRow: false)
                }
                .padding(10)
                .background(
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .fill(gameBoardCardFill)
                        .shadow(color: .black.opacity(0.06), radius: 10, y: 3)
                )

                GameStatusSummary(compact: true)
                    .frame(maxWidth: .infinity, alignment: .leading)

                HistoryReviewStatusBanner()

                BotModeHintBanner()

                TransientBoardMessageBanner(
                    message: viewState.transientBoardMessage,
                    onDismiss: { viewState.clearTransientBoardMessage() }
                )
            } else {
                ChessBoardContainer(
                    viewState: viewState,
                    flipped: viewState.displayOptions.boardFlipped,
                    zoom: viewState.boardZoom,
                    showCoordinates: viewState.displayOptions.showCoordinates,
                    animatePieces: viewState.displayOptions.moveAnimations,
                    invalidFlashFrom: viewState.invalidFlashFrom,
                    invalidFlashTo: viewState.invalidFlashTo,
                    remoteMovesFromAppEnabled: viewState.displayOptions.remoteMoves,
                    onInvalidRemoteMove: { from, to in
                        viewState.showInvalidMove(from: from, to: to)
                    },
                    onRemoteMoveRequestFailed: { msg in
                        viewState.showTransientBoardMessage(msg)
                    }
                )
                .aspectRatio(1, contentMode: .fit)
                .frame(maxWidth: .infinity)
            }

            BoardLampQuickStrip()
        }
    }

    // MARK: - Sidebar

    private var sidebar: some View {
        ScrollView(.vertical, showsIndicators: true) {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                WideStatusCard(viewState: viewState)

                PrePuzzleRestoreBanner { viewState.showTransientBoardMessage($0) }

                MoveEvaluationBanner()

                WideActionButtons(
                    viewState: viewState,
                    isHintBusy: $isHintBusy
                )

                GameCoachSection(viewState: viewState)

                WideOptionsPanel(viewState: viewState)

                if let moves = store.snapshot?.history.moves, !moves.isEmpty {
                    MoveHistoryCompactInteractiveView(
                        moves: moves,
                        selectedMoveIndex: Bindable(store).historyReviewMoveIndex
                    )
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.bottom, Theme.Spacing.m)
        }
    }
}

// MARK: - Wide Status Card

struct WideStatusCard: View {
    @Environment(BoardConnectionStore.self) private var store
    @Bindable var viewState: GameViewState

    var body: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            HStack {
                if let player = store.snapshot?.status.currentPlayer {
                    PlayerIndicator(player: player, isActive: true)
                }

                Spacer()

                if store.snapshot?.status.inCheck == true {
                    Label("Šach!", systemImage: "exclamationmark.triangle.fill")
                        .font(.subheadline)
                        .foregroundStyle(.red)
                }
            }

            Divider()

            HStack {
                Label("\(store.snapshot?.status.moveCount ?? 0) tahů", systemImage: "number")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Spacer()

                ConnectionStatusCompact()
            }
        }
        .padding(Theme.Spacing.l)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(.ultraThinMaterial)
        )
    }
}

// MARK: - Wide Action Buttons

struct WideActionButtons: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network

    @Bindable var viewState: GameViewState
    @Binding var isHintBusy: Bool

    var body: some View {
        VStack(spacing: Theme.Spacing.s) {
            Button {
                viewState.showNewGameSetup = true
            } label: {
                Label("Nová hra", systemImage: "plus")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .tint(Theme.accent)

            if store.snapshot?.status.isGameFinished == true {
                Button {
                    viewState.showEndgameReport = true
                } label: {
                    Label("Souhrn partie", systemImage: "chart.bar.doc.horizontal")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
            }

            HStack(spacing: Theme.Spacing.s) {
                Button {
                    Task { await store.requestUndo() }
                } label: {
                    Label("Zpět", systemImage: "arrow.uturn.backward")
                }
                .buttonStyle(.bordered)
                .disabled((store.snapshot?.status.moveCount ?? 0) == 0 || store.snapshot?.status.webLocked == true)

                Button {
                    Task {
                        isHintBusy = true
                        await store.requestHint(internetPathSatisfied: network.isInternetLikelyForStockfish)
                        isHintBusy = false
                    }
                } label: {
                    Label("Nápověda", systemImage: "lightbulb")
                }
                .buttonStyle(.bordered)
                .tint(.yellow)
                .disabled(
                    isHintBusy
                        || store.snapshot == nil
                        || store.snapshot?.status.webLocked == true
                )
            }

            Button {
                viewState.displayOptions.boardFlipped.toggle()
            } label: {
                Label("Otočit desku", systemImage: "arrow.up.arrow.down")
            }
            .buttonStyle(.borderless)

            if store.snapshot != nil, store.puzzleBoardPreview == nil, store.historyReviewMoveIndex == nil {
                if viewState.sandboxMode {
                    SandboxModePanel(
                        viewState: viewState,
                        onSyncFromBoard: { await viewState.exitSandboxAndRefresh(store: store) }
                    )
                } else {
                    SandboxEnterRow {
                        guard let b = store.snapshot?.board else { return }
                        viewState.enterSandboxMode(board: b)
                    }
                }
            }
        }
    }
}

// MARK: - Wide Options Panel

struct WideOptionsPanel: View {
    @Environment(BoardConnectionStore.self) private var store
    @Bindable var viewState: GameViewState

    var body: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text("Nastavení zobrazení")
                .font(.caption)
                .foregroundStyle(.secondary)

            Toggle("Souřadnice", isOn: $viewState.displayOptions.showCoordinates)
            Toggle("Animace", isOn: $viewState.displayOptions.moveAnimations)
            Toggle(GameRemoteMovesCopy.toggleTitle, isOn: $viewState.displayOptions.remoteMoves)
                .accessibilityHint(wideRemoteMovesAccessibilityHint)
            Toggle("Učící mód", isOn: $viewState.sessionConfig.learningMode)

            Picker("Nápověda", selection: Bindable(store).moveHintTier) {
                ForEach(MoveHintTier.allCases) { tier in
                    Text(tier.shortTitle).tag(tier)
                }
            }
            .pickerStyle(.menu)

            RemoteMovesHintBanner(remoteMovesEnabled: viewState.displayOptions.remoteMoves)
        }
        .padding(Theme.Spacing.l)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(.ultraThinMaterial)
        )
    }

    private var wideRemoteMovesAccessibilityHint: String {
        var s = GameRemoteMovesCopy.usageHint
        if viewState.displayOptions.remoteMoves,
           let r = store.remoteChessFromAppBlockedReason?.trimmingCharacters(in: .whitespacesAndNewlines),
           !r.isEmpty {
            s += " " + r
        }
        return s
    }
}

// MARK: - Connection Status Compact

struct ConnectionStatusCompact: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(compactDotColor)
                .frame(width: 8, height: 8)

            Text(compactLabel)
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
    }

    private var compactDotColor: Color {
        switch store.boardLinkIndicatorTier {
        case .live: return .green
        case .connecting: return .orange
        case .offline: return .red
        }
    }

    private var compactLabel: String {
        switch store.boardLinkIndicatorTier {
        case .live: return "Připojeno"
        case .connecting: return "Připojování…"
        case .offline: return store.hasActiveConnection ? "Bez odezvy" : "Odpojeno"
        }
    }
}

// MARK: - Move History Compact

struct MoveHistoryCompactInteractiveView: View {
    @Environment(BoardConnectionStore.self) private var store

    let moves: [HistoryMove]
    @Binding var selectedMoveIndex: Int?

    var body: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text("Historie tahů")
                .font(.caption)
                .foregroundStyle(.secondary)
            MoveHistoryView(
                moves: moves,
                selectedMoveIndex: $selectedMoveIndex,
                interactive: true,
                gradeForGlobalMoveIndex: { g in
                    store.moveEvalHistory.first { $0.moveIndex1Based == g + 1 }?.grade
                }
            )
        }
        .padding(Theme.Spacing.l)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(.ultraThinMaterial)
        )
    }
}

// MARK: - Preview

#Preview {
    GameWideLayoutView(
        viewState: GameViewState(),
        isHintBusy: .constant(false)
    )
    .environment(BoardConnectionStore())
    .environment(NetworkStatusMonitor())
    .environmentObject(LearningModeManager())
}
