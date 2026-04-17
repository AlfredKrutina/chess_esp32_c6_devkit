//
//  BoardHTTPModels.swift
//  JSON pro GET /api/timer a WiFi status z ESP.
//

import Foundation

/// Odpověď `GET /api/timer` / vnořený `clock` ve snapshotu (viz `timer_get_json` ve firmware).
struct BoardTimerHTTPState: Codable, Equatable {
    let whiteTimeMs: UInt32
    let blackTimeMs: UInt32
    let timerRunning: Bool
    let isWhiteTurn: Bool
    let gamePaused: Bool
    let timeExpired: Bool
    let config: TimerConfigPart
    let totalMoves: UInt32
    let avgMoveTimeMs: UInt32

    enum CodingKeys: String, CodingKey {
        case whiteTimeMs = "white_time_ms"
        case blackTimeMs = "black_time_ms"
        case timerRunning = "timer_running"
        case isWhiteTurn = "is_white_turn"
        case gamePaused = "game_paused"
        case timeExpired = "time_expired"
        case config
        case totalMoves = "total_moves"
        case avgMoveTimeMs = "avg_move_time_ms"
    }

    struct TimerConfigPart: Codable, Equatable {
        let type: Int
        let name: String
        let description: String
        let initialTimeMs: UInt32
        let incrementMs: UInt32
        let isFast: Bool

        enum CodingKeys: String, CodingKey {
            case type, name, description
            case initialTimeMs = "initial_time_ms"
            case incrementMs = "increment_ms"
            case isFast = "is_fast"
        }
    }

    /// `TIME_CONTROL_NONE` ve firmware = typ **0** — hra bez odpočtu; hlavní UI pak neukazuje časomíru.
    var isTimeControlEnabled: Bool { config.type != 0 }
}

/// `wifi_get_sta_status_json` / GET /api/wifi/status.
struct ESPWiFiStatusJSON: Codable {
    let apSsid: String
    let apIp: String
    let apClients: Int
    let staSsid: String
    let staIp: String
    let staConnected: Bool
    let online: Bool
    let locked: Bool
}

struct ESPJSONSuccessMessage: Decodable {
    let success: Bool?
    let message: String?
    let error: String?
}
