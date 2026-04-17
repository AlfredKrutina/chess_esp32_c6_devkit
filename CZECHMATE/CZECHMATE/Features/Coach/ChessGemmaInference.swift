//
//  ChessGemmaInference.swift
//  CZECHMATE — Gemma 2B přes MediaPipe LLM Inference API (FlatBuffer .bin).
//
//  Integrace: v adresáři CZECHMATE spusť `pod install` a otevři CZECHMATE.xcworkspace.
//  Oficiální balíček je CocoaPods (`MediaPipeTasksGenAI`); Swift Package Manager Google pro GenAI na iOS neuvádí.

import Foundation

/// Rozliší build s CocoaPods (`pod install` + .xcworkspace) vs. čistý .xcodeproj bez MediaPipe.
enum CoachMediaPipeBuildAvailability {
    static var isMediaPipeLinked: Bool {
        #if canImport(MediaPipeTasksGenAI)
        true
        #else
        false
        #endif
    }
}

enum ChessCoachInferenceError: Error, LocalizedError {
    case modelFileMissing
    /// MediaPipe vyhodil chybu při `LlmInference` init — `details` je z knihovny (formát souboru, RAM, …).
    case loadFailed(details: String)
    case notLoaded
    case sessionFailed(Error)
    case mediaPipeNotLinked

    var errorDescription: String? {
        switch self {
        case .modelFileMissing: return "Soubor modelu neexistuje na disku."
        case .loadFailed(let details):
            return "Model se nepodařilo načíst: \(details)"
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

    private static func describeLlmInitFailure(_ error: Error) -> String {
        let ns = error as NSError
        var parts: [String] = [error.localizedDescription]
        if let reason = ns.userInfo[NSLocalizedFailureReasonErrorKey] as? String,
           !reason.isEmpty,
           reason != error.localizedDescription {
            parts.append(reason)
        }
        if let underlying = ns.userInfo[NSUnderlyingErrorKey] as? Error {
            parts.append((underlying as NSError).localizedDescription)
        }
        return parts.joined(separator: " — ")
    }

    var isLoaded: Bool {
        lock.lock()
        defer { lock.unlock() }
        return inference != nil
    }

    func loadModel(at url: URL) throws {
        unload()
        guard FileManager.default.fileExists(atPath: url.path) else {
            AppDebugLog.coachTrace("ChessGemmaInference loadModel abort — file missing \(url.lastPathComponent)")
            throw ChessCoachInferenceError.modelFileMissing
        }
        if let attrs = try? FileManager.default.attributesOfItem(atPath: url.path),
           let sz = attrs[.size] as? Int64 {
            AppDebugLog.coachTrace("ChessGemmaInference loadModel begin \(url.lastPathComponent) bytes=\(sz)")
        } else {
            AppDebugLog.coachTrace("ChessGemmaInference loadModel begin \(url.lastPathComponent)")
        }
        let options = LlmInference.Options(modelPath: url.path)
        // Výchozí model z aplikace je `gemma-2b-it-gpu-int4.bin` — musí běžet na GPU backendu (Metal).
        // CPU int4 váže XNNPACK graf, který na iOS často hlásí chybějící `XnnLlmResourceCalculator`.
        options.preferredBackend = .gpu
        AppDebugLog.coachTrace(
            "ChessGemmaInference LlmInference.Options ready path=\(url.lastPathComponent) backend=gpu sim=\(AppProcessContext.isSimulatorRuntime) xctestHost=\(AppProcessContext.isXCTestHostProcess)"
        )
        // MediaPipe: součet vstup + výstup; krátká odpověď (~128 tok.) vyžaduje rezervu pro předprompt.
        options.maxTokens = 512
        do {
            let llm = try LlmInference(options: options)
            lock.lock()
            inference = llm
            lock.unlock()
            AppDebugLog.coachTrace("ChessGemmaInference LlmInference created OK")
        } catch {
            let detail = Self.describeLlmInitFailure(error)
            AppDebugLog.coachTrace("ChessGemmaInference LlmInference init FAILED — \(detail)")
            throw ChessCoachInferenceError.loadFailed(details: detail)
        }
    }

    func unload() {
        lock.lock()
        let had = inference != nil
        inference = nil
        lock.unlock()
        if had {
            AppDebugLog.coachTrace("ChessGemmaInference unload (released LlmInference)")
        }
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
            AppDebugLog.coachTrace("ChessGemmaInference session create FAILED — \(error.localizedDescription)")
            throw ChessCoachInferenceError.sessionFailed(error)
        }
        do {
            try session.addQueryChunk(inputText: userPrompt)
        } catch {
            AppDebugLog.coachTrace("ChessGemmaInference addQueryChunk FAILED — \(error.localizedDescription)")
            throw ChessCoachInferenceError.sessionFailed(error)
        }

        AppDebugLog.coachTrace("ChessGemmaInference stream start promptChars=\(userPrompt.count)")
        let stream = session.generateResponseAsync()
        var accumulated = ""
        do {
            for try await partial in stream {
                accumulated += partial
                onDelta(partial)
            }
        } catch {
            AppDebugLog.coachTrace("ChessGemmaInference stream iterator FAILED — \(error.localizedDescription)")
            throw ChessCoachInferenceError.sessionFailed(error)
        }
        let trimmed = accumulated.trimmingCharacters(in: .whitespacesAndNewlines)
        AppDebugLog.coachTrace("ChessGemmaInference stream done outChars=\(trimmed.count)")
        return trimmed
    }
}

#else

/// Bez CocoaPods / MediaPipe — sestavení projektu bez `pod install`; inference vždy nedostupná (záložní mock v manageru).
final class ChessGemmaInference: @unchecked Sendable {
    var isLoaded: Bool { false }

    func loadModel(at url: URL) throws {
        guard FileManager.default.fileExists(atPath: url.path) else {
            AppDebugLog.coachTrace("ChessGemmaInference STUB loadModel — file missing \(url.lastPathComponent)")
            throw ChessCoachInferenceError.modelFileMissing
        }
        AppDebugLog.coachTrace("ChessGemmaInference STUB loadModel — MediaPipe not linked (use .xcworkspace + pod install)")
        throw ChessCoachInferenceError.mediaPipeNotLinked
    }

    func unload() {}

    func streamResponse(
        userPrompt: String,
        onDelta: @escaping @Sendable (String) -> Void
    ) async throws -> String {
        AppDebugLog.coachTrace("ChessGemmaInference STUB streamResponse rejected promptChars=\(userPrompt.count)")
        _ = userPrompt
        _ = onDelta
        throw ChessCoachInferenceError.mediaPipeNotLinked
    }
}

#endif
