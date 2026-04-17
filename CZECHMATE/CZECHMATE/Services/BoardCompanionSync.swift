//
//  BoardCompanionSync.swift
//  Throttle: WatchConnectivity + Live Activity ze stejného snapshotu.
//

import Foundation
import CZECHMATEShared

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
            let fen = FENBuilder.boardAndStatusToFEN(board: snap.board, status: snap.status, halfmoveCount: snap.history.moves.count) ?? ""
            WatchConnectivityBridgeEnhanced.pushCompanionContext(snapshot: snap, timer: timer, fen: fen)
        }
        let timerRunning = timer?.timerRunning ?? snap.status.isTimerRunning
        let laInterval = timerRunning ? liveActivityMinIntervalTimerActive : liveActivityMinInterval
        if now.timeIntervalSince(lastLiveActivityAt) >= laInterval {
            lastLiveActivityAt = now
            if MatchLiveActivityManager.isLiveActivityEnabledByUser {
                Task {
                    await MatchLiveActivityManager.shared.applySnapshot(snap, timer: timer)
                }
            }
        }
        #endif
    }

    /// - Parameter notifyWatchConnectionLost: Pouze při vědomém odpojení uživatele (Nastavení). Interní restart transportu (`startPolling`) nesmí poslat falešné „spojení ztraceno“ na Watch.
    static func onConnectionStopped(notifyWatchConnectionLost: Bool = false) {
        #if os(iOS)
        if notifyWatchConnectionLost {
            WatchConnectivityBridgeEnhanced.pushConnectionLost()
        }
        Task {
            await MatchLiveActivityManager.shared.connectionStopped()
        }
        #endif
    }
}

#if os(iOS)
extension BoardCompanionSync {
    /// Horizont pro ActivityKit `staleDate` — těsně za dalším plánovaným throttlovaným updatem.
    static func liveActivityStaleDate(now: Date = .init(), timerRunning: Bool) -> Date {
        let minInterval = timerRunning ? liveActivityMinIntervalTimerActive : liveActivityMinInterval
        return now.addingTimeInterval(minInterval * 2 + 0.35)
    }
}
#endif

