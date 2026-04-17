//
//  FENBuilderTests.swift
//  CZECHMATETests
//

import Foundation
import Testing
@testable import CZECHMATE

struct FENBuilderTests {
    @Test func startingPositionFEN() throws {
        // Row 0 = rank 8 (černá), row 7 = rank 1 — jako po normalizaci snapshotu / golden JSON.
        var board: [[String]] = Array(repeating: Array(repeating: " ", count: 8), count: 8)
        let blackBack = ["r", "n", "b", "q", "k", "b", "n", "r"]
        let whiteBack = ["R", "N", "B", "Q", "K", "B", "N", "R"]
        for c in 0 ..< 8 {
            board[0][c] = String(blackBack[c])
            board[1][c] = "p"
            board[6][c] = "P"
            board[7][c] = String(whiteBack[c])
        }
        let status = try JSONDecoder().decode(
            GameStatus.self,
            from: Data(
                """
                {"game_state":"active","current_player":"White","move_count":0,"in_check":false}
                """.utf8
            )
        )
        let history = MoveHistory(moves: [])
        let fen = FENBuilder.boardAndStatusToFEN(board: board, status: status, history: history)
        #expect(fen != nil)
        #expect(fen?.contains("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR") == true)
        #expect(fen?.contains(" w ") == true)
    }

    @Test func fenParserPlacementMatchesCanonicalSnapshotRows() throws {
        let start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        let b = FENParser.parseBoard(fromFEN: start)
        #expect(b != nil)
        #expect(b![0][0] == "r")
        #expect(b![7][4] == "K")
        #expect(b![6][4] == "P")
    }
}
