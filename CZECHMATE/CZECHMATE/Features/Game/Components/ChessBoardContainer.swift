//
//  ChessBoardContainer.swift
//  CZECHMATE — Adapter for ChessBoardView with simplified interface
//
//  Wraps existing ChessBoardView with new simplified init
//

import CZECHMATEShared
import SwiftUI

struct ChessBoardContainer: View {
    @Environment(BoardConnectionStore.self) private var store
    @AppStorage("czechmate.boardStyleRaw") private var boardStyleRaw = ChessBoardStyle.wooden.rawValue

    @Bindable var viewState: GameViewState

    var flipped: Bool = false
    var zoom: CGFloat = 1.0
    var showCoordinates: Bool = true
    var animatePieces: Bool = true
    var invalidFlashFrom: String?
    var invalidFlashTo: String?
    /// Zapne vzdálené tahy z aplikace (Wi‑Fi `/api/move` nebo BLE `move`), pokud je deska připojená.
    var remoteMovesFromAppEnabled: Bool = false
    /// Volitelné: bliknutí při neplatném tahu (např. `GameViewState.showInvalidMove`).
    var onInvalidRemoteMove: ((String, String) -> Void)?
    /// Volitelné: text z `lastError` nebo výchozí zpráva, když deska tah ne přijme.
    var onRemoteMoveRequestFailed: ((String) -> Void)?

    @State private var internalZoom: CGFloat = 1.0
    @State private var remoteMoveFrom: String?
    @State private var promotionPair: (from: String, to: String)?
    @State private var showPromotionPick = false

    /// Výchozí postavení (startovní pozice), když ještě nemáme živý snímek z desky.
    private static var placeholderStartBoard: [[String]] {
        let fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        return FENParser.parseBoard(fromFEN: fen)
            ?? Array(repeating: Array(repeating: " ", count: 8), count: 8)
    }

    private var board: [[String]] {
        if let preview = store.puzzleBoardPreview {
            return preview
        }
        if let idx = store.historyReviewMoveIndex, let snap = store.snapshot {
            let n = idx + 1
            if let replay = HistoryBoardReplay.board(afterApplyingFirst: n, moves: snap.history.moves) {
                return replay
            }
        }
        if viewState.sandboxHasValidBoard {
            return viewState.sandboxBoard
        }
        if let live = store.snapshot?.board {
            return live
        }
        return Self.placeholderStartBoard
    }

    private var lastMoveFrom: String? {
        store.snapshot?.history.moves.last?.from
    }

    private var lastMoveTo: String? {
        store.snapshot?.history.moves.last?.to
    }

    private var remoteInteractionActive: Bool {
        remoteMovesFromAppEnabled
            && store.supportsRemoteChessCommands
            && store.snapshot?.status.webLocked != true
            && store.snapshot?.status.isErrorRecoveryActive != true
            && store.snapshot?.status.castlingInProgress != true
            && store.puzzleBoardPreview == nil
            && store.historyReviewMoveIndex == nil
            && !viewState.sandboxMode
    }

    private var sandboxInteractionActive: Bool {
        viewState.sandboxHasValidBoard
            && store.puzzleBoardPreview == nil
            && store.historyReviewMoveIndex == nil
    }

    private var effectiveRemoteSelectionSquare: String? {
        if sandboxInteractionActive {
            return viewState.sandboxSelectedSquare
        }
        return remoteInteractionActive ? remoteMoveFrom : nil
    }

    private var boardTapHandler: ((String) -> Void)? {
        if sandboxInteractionActive {
            return { sq in handleSandboxSquareTap(sq) }
        }
        if remoteInteractionActive, let snap = store.snapshot {
            return { sq in handleRemoteSquareTap(sq, snap: snap) }
        }
        return nil
    }

    private var resolvedBoardStyle: ChessBoardStyle {
        ChessBoardStyle(rawValue: boardStyleRaw) ?? .wooden
    }

