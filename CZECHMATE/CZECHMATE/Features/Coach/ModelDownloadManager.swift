//
//  ModelDownloadManager.swift
//  CZECHMATE — Stažení lokálního LLM (~1,3 GB) přes URLSession; ukládá jako coach_gemma_mediapipe.bin.
//

import Combine
import Foundation
import SwiftUI

enum CoachModelDownloadState: String, Sendable {
    case idle
    case downloading
    case paused
    case completed
    case failed
}

/// Konstanty pro `nonisolated` URLSession delegate (nesmí číst `@MainActor` static z třídy).
private enum CoachModelDiskLayout {
    static let directoryComponent = "ChessCoachModels"
    static let fileName = "coach_gemma_mediapipe.bin"
    /// Pod ~100 MB jde typicky o HTML chybovou stránku, ne o Gemma 2B int4 FlatBuffer.
    static let minimumExpectedBytes: Int64 = 100_000_000
    /// Horní mez proti poškozenému / neočekávanému streamu (~2,2 GB).
    static let maximumExpectedBytes: Int64 = 2_300_000_000
}

/// Stažení MediaPipe FlatBuffer `.bin` (Gemma 2B IT **GPU** int4 — na iOS se má používat s `LlmInference.Options.preferredBackend = .gpu`).
/// CPU int4 (`gemma-2b-it-cpu-int4.bin`) na iOS často končí chybou „XnnLlmResourceCalculator“ — viz MediaPipe iOS / ukázka `llm_inference/ios`.
/// Zdroj: https://huggingface.co/google/gemma-2b-it-tflite — přehled: https://ai.google.dev/edge/mediapipe/solutions/genai/llm_inference/index#models
/// Volitelný přepis: `UserDefaults` klíč `czechmate.coach.modelDownloadURL` — v Release pouze **https**; v DEBUG i `http` (např. lokální mirror).
@MainActor
final class ModelDownloadManager: NSObject, ObservableObject {
    private static let installedKey = "czechmate.coach.llmModelInstalled"
    private static let customDownloadURLKey = "czechmate.coach.modelDownloadURL"
    private static var coachModelDirectoryComponent: String { CoachModelDiskLayout.directoryComponent }
    static let coachGemmaModelFileName = CoachModelDiskLayout.fileName

    /// Oficiální FlatBuffer z Google (`gemma-2b-it-tflite`); po stažení uložíme pod `coachGemmaModelFileName`.
    /// Na Hugging Face může být potřeba jednorázově odsouhlasit licenci Gemma v prohlížeči, jinak server vrátí HTML.
    private static let defaultCoachModelDownloadURLString =
        "https://huggingface.co/google/gemma-2b-it-tflite/resolve/main/gemma-2b-it-gpu-int4.bin"

    static let displayedFileSizeDescription = "cca 1,3 GB"

    /// Host výsledné stahovací URL (pro transparentní UI — skutečný síťový zdroj).
    var downloadSourceHostForDisplay: String {
        resolvedDownloadURL()?.host ?? "—"
    }

    var coachModelsDirectoryURL: URL? {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first?
            .appendingPathComponent(Self.coachModelDirectoryComponent, isDirectory: true)
    }

    var resolvedModelFileURL: URL? {
        guard let dir = coachModelsDirectoryURL else { return nil }
        return dir.appendingPathComponent(Self.coachGemmaModelFileName, isDirectory: false)
    }

