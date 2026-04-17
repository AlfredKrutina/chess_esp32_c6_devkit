//
//  GameSnapshotJSONTests.swift
//  CZECHMATETests
//

import XCTest
@testable import CZECHMATE

final class GameSnapshotJSONTests: XCTestCase {
    func testDecodeStateVersion() throws {
        let board = (0 ..< 8).map { _ in (0 ..< 8).map { _ in "." } }
        let root: [String: Any] = [
            "state_version": 42,
            "board": board,
            "timestamp": 0,
            "status": [
                "game_state": "active",
                "current_player": "white",
                "move_count": 0,
                "in_check": false,
                "game_end": ["ended": false],
            ] as [String: Any],
            "history": ["moves": []],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
        ]
        let json = try JSONSerialization.data(withJSONObject: root)
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(json)
        let dec = JSONDecoder.forGameSnapshot()
        let snap = try dec.decode(GameSnapshot.self, from: fixed)
        XCTAssertEqual(snap.stateVersion, 42)
    }

    /// Reprezentativní snapshot — držet v souladu s `context/snapshot_golden.json`.
    func testDecodeGoldenRepresentativeFirmwareSnapshot() throws {
        let json = Self.goldenRepresentativeJSON.data(using: .utf8)!
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(json)
        let dec = JSONDecoder.forGameSnapshot()
        let snap = try dec.decode(GameSnapshot.self, from: fixed)
        XCTAssertEqual(snap.stateVersion, 1)
        XCTAssertEqual(snap.status.gameState, "active")
        XCTAssertEqual(snap.status.currentPlayer, "White")
        XCTAssertEqual(snap.history.moves.count, 0)
        XCTAssertEqual(snap.status.errorState?.active, false)
        XCTAssertEqual(snap.status.matrixGuardActive, false)
        XCTAssertEqual(snap.status.lightMode, "game")
        XCTAssertEqual(snap.status.chessHintLimit, 3)
    }

    func testDecodeClockFromSnapshot() throws {
        let board = (0 ..< 8).map { _ in (0 ..< 8).map { _ in "." } }
        let clock: [String: Any] = [
            "white_time_ms": 300_000,
            "black_time_ms": 300_000,
            "timer_running": true,
            "is_white_turn": true,
            "game_paused": false,
            "time_expired": false,
            "config": [
                "type": 5,
                "name": "Blitz 5+0",
                "description": "test",
                "initial_time_ms": 300_000,
                "increment_ms": 0,
                "is_fast": true,
            ],
            "total_moves": 0,
            "avg_move_time_ms": 0,
        ]
        let root: [String: Any] = [
            "state_version": 3,
            "board": board,
            "timestamp": 200,
            "status": [
                "game_state": "active",
                "current_player": "white",
                "move_count": 0,
                "in_check": false,
                "game_end": ["ended": false],
            ] as [String: Any],
            "history": ["moves": []],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
            "clock": clock,
        ]
        let json = try JSONSerialization.data(withJSONObject: root)
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(json)
        let dec = JSONDecoder.forGameSnapshot()
        let snap = try dec.decode(GameSnapshot.self, from: fixed)
        XCTAssertEqual(snap.clock?.whiteTimeMs, 300_000)
        XCTAssertEqual(snap.clock?.config.type, 5)
    }

    func testDecodeWithOneMoveInHistory() throws {
        let board = (0 ..< 8).map { _ in (0 ..< 8).map { _ in "." } }
        let root: [String: Any] = [
            "state_version": 2,
            "board": board,
            "timestamp": 100,
            "status": [
                "game_state": "active",
                "current_player": "black",
                "move_count": 1,
                "in_check": false,
                "game_end": ["ended": false],
            ] as [String: Any],
            "history": [
                "moves": [
                    ["from": "e2", "to": "e4", "piece": "P"],
                ],
            ],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
        ]
        let json = try JSONSerialization.data(withJSONObject: root)
        let fixed = GameJSONRepair.repairStatusDataIfNeeded(json)
        let dec = JSONDecoder.forGameSnapshot()
        let snap = try dec.decode(GameSnapshot.self, from: fixed)
        XCTAssertEqual(snap.history.moves.count, 1)
        XCTAssertEqual(snap.history.moves[0].from, "e2")
        XCTAssertEqual(snap.history.moves[0].to, "e4")
    }

