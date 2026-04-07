//
//  ChessGemmaInference.swift
//  CZECHMATE — Gemma 2B přes MediaPipe LLM Inference API (FlatBuffer .bin).
//
//  Integrace: v adresáři CZECHMATE spusť `pod install` a otevři CZECHMATE.xcworkspace.
//  Oficiální balíček je CocoaPods (`MediaPipeTasksGenAI`); Swift Package Manager Google pro GenAI na iOS neuvádí.

import Foundation

enum ChessCoachInferenceError: Error, LocalizedError {
    case modelFileMissing
    case loadFailed
    case notLoaded
    case sessionFailed(Error)
    case mediaPipeNotLinked

    var errorDescription: String? {
        switch self {
        case .modelFileMissing: return "Soubor modelu neexistuje na disku."
        case .loadFailed: return "Model se nepodařilo načíst (formát, RAM, …)."
        case .notLoaded: return "Model není načten."
        case .sessionFailed: return "Generování selhalo."
        case .mediaPipeNotLinked:
            return "MediaPipe není v buildu — spusť `pod install` a otevři .xcworkspace."
        }
    }
}

#if canImport(MediaPipeTasksGenAI)
import MediaPipeTasksGenAI
import MediaPipeTasksGenAIC

/// Drží jednu instanci `LlmInference` — načte se v učícím módu; uvolní se `unload()`.
final class ChessGemmaInference: @unchecked Sendable {
    private let lock = NSLock()
    private var inference: LlmInference?

    var isLoaded: Bool {
        lock.lock()
        defer { lock.unlock() }
        return inference != nil
    }

    func loadModel(at url: URL) throws {
        unload()
        guard FileManager.default.fileExists(atPath: url.path) else {
            throw ChessCoachInferenceError.modelFileMissing
        }
        let options = LlmInference.Options(modelPath: url.path)
        // MediaPipe: součet vstup + výstup; krátká odpověď (~128 tok.) vyžaduje rezervu pro předprompt.
        options.maxTokens = 512
        do {
            let llm = try LlmInference(options: options)
            lock.lock()
            inference = llm
            lock.unlock()
        } catch {
            throw ChessCoachInferenceError.loadFailed
        }
    }

    func unload() {
        lock.lock()
        inference = nil
        lock.unlock()
    }

    func streamResponse(
        userPrompt: String,
        onDelta: @escaping @Sendable (String) -> Void
    ) async throws -> String {
        lock.lock()
        guard let inf = inference else {
            lock.unlock()
            throw ChessCoachInferenceError.notLoaded
        }
        lock.unlock()

        let sessionOpts = LlmInference.Session.Options()
        sessionOpts.topk = 40
        sessionOpts.temperature = 0.7
        sessionOpts.topp = 0.9

        let session: LlmInference.Session
        do {
            session = try LlmInference.Session(llmInference: inf, options: sessionOpts)
        } catch {
            throw ChessCoachInferenceError.sessionFailed(error)
        }
        do {
            try session.addQueryChunk(inputText: userPrompt)
        } catch {
            throw ChessCoachInferenceError.sessionFailed(error)
        }

        let stream = session.generateResponseAsync()
        var accumulated = ""
        do {
            for try await partial in stream {
                accumulated += partial
                onDelta(partial)
            }
        } catch {
            throw ChessCoachInferenceError.sessionFailed(error)
        }
        return accumulated.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

#else

/// Bez CocoaPods / MediaPipe — sestavení projektu bez `pod install`; inference vždy nedostupná (záložní mock v manageru).
final class ChessGemmaInference: @unchecked Sendable {
    var isLoaded: Bool { false }

    func loadModel(at url: URL) throws {
        guard FileManager.default.fileExists(atPath: url.path) else {
            throw ChessCoachInferenceError.modelFileMissing
        }
        throw ChessCoachInferenceError.mediaPipeNotLinked
    }

    func unload() {}

    func streamResponse(
        userPrompt: String,
        onDelta: @escaping @Sendable (String) -> Void
    ) async throws -> String {
        _ = userPrompt
        _ = onDelta
        throw ChessCoachInferenceError.mediaPipeNotLinked
    }
}

#endif
