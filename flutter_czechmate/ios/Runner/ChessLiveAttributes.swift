// KEEP IN SYNC: ios/ChessLiveActivityExtension/ChessLiveAttributes.swift (must match for ActivityKit).
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
      phase: String
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
    }
  }

  public var matchTitle: String

  public init(matchTitle: String) {
    self.matchTitle = matchTitle
  }
}
#endif
