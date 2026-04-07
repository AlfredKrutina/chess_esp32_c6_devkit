//
//  MoveEvaluation.swift
//  CZECHMATE
//
//  Logika odpovídá evaluateMoveAsync / evalToWhitePerspective v chess_app.js.
//

import Foundation

enum MoveGrade: String, Sendable {
    case best
    case good
    case inaccuracy
    case mistake
    case blunder
    case error
}

struct MoveEvaluationResult: Sendable {
    let grade: MoveGrade
    let message: String
}

enum MoveEvaluation {
    /// Eval z API je z perspektivy bílého; sjednotit podle toho, kdo je na tahu ve FEN.
    static func evalToWhitePerspective(fen: String, evalRaw: Double) -> Double {
        guard fen.contains(" b ") else { return evalRaw }
        return -evalRaw
    }

    static func normalizeUci(from: String, to: String) -> String {
        let a = from.trimmingCharacters(in: .whitespaces).lowercased()
        let b = to.trimmingCharacters(in: .whitespaces).lowercased()
        return a + b
    }

    /// `moveIndex1Based` = délka historie po tahu (jako historyLength v JS).
    static func classify(
        playedFrom: String,
        playedTo: String,
        bestFrom: String,
        bestTo: String,
        evalBeforeWhite: Double?,
        evalAfterWhite: Double?,
        moveIndex1Based: Int,
        bestMoveFormatted: String
    ) -> MoveEvaluationResult {
        let played = normalizeUci(from: playedFrom, to: playedTo)
        let best = normalizeUci(from: bestFrom, to: bestTo)
        if played == best {
            return MoveEvaluationResult(grade: .best, message: "Výborný tah! Byl to nejlepší tah.")
        }
        guard let eb = evalBeforeWhite, let ea = evalAfterWhite else {
            return MoveEvaluationResult(
                grade: .inaccuracy,
                message: "Slabší tah. Lepší bylo \(bestMoveFormatted)."
            )
        }
        let whiteJustMoved = (moveIndex1Based - 1) % 2 == 0
        var scoreDrop = whiteJustMoved ? (eb - ea) : (ea - eb)
        if scoreDrop < 0 { scoreDrop = 0 }
        if scoreDrop <= 0.20 {
            return MoveEvaluationResult(grade: .good, message: "Dobrý tah.")
        }
        if scoreDrop <= 0.50 {
            return MoveEvaluationResult(grade: .inaccuracy, message: "Slabší tah. Lepší bylo \(bestMoveFormatted).")
        }
        if scoreDrop <= 1.00 {
            return MoveEvaluationResult(grade: .mistake, message: "Chyba. Pozice se zhoršila. Lepší bylo \(bestMoveFormatted).")
        }
        return MoveEvaluationResult(grade: .blunder, message: "Vážná chyba. Lepší bylo \(bestMoveFormatted).")
    }
}
