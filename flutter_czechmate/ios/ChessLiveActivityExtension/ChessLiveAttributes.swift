// KEEP IN SYNC: ios/Runner/ChessLiveAttributes.swift (must match for ActivityKit).
import Foundation

#if canImport(ActivityKit)
import ActivityKit

public struct ChessLiveAttributes: ActivityAttributes {
  public struct ContentState: Codable, Hashable {
    public var whiteTimeMs: Int
    public var blackTimeMs: Int
    public var isWhiteTurn: Bool
    public var timerRunning: Bool
    public var gamePaused: Bool
    public var timeExpired: Bool
    public var transport: String
    public var totalMoves: Int
    public var timeControlLabel: String
    public var gameFinished: Bool
    public var phase: String
    /// Low-time threshold for the side on move (10 % of base time, 10–60 s).
    public var lowTimeMs: Int
    /// Epoch ms when the iPhone received these clock values; the running side counts down from it.
    public var clockAnchorMs: Int
    /// Board link as the iPhone game screen sees it: `live` | `connecting` | `offline`.
    public var link: String
    /// App language (`cs` | `en`); empty means device language.
    public var lang: String

    public init(
      whiteTimeMs: Int,
      blackTimeMs: Int,
      isWhiteTurn: Bool,
      timerRunning: Bool,
      gamePaused: Bool,
      timeExpired: Bool,
      transport: String,
      totalMoves: Int,
      timeControlLabel: String,
      gameFinished: Bool,
      phase: String,
      lowTimeMs: Int,
      clockAnchorMs: Int,
      link: String,
      lang: String
    ) {
      self.whiteTimeMs = whiteTimeMs
      self.blackTimeMs = blackTimeMs
      self.isWhiteTurn = isWhiteTurn
      self.timerRunning = timerRunning
      self.gamePaused = gamePaused
      self.timeExpired = timeExpired
      self.transport = transport
      self.totalMoves = totalMoves
      self.timeControlLabel = timeControlLabel
      self.gameFinished = gameFinished
      self.phase = phase
      self.lowTimeMs = lowTimeMs
      self.clockAnchorMs = clockAnchorMs
      self.link = link
      self.lang = lang
    }
  }

  public var matchTitle: String

  public init(matchTitle: String) {
    self.matchTitle = matchTitle
  }
}
#endif
