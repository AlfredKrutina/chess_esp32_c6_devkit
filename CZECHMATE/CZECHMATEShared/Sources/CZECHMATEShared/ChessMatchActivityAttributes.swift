//
//  ChessMatchActivityAttributes.swift
//  Sdílené ActivityAttributes (iOS app + Widget extension).
//

#if os(iOS)
import ActivityKit

public struct ChessMatchAttributes: ActivityAttributes {
    public struct ContentState: Codable, Hashable, Sendable {
        public var moveCount: Int
        public var currentPlayer: String
        public var whiteTimeSeconds: UInt32?
        public var blackTimeSeconds: UInt32?
        public var subtitle: String
        public var inCheck: Bool
        public var gameEnded: Bool
        /// Krátký text o materiálu (např. „Navrch bílý (+2)“).
        public var advantageSummary: String
        /// Zda běží šachové hodiny (zobrazit odpočty v UI).
        public var timerActive: Bool

        public init(
            moveCount: Int,
            currentPlayer: String,
            whiteTimeSeconds: UInt32?,
            blackTimeSeconds: UInt32?,
            subtitle: String,
            inCheck: Bool,
            gameEnded: Bool,
            advantageSummary: String = "",
            timerActive: Bool = false
        ) {
            self.moveCount = moveCount
            self.currentPlayer = currentPlayer
            self.whiteTimeSeconds = whiteTimeSeconds
            self.blackTimeSeconds = blackTimeSeconds
            self.subtitle = subtitle
            self.inCheck = inCheck
            self.gameEnded = gameEnded
            self.advantageSummary = advantageSummary
            self.timerActive = timerActive
        }
    }

    public var matchId: String

    public init(matchId: String) {
        self.matchId = matchId
    }
}
#endif