    var body: some View {
        ChessBoardView(
            board: board,
            flipped: flipped,
            highlightFrom: store.boardHintHighlightFrom,
            highlightTo: store.boardHintHighlightTo,
            lastMoveFrom: lastMoveFrom,
            lastMoveTo: lastMoveTo,
            zoom: $internalZoom,
            remoteSelectionSquare: effectiveRemoteSelectionSquare,
            errorInvalidSquare: store.snapshot?.status.errorState?.invalidPos,
            errorOriginalSquare: store.snapshot?.status.errorState?.originalPos,
            invalidMoveFlashFrom: invalidFlashFrom,
            invalidMoveFlashTo: invalidFlashTo,
            onRemoteSquareTap: boardTapHandler,
            animatePieces: animatePieces,
            showCoordinates: showCoordinates,
            boardStyle: resolvedBoardStyle,
            sandboxFieldColors: viewState.sandboxHasValidBoard
        )
        .onAppear {
            internalZoom = zoom
        }
        .onChange(of: zoom) { _, newValue in
            internalZoom = newValue
        }
        .confirmationDialog("Vyber promoci figury", isPresented: $showPromotionPick, titleVisibility: .visible) {
            Button("Dáma") { confirmPromotion("q") }
            Button("Věž") { confirmPromotion("r") }
            Button("Střelec") { confirmPromotion("b") }
            Button("Jezdec") { confirmPromotion("n") }
            Button("Zrušit", role: .cancel) {
                promotionPair = nil
                remoteMoveFrom = nil
            }
        }
    }

    private func handleSandboxSquareTap(_ sq: String) {
        let result = viewState.sandboxProcessTap(square: sq)
        switch result {
        case .none, .deselectedSquare:
            break
        case .selectionChanged:
            #if os(iOS)
            HapticSettings.lightImpactIfEnabled()
            #endif
            SoundManager.shared.playPieceSelectSound()
        case .movedLocally:
            #if os(iOS)
            HapticSettings.mediumImpactIfEnabled()
            #endif
            SoundManager.shared.playMoveSound()
        case .needsPromotion(let from, let to):
            promotionPair = (from, to)
            showPromotionPick = true
        }
    }

    private func handleRemoteSquareTap(_ sq: String, snap: GameSnapshot) {
        let snapBoard = snap.board
        if let from = remoteMoveFrom {
            if from == sq {
                remoteMoveFrom = nil
                #if os(iOS)
                HapticSettings.lightImpactIfEnabled()
                #endif
                return
            }
            if RemoteChessMoveLegality.moveNeedsPromotionPrompt(board: snapBoard, from: from, to: sq) {
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
                        onInvalidRemoteMove?(from, sq)
                        let msg = (store.lastError?.trimmingCharacters(in: .whitespacesAndNewlines)).flatMap { $0.isEmpty ? nil : $0 }
                            ?? "Deska tah nepřijala. Zkontroluj pravidla, zámek z webu nebo spojení."
                        onRemoteMoveRequestFailed?(msg)
                        #if os(iOS)
                        HapticSettings.notificationErrorIfEnabled()
                        #endif
                    }
                }
            }
        } else {
            if let ch = RemoteChessMoveLegality.pieceChar(board: snapBoard, square: sq),
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
        if viewState.sandboxMode {
            viewState.sandboxCompletePromotion(from: p.from, to: p.to, letter: letter)
            promotionPair = nil
            remoteMoveFrom = nil
            showPromotionPick = false
            #if os(iOS)
            HapticSettings.mediumImpactIfEnabled()
            #endif
            SoundManager.shared.playMoveSound()
            return
        }
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
            showPromotionPick = false
            if ok {
                #if os(iOS)
                HapticSettings.mediumImpactIfEnabled()
                #endif
                SoundManager.shared.playMoveSound()
            } else {
                onInvalidRemoteMove?(p.from, p.to)
                let msg = (store.lastError?.trimmingCharacters(in: .whitespacesAndNewlines)).flatMap { $0.isEmpty ? nil : $0 }
                    ?? "Promoce nebo tah nebyl přijat."
                onRemoteMoveRequestFailed?(msg)
                #if os(iOS)
                HapticSettings.notificationErrorIfEnabled()
                #endif
            }
        }
    }

}

// MARK: - Simplified init for Game Views

extension ChessBoardView {
    /// Zjednodušený init pro novou modulární architekturu (placeholder).
    init(
        flipped: Bool = false,
        showCoordinates: Bool = true,
        invalidFlashFrom: String? = nil,
        invalidFlashTo: String? = nil
    ) {
        self.init(
            board: Array(repeating: Array(repeating: "", count: 8), count: 8),
            flipped: flipped,
            zoom: .constant(1.0)
        )
    }
}
