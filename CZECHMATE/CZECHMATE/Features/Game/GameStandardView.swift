//
//  GameStandardView.swift
//  CZECHMATE — Standard phone layout for game view
//
//  Optimized for iPhone with vertical scrolling layout
//

import CZECHMATEShared
import SwiftUI
#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

struct GameStandardView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass

    @Bindable var viewState: GameViewState
    @Binding var isHintBusy: Bool

    private var showProminentConnectionCard: Bool {
        if store.isScanning { return true }
        if !store.hasActiveConnection { return true }
        if store.boardLinkIndicatorTier != .live { return true }
        if let e = store.lastError, !e.isEmpty { return true }
        return false
    }

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
        ScrollView {
            VStack(spacing: Theme.Spacing.l) {
                if showProminentConnectionCard {
                    ConnectionStatusCard()
                        .padding(.horizontal, Theme.Spacing.l)
                }

                PrePuzzleRestoreBanner { viewState.showTransientBoardMessage($0) }
                    .padding(.horizontal, Theme.Spacing.l)

                VStack(spacing: Theme.Spacing.s) {
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

                        MoveEvaluationBanner()

                        BotModeHintBanner()

                        if let moves = store.snapshot?.history.moves, !moves.isEmpty {
                            MoveHistoryView(
                                moves: moves,
                                selectedMoveIndex: Bindable(store).historyReviewMoveIndex,
                                interactive: true,
                                gradeForGlobalMoveIndex: { g in
                                    store.moveEvalHistory.first { $0.moveIndex1Based == g + 1 }?.grade
                                }
                            )
                            .frame(maxWidth: .infinity, alignment: .leading)
                        }

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
                }
                .padding(.horizontal, Theme.Spacing.l)

                VStack(spacing: Theme.Spacing.m) {
                    RemoteMovesHintBanner(remoteMovesEnabled: viewState.displayOptions.remoteMoves)

                    GameControlPanel(
                        viewState: viewState,
                        isHintBusy: $isHintBusy
                    )
                }
                .padding(.top, Theme.Spacing.s)
                .padding(.horizontal, Theme.Spacing.l)

                GameCoachSection(viewState: viewState)
                    .padding(.horizontal, Theme.Spacing.l)

                BoardLampQuickStrip()
                    .padding(.horizontal, Theme.Spacing.l)

            }
            .padding(.vertical, Theme.Spacing.l)
        }
    }
}

// MARK: - Game Status Summary

struct GameStatusSummary: View {
    @Environment(BoardConnectionStore.self) private var store

    var compact: Bool = false

    var body: some View {
        HStack(spacing: compact ? Theme.Spacing.m : Theme.Spacing.l) {
            if let player = store.snapshot?.status.currentPlayer {
                PlayerIndicator(
                    player: player,
                    isActive: true,
                    compact: compact
                )
            }

            Spacer(minLength: Theme.Spacing.s)

            if let count = store.snapshot?.status.moveCount {
                Label("\(count)", systemImage: "number")
                    .font(compact ? .caption.weight(.medium) : .subheadline)
                    .foregroundStyle(.secondary)
            }

            if store.snapshot?.status.inCheck == true {
                Label("Šach", systemImage: "exclamationmark.shield.fill")
                    .font(compact ? .caption.weight(.semibold) : .subheadline)
                    .foregroundStyle(.red)
            }
        }
        .padding(compact ? Theme.Spacing.m : Theme.Spacing.l)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(boardChromeFill)
                .shadow(color: .black.opacity(0.06), radius: 3, y: 1)
        )
        .accessibilityElement(children: .combine)
        .accessibilityLabel(gameStatusAccessibilityLabel)
    }

    private var gameStatusAccessibilityLabel: String {
        var parts: [String] = []
        if let player = store.snapshot?.status.currentPlayer {
            let white = player.lowercased() == "white" || player.lowercased() == "bílý"
            parts.append(white ? "Na tahu bílý" : "Na tahu černý")
        }
        if let count = store.snapshot?.status.moveCount {
            parts.append("Tah \(count)")
        }
        if store.snapshot?.status.inCheck == true {
            parts.append("Šach")
        }
        return parts.joined(separator: ", ")
    }

    private var boardChromeFill: Color {
        #if os(iOS)
        Color(uiColor: .secondarySystemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .controlBackgroundColor)
        #else
        Color.gray.opacity(0.14)
        #endif
    }
}

// MARK: - Player Indicator

struct PlayerIndicator: View {
    let player: String
    let isActive: Bool
    var compact: Bool = false

    private var isWhite: Bool {
        player.lowercased() == "white" || player.lowercased() == "bílý"
    }

    private var displayName: String {
        isWhite ? "Bílý" : "Černý"
    }

    var body: some View {
        HStack(spacing: compact ? 6 : 8) {
            Circle()
                .fill(isWhite ? .white : .black)
                .stroke(isWhite ? .gray.opacity(0.3) : .clear, lineWidth: 1)
                .frame(width: compact ? 12 : 16, height: compact ? 12 : 16)

            Text(compact ? displayName : "Na tahu: \(displayName)")
                .font(compact ? .subheadline.weight(.semibold) : .headline)
        }
        .padding(.horizontal, compact ? 10 : 12)
        .padding(.vertical, compact ? 4 : 6)
        .background(
            Capsule()
                .fill(isWhite ? .white.opacity(0.12) : .black.opacity(0.12))
        )
    }
}

// MARK: - Preview

#Preview {
    GameStandardView(
        viewState: GameViewState(),
        isHintBusy: .constant(false)
    )
    .environment(BoardConnectionStore())
}
