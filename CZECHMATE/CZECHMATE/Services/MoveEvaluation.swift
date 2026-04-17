//
//  MoveEvaluation.swift
//  CZECHMATE
//
//  Logika odpovídá evaluateMoveAsync / evalToWhitePerspective v chess_app.js.
//

import Foundation

enum MoveGrade: String, Sendable, Equatable {
    case best
    case good
    case inaccuracy
    case mistake
    case blunder
    case error
}

struct MoveEvaluationResult: Sendable, Equatable {
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
            return MoveEvaluationResult(grade: .best, message: "Výborný tah — engine by zvolil totéž.")
        }
        guard let eb = evalBeforeWhite, let ea = evalAfterWhite else {
            return MoveEvaluationResult(
                grade: .inaccuracy,
                message: "Slabší tah (bez přesného skóre). Lepší bylo \(bestMoveFormatted)."
            )
        }
        let whiteJustMoved = (moveIndex1Based - 1) % 2 == 0
        var scoreDrop = whiteJustMoved ? (eb - ea) : (ea - eb)
        if scoreDrop < 0 { scoreDrop = 0 }
        let cpLost = Int((scoreDrop * 100).rounded())
        if scoreDrop <= 0.20 {
            return MoveEvaluationResult(grade: .good, message: "Dobrý tah — ztráta jen malá (asi \(cpLost) centipawnů).")
        }
        if scoreDrop <= 0.50 {
            return MoveEvaluationResult(
                grade: .inaccuracy,
                message: "Nepřesnost — z pohledu engine ztrácíš asi \(cpLost) cp. Silnější bylo \(bestMoveFormatted)."
            )
        }
        if scoreDrop <= 1.00 {
            return MoveEvaluationResult(
                grade: .mistake,
                message: "Chyba — pozice se výrazně zhoršila (cca \(cpLost) cp). Lepší bylo \(bestMoveFormatted)."
            )
        }
        return MoveEvaluationResult(
            grade: .blunder,
            message: "Hrubka — velký propad (cca \(cpLost) cp). Mnohem lepší bylo \(bestMoveFormatted)."
        )
    }
}
