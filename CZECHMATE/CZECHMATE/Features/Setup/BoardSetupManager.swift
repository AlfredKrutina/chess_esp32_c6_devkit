//
//  BoardSetupManager.swift
//  Porovnání cílového FEN ↔ fyzická deska, LED nápověda, auto-krok (parita s webovým tutoriálem).
//

import Foundation
import Observation
#if os(iOS)
import UIKit
#endif

@MainActor
@Observable
final class BoardSetupManager {
    struct Step: Identifiable, Equatable {
        let id: UUID
        let square: String
        let pieceChar: Character
        let labelLine: String

        init(square: String, piece: Character, labelLine: String) {
            id = UUID()
            self.square = square.lowercased()
            pieceChar = piece
            self.labelLine = labelLine
        }
    }

    enum Mode: Equatable {
        /// Firmware prázdná logická deska + `matrix_occupied` v `/api/status`.
        case standardStartingPosition
        /// Puzzle / vlastní FEN: prázdná logická deska (`setup_tutorial` start před průvodcem), krok podle `matrix_occupied`;
        /// po posledním poli volitelně `new_game`+FEN na MCU (`loadFenWhenDone`).
        case fenGuidedPlacement(loadFenWhenDone: Bool)
    }

    enum Phase: Equatable {
        case idle
        case running
        case validatingFinish
        case completed
        case failed(String)
    }

    private static let ledRefreshSeconds: UInt64 = 600_000_000
    private static let statusPollSeconds: UInt64 = 400_000_000
    private static let stableTicksRequired = 2

    let mode: Mode
    let targetFEN: String
    private(set) var steps: [Step] = []
    private(set) var currentIndex: Int = 0
    private(set) var phase: Phase = .idle

    /// Poslední chyba z API (zobrazení v overlay).
    private(set) var lastAPIError: String?

    private weak var store: BoardConnectionStore?
    private var ledLoopTask: Task<Void, Never>?
    private var statusPollTask: Task<Void, Never>?
    private var occStable = 0
    private var pieceStable = 0
    private var advanceInFlight = false

    var currentStep: Step? {
        guard phase == .running || phase == .idle else { return nil }
        guard currentIndex < steps.count else { return nil }
        return steps[currentIndex]
    }

    var totalSteps: Int { steps.count }

    /// 0…1 — dokončené kroky / celkem (aktuální krok ještě nepočítá jako hotový).
    var progressFraction: Double {
        guard !steps.isEmpty else { return 0 }
        return Double(currentIndex) / Double(steps.count)
    }

    var headlineInstruction: String {
        guard let s = currentStep else {
            return ""
        }
        return "Polož \(ChessSetupPieceGlyph.symbol(for: s.pieceChar)) na \(s.square.uppercased())"
    }

    var progressLabel: String {
        let rem = max(0, steps.count - currentIndex)
        if steps.isEmpty { return "" }
        return "Zbývá postavit: \(rem) \(rem == 1 ? "figurka" : (rem < 5 ? "figurky" : "figurek"))"
    }

    var usesMatrixSensors: Bool {
        switch mode {
        case .standardStartingPosition, .fenGuidedPlacement:
            return true
        }
    }

    init(mode: Mode, targetFEN: String, store: BoardConnectionStore) {
        self.mode = mode
        self.targetFEN = targetFEN
        self.store = store
        let raw = BoardSetupFENSteps.build(from: targetFEN)
        steps = raw.map { Step(square: $0.square, piece: $0.piece, labelLine: $0.label) }
    }

    func start() async {
        phase = .running
        currentIndex = 0
        occStable = 0
        pieceStable = 0
        lastAPIError = nil
        AppDebugLog.staging("BoardSetupManager start mode=\(mode) steps=\(steps.count)")
        await applyHighlightForCurrentStep()
        startLedRefreshLoop()
        if usesMatrixSensors {
            startStatusPolling()
        }
    }

    func consumeSnapshot(_ snap: GameSnapshot?) {
        guard phase == .running, let step = currentStep, let snap else { return }
        if pieceMatches(expected: step.pieceChar, board: snap.board, square: step.square) {
            pieceStable += 1
        } else {
            pieceStable = 0
        }
        scheduleAdvanceIfStable()
    }

    func cancel() async {
        stopLoops()
        store?.syncSetupWizardTargetSquare(nil)
        occStable = 0
        pieceStable = 0
        switch mode {
        case .standardStartingPosition, .fenGuidedPlacement:
            try? await store?.postSetupTutorialAPI(.cancel)
        }
        try? await store?.postSetupHintClearOnly()
        phase = .idle
        store?.clearBoardSetupManagerIfMatches(self)
    }

    /// Přeskočení kroku bez detekce na desce (poškozené pole, nefunkční senzor, LED nejde).
    func skipCurrentStep() async {
        guard phase == .running, currentStep != nil else { return }
        guard !advanceInFlight else { return }
        advanceInFlight = true
        defer { advanceInFlight = false }
        #if os(iOS)
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
        #endif
        occStable = 0
        pieceStable = 0
        try? await store?.postSetupHintClearOnly()
        currentIndex += 1
        if currentIndex >= steps.count {
            stopLoops()
            store?.syncSetupWizardTargetSquare(nil)
            await finalizeAfterAllPiecesPlaced()
            return
        }
        await applyHighlightForCurrentStep()
    }

    /// Dokončení po úspěchu (např. přepnutí tabu) — volitelně z UI.
    func acknowledgeCompletion() {
        if phase == .completed {
            store?.clearBoardSetupManagerIfMatches(self)
        }
    }

