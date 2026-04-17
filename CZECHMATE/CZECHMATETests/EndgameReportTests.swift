//
//  EndgameReportTests.swift
//  CZECHMATETests — souhrn partie: časové řady a klíč session.
//

import Foundation
import Testing
@testable import CZECHMATE

struct EndgameReportTests {
    @Test func thinkPlySecondsFromPrevious() throws {
        let m1 = try decodeMove(#"{"from":"e2","to":"e4","timestamp":100}"#)
        let m2 = try decodeMove(#"{"from":"e7","to":"e5","timestamp":130}"#)
        let pts = GameEndReportTiming.thinkPlyPoints(from: [m1, m2])
        #expect(pts.count == 2)
        #expect(pts[0].secondsFromPrevious == nil)
        #expect(pts[1].secondsFromPrevious == 30)
    }

    @Test func cumulativeFromThinkPoints() throws {
        let m1 = try decodeMove(#"{"from":"e2","to":"e4","timestamp":0}"#)
        let m2 = try decodeMove(#"{"from":"e7","to":"e5","timestamp":60}"#)
        let m3 = try decodeMove(#"{"from":"g1","to":"f3","timestamp":90}"#)
        let think = GameEndReportTiming.thinkPlyPoints(from: [m1, m2, m3])
        let cum = GameEndReportTiming.cumulativePoints(from: think)
        #expect(cum.count == 2)
        #expect(cum.last?.totalSeconds == 90)
    }

    @Test func sessionFingerprintPrefersGameId() throws {
        let board = (0 ..< 8).map { _ in (0 ..< 8).map { _ in "." } }
        let root: [String: Any] = [
            "board": board,
            "timestamp": 1,
            "game_id": "match-xyz",
            "status": [
                "game_state": "finished",
                "current_player": "white",
                "move_count": 2,
                "in_check": false,
                "game_end": ["ended": true],
            ] as [String: Any],
            "history": ["moves": []],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
        ]
        let snap = try decodeSnapshot(root)
        let fp = GameEndReportSessionKey.fingerprint(for: snap)
        #expect(fp == "gid:match-xyz")
    }

    @Test func sessionFingerprintWithoutGameIdUsesBoard() throws {
        let board = (0 ..< 8).map { _ in (0 ..< 8).map { _ in "." } }
        let root: [String: Any] = [
            "board": board,
            "timestamp": 1,
            "status": [
                "game_state": "finished",
                "current_player": "white",
                "move_count": 1,
                "in_check": false,
                "game_end": ["ended": true],
            ] as [String: Any],
            "history": ["moves": []],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
        ]
        let snap = try decodeSnapshot(root)
        let fp = GameEndReportSessionKey.fingerprint(for: snap)
        #expect(fp?.hasPrefix("end-1-") == true)
    }

    @Test func pgnContainsResultTag() throws {
        let json = Data(
            """
            [{"from":"e2","to":"e4","piece":"P","san":"e4"},{"from":"e7","to":"e5","piece":"p","san":"e5"}]
            """.utf8
        )
        let moves = try JSONDecoder().decode([GameHistoryMove].self, from: json)
        let pgn = GameHistoryPGN.build(moves: moves, result: .draw)
        #expect(pgn.contains("[Result \"1/2-1/2\"]"))
        #expect(pgn.contains("1. e4 e5"))
    }

    // MARK: - Helpers

    private func decodeMove(_ json: String) throws -> GameHistoryMove {
        try JSONDecoder().decode(GameHistoryMove.self, from: Data(json.utf8))
    }

    private func decodeSnapshot(_ root: [String: Any]) throws -> GameSnapshot {
        let data = try JSONSerialization.data(withJSONObject: root)
        return try GameSnapshot.decodeFromBoardDataRepairingAndNormalizing(data)
    }
}
