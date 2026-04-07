//
//  GameStatus.swift
//  CZECHMATE
//

import Foundation

/// Pole ze `game_get_status_json` + `inject_web_status_fields` (parita s webem / `context/snapshot_golden.json`).
struct GameStatus: Decodable, Sendable {
    let gameState: String
    let currentPlayer: String
    let moveCount: UInt32
    /// Zbývající / stav času z `game_get_status_json` (jednotky jako ve firmware).
    let whiteTime: UInt32?
    let blackTime: UInt32?
    let inCheck: Bool
    let checkmate: Bool?
    let stalemate: Bool?
    let webLocked: Bool?
    let internetConnected: Bool?
    let brightness: Int?
    let castlingInProgress: Bool?
    let gameEnd: GameEndStatus?
    let pieceLifted: PieceLifted?
    /// Neplatný tah / návrat — stejné jako web (červené pole na desce).
    let errorState: ErrorStateJSON?
    let restoreState: RestoreStateJSON?
    let boardSetupTutorial: Bool?
    let puzzle: PuzzleStatusJSON?
    let guidedCaptureHintsEnabled: Bool?
    let ledGuidanceLevel: Int?
    let matrixGuardActive: Bool?
    let matrixGuardConflicts: Int?
    let matrixGuardLiftedLow: Int?
    let matrixGuardLiftedHigh: Int?
    let matrixGuardDroppedLow: Int?
    let matrixGuardDroppedHigh: Int?
    let lightMode: String?
    let lightState: Bool?
    let lightR: Int?
    let lightG: Int?
    let lightB: Int?
    let autoLampTimeoutSec: Int?

    /// Hodiny běží jen v aktivní hře (firmware vrací např. „Active“).
    var isTimerRunning: Bool {
        let s = gameState.lowercased()
        return s == "active" || s == "playing"
    }

    /// Hra skončila (MCU nebo šach mat/pat z `game_end`).
    var isGameFinished: Bool {
        let s = gameState.lowercased()
        if s == "finished" { return true }
        return gameEnd?.ended == true
    }

    /// Aktivní „chytrá“ oprava tahu (červené pole + návrat figurky).
    var isErrorRecoveryActive: Bool {
        errorState?.active == true
    }

    enum CodingKeys: String, CodingKey {
        case gameState = "game_state"
        case currentPlayer = "current_player"
        case moveCount = "move_count"
        case whiteTime = "white_time"
        case blackTime = "black_time"
        case inCheck = "in_check"
        case checkmate
        case stalemate
        case webLocked = "web_locked"
        case internetConnected = "internet_connected"
        case brightness
        case castlingInProgress = "castling_in_progress"
        case gameEnd = "game_end"
        case pieceLifted = "piece_lifted"
        case errorState = "error_state"
        case restoreState = "restore_state"
        case boardSetupTutorial = "board_setup_tutorial"
        case puzzle
        case guidedCaptureHintsEnabled = "guided_capture_hints_enabled"
        case ledGuidanceLevel = "led_guidance_level"
        case matrixGuardActive = "matrix_guard_active"
        case matrixGuardConflicts = "matrix_guard_conflicts"
        case matrixGuardLiftedLow = "matrix_guard_lifted_low"
        case matrixGuardLiftedHigh = "matrix_guard_lifted_high"
        case matrixGuardDroppedLow = "matrix_guard_dropped_low"
        case matrixGuardDroppedHigh = "matrix_guard_dropped_high"
        case lightMode = "light_mode"
        case lightState = "light_state"
        case lightR = "light_r"
        case lightG = "light_g"
        case lightB = "light_b"
        case autoLampTimeoutSec = "auto_lamp_timeout_sec"
    }
}

struct ErrorStateJSON: Decodable, Sendable {
    let active: Bool
    let invalidPos: String?
    let originalPos: String?
    let errorCount: Int?

    enum CodingKeys: String, CodingKey {
        case active
        case invalidPos = "invalid_pos"
        case originalPos = "original_pos"
        case errorCount = "error_count"
    }
}

struct RestoreStateJSON: Decodable, Sendable {
    let snapshotLoaded: Bool?
    let snapshotFallbackUsed: Bool?
    let snapshotRestoreFailed: Bool?
    let snapshotSaveFailed: Bool?
    let resyncRequired: Bool?
    let bootNewGameTriggered: Bool?

    enum CodingKeys: String, CodingKey {
        case snapshotLoaded = "snapshot_loaded"
        case snapshotFallbackUsed = "snapshot_fallback_used"
        case snapshotRestoreFailed = "snapshot_restore_failed"
        case snapshotSaveFailed = "snapshot_save_failed"
        case resyncRequired = "resync_required"
        case bootNewGameTriggered = "boot_new_game_triggered"
    }
}

struct PuzzleStatusJSON: Decodable, Sendable {
    let active: Bool?
    let setupActive: Bool?
    let setupId: UInt32?
    let physicalMatch: Bool?
    let fen: String?
    let id: UInt32?
    let difficulty: UInt32?
    let title: String?
    let teaser: String?
    let feedback: String?
    let message: String?

    enum CodingKeys: String, CodingKey {
        case active
        case setupActive = "setup_active"
        case setupId = "setup_id"
        case physicalMatch = "physical_match"
        case fen
        case id
        case difficulty
        case title
        case teaser
        case feedback
        case message
    }
}

struct GameEndStatus: Decodable, Sendable {
    let ended: Bool
    let reason: String?
    let winner: String?
    let loser: String?
}

struct PieceLifted: Decodable, Sendable {
    let lifted: Bool
    let row: Int
    let col: Int
    let piece: String
    let notation: String
}
