//
//  ChessboardWebSocketClient.swift
//  CZECHMATE
//

import Foundation

/// WebSocket na `ws://<host>/ws` — stejný JSON jako `GET /api/game/snapshot`.
actor ChessboardWebSocketClient {
    private var task: URLSessionWebSocketTask?
    private var receiveLoopTask: Task<Void, Never>?
    private let session: URLSession

    init(session: URLSession = .shared) {
        self.session = session
    }

    /// Převod `http(s)://host[:port]/path` → `ws(s)://host[:port]/ws`.
    static func websocketURL(from baseURL: URL) -> URL? {
        var c = URLComponents(url: baseURL, resolvingAgainstBaseURL: false)
        guard let scheme = c?.scheme?.lowercased() else { return nil }
        if scheme == "https" {
            c?.scheme = "wss"
        } else if scheme == "http" {
            c?.scheme = "ws"
        } else {
            return nil
        }
        c?.path = "/ws"
        c?.query = nil
        c?.fragment = nil
        return c?.url
    }

    func disconnect() {
        receiveLoopTask?.cancel()
        receiveLoopTask = nil
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
    }

    func connect(
        baseURL: URL,
        onSnapshot: @escaping @Sendable (GameSnapshot) -> Void,
        onDisconnect: @escaping @Sendable () -> Void
    ) async {
        disconnect()
        guard let wsURL = Self.websocketURL(from: baseURL) else {
            AppDebugLog.staging("WebSocket: neplatná base URL pro WS (zkontroluj http/https + host)")
            return
        }
        AppDebugLog.staging("WS connect \(wsURL.absoluteString)")
        let socketTask = session.webSocketTask(with: wsURL)
        task = socketTask
        socketTask.resume()
        receiveLoopTask = Task {
            await Self.runReceiveLoop(
                task: socketTask,
                onSnapshot: onSnapshot,
                onDisconnect: onDisconnect
            )
        }
    }

    private nonisolated static func runReceiveLoop(
        task: URLSessionWebSocketTask?,
        onSnapshot: @escaping @Sendable (GameSnapshot) -> Void,
        onDisconnect: @escaping @Sendable () -> Void
    ) async {
        guard let task else { return }
        while !Task.isCancelled {
            do {
                let message = try await task.receive()
                switch message {
                case let .string(text):
                    if let snap = decodeSnapshot(from: text) {
                        onSnapshot(snap)
                    }
                case let .data(data):
                    if let snap = decodeSnapshot(from: data) {
                        onSnapshot(snap)
                    }
                @unknown default:
                    break
                }
            } catch {
                AppDebugLog.stagingError("WebSocket receive", error)
                onDisconnect()
                return
            }
        }
        onDisconnect()
    }

    private nonisolated static func decodeSnapshot(from text: String) -> GameSnapshot? {
        guard let data = text.data(using: .utf8) else { return nil }
        return decodeSnapshot(from: data)
    }

    private nonisolated static func decodeSnapshot(from data: Data) -> GameSnapshot? {
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(data)
        let decoder = JSONDecoder.forGameSnapshot()
        do {
            return try decoder.decode(GameSnapshot.self, from: fixed)
        } catch {
            let preview = String(data: fixed.prefix(160), encoding: .utf8) ?? ""
            AppDebugLog.staging("WebSocket frame JSON decode failed: \(error) preview=\(preview)")
            return nil
        }
    }
}