    /// Soubor `coach_gemma_mediapipe.bin` existuje a má očekávanou velikost (stejná pravidla jako po stažení — ne HTML zástupce).
    var hasMediaPipeModelOnDisk: Bool {
        guard let url = resolvedModelFileURL else { return false }
        guard FileManager.default.fileExists(atPath: url.path) else { return false }
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: url.path),
              let size = attrs[.size] as? Int64 else { return false }
        return size >= CoachModelDiskLayout.minimumExpectedBytes
            && size <= CoachModelDiskLayout.maximumExpectedBytes
    }

    @Published private(set) var progress: Double = 0
    @Published private(set) var isPaused = false
    @Published private(set) var state: CoachModelDownloadState = .idle
    /// `true` jen pokud existuje stažený `.bin` (synchronizováno z disku v `init` a po dokončení stažení).
    @Published private(set) var isModelInstalled: Bool = false

    @Published private(set) var lastErrorDescription: String?

    private var resumeData: Data?
    private var activeDownloadTask: URLSessionDownloadTask?

    private lazy var downloadSession: URLSession = {
        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForRequest = 120
        cfg.timeoutIntervalForResource = 60 * 60 * 8
        cfg.allowsCellularAccess = true
        cfg.waitsForConnectivity = true
        return URLSession(configuration: cfg, delegate: self, delegateQueue: nil)
    }()

    override init() {
        super.init()
        ensureCoachModelsDirectoryExists()
        syncInstalledFlagFromDisk()
        let path = resolvedModelFileURL?.path ?? "nil"
        AppDebugLog.coachTrace(
            "ModelDownloadManager init installed=\(isModelInstalled) hasUsableBin=\(hasMediaPipeModelOnDisk) path=\(path)"
        )
    }

    private func syncInstalledFlagFromDisk() {
        let onDisk = hasMediaPipeModelOnDisk
        isModelInstalled = onDisk
        UserDefaults.standard.set(onDisk, forKey: Self.installedKey)
    }

    /// Zarovná `isModelInstalled` a UI stav s reálným stavem souboru (záloha, smazání, první spuštění po stažení).
    func reconcileWithDisk() {
        let prevState = state
        syncInstalledFlagFromDisk()
        if hasMediaPipeModelOnDisk {
            // Soubor je na disku — nesnižovat probíhající stažení (.downloading / .paused).
            if state != .downloading, state != .paused {
                state = .completed
                progress = 1.0
            }
        } else if state == .completed {
            state = .idle
            progress = 0
            lastErrorDescription = nil
        }
        AppDebugLog.coachTrace(
            "ModelDownloadManager reconcile prevState=\(prevState.rawValue) now=\(state.rawValue) installed=\(isModelInstalled) hasUsableBin=\(hasMediaPipeModelOnDisk)"
        )
    }

    private func ensureCoachModelsDirectoryExists() {
        guard let dir = coachModelsDirectoryURL else { return }
        if !FileManager.default.fileExists(atPath: dir.path) {
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        }
    }

    /// Vlastní URL z UserDefaults (pokud platí pro daný build) → výchozí `google/gemma-2b-it-tflite` (GPU int4 `.bin`).
    func resolvedDownloadURL() -> URL? {
        if let s = UserDefaults.standard.string(forKey: Self.customDownloadURLKey),
           let u = URL(string: s) {
            #if DEBUG
            if u.scheme == "https" || u.scheme == "http" { return u }
            #else
            if u.scheme == "https" { return u }
            #endif
        }
        return URL(string: Self.defaultCoachModelDownloadURLString)
    }

    /// Zahájí skutečné stažení; po úspěchu uloží soubor jako `coach_gemma_mediapipe.bin`.
    func startDownload() {
        cancelActiveDownload(resetResumeData: true)
        guard let url = resolvedDownloadURL() else {
            lastErrorDescription = "Chybí platná URL pro stažení modelu."
            state = .failed
            return
        }
        ensureCoachModelsDirectoryExists()
        lastErrorDescription = nil
        progress = 0
        isPaused = false
        state = .downloading
        resumeData = nil

        var request = URLRequest(url: url)
        let ver = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0"
        request.setValue("CZECHMATE/\(ver) (iOS; coach-model)", forHTTPHeaderField: "User-Agent")
        request.timeoutInterval = 60 * 60 * 8

        let task = downloadSession.downloadTask(with: request)
        activeDownloadTask = task
        task.resume()
        AppDebugLog.coachTrace(
            "ModelDownloadManager startDownload host=\(url.host ?? "?") path=\(url.path) customURL=\(UserDefaults.standard.string(forKey: Self.customDownloadURLKey) != nil)"
        )
    }

    func pause() {
        guard state == .downloading, let task = activeDownloadTask else { return }
        AppDebugLog.coachTrace("ModelDownloadManager pause")
        task.cancel(byProducingResumeData: { [weak self] data in
            Task { @MainActor [weak self] in
                guard let self, let data else { return }
                self.resumeData = data
                self.activeDownloadTask = nil
                self.state = .paused
                self.isPaused = true
            }
        })
    }

    func resume() {
        guard state == .paused, let data = resumeData else { return }
        AppDebugLog.coachTrace("ModelDownloadManager resume resumeDataBytes=\(data.count)")
        lastErrorDescription = nil
        state = .downloading
        isPaused = false
        let task = downloadSession.downloadTask(withResumeData: data)
        activeDownloadTask = task
        task.resume()
    }

    func cancelActiveDownload(resetResumeData: Bool = true) {
        AppDebugLog.coachTrace("ModelDownloadManager cancelActiveDownload resetResume=\(resetResumeData) state=\(state.rawValue)")
        activeDownloadTask?.cancel()
        activeDownloadTask = nil
        if resetResumeData {
            resumeData = nil
        }
        if state == .downloading || state == .paused {
            state = .idle
            progress = 0
            isPaused = false
        }
    }

    func markInstalledForTesting() {
        syncInstalledFlagFromDisk()
        state = hasMediaPipeModelOnDisk ? .completed : .idle
        progress = hasMediaPipeModelOnDisk ? 1.0 : 0
    }

    private func failDownload(message: String) {
        activeDownloadTask = nil
        resumeData = nil
        lastErrorDescription = message
        state = .failed
        progress = 0
        isPaused = false
        AppDebugLog.coachTrace("ModelDownloadManager failed — \(message)")
    }

    private func completeDownloadSuccess() {
        activeDownloadTask = nil
        resumeData = nil
        progress = 1.0
        state = .completed
        isPaused = false
        syncInstalledFlagFromDisk()
        let bytes: Int64 = {
            guard let u = resolvedModelFileURL,
                  let a = try? FileManager.default.attributesOfItem(atPath: u.path),
                  let s = a[.size] as? Int64 else { return -1 }
            return s
        }()
        AppDebugLog.coachTrace(
            "ModelDownloadManager download finished hasUsableBin=\(hasMediaPipeModelOnDisk) bytes=\(bytes)"
        )
    }

}

