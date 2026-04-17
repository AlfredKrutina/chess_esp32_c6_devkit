//
//  ChessCoachManager.swift
//  CZECHMATE — Stockfish + lokální Gemma 2B (MediaPipe LLM Inference). „Mock“ jen záložní text trenéra, když chybí model — ne stažení.
//

import Combine
import Foundation
import SwiftUI

@MainActor
final class ChessCoachManager: ObservableObject {
    /// Streamovaný text pro bublinu (`@Published` pro SwiftUI).
    @Published var advice: String = ""
    @Published private(set) var isThinking = false
    /// Živý výstup z Gemmy — v `CoachBubbleView` vypne simulovaný typewriter.
    @Published private(set) var isStreamingLLM = false
    @Published private(set) var lastGeneratedAdvice: String?
    /// `true` po úspěšném `LlmInference` load z `.bin` (MediaPipe).
    @Published private(set) var isInferenceReady = false

    /// Historie chatu „Gemma“ na záložce Hra (uživatelské zprávy + odpovědi modelu).
    @Published private(set) var coachChatLines: [CoachChatLine] = []
    @Published var coachChatIsSending = false

    weak var learningMode: LearningModeManager?

    private let gemma = ChessGemmaInference()
    private var learningModeActive = false

    /// Poslední chyba z `loadModel` (MediaPipe / chybějící soubor) — pro srozumitelnější záložní text v bublině.
    private var lastLoadModelFailureDescription: String?

    /// Při vypnutí učícího módu uvolní RAM (model z paměti).
    func syncLearningModeAndModel(
        learningModeActive: Bool,
        modelDownload: ModelDownloadManager
    ) {
        self.learningModeActive = learningModeActive
        AppDebugLog.coachTrace(
            "ChessCoachManager sync learningMode=\(learningModeActive) hasUsableModel=\(modelDownload.hasMediaPipeModelOnDisk) mediapipeInBuild=\(CoachMediaPipeBuildAvailability.isMediaPipeLinked) gemmaLoaded=\(gemma.isLoaded) inferenceReady=\(isInferenceReady)"
        )
        if !learningModeActive {
            advice = ""
            isStreamingLLM = false
            isInferenceReady = false
            coachChatLines = []
            coachChatIsSending = false
            gemma.unload()
            AppDebugLog.coachTrace("ChessCoachManager unload (learning mode off)")
            return
        }
        guard modelDownload.hasMediaPipeModelOnDisk,
              let url = modelDownload.resolvedModelFileURL else {
            isInferenceReady = false
            gemma.unload()
            AppDebugLog.coachTrace("ChessCoachManager sync skip load — no usable model file on disk")
            return
        }
        AppDebugLog.coachTrace("ChessCoachManager sync scheduling loadModel \(url.lastPathComponent)")
        Task { await loadModel(from: url) }
    }

    func loadModel(from url: URL) async {
        guard learningModeActive else {
            AppDebugLog.coachTrace("ChessCoachManager loadModel aborted — learning mode off")
            return
        }
        if gemma.isLoaded {
            isInferenceReady = true
            AppDebugLog.coachTrace("ChessCoachManager loadModel skipped — already loaded")
            return
        }
        isInferenceReady = false
        AppDebugLog.coachTrace("ChessCoachManager loadModel begin \(url.lastPathComponent)")
        do {
            try gemma.loadModel(at: url)
            isInferenceReady = true
            lastLoadModelFailureDescription = nil
            AppDebugLog.coachTrace("ChessCoachManager loadModel OK \(url.lastPathComponent)")
        } catch {
            gemma.unload()
            isInferenceReady = false
            lastLoadModelFailureDescription = error.localizedDescription
            AppDebugLog.coachTrace("ChessCoachManager loadModel FAILED — \(error.localizedDescription)")
        }
    }

    func unloadModel() {
        advice = ""
        isStreamingLLM = false
        isInferenceReady = false
        coachChatLines = []
        coachChatIsSending = false
        gemma.unload()
    }

