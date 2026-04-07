//
//  GameSessionTracker.swift
//  Začátek/konec relace při připojení k desce (SwiftData).
//

import Foundation
import SwiftData

@MainActor
enum GameSessionTracker {
    private static var activeSessionID: UUID?

    static func sessionStarted(linkKind: String, modelContext: ModelContext) {
        let e = GameSessionEntity(linkKind: linkKind, peakMoveCount: 0)
        modelContext.insert(e)
        activeSessionID = e.id
        try? modelContext.save()
    }

    static func updatePeakMoveCount(_ n: Int, modelContext: ModelContext) {
        guard let id = activeSessionID else { return }
        let fd = FetchDescriptor<GameSessionEntity>()
        guard let all = try? modelContext.fetch(fd),
              let s = all.first(where: { $0.id == id }) else { return }
        if n > s.peakMoveCount {
            s.peakMoveCount = n
            try? modelContext.save()
        }
    }

    static func sessionEnded(modelContext: ModelContext) {
        guard let id = activeSessionID else { return }
        let fd = FetchDescriptor<GameSessionEntity>()
        if let s = try? modelContext.fetch(fd).first(where: { $0.id == id }) {
            s.endedAt = Date()
            try? modelContext.save()
        }
        activeSessionID = nil
    }
}
