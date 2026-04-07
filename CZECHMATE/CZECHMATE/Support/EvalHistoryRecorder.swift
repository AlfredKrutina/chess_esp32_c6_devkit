//
//  EvalHistoryRecorder.swift
//  Lokální historie evalů (z nápovědy) pro jednoduchý graf — UserDefaults + Codable.
//

import Foundation

struct EvalHistoryPoint: Codable, Identifiable {
    var id: UUID
    var recordedAt: Date
    var evalPawns: Double

    init(id: UUID = UUID(), recordedAt: Date = Date(), evalPawns: Double) {
        self.id = id
        self.recordedAt = recordedAt
        self.evalPawns = evalPawns
    }
}

enum EvalHistoryRecorder {
    private static let key = "czechmate.evalHistory.v1"

    static func append(evalPawns: Double) {
        var pts = load()
        pts.append(EvalHistoryPoint(evalPawns: evalPawns))
        if pts.count > 300 {
            pts.removeFirst(pts.count - 300)
        }
        if let data = try? JSONEncoder().encode(pts) {
            UserDefaults.standard.set(data, forKey: key)
        }
    }

    static func load() -> [EvalHistoryPoint] {
        guard let data = UserDefaults.standard.data(forKey: key),
              let pts = try? JSONDecoder().decode([EvalHistoryPoint].self, from: data) else {
            return []
        }
        return pts
    }

    static func clear() {
        UserDefaults.standard.removeObject(forKey: key)
    }
}
