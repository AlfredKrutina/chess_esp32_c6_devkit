//
//  MoveEvaluationTests.swift
//  CZECHMATETests
//

import Foundation
import Testing
@testable import CZECHMATE

struct MoveEvaluationTests {
    @Test func normalizeUci() {
        #expect(MoveEvaluation.normalizeUci(from: "e2", to: "e4") == "e2e4")
    }

    @Test func evalToWhitePerspectiveBlackToMove() {
        let fen = "8/8/8/8/8/8/8/8 b - - 0 1"
        #expect(MoveEvaluation.evalToWhitePerspective(fen: fen, evalRaw: 1.0) == -1.0)
    }

    @Test func classifyBestMatch() {
        let r = MoveEvaluation.classify(
            playedFrom: "e2",
            playedTo: "e4",
            bestFrom: "e2",
            bestTo: "e4",
            evalBeforeWhite: 0.2,
            evalAfterWhite: 0.2,
            moveIndex1Based: 1,
            bestMoveFormatted: "e2-e4"
        )
        #expect(r.grade == .best)
    }
}
