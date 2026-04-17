//
//  GameBoardOnlyView.swift
//  CZECHMATE — Minimalist board-only layout
//
//  Focus mode with just the chess board and essential info overlay
//

import CZECHMATEShared
import SwiftUI

struct GameBoardOnlyView: View {
    @Environment(BoardConnectionStore.self) private var store

    @Bindable var viewState: GameViewState
    @State private var showOverlay = true

    var body: some View {
        ZStack {
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
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            if showOverlay {
                VStack {
                    HStack {
                        if let player = store.snapshot?.status.currentPlayer {
                            PlayerPill(player: player)
                        }

                        Spacer()

                        if let count = store.snapshot?.status.moveCount {
                            MoveCountBadge(count: count)
                        }

                        Button {
                            withAnimation(.easeInOut(duration: 0.2)) {
                                showOverlay = false
                            }
                        } label: {
                            Image(systemName: "eye.slash")
                                .font(.title3)
                                .padding(10)
                                .background(.ultraThinMaterial)
                                .clipShape(Circle())
                        }
                        .accessibilityLabel("Skrýt přehled nad deskou")
                    }
                    .padding()
                    .background(.ultraThinMaterial.opacity(0.001))

                    HistoryReviewStatusBanner()
                        .padding(.horizontal, 12)

                    PrePuzzleRestoreBanner { viewState.showTransientBoardMessage($0) }
                        .padding(.horizontal, 12)

                    PuzzlePreviewModeBanner()
                        .padding(.horizontal, 12)

                    BotModeHintBanner()
                        .padding(.horizontal, 12)

                    MoveEvaluationBanner(compact: true)
                        .padding(.horizontal, 12)

                    if let snap = store.snapshot, store.puzzleBoardPreview == nil {
                        let timerBoard = viewState.sandboxHasValidBoard ? viewState.sandboxBoard : snap.board
                        GameBoardTimerLineIfNeeded(board: timerBoard, isBlackRow: true)
                            .padding(.horizontal, 12)
                        GameBoardTimerPauseStripIfNeeded()
                            .padding(.horizontal, 12)
                    }

                    Spacer()
                        .allowsHitTesting(false)

                    if let snap = store.snapshot, store.puzzleBoardPreview == nil {
                        let timerBoard = viewState.sandboxHasValidBoard ? viewState.sandboxBoard : snap.board
                        GameBoardTimerLineIfNeeded(board: timerBoard, isBlackRow: false)
                            .padding(.horizontal, 12)
                    }

                    HStack(spacing: 20) {
                        Button {
                            viewState.displayOptions.boardFlipped.toggle()
                        } label: {
                            Image(systemName: "arrow.up.arrow.down")
                                .font(.title3)
                                .padding(12)
                                .background(.ultraThinMaterial)
                                .clipShape(Circle())
                        }

                        Button {
                            viewState.layoutMode = .standard
                        } label: {
                            Image(systemName: "rectangle.grid.2x2")
                                .font(.title3)
                                .padding(12)
                                .background(.ultraThinMaterial)
                                .clipShape(Circle())
                        }
                        .accessibilityLabel("Zpět na standardní rozložení")
                    }
                    .padding(.bottom, 30)
                    .background(.ultraThinMaterial.opacity(0.001))
                }
                .transition(.opacity)
            }

            if !showOverlay {
                VStack {
                    HStack {
                        Spacer()
                        Button {
                            withAnimation(.easeInOut(duration: 0.2)) {
                                showOverlay = true
                            }
                        } label: {
                            Image(systemName: "eye")
                                .font(.title3)
                                .padding(10)
                                .background(.ultraThinMaterial)
                                .clipShape(Circle())
                        }
                        .padding(.trailing, 12)
                        .padding(.top, 8)
                        .accessibilityLabel("Zobrazit přehled nad deskou")
                    }
                    Spacer()
                }
                .allowsHitTesting(true)
            }

            VStack(spacing: 8) {
                if viewState.sandboxMode {
                    SandboxBoardOnlyStrip(
                        viewState: viewState,
                        onSyncFromBoard: { await viewState.exitSandboxAndRefresh(store: store) }
                    )
                }
                TransientBoardMessageBanner(
                    message: viewState.transientBoardMessage,
                    onDismiss: { viewState.clearTransientBoardMessage() }
                )
                RemoteMovesHintBanner(remoteMovesEnabled: viewState.displayOptions.remoteMoves)
                    .allowsHitTesting(false)
            }
            .padding(.horizontal, 12)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)
            .padding(.bottom, 8)
        }
    }
}

// MARK: - Player Pill

struct PlayerPill: View {
    let player: String

    private var isWhite: Bool {
        player.lowercased() == "white" || player.lowercased() == "bílý"
    }

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(isWhite ? .white : .black)
                .frame(width: 12, height: 12)
                .overlay(
                    Circle()
                        .stroke(isWhite ? Color.gray.opacity(0.4) : Color.clear, lineWidth: 1)
                )
            Text(isWhite ? "Bílý" : "Černý")
                .font(.subheadline.weight(.semibold))
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(.ultraThinMaterial)
        .clipShape(Capsule())
    }
}

// MARK: - Move Count Badge

struct MoveCountBadge: View {
    let count: UInt32

    var body: some View {
        Text("\(count)")
            .font(.caption.monospacedDigit().weight(.semibold))
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .background(.ultraThinMaterial)
            .clipShape(Capsule())
    }
}