    func clearCoachChat() {
        coachChatLines = []
        coachChatIsSending = false
    }

    /// Uživatelská zpráva → stream odpovědi z Gemmy (učící mód + stažený model).
    func sendChessChatMessage(
        _ raw: String,
        store: BoardConnectionStore,
        modelDownload: ModelDownloadManager,
        userLevel: Int
    ) async {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        AppDebugLog.coachTrace(
            "ChessCoachManager chat send chars=\(trimmed.count) learningMode=\(learningModeActive) hasModel=\(modelDownload.hasMediaPipeModelOnDisk) gemmaLoaded=\(gemma.isLoaded)"
        )
        if !learningModeActive {
            coachChatLines.append(CoachChatLine(id: UUID(), isUser: true, text: trimmed, streaming: false))
            beginAssistantPlaceholder(
                "Zapni Učící mód na kartě Hra (ikona ovládání), stáhni model trenéra a zkus chat znovu."
            )
            AppDebugLog.coachTrace("ChessCoachManager chat blocked — learning mode off")
            return
        }

        coachChatIsSending = true
        coachChatLines.append(CoachChatLine(id: UUID(), isUser: true, text: trimmed, streaming: false))
        let assistantId = UUID()
        coachChatLines.append(CoachChatLine(id: assistantId, isUser: false, text: "", streaming: true))

        let fen: String? = {
            guard let snap = store.snapshot else { return nil }
            let n = snap.history.moves.count
            return FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, halfmoveCount: n)
        }()

        let prompt = buildMediaPipeGemmaChessChatPrompt(
            userMessage: trimmed,
            fen: fen,
            userLevel: userLevel
        )

        if modelDownload.hasMediaPipeModelOnDisk,
           let url = modelDownload.resolvedModelFileURL {
            if !gemma.isLoaded {
                await loadModel(from: url)
            }
        }

        if gemma.isLoaded {
            isStreamingLLM = true
            AppDebugLog.coachTrace("ChessCoachManager chat stream start promptChars=\(prompt.count)")
            let bridge = CoachChatDeltaBridge(manager: self, assistantId: assistantId)
            do {
                _ = try await Task.detached(priority: .userInitiated) { [gemma, prompt, bridge] in
                    try await gemma.streamResponse(userPrompt: prompt) { delta in
                        bridge.append(delta)
                    }
                }.value
                markAssistantStreamingComplete(id: assistantId)
                isStreamingLLM = false
                coachChatIsSending = false
                learningMode?.registerCoachAdviceStreamCompleted()
                AppDebugLog.coachTrace("ChessCoachManager chat stream finished OK")
            } catch {
                let partial = coachChatLineText(id: assistantId)
                let tail = "Model teď nedokončil odpověď: \(error.localizedDescription). Zkus otázku zkrátit."
                finalizeAssistantLine(id: assistantId, text: partial.isEmpty ? tail : "\(partial)\n\n\(tail)")
                isStreamingLLM = false
                coachChatIsSending = false
                AppDebugLog.coachTrace("ChessCoachManager chat stream FAILED — \(error.localizedDescription)")
            }
            return
        }