// MARK: - URLSessionDownloadDelegate

extension ModelDownloadManager: URLSessionDownloadDelegate {
    nonisolated func urlSession(
        _ session: URLSession,
        downloadTask: URLSessionDownloadTask,
        didFinishDownloadingTo location: URL
    ) {
        guard let http = downloadTask.response as? HTTPURLResponse else {
            Task { @MainActor [weak self] in
                self?.failDownload(message: "Chybí HTTP odpověď serveru.")
            }
            return
        }
        guard (200 ... 299).contains(http.statusCode) else {
            Task { @MainActor [weak self] in
                self?.failDownload(message: "Server vrátil stav \(http.statusCode). Zkus to znovu později nebo vlastní URL v nastavení.")
            }
            return
        }

        // Finální odpověď po redirectech (HF → CDN) musí být binární; HTML chybovka typicky text/html.
        // Neodmítáme všechno `text/*` — první hop HF může být 302 s text/plain, ale task.response je vždy poslední odpověď.
        let mime = http.mimeType?.lowercased() ?? ""
        if mime.contains("html") {
            Task { @MainActor [weak self] in
                self?.failDownload(
                    message: "Server odeslal HTML místo modelu (Content-Type: \(http.mimeType ?? "?")). Zkus to znovu nebo jinou URL."
                )
            }
            return
        }

        let fm = FileManager.default
        guard let dest = Self.applicationSupportModelFileURL() else {
            Task { @MainActor [weak self] in
                self?.failDownload(message: "Nelze zapisovat do Application Support.")
            }
            return
        }
        let partURL = dest.deletingLastPathComponent()
            .appendingPathComponent(CoachModelDiskLayout.fileName + ".part", isDirectory: false)

        do {
            try fm.createDirectory(at: dest.deletingLastPathComponent(), withIntermediateDirectories: true)
            if fm.fileExists(atPath: partURL.path) {
                try fm.removeItem(at: partURL)
            }
            try fm.copyItem(at: location, to: partURL)
            let attrs = try fm.attributesOfItem(atPath: partURL.path)
            let size = attrs[.size] as? Int64 ?? 0
            if size < CoachModelDiskLayout.minimumExpectedBytes {
                try? fm.removeItem(at: partURL)
                Task { @MainActor [weak self] in
                    self?.failDownload(message: "Stažený soubor je příliš malý (\(size) B) — pravděpodobně chybová stránka, ne model.")
                }
                return
            }
            if size > CoachModelDiskLayout.maximumExpectedBytes {
                try? fm.removeItem(at: partURL)
                Task { @MainActor [weak self] in
                    self?.failDownload(message: "Stažený soubor je příliš velký — neočekávaný obsah. Zkontroluj URL nebo kontaktuj podporu.")
                }
                return
            }
            if fm.fileExists(atPath: dest.path) {
                try fm.removeItem(at: dest)
            }
            try fm.moveItem(at: partURL, to: dest)
        } catch {
            try? fm.removeItem(at: partURL)
            let msg = error.localizedDescription
            Task { @MainActor [weak self] in
                self?.failDownload(message: msg)
            }
            return
        }

        Task { @MainActor [weak self] in
            self?.completeDownloadSuccess()
        }
    }

    private nonisolated static func applicationSupportModelFileURL() -> URL? {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first?
            .appendingPathComponent(CoachModelDiskLayout.directoryComponent, isDirectory: true)
            .appendingPathComponent(CoachModelDiskLayout.fileName, isDirectory: false)
    }

    nonisolated func urlSession(
        _ session: URLSession,
        downloadTask: URLSessionDownloadTask,
        didWriteData bytesWritten: Int64,
        totalBytesWritten: Int64,
        totalBytesExpectedToWrite: Int64
    ) {
        let p: Double
        if totalBytesExpectedToWrite > 0 {
            p = Double(totalBytesWritten) / Double(totalBytesExpectedToWrite)
        } else {
            p = 0
        }
        Task { @MainActor [weak self] in
            self?.progress = min(1.0, p)
        }
    }

    nonisolated func urlSession(
        _ session: URLSession,
        task: URLSessionTask,
        didCompleteWithError error: Error?
    ) {
        if let error {
            let ns = error as NSError
            if ns.domain == NSURLErrorDomain, ns.code == NSURLErrorCancelled {
                return
            }
            let msg = error.localizedDescription
            Task { @MainActor [weak self] in
                guard let self else { return }
                if self.state == .downloading {
                    self.failDownload(message: msg)
                }
            }
            return
        }
    }
}
