//
//  LearningModeManager.swift
//  CZECHMATE — Učící mód: jiné UI, trenér, záchranná brzda při hrubce.
//

import Combine
import Foundation
import SwiftUI

/// Stav „záchranné brzdy“ po detekci blunderu (Stockfish).
struct BlunderBrakePresentation: Identifiable, Equatable {
    let id = UUID()
    /// Index tahu v historii (1-based), aby se stejná hrubka neopakovala při každém překreslení.
    let moveCountAtTrigger: Int
    let coachMessage: String

    static func == (lhs: BlunderBrakePresentation, rhs: BlunderBrakePresentation) -> Bool {
        lhs.id == rhs.id
    }
}

@MainActor
final class LearningModeManager: ObservableObject {
    /// Zapnutý učící mód — mění `GameView` (bez stresových hodin, trenérské prvky).
    @Published var isLearningModeActive = false

    /// Modální „brzda“ po fatální chybě.
    @Published var blunderBrake: BlunderBrakePresentation?

    /// Uživatel chtěl zapnout režim, ale chybí model — dokončí se po onboarding / stažení.
    @Published var pendingActivateAfterModel = false

    /// Po dokončení streamu rady od lokální Gemmy (MediaPipe) — pro gamifikaci / „přečteno“.
    @Published private(set) var coachAdviceStreamsCompleted: Int = 0

    /// Zhoršení v pěšácích (stejná logika jako `MoveEvaluation.classify`) — okamžitá brzda přes eval API.
    /// Konfigurace: `UserDefaults` klíč `czechmate.coach.blunderEvalDropThreshold` (výchozí 2.0).
    var blunderEvalDropThreshold: Double {
        let x = UserDefaults.standard.double(forKey: "czechmate.coach.blunderEvalDropThreshold")
        return x > 0 ? x : 2.0
    }

    private var lastHandledBlunderMoveCount: Int?

    /// Volat po dokončení reálného stažení modelu (soubor na disku) nebo po přeskočení, pokud už model je.
    func completePendingActivationIfNeeded(modelReady: Bool) {
        guard modelReady, pendingActivateAfterModel else { return }
        AppDebugLog.coachTrace("LearningMode pending activation → ON (model ready)")
        pendingActivateAfterModel = false
        isLearningModeActive = true
    }

    /// Zrušení čekání na model (uživatel zavřel onboarding).
    func cancelPendingActivation() {
        AppDebugLog.coachTrace("LearningMode cancel pending activation")
        pendingActivateAfterModel = false
    }

    /// Porovnání eval před/po tahu (paralelní Stockfish) — nečeká na `lastMoveEvaluation` z desky.
    func applyEvalDropBlunderBrake(
        evalBeforeWhite: Double?,
        evalAfterWhite: Double?,
        moveIndex1Based: Int,
        moveCount: Int
    ) {
        guard isLearningModeActive else { return }
        guard let eb = evalBeforeWhite, let ea = evalAfterWhite else { return }
        if lastHandledBlunderMoveCount == moveCount { return }

        let whiteJustMoved = (moveIndex1Based - 1) % 2 == 0
        var scoreDrop = whiteJustMoved ? (eb - ea) : (ea - eb)
        if scoreDrop < 0 { scoreDrop = 0 }
        guard scoreDrop > blunderEvalDropThreshold else { return }

        lastHandledBlunderMoveCount = moveCount
        let msg =
            "Jsi si jistý? Pozice se výrazně zhoršila (o \(String(format: "%.1f", scoreDrop)) pěš.). Zkus najít lepší obranu."
        blunderBrake = BlunderBrakePresentation(
            moveCountAtTrigger: moveCount,
            coachMessage: msg
        )
        AppDebugLog.coachTrace("LearningMode eval-drop brake moveCount=\(moveCount) drop=\(scoreDrop)")
    }

    /// Po vyhodnocení tahu z vedlejšího procesu (grade == blunder).
    func consumeMoveEvaluationForBlunderBrake(
        evaluation: MoveEvaluationResult?,
        currentMoveCount: Int
    ) {
        guard isLearningModeActive else { return }
        guard let ev = evaluation, ev.grade == .blunder else { return }
        if lastHandledBlunderMoveCount == currentMoveCount { return }
        lastHandledBlunderMoveCount = currentMoveCount

        let msg =
            "Jsi si jistý? Tímto tahem ztratíš dámu. Zkus najít lepší obranu."
        blunderBrake = BlunderBrakePresentation(
            moveCountAtTrigger: currentMoveCount,
            coachMessage: msg
        )
        AppDebugLog.coachTrace("LearningMode grade blunder brake moveCount=\(currentMoveCount)")
    }

    func dismissBlunderBrake() {
        blunderBrake = nil
    }

    /// Volat po dokončení generování textu trenéra (stream skončil bez chyby).
    func registerCoachAdviceStreamCompleted() {
        coachAdviceStreamsCompleted += 1
        AppDebugLog.coachTrace("LearningMode coach advice stream completed total=\(coachAdviceStreamsCompleted)")
    }

    func resetBlunderTrackingForNewGame() {
        lastHandledBlunderMoveCount = nil
    }
}