        let hint: String
        if let err = lastLoadModelFailureDescription {
            hint =
                "Lokální Gemma není k dispozici (\(err)). V nastavení trenéra stáhni model nebo ověř build s MediaPipe (xcworkspace + CocoaPods)."
        } else {
            hint =
                "Lokální model trenéra není načtený. V Učícím módu dokonči stažení Gemmy, pak můžeš psát do chatu."
        }
        finalizeAssistantLine(id: assistantId, text: hint)
        coachChatIsSending = false
        AppDebugLog.coachTrace("ChessCoachManager chat fallback (gemma not loaded) hint=\(hint.prefix(120))…")
    }

    private func beginAssistantPlaceholder(_ text: String) {
        coachChatLines.append(CoachChatLine(id: UUID(), isUser: false, text: text, streaming: false))
    }

    private func coachChatLineText(id: UUID) -> String {
        coachChatLines.first { $0.id == id }?.text ?? ""
    }

    private func finalizeAssistantLine(id: UUID, text: String) {
        guard let idx = coachChatLines.firstIndex(where: { $0.id == id }) else { return }
        var line = coachChatLines[idx]
        line.text = text
        line.streaming = false
        coachChatLines[idx] = line
    }

    private func markAssistantStreamingComplete(id: UUID) {
        guard let idx = coachChatLines.firstIndex(where: { $0.id == id }) else { return }
        var line = coachChatLines[idx]
        line.streaming = false
        coachChatLines[idx] = line
    }

    fileprivate func appendCoachChatDelta(assistantId: UUID, delta: String) {
        guard let idx = coachChatLines.firstIndex(where: { $0.id == assistantId }) else { return }
        var line = coachChatLines[idx]
        line.text += delta
        coachChatLines[idx] = line
    }

    // MARK: - Plán pozice (stream)

    func streamPositionPlanSummary(
        store: BoardConnectionStore,
        modelDownload: ModelDownloadManager,
        userLevel: Int
    ) async {
        advice = ""
        lastGeneratedAdvice = nil
        isThinking = true
        isStreamingLLM = false
        lastLoadModelFailureDescription = nil
        AppDebugLog.coachTrace(
            "ChessCoachManager position plan start learningMode=\(learningModeActive) hasModel=\(modelDownload.hasMediaPipeModelOnDisk)"
        )

        let fen: String
        let bestUci: String
        let evalWhite: Double
        do {
            let (f, best) = try await store.fetchCoachStockfishAnalysis()
            fen = f
            bestUci = "\(best.from)\(best.to)"
            evalWhite =
                best.evalPawns.map { MoveEvaluation.evalToWhitePerspective(fen: f, evalRaw: $0) } ?? 0
        } catch {
            advice = "Nepodařilo se načíst Stockfish: \(NetworkErrorFormatter.userMessage(for: error))"
            lastGeneratedAdvice = advice
            isThinking = false
            AppDebugLog.coachTrace("ChessCoachManager position plan Stockfish FAILED — \(error.localizedDescription)")
            return
        }

        AppDebugLog.coachTrace("ChessCoachManager position plan Stockfish OK fenChars=\(fen.count) bestUci=\(bestUci)")

        let prompt = buildMediaPipeGemmaCoachPrompt(
            fen: fen,
            evalWhitePawns: evalWhite,
            bestMoveUci: bestUci,
            userLevel: userLevel,
            mode: .wholePositionPlan
        )

        if learningModeActive,
           modelDownload.hasMediaPipeModelOnDisk,
           let url = modelDownload.resolvedModelFileURL {
            if !gemma.isLoaded {
                advice = "Trenér se připravuje…"
                isThinking = true
                AppDebugLog.coachTrace("ChessCoachManager position plan loadModel before stream")
                await loadModel(from: url)
            }
        }

        if gemma.isLoaded {
            isStreamingLLM = true
            isThinking = false
            AppDebugLog.coachTrace("ChessCoachManager position plan LLM stream start promptChars=\(prompt.count)")
            let bridge = CoachAdviceDeltaBridge(manager: self)
            do {
                let final: String = try await Task.detached(priority: .userInitiated) { [gemma, prompt, bridge] in
                    try await gemma.streamResponse(userPrompt: prompt) { delta in
                        bridge.append(delta)
                    }
                }.value
                lastGeneratedAdvice = final
                isStreamingLLM = false
                learningMode?.registerCoachAdviceStreamCompleted()
                AppDebugLog.coachTrace("ChessCoachManager position plan LLM stream OK outChars=\(final.count)")
            } catch {
                isStreamingLLM = false
                AppDebugLog.coachTrace("ChessCoachManager position plan LLM stream FAILED — \(error.localizedDescription)")
                await streamFallbackMock(
                    evalWhite: evalWhite,
                    bestUci: bestUci,
                    variant: .inferenceFailed(error.localizedDescription)
                )
                lastGeneratedAdvice = advice
                learningMode?.registerCoachAdviceStreamCompleted()
            }
            return
        }

        let loadHint: CoachFallbackVariant
        if let err = lastLoadModelFailureDescription {
            loadHint = .loadModelFailed(err)
        } else {
            loadHint = .standard
        }
        let fallbackTag: String = {
            switch loadHint {
            case .standard: return "standard"
            case .loadModelFailed(let s): return "loadModelFailed(\(String(s.prefix(100))))"
            case .inferenceFailed(let s): return "inferenceFailed(\(String(s.prefix(100))))"
            }
        }()
        AppDebugLog.coachTrace("ChessCoachManager position plan mock fallback \(fallbackTag)")
        await streamFallbackMock(evalWhite: evalWhite, bestUci: bestUci, variant: loadHint)
        lastGeneratedAdvice = advice
        isThinking = false
        learningMode?.registerCoachAdviceStreamCompleted()
    }

    private enum CoachFallbackVariant {
        case standard
        case loadModelFailed(String)
        case inferenceFailed(String)
    }

    private func streamFallbackMock(
        evalWhite: Double,
        bestUci: String,
        variant: CoachFallbackVariant = .standard
    ) async {
        isThinking = false
        isStreamingLLM = true
        let evalStr = String(format: "%.2f", evalWhite)
        let full: String
        switch variant {
        case .standard:
            full =
                "Jsem AI Trenér z Czechmate. Teď běží jen záložní režim bez lokálního modelu. Podle Stockfish je eval z pohledu bílého \(evalStr) v pěšácích a silný pokus je \(bestUci). Zkus znovu po načtení modelu trenéra v nastavení učícího módu."
        case .loadModelFailed(let reason):
            full =
                "Jsem AI Trenér z Czechmate. Lokální model se nepodařilo načíst: \(reason). Podle Stockfish je eval z pohledu bílého \(evalStr) a silný pokus je \(bestUci). Ověř, že build používá CZECHMATE.xcworkspace s CocoaPods (MediaPipe), že soubor je správný formát pro MediaPipe (ne GGUF) a že má telefon dostatek RAM."
        case .inferenceFailed(let reason):
            full =
                "Jsem AI Trenér z Czechmate. Model byl načten, ale generování textu selhalo: \(reason). Podle Stockfish je eval z pohledu bílého \(evalStr) a silný pokus je \(bestUci). Zkus akci zopakovat; při opakovaném pádu zkraťte pozici nebo restartujte aplikaci."
        }
        advice = ""
        let parts = full.split(separator: " ", omittingEmptySubsequences: false).map { String($0) + " " }
        for p in parts {
            try? await Task.sleep(nanoseconds: 28_000_000)
            advice += p
        }
        isStreamingLLM = false
    }
}

// Pomocník pro doručení tokenů na MainActor z `Task.detached`.
private final class CoachAdviceDeltaBridge: @unchecked Sendable {
    weak var manager: ChessCoachManager?

    init(manager: ChessCoachManager) {
        self.manager = manager
    }

    func append(_ delta: String) {
        Task { @MainActor [weak manager] in
            manager?.advice += delta
        }
    }
}

private final class CoachChatDeltaBridge: @unchecked Sendable {
    weak var manager: ChessCoachManager?
    let assistantId: UUID

    init(manager: ChessCoachManager, assistantId: UUID) {
        self.manager = manager
        self.assistantId = assistantId
    }

    func append(_ delta: String) {
        Task { @MainActor [weak manager] in
            manager?.appendCoachChatDelta(assistantId: assistantId, delta: delta)
        }
    }
}
