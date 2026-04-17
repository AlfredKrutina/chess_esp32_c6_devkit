//  FENBuilder+Compat.swift
//  Compatibility overload for FENBuilder to accept history parameter.

import Foundation
import CZECHMATEShared

extension FENBuilder {
    /// Backward-compatible overload that accepts full move history and forwards
    /// to the new API that takes a half-move count.
    static func boardAndStatusToFEN(
        board: [[String]],
        status: GameStatus,
        history: MoveHistory
    ) -> String? {
        return boardAndStatusToFEN(
            board: board,
            status: status,
            halfmoveCount: history.moves.count
        )
    }
}
