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
    static let minimumExpectedBytes: Int64 = 100_000_000
}

/// Stažení MediaPipe FlatBuffer modelu (Gemma 2B IT int4, stejný formát jako oficiální ukázka `gemma-2b-it-cpu-int4.bin`).
/// Výchozí URL: veřejné zrcadlo na Hugging Face (viz licence Gemma / podmínky poskytovatele).
/// Volitelný přepis: `UserDefaults` klíč `czechmate.coach.modelDownloadURL` (řetězec HTTPS).
@MainActor
final class ModelDownloadManager: NSObject, ObservableObject {
    private static let installedKey = "czechmate.coach.llmModelInstalled"
    private static let customDownloadURLKey = "czechmate.coach.modelDownloadURL"
    private static var coachModelDirectoryComponent: String { CoachModelDiskLayout.directoryComponent }
    static let coachGemmaModelFileName = CoachModelDiskLayout.fileName

    /// MediaPipe ukázka používá stejný binární formát; po stažení uložíme pod `coachGemmaModelFileName`.
    /// Pinned `resolve/main` — při změně repa uprav URL nebo použij vlastní přes UserDefaults.
    private static let defaultHuggingFaceModelURLString =
        "https://huggingface.co/metsman/gemma-2b-it-cpu-int4-org/resolve/main/gemma-2b-it-cpu-int4.bin"

    static let displayedFileSizeDescription = "cca 1,3 GB"

    var coachModelsDirectoryURL: URL? {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first?
            .appendingPathComponent(Self.coachModelDirectoryComponent, isDirectory: true)
    }

    var resolvedModelFileURL: URL? {
        guard let dir = coachModelsDirectoryURL else { return nil }
        return dir.appendingPathComponent(Self.coachGemmaModelFileName, isDirectory: false)
    }

    var hasMediaPipeModelOnDisk: Bool {
        guard let url = resolvedModelFileURL else { return false }
        return FileManager.default.fileExists(atPath: url.path)
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
    }

    private func syncInstalledFlagFromDisk() {
        let onDisk = hasMediaPipeModelOnDisk
        isModelInstalled = onDisk
        UserDefaults.standard.set(onDisk, forKey: Self.installedKey)
    }

    private func ensureCoachModelsDirectoryExists() {
        guard let dir = coachModelsDirectoryURL else { return }
        if !FileManager.default.fileExists(atPath: dir.path) {
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        }
    }

    /// Řeší výslednou HTTPS adresu: vlastní (UserDefaults) → výchozí Hugging Face.
    func resolvedDownloadURL() -> URL? {
        if let s = UserDefaults.standard.string(forKey: Self.customDownloadURLKey),
           let u = URL(string: s), u.scheme == "https" || u.scheme == "http" {
            return u
        }
        return URL(string: Self.defaultHuggingFaceModelURLString)
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
        request.setValue("CZECHMATE-iOS/1.0", forHTTPHeaderField: "User-Agent")
        request.timeoutInterval = 60 * 60 * 8

        let task = downloadSession.downloadTask(with: request)
        activeDownloadTask = task
        task.resume()
        #if DEBUG
        AppDebugLog.staging(
            "ModelDownloadManager: startDownload host=\(url.host ?? "?") path=\(url.path) (customURL=\(UserDefaults.standard.string(forKey: Self.customDownloadURLKey) != nil))"
        )
        #endif
    }

    func pause() {
        guard state == .downloading, let task = activeDownloadTask else { return }
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
        lastErrorDescription = nil
        state = .downloading
        isPaused = false
        let task = downloadSession.downloadTask(withResumeData: data)
        activeDownloadTask = task
        task.resume()
    }

    func cancelActiveDownload(resetResumeData: Bool = true) {
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
        #if DEBUG
        AppDebugLog.staging("ModelDownloadManager: failed — \(message)")
        #endif
    }

    private func completeDownloadSuccess() {
        activeDownloadTask = nil
        resumeData = nil
        progress = 1.0
        state = .completed
        isPaused = false
        syncInstalledFlagFromDisk()
        #if DEBUG
        AppDebugLog.staging("ModelDownloadManager: file saved hasMediaPipeModelOnDisk=\(hasMediaPipeModelOnDisk)")
        #endif
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