    /// MCU `game_get_board_json`: řádek 0 = a1–h1 (bílá). Po dekódování musí být jako golden (rank 8 nahoře) a e2–e4 legální.
    func testFirmwareBoardRowsNormalizeForLocalMoveValidation() throws {
        let whiteBack = ["R", "N", "B", "Q", "K", "B", "N", "R"]
        let blackBack = ["r", "n", "b", "q", "k", "b", "n", "r"]
        var rows: [[String]] = []
        rows.append(whiteBack)
        rows.append((0 ..< 8).map { _ in "P" })
        for _ in 2 ..< 6 {
            rows.append(Array(repeating: " ", count: 8))
        }
        rows.append((0 ..< 8).map { _ in "p" })
        rows.append(blackBack)
        let root: [String: Any] = [
            "state_version": 9,
            "board": rows,
            "timestamp": 1,
            "status": [
                "game_state": "active",
                "current_player": "White",
                "move_count": 0,
                "in_check": false,
                "game_end": ["ended": false],
            ] as [String: Any],
            "history": ["moves": []],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
        ]
        let data = try JSONSerialization.data(withJSONObject: root)
        let snap = try GameSnapshot.decodeFromBoardDataRepairingAndNormalizing(data)
        XCTAssertEqual(snap.board[0][0].trimmingCharacters(in: .whitespaces), "r")
        XCTAssertEqual(snap.board[7][4].trimmingCharacters(in: .whitespaces), "K")
        let block = RemoteChessMoveLegality.validate(snapshot: snap, from: "e2", to: "e4", promotion: nil)
        XCTAssertNil(block, "Očekáván legální první tah bílého po normalizaci řádků")
    }

    /// Po převrácení řádků musí `piece_lifted.row` odpovídat kanonické mřížce (MCU řádek 1 → řádek 6).
    func testFirmwareNormalizationMirrorsPieceLiftedRow() throws {
        let whiteBack = ["R", "N", "B", "Q", "K", "B", "N", "R"]
        let blackBack = ["r", "n", "b", "q", "k", "b", "n", "r"]
        var rows: [[String]] = []
        rows.append(whiteBack)
        rows.append((0 ..< 8).map { _ in "P" })
        for _ in 2 ..< 6 {
            rows.append(Array(repeating: " ", count: 8))
        }
        rows.append((0 ..< 8).map { _ in "p" })
        rows.append(blackBack)
        let root: [String: Any] = [
            "state_version": 9,
            "board": rows,
            "timestamp": 1,
            "status": [
                "game_state": "active",
                "current_player": "White",
                "move_count": 0,
                "in_check": false,
                "game_end": ["ended": false],
                "piece_lifted": [
                    "lifted": true,
                    "row": 1,
                    "col": 4,
                    "piece": "P",
                    "notation": "e2",
                ],
            ] as [String: Any],
            "history": ["moves": []],
            "captured": [
                "white_captured": [String](),
                "black_captured": [String](),
            ],
        ]
        let data = try JSONSerialization.data(withJSONObject: root)
        let snap = try GameSnapshot.decodeFromBoardDataRepairingAndNormalizing(data)
        XCTAssertEqual(snap.status.pieceLifted?.row, 6)
        XCTAssertEqual(snap.status.pieceLifted?.col, 4)
    }

    private static let goldenRepresentativeJSON: String = """
    {
      "state_version": 1,
      "board": [
        ["r", "n", "b", "q", "k", "b", "n", "r"],
        ["p", "p", "p", "p", "p", "p", "p", "p"],
        [" ", " ", " ", " ", " ", " ", " ", " "],
        [" ", " ", " ", " ", " ", " ", " ", " "],
        [" ", " ", " ", " ", " ", " ", " ", " "],
        [" ", " ", " ", " ", " ", " ", " ", " "],
        ["P", "P", "P", "P", "P", "P", "P", "P"],
        ["R", "N", "B", "Q", "K", "B", "N", "R"]
      ],
      "timestamp": 12345678,
      "status": {
        "game_state": "active",
        "current_player": "White",
        "move_count": 0,
        "white_time": 300,
        "black_time": 300,
        "in_check": false,
        "checkmate": false,
        "stalemate": false,
        "piece_lifted": {
          "lifted": false,
          "row": 0,
          "col": 0,
          "piece": " ",
          "notation": ""
        },
        "castling_in_progress": false,
        "game_end": {
          "ended": false,
          "reason": "",
          "winner": "",
          "loser": ""
        },
        "error_state": {
          "active": false
        },
        "restore_state": {
          "snapshot_loaded": false,
          "snapshot_fallback_used": false,
          "snapshot_restore_failed": false,
          "snapshot_save_failed": false,
          "resync_required": false,
          "boot_new_game_triggered": false
        },
        "board_setup_tutorial": false,
        "puzzle": {
          "active": false,
          "setup_active": false,
          "setup_id": 0,
          "physical_match": false,
          "fen": "",
          "id": 0,
          "difficulty": 0,
          "title": "",
          "teaser": "",
          "feedback": "",
          "message": ""
        },
        "web_locked": false,
        "internet_connected": true,
        "brightness": 50,
        "guided_capture_hints_enabled": true,
        "led_guidance_level": 5,
        "matrix_guard_active": false,
        "matrix_guard_conflicts": 0,
        "matrix_guard_lifted_low": 0,
        "matrix_guard_lifted_high": 0,
        "matrix_guard_dropped_low": 0,
        "matrix_guard_dropped_high": 0,
        "light_mode": "game",
        "light_state": true,
        "light_r": 255,
        "light_g": 255,
        "light_b": 255,
        "auto_lamp_timeout_sec": 300,
        "chess_hint_limit": 3
      },
      "history": {
        "moves": []
      },
      "captured": {
        "white_captured": [],
        "black_captured": []
      }
    }
    """
}
