//
//  GameSnapshot.swift
//  CZECHMATE
//

import Foundation

/// Odpověď `GET /api/game/snapshot` — board + status + history + captured (+ volitelný `clock` z firmware).
struct GameSnapshot: Decodable, Sendable {
    /// Monotónní verze z API (`state_version`); pro ETag / If-None-Match.
    let stateVersion: UInt32?
    let board: [[String]]
    let timestamp: UInt64
    let status: GameStatus
    let history: GameMoveHistory
    let captured: CapturedPieces
    /// Stejná data jako `GET /api/timer` — přítomné u nového firmware v jednom snímku (bez druhého HTTP).
    let clock: BoardTimerHTTPState?
    /// Volitelné ID partie z firmware / webu — stabilní klíč pro souhrn (když je v JSON).
    let gameId: String?

    /// Explicitní klíče — dekódovat přes `JSONDecoder.forGameSnapshot()` (viz
    /// `JSONDecoder+GameSnapshot.swift`). `convertFromSnakeCase` rozbije `GameStatus`.
    enum CodingKeys: String, CodingKey {
        case stateVersion = "state_version"
        case board
        case timestamp
        case status
        case history
        case captured
        case clock
        case gameId = "game_id"
    }

    func replacingStatus(_ newStatus: GameStatus) -> GameSnapshot {
        GameSnapshot(
            stateVersion: stateVersion,
            board: board,
            timestamp: timestamp,
            status: newStatus,
            history: history,
            captured: captured,
            clock: clock,
            gameId: gameId
        )
    }
}

struct GameMoveHistory: Decodable, Sendable {
    let moves: [GameHistoryMove]
}

struct GameHistoryMove: Decodable, Sendable {
    let from: String?
    let to: String?
    let piece: String?
    let timestamp: UInt64?
    let san: String?
}

struct CapturedPieces: Decodable, Sendable {
    let whiteCaptured: [String]
    let blackCaptured: [String]

    enum CodingKeys: String, CodingKey {
        case whiteCaptured = "white_captured"
        case blackCaptured = "black_captured"
    }
}

