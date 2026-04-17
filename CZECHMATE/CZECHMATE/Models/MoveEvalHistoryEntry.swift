//
//  MoveEvalHistoryEntry.swift
//  CZECHMATE — poslední evaluace tahů (Stockfish) pro graf v souhrnu partie.
//

import Foundation

struct MoveEvalHistoryEntry: Identifiable, Sendable, Equatable {
    var id: Int { moveIndex1Based }
    /// Počet tahů v historii po zahrání (1…n).
    let moveIndex1Based: Int
    /// Eval pozice po tahu z perspektivy bílého (pány); nil = neznámé.
    let evalWhitePawns: Double?
    let grade: MoveGrade
    /// Zhoršení v pěšcích pro stranu, která táhla (>= 0).
    let lossPawns: Double?
}
