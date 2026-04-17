//
//  GameViewState.swift
//  CZECHMATE — Centralized game UI state management
//
//  Separates UI state from game logic for better maintainability
//

import Foundation
import Observation
import SwiftUI

// MARK: - Sandbox (lokální tahy jen v aplikaci)

enum SandboxSquareTapResult: Equatable {
    case none
    case deselectedSquare
    case selectionChanged
    case movedLocally
    case needsPromotion(from: String, to: String)
}

/// Jednotné texty pro přepínač tahů z aplikace na desku (HIG — stejné znění ve všech layoutech).
enum GameRemoteMovesCopy {
    static let toggleTitle = "Tahy z aplikace"
    /// Nápověda pod přepínačem / VoiceOver hint.
    static let usageHint =
        "Klepni na šachovnici nejdřív na pole s figurou, pak na cílové pole. Funguje jen když deska a spojení příkaz přijmou."
}

/// Layout modes for game display
enum GameLayoutMode: String, CaseIterable {
    case boardOnly = "boardOnly"
    case standard = "standard"
    case wide = "wide"
    
    var icon: String {
        switch self {
        case .boardOnly: return "rectangle.dashed"
        case .standard: return "rectangle.fill"
        case .wide: return "arrow.left.and.right"
        }
    }
    
    var label: String {
        switch self {
        case .boardOnly: return "Jen deska"
        case .standard: return "Standard"
        case .wide: return "Široký"
        }
    }
}

/// Display options for game view (synchronizované s @AppStorage `czechmate.*` v `GameContainerView.onAppear` a `GameContainerPrefsSyncModifier`).
struct GameDisplayOptions: Equatable {
    var boardFlipped: Bool = false
    var showCoordinates: Bool = true
    var moveAnimations: Bool = true
    var remoteMoves: Bool = false
    
    mutating func resetToDefaults() {
        boardFlipped = false
        showCoordinates = true
        moveAnimations = true
        remoteMoves = false
    }
}

/// Game session configuration
struct GameSessionConfig {
    var learningMode: Bool = false
    var coachUserLevel: Int = 4
    var isTimerRunning: Bool = false
}

/// Centralized state management for Game views
@Observable
@MainActor
final class GameViewState {
    // MARK: - Layout State
    var layoutMode: GameLayoutMode = .standard
    var isControlPanelExpanded: Bool = false
    
    // MARK: - Display Options
    var displayOptions = GameDisplayOptions()
    
    // MARK: - Session Config
    var sessionConfig = GameSessionConfig()
    
    // MARK: - UI State
    var showConnection: Bool = false
    var showNewGameSetup: Bool = false
    var showGameControls: Bool = false
    var showCoachOnboarding: Bool = false
    var showPositionCoach: Bool = false
    var showCoachChat: Bool = false
    /// Souhrn partie po skončení (sheet ve stylu přehledu výsledku).
    var showEndgameReport: Bool = false
    
    // MARK: - Board Interaction State
    var boardZoom: CGFloat = 1.0
    var virtualPickupSquare: String = ""
    var virtualDropSquare: String = ""
    var invalidFlashFrom: String?
    var invalidFlashTo: String?
    /// Krátká zpráva pod deskou (např. selhání vzdáleného tahu) — sama zmizí po timeoutu.
    var transientBoardMessage: String?
    @ObservationIgnored private var transientBoardMessageTask: Task<Void, Never>?

    // MARK: Sandbox (parita s webovým „Zkusit tahy“)

    /// Lokální pozice na obrazovce; deska / MCU se nemění.
    var sandboxMode: Bool = false
    /// Řádek 0 = rank 8 — stejně jako `GameSnapshot.board`.
    var sandboxBoard: [[String]] = []
    var sandboxSelectedSquare: String?
    @ObservationIgnored private var sandboxUndoStack: [[[String]]] = []

    var sandboxHasValidBoard: Bool { sandboxMode && sandboxBoard.count == 8 }

    /// Počet lokálních tahů, které jde vrátit (až na pozici při zapnutí sandboxu).
    var sandboxUndoCount: Int { sandboxUndoStack.count }

    // MARK: - Computed Properties
    var isBoardOnlyMode: Bool { layoutMode == .boardOnly }
    
    var useWideLayout: Bool { layoutMode == .wide }
    
    // MARK: - Actions
    func cycleLayoutMode() {
        let allCases = GameLayoutMode.allCases
        if let currentIndex = allCases.firstIndex(of: layoutMode) {
            let nextIndex = (currentIndex + 1) % allCases.count
            layoutMode = allCases[nextIndex]
        }
    }
    
    func resetUIState() {
        isControlPanelExpanded = false
        virtualPickupSquare = ""
        virtualDropSquare = ""
        invalidFlashFrom = nil
        invalidFlashTo = nil
        clearTransientBoardMessage()
        exitSandboxMode()
    }

    /// Kopie aktuální pozice z parametrů (např. `snapshot.board`) — tahy jen v UI.
    func enterSandboxMode(board liveBoard: [[String]]) {
        guard liveBoard.count == 8 else { return }
        sandboxBoard = Self.copyBoard(liveBoard)
        sandboxMode = true
        sandboxUndoStack.removeAll()
        sandboxSelectedSquare = nil
        #if DEBUG
        AppDebugLog.staging("Sandbox: zapnuto (lokální tahy)")
        #endif
    }

