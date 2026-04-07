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
    let history: MoveHistory
    let captured: CapturedPieces
    /// Stejná data jako `GET /api/timer` — přítomné u nového firmware v jednom snímku (bez druhého HTTP).
    let clock: BoardTimerHTTPState?

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
    }
}

struct MoveHistory: Decodable, Sendable {
    let moves: [HistoryMove]
}

struct HistoryMove: Decodable, Sendable {
    let from: String
    let to: String
    let piece: String
    let timestamp: UInt32?
    /// SAN z firmwaru / enginu (volitelné). Když chybí, zobrazíme kompaktní notaci z from/to/piece.
    let san: String?

    enum CodingKeys: String, CodingKey {
        case from
        case to
        case piece
        case timestamp
        case san
    }
}

struct CapturedPieces: Decodable, Sendable {
    let whiteCaptured: [String]
    let blackCaptured: [String]

    enum CodingKeys: String, CodingKey {
        case whiteCaptured = "white_captured"
        case blackCaptured = "black_captured"
    }
}
