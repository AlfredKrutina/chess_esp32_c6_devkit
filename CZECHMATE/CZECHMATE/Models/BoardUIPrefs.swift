//
//  BoardUIPrefs.swift
//  Parita s webem: GET/POST /api/settings/ui (NVS).
//

import Foundation

/// Kořen JSON z webu — `buildUiPrefsPayload()` v `chess_app.js`.
struct BoardUISettingsEnvelope: Codable, Sendable, Equatable {
    var version: Int
    var prefs: BoardUIPrefsPayload

    static let empty = BoardUISettingsEnvelope(version: 1, prefs: .init())

    enum CodingKeys: String, CodingKey {
        case version
        case prefs
    }

    init(version: Int, prefs: BoardUIPrefsPayload) {
        self.version = version
        self.prefs = prefs
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        version = try c.decodeIfPresent(Int.self, forKey: .version) ?? 1
        prefs = try c.decodeIfPresent(BoardUIPrefsPayload.self, forKey: .prefs) ?? BoardUIPrefsPayload()
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(version, forKey: .version)
        try c.encode(prefs, forKey: .prefs)
    }
}

struct BoardUIPrefsPayload: Codable, Sendable, Equatable {
    /// Sloučení s aktuálním `hintDepth` / eval z aplikace před POST na desku.
    mutating func applyLocalHintAndEval(hintDepth: Int, moveEvaluationEnabled: Bool) {
        self.chessHintDepth = min(18, max(1, hintDepth))
        self.chessEvaluateMove = moveEvaluationEnabled
    }

    var chessHintDepth: Int
    var chessEvaluateMove: Bool
    var chessHintLimit: Int
    var chessHintAwardBest: Bool
    var chessHintAwardGood: Bool
    var chessHintAwardCapture: Bool
    var chessShowHintStats: Bool
    var chessBotLedTargetOnlyAfterLift: Bool
    var chessTutorialsEnabled: Bool
    var chess_confirm_new_game: Bool
    var botSettings: BotSettingsPayload

    static let webDefaults = BoardUIPrefsPayload(
        chessHintDepth: 10,
        chessEvaluateMove: false,
        chessHintLimit: 0,
        chessHintAwardBest: true,
        chessHintAwardGood: false,
        chessHintAwardCapture: false,
        chessShowHintStats: false,
        chessBotLedTargetOnlyAfterLift: false,
        chessTutorialsEnabled: false,
        chess_confirm_new_game: false,
        botSettings: .init()
    )

    enum CodingKeys: String, CodingKey {
        case chessHintDepth
        case chessEvaluateMove
        case chessHintLimit
        case chessHintAwardBest
        case chessHintAwardGood
        case chessHintAwardCapture
        case chessShowHintStats
        case chessBotLedTargetOnlyAfterLift
        case chessTutorialsEnabled
        case chess_confirm_new_game
        case botSettings
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        chessHintDepth = try c.decodeIfPresent(Int.self, forKey: .chessHintDepth) ?? Self.webDefaults.chessHintDepth
        chessEvaluateMove = try c.decodeIfPresent(Bool.self, forKey: .chessEvaluateMove) ?? Self.webDefaults.chessEvaluateMove
        chessHintLimit = try c.decodeIfPresent(Int.self, forKey: .chessHintLimit) ?? Self.webDefaults.chessHintLimit
        chessHintAwardBest = try c.decodeIfPresent(Bool.self, forKey: .chessHintAwardBest) ?? Self.webDefaults.chessHintAwardBest
        chessHintAwardGood = try c.decodeIfPresent(Bool.self, forKey: .chessHintAwardGood) ?? Self.webDefaults.chessHintAwardGood
        chessHintAwardCapture = try c.decodeIfPresent(Bool.self, forKey: .chessHintAwardCapture) ?? Self.webDefaults.chessHintAwardCapture
        chessShowHintStats = try c.decodeIfPresent(Bool.self, forKey: .chessShowHintStats) ?? Self.webDefaults.chessShowHintStats
        chessBotLedTargetOnlyAfterLift =
            try c.decodeIfPresent(Bool.self, forKey: .chessBotLedTargetOnlyAfterLift) ?? Self.webDefaults.chessBotLedTargetOnlyAfterLift
        chessTutorialsEnabled = try c.decodeIfPresent(Bool.self, forKey: .chessTutorialsEnabled) ?? Self.webDefaults.chessTutorialsEnabled
        chess_confirm_new_game =
            try c.decodeIfPresent(Bool.self, forKey: .chess_confirm_new_game) ?? Self.webDefaults.chess_confirm_new_game
        botSettings = try c.decodeIfPresent(BotSettingsPayload.self, forKey: .botSettings) ?? .init()
    }

    init(
        chessHintDepth: Int,
        chessEvaluateMove: Bool,
        chessHintLimit: Int,
        chessHintAwardBest: Bool,
        chessHintAwardGood: Bool,
        chessHintAwardCapture: Bool,
        chessShowHintStats: Bool,
        chessBotLedTargetOnlyAfterLift: Bool,
        chessTutorialsEnabled: Bool,
        chess_confirm_new_game: Bool,
        botSettings: BotSettingsPayload
    ) {
        self.chessHintDepth = min(18, max(1, chessHintDepth))
        self.chessEvaluateMove = chessEvaluateMove
        self.chessHintLimit = max(0, chessHintLimit)
        self.chessHintAwardBest = chessHintAwardBest
        self.chessHintAwardGood = chessHintAwardGood
        self.chessHintAwardCapture = chessHintAwardCapture
        self.chessShowHintStats = chessShowHintStats
        self.chessBotLedTargetOnlyAfterLift = chessBotLedTargetOnlyAfterLift
        self.chessTutorialsEnabled = chessTutorialsEnabled
        self.chess_confirm_new_game = chess_confirm_new_game
        self.botSettings = botSettings
    }

    init() {
        self = Self.webDefaults
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(chessHintDepth, forKey: .chessHintDepth)
        try c.encode(chessEvaluateMove, forKey: .chessEvaluateMove)
        try c.encode(chessHintLimit, forKey: .chessHintLimit)
        try c.encode(chessHintAwardBest, forKey: .chessHintAwardBest)
        try c.encode(chessHintAwardGood, forKey: .chessHintAwardGood)
        try c.encode(chessHintAwardCapture, forKey: .chessHintAwardCapture)
        try c.encode(chessShowHintStats, forKey: .chessShowHintStats)
        try c.encode(chessBotLedTargetOnlyAfterLift, forKey: .chessBotLedTargetOnlyAfterLift)
        try c.encode(chessTutorialsEnabled, forKey: .chessTutorialsEnabled)
        try c.encode(chess_confirm_new_game, forKey: .chess_confirm_new_game)
        try c.encode(botSettings, forKey: .botSettings)
    }
}

struct BotSettingsPayload: Codable, Sendable, Equatable {
    var mode: String
    var strength: String
    var side: String

    init(mode: String = "pvp", strength: String = "10", side: String = "white") {
        self.mode = mode
        self.strength = strength
        self.side = side
    }

    var isBotMode: Bool { mode.lowercased() == "bot" }

    func botSideIsWhite() -> Bool {
        side.lowercased() == "white"
    }
}

// MARK: - MQTT / Demo (REST)

struct ESPMQTTStatusJSON: Codable, Sendable {
    let host: String
    let port: Int
    let username: String
    /// Firmware vrací "***" místo hesla.
    let password: String
    let wifiConnected: Bool
    let mqttConnected: Bool
    let mode: String
}

struct ESPDemoStatusJSON: Codable, Sendable {
    let enabled: Bool
}
