//
//  BoardCompanionSync.swift
//  Throttle: WatchConnectivity + Live Activity ze stejného snapshotu.
//

import Foundation

@MainActor
enum BoardCompanionSync {
    private static var lastWatchPushAt: Date = .distantPast
    private static var lastLiveActivityAt: Date = .distantPast
    private static let watchMinInterval: TimeInterval = 0.75
    private static let liveActivityMinInterval: TimeInterval = 1.2
    /// Při běžících hodinách častější push na Live Activity (lokální odpočet má smysl).
    private static let liveActivityMinIntervalTimerActive: TimeInterval = 0.85

    /// Volat po každém novém snapshotu (WS i REST).
    static func onSnapshotUpdated(_ snap: GameSnapshot, timer: BoardTimerHTTPState?) {
        #if os(iOS)
        let now = Date()
        if now.timeIntervalSince(lastWatchPushAt) >= watchMinInterval {
            lastWatchPushAt = now
            let fen = FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, history: snap.history) ?? ""
            WatchConnectivityBridge.pushCompanionContext(snapshot: snap, timer: timer, fen: fen)
        }
        let timerRunning = timer?.timerRunning ?? snap.status.isTimerRunning
        let laInterval = timerRunning ? liveActivityMinIntervalTimerActive : liveActivityMinInterval
        if now.timeIntervalSince(lastLiveActivityAt) >= laInterval {
            lastLiveActivityAt = now
            Task {
                await MatchLiveActivityManager.shared.applySnapshot(snap, timer: timer)
            }
        }
        #endif
    }

    /// - Parameter notifyWatchConnectionLost: Pouze při vědomém odpojení uživatele (Nastavení). Interní restart transportu (`startPolling`) nesmí poslat falešné „spojení ztraceno“ na Watch.
    static func onConnectionStopped(notifyWatchConnectionLost: Bool = false) {
        #if os(iOS)
        if notifyWatchConnectionLost {
            WatchConnectivityBridge.pushConnectionLost()
        }
        Task {
            await MatchLiveActivityManager.shared.connectionStopped()
        }
        #endif
    }
}