    func exitSandboxMode() {
        sandboxMode = false
        sandboxBoard = []
        sandboxUndoStack.removeAll()
        sandboxSelectedSquare = nil
    }

    func exitSandboxAndRefresh(store: BoardConnectionStore) async {
        exitSandboxMode()
        await store.refreshSnapshotFromServer()
        #if DEBUG
        AppDebugLog.staging("Sandbox: vypnuto, obnoven snímek z desky")
        #endif
    }

    func sandboxUndoLastMove() {
        guard let prev = sandboxUndoStack.popLast() else { return }
        sandboxBoard = Self.copyBoard(prev)
        sandboxSelectedSquare = nil
    }

    /// Vrátí najednou pozici z okamžiku zapnutí sandboxu (bez limitu počtu kroků).
    func sandboxUndoToEntryPosition() {
        guard let entry = sandboxUndoStack.first else { return }
        sandboxBoard = Self.copyBoard(entry)
        sandboxUndoStack.removeAll()
        sandboxSelectedSquare = nil
    }

    /// Klepnutí na pole v sandboxu (vlastní pravidla: libovolná figurka, výměna výběru při vlastní barvě na cíli).
    func sandboxProcessTap(square: String) -> SandboxSquareTapResult {
        guard sandboxMode, sandboxBoard.count == 8 else { return .none }
        guard FirmwareSquareNotation.indices(from: square) != nil else { return .none }

        if let from = sandboxSelectedSquare {
            if from == square {
                sandboxSelectedSquare = nil
                return .deselectedSquare
            }
            if let movingCh = RemoteChessMoveLegality.pieceChar(board: sandboxBoard, square: from),
               let destCh = RemoteChessMoveLegality.pieceChar(board: sandboxBoard, square: square) {
                let movingWhite = movingCh.isUppercase && movingCh.isLetter
                let destWhite = destCh.isUppercase && destCh.isLetter
                if movingWhite == destWhite {
                    sandboxSelectedSquare = square
                    return .selectionChanged
                }
            }
            if RemoteChessMoveLegality.moveNeedsPromotionPrompt(board: sandboxBoard, from: from, to: square) {
                return .needsPromotion(from: from, to: square)
            }
            sandboxExecuteMove(from: from, to: square, promotionLetter: nil)
            return .movedLocally
        } else {
            if RemoteChessMoveLegality.pieceChar(board: sandboxBoard, square: square) != nil {
                sandboxSelectedSquare = square
                return .selectionChanged
            }
            return .none
        }
    }

    /// Dokončení promoce po dialogu (`q` / `r` / `b` / `n`).
    func sandboxCompletePromotion(from: String, to: String, letter: String) {
        let lower = letter.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard let first = lower.first, "qrnb".contains(first) else { return }
        sandboxExecuteMove(from: from, to: to, promotionLetter: String(first))
    }

    private func sandboxExecuteMove(from: String, to: String, promotionLetter: String?) {
        guard let fIdx = FirmwareSquareNotation.indices(from: from),
              let tIdx = FirmwareSquareNotation.indices(from: to) else { return }
        let movingCell = sandboxBoard[fIdx.row][fIdx.col].trimmingCharacters(in: .whitespaces)
        guard let movingCh = movingCell.first, movingCh != " " else { return }

        sandboxUndoStack.append(Self.copyBoard(sandboxBoard))

        var newPiece = String(movingCh)
        if let pl = promotionLetter, let plChar = pl.lowercased().first {
            let isWhite = movingCh.isUppercase && movingCh.isLetter
            newPiece = isWhite ? String(plChar.uppercased()) : String(plChar)
        }
        var b = Self.copyBoard(sandboxBoard)
        b[tIdx.row][tIdx.col] = newPiece
        b[fIdx.row][fIdx.col] = " "
        sandboxBoard = b
        sandboxSelectedSquare = nil
    }

    private static func copyBoard(_ board: [[String]]) -> [[String]] {
        board.map { row in row.map { String($0) } }
    }

    /// Zobrazí text pod deskou a po pár sekundách ho skryje (nahradí předchozí stejný typ hlášení).
    func showTransientBoardMessage(_ text: String, durationSeconds: UInt64 = 4) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        transientBoardMessageTask?.cancel()
        transientBoardMessage = trimmed
        transientBoardMessageTask = Task { @MainActor in
            try? await Task.sleep(nanoseconds: durationSeconds * 1_000_000_000)
            guard !Task.isCancelled else { return }
            transientBoardMessage = nil
            transientBoardMessageTask = nil
        }
    }

    func clearTransientBoardMessage() {
        transientBoardMessageTask?.cancel()
        transientBoardMessageTask = nil
        transientBoardMessage = nil
    }
    
    func showInvalidMove(from: String, to: String) {
        invalidFlashFrom = from
        invalidFlashTo = to
        
        // Clear after animation
        Task {
            try? await Task.sleep(for: .milliseconds(600))
            invalidFlashFrom = nil
            invalidFlashTo = nil
        }
    }
}