    private func startLedRefreshLoop() {
        ledLoopTask?.cancel()
        ledLoopTask = Task { [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: Self.ledRefreshSeconds)
                guard !Task.isCancelled else { break }
                await self.refreshLedIfRunning()
            }
        }
    }

    private func startStatusPolling() {
        statusPollTask?.cancel()
        statusPollTask = Task { [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: Self.statusPollSeconds)
                guard !Task.isCancelled else { break }
                await self.pollMatrixIfRunning()
            }
        }
    }

    private func stopLoops() {
        ledLoopTask?.cancel()
        ledLoopTask = nil
        statusPollTask?.cancel()
        statusPollTask = nil
    }

    private func refreshLedIfRunning() async {
        guard phase == .running, let sq = currentStep?.square else { return }
        try? await store?.postSetupHintDestinationOnly(to: sq)
    }

    private func pollMatrixIfRunning() async {
        guard phase == .running, let step = currentStep, usesMatrixSensors else { return }
        guard let idx = BoardSetupFENSteps.matrixIndex(forSquare: step.square) else { return }
        guard let row = await store?.matrixOccupiedForBoardSetupWizard() else {
            occStable = 0
            return
        }
        guard row.indices.contains(idx), row[idx] == 1 else {
            occStable = 0
            return
        }
        occStable += 1
        scheduleAdvanceIfStable()
    }

    private func scheduleAdvanceIfStable() {
        guard phase == .running, !advanceInFlight else { return }
        let occOk = usesMatrixSensors && occStable >= Self.stableTicksRequired
        let pieceOk = pieceStable >= Self.stableTicksRequired
        guard occOk || pieceOk else { return }
        advanceInFlight = true
        Task {
            defer { advanceInFlight = false }
            await advanceAfterConfirmedPlacement()
        }
    }

    private func advanceAfterConfirmedPlacement() async {
        guard phase == .running else { return }
        #if os(iOS)
        let gen = UINotificationFeedbackGenerator()
        gen.notificationOccurred(.success)
        #endif
        occStable = 0
        pieceStable = 0
        try? await store?.postSetupHintClearOnly()
        currentIndex += 1
        if currentIndex >= steps.count {
            stopLoops()
            store?.syncSetupWizardTargetSquare(nil)
            await finalizeAfterAllPiecesPlaced()
            return
        }
        await applyHighlightForCurrentStep()
    }

    private func applyHighlightForCurrentStep() async {
        guard let sq = currentStep?.square else {
            store?.syncSetupWizardTargetSquare(nil)
            return
        }
        store?.syncSetupWizardTargetSquare(sq)
        try? await store?.postSetupHintDestinationOnly(to: sq)
    }

    private func finalizeAfterAllPiecesPlaced() async {
        store?.syncSetupWizardTargetSquare(nil)
        switch mode {
        case .standardStartingPosition:
            phase = .validatingFinish
            do {
                try await postFinishSetupTutorialWithRetries()
                lastAPIError = nil
                phase = .completed
            } catch {
                let msg = NetworkErrorFormatter.userMessage(for: error)
                lastAPIError = msg
                phase = .failed(msg)
            }
        case .fenGuidedPlacement(let loadFenWhenDone):
            phase = .validatingFinish
            try? await store?.postSetupTutorialAPI(.cancel)
            if loadFenWhenDone {
                guard let store else {
                    let msg = "Chybí spojení s deskou."
                    lastAPIError = msg
                    phase = .failed(msg)
                    return
                }
                let ok = await store.applyPuzzleFenOnBoardAfterGuidedSetup(targetFEN)
                if !ok {
                    let msg = store.lastError ?? "Nepodařilo se nahrát pozici na desku."
                    lastAPIError = msg
                    phase = .failed(msg)
                    return
                }
            }
            lastAPIError = nil
            phase = .completed
        }
    }

    /// Po poslední figurce počkáme na ustálení reedů; při HTTP 409 z ESP zopakujeme (firmware dělá majority vote).
    private func postFinishSetupTutorialWithRetries() async throws {
        guard let store else {
            throw BoardSetupStoreError.boardConnectionRequired
        }
        let settleNs: UInt64 = 300_000_000
        let retryDelayNs: UInt64 = 400_000_000
        let maxAttempts = 5
        try await Task.sleep(nanoseconds: settleNs)
        for attempt in 1 ... maxAttempts {
            do {
                try await store.postSetupTutorialAPI(.finish)
                return
            } catch {
                if let ce = error as? ChessboardAPIError,
                   case .httpStatus(let code, _) = ce,
                   code == 409,
                   attempt < maxAttempts {
                    AppDebugLog.staging(
                        "POST setup_tutorial finish 409 (matrix), retry \(attempt)/\(maxAttempts)"
                    )
                    try await Task.sleep(nanoseconds: retryDelayNs)
                    continue
                }
                throw error
            }
        }
    }

    private func pieceMatches(expected: Character, board: [[String]], square: String) -> Bool {
        guard let idx = FirmwareSquareNotation.indices(from: square),
              idx.row >= 0, idx.row < 8, idx.col >= 0, idx.col < 8
        else { return false }
        let cell = normalizeCell(board[idx.row][idx.col])
        return cell == String(expected)
    }

    private func normalizeCell(_ s: String) -> String {
        let t = s.trimmingCharacters(in: .whitespaces)
        if t.isEmpty || t == "." { return "" }
        return t
    }
}
