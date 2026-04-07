//
//  CzechmateSwiftDataModels.swift
//  Puzzle knihovna, herní relace, historie evalů pro graf.
//

import Foundation
import SwiftData

@Model
final class PuzzleLibraryItem {
    var id: UUID
    var createdAt: Date
    var title: String
    var fen: String
    /// UCI řetězec řešení nebo prázdné
    var solutionUCI: String
    var source: String
    var notes: String

    init(
        id: UUID = UUID(),
        createdAt: Date = Date(),
        title: String,
        fen: String,
        solutionUCI: String = "",
        source: String = "ruční",
        notes: String = ""
    ) {
        self.id = id
        self.createdAt = createdAt
        self.title = title
        self.fen = fen
        self.solutionUCI = solutionUCI
        self.source = source
        self.notes = notes
    }
}

@Model
final class GameSessionEntity {
    var id: UUID
    var startedAt: Date
    var endedAt: Date?
    /// wifi / ble / mock
    var linkKind: String
    var peakMoveCount: Int

    init(
        id: UUID = UUID(),
        startedAt: Date = Date(),
        endedAt: Date? = nil,
        linkKind: String,
        peakMoveCount: Int = 0
    ) {
        self.id = id
        self.startedAt = startedAt
        self.endedAt = endedAt
        self.linkKind = linkKind
        self.peakMoveCount = peakMoveCount
    }
}
