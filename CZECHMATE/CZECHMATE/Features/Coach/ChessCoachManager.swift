//
//  ChessCoachManager.swift
//  CZECHMATE — Stockfish + lokální Gemma 2B (MediaPipe LLM Inference); záložní stream mock.
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
        if !learningModeActive {
            advice = ""
            isStreamingLLM = false
            isInferenceReady = false
            gemma.unload()
            #if DEBUG
            AppDebugLog.staging("ChessCoachManager: unload MediaPipe Gemma (learning mode off)")
            #endif
            return
        }
        guard modelDownload.hasMediaPipeModelOnDisk,
              let url = modelDownload.resolvedModelFileURL else {
            isInferenceReady = false
            gemma.unload()
            return
        }
        Task { await loadModel(from: url) }
    }

    func loadModel(from url: URL) async {
        guard learningModeActive else { return }
        if gemma.isLoaded {
            isInferenceReady = true
            return
        }
        isInferenceReady = false
        do {
            try gemma.loadModel(at: url)
            isInferenceReady = true
            lastLoadModelFailureDescription = nil
            #if DEBUG
            AppDebugLog.staging("ChessCoachManager: MediaPipe Gemma loaded \(url.lastPathComponent)")
            #endif
        } catch {
            gemma.unload()
            isInferenceReady = false
            lastLoadModelFailureDescription = error.localizedDescription
            #if DEBUG
            AppDebugLog.staging("ChessCoachManager: MediaPipe load failed — \(error.localizedDescription)")
            #endif
        }
    }

    func unloadModel() {
        advice = ""
        isStreamingLLM = false
        isInferenceReady = false
        gemma.unload()
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
            return
        }

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
                await loadModel(from: url)
            }
        }

        if gemma.isLoaded {
            isStreamingLLM = true
            isThinking = false
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
            } catch {
                isStreamingLLM = false
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
