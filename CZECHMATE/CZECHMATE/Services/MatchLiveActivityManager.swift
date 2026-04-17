//
//  MatchLiveActivityManager.swift
//  ActivityKit — Live Activity pro Smart Stack (hodinky) a Lock Screen.
//

import Foundation

#if os(iOS)
import ActivityKit
import CZECHMATEShared

@MainActor
final class MatchLiveActivityManager {
    static let shared = MatchLiveActivityManager()

    /// Sjednocený klíč s `SettingsTabView` (`@AppStorage`). Výchozí při chybějícím klíči: zapnuto.
    static let liveActivityEnabledDefaultsKey = "czechmate.liveActivityEnabled"

    static var isLiveActivityEnabledByUser: Bool {
        if UserDefaults.standard.object(forKey: liveActivityEnabledDefaultsKey) == nil { return true }
        return UserDefaults.standard.bool(forKey: liveActivityEnabledDefaultsKey)
    }

    private var activity: Activity<ChessMatchAttributes>?

    private init() {}

    func applySnapshot(_ snap: GameSnapshot, timer: BoardTimerHTTPState?) async {
        guard ActivityAuthorizationInfo().areActivitiesEnabled else {
            #if DEBUG
            print("[staging] Live Activities disabled in Settings")
            #endif
            return
        }

        guard Self.isLiveActivityEnabledByUser else {
            if activity != nil {
                await endActivity(reason: "user_disabled_app_setting")
            }
            #if DEBUG
            print("[staging] Live Activity vypnutá v CZECHMATE Nastavení")
            #endif
            return
        }

        let gameEnded = isGameEnded(snap)
        let shouldShow = !gameEnded && (snap.status.isTimerRunning || snap.status.moveCount >= 1)

        if gameEnded || !shouldShow {
            if activity != nil {
                await endActivity(reason: gameEnded ? "game_ended" : "not_in_game")
            }
            return
        }

        let state = makeContentState(snap, timer: timer)
        let stale = BoardCompanionSync.liveActivityStaleDate(timerRunning: state.timerActive)

        if let existing = activity {
            await existing.update(ActivityContent(state: state, staleDate: stale))
            #if DEBUG
            print("[staging] LiveActivity update matchId=\(existing.attributes.matchId)")
            #endif
            return
        }

        let matchId = UUID().uuidString
        let attributes = ChessMatchAttributes(matchId: matchId)
        let content = ActivityContent(state: state, staleDate: stale)
        do {
            activity = try Activity.request(attributes: attributes, content: content, pushType: nil)
            #if DEBUG
            print("[staging] LiveActivity started matchId=\(matchId)")
            #endif
        } catch {
            #if DEBUG
            print("[staging] LiveActivity request failed: \(error.localizedDescription)")
            #endif
        }
    }

    func connectionStopped() async {
        await endActivity(reason: "connection_stopped")
    }

    /// Okamžité ukončení živé aktivity po vypnutí přepínače v Nastavení (bez čekání na další snapshot).
    func dismissForUserPreferenceOff() async {
        await endActivity(reason: "user_preference_off")
    }

    private func endActivity(reason: String) async {
        guard let existing = activity else { return }
        await existing.end(nil, dismissalPolicy: .immediate)
        activity = nil
        #if DEBUG
        print("[staging] LiveActivity ended: \(reason)")
        #endif
    }

    private func isGameEnded(_ snap: GameSnapshot) -> Bool {
        if snap.status.isGameFinished { return true }
        if snap.status.gameEnd?.ended == true { return true }
        if snap.status.checkmate == true { return true }
        if snap.status.stalemate == true { return true }
        return false
    }

    private func makeContentState(_ snap: GameSnapshot, timer: BoardTimerHTTPState?) -> ChessMatchAttributes.ContentState {
        let last = snap.history.moves.last
        let lastFrom = last?.from
        let lastTo = last?.to
        let sub: String
        if let f = lastFrom, let t = lastTo {
            sub = "\(f) → \(t) · tah \(snap.status.moveCount)"
        } else {
            sub = "Tah \(snap.status.moveCount)"
        }
        let boardClock = timer ?? snap.clock
        let whiteSec = boardClock.map { UInt32($0.whiteTimeMs / 1000) } ?? snap.status.whiteTime
        let blackSec = boardClock.map { UInt32($0.blackTimeMs / 1000) } ?? snap.status.blackTime
        let timerActive = boardClock.map { $0.timerRunning && !$0.gamePaused } ?? snap.status.isTimerRunning
        return ChessMatchAttributes.ContentState(
            moveCount: Int(snap.status.moveCount),
            currentPlayer: snap.status.currentPlayer,
            whiteTimeSeconds: whiteSec,
            blackTimeSeconds: blackSec,
            subtitle: sub,
            inCheck: snap.status.inCheck,
            gameEnded: false,
            advantageSummary: MaterialEvaluator.advantageSummary(board: snap.board),
            timerActive: timerActive
        )
    }
}

#else

@MainActor
final class MatchLiveActivityManager {
    static let shared = MatchLiveActivityManager()
    static let liveActivityEnabledDefaultsKey = "czechmate.liveActivityEnabled"
    static var isLiveActivityEnabledByUser: Bool { true }
    private init() {}
    func applySnapshot(_: GameSnapshot, timer _: BoardTimerHTTPState?) async {}
    func connectionStopped() async {}
    func dismissForUserPreferenceOff() async {}
}

#endif
