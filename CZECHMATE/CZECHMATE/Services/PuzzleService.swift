//
//  PuzzleService.swift
//  Lichess veřejné API — denní puzzle (FEN, řešení, rating).
//

import Foundation

/// Stažená denní úloha z Lichess (bez surového JSONu v UI).
struct DailyPuzzle: Sendable, Equatable {
    let id: String
    let fen: String
    let rating: Int?
    /// UCI tahy (např. "e2e4").
    let solutionMoves: [String]
    let themes: [String]
}

enum PuzzleServiceError: LocalizedError {
    case invalidHTTP
    case missingFen
    case decodingFailed

    var errorDescription: String? {
        switch self {
        case .invalidHTTP: return "Neplatná odpověď serveru."
        case .missingFen: return "V odpovědi chybí pozice (FEN)."
        case .decodingFailed: return "Nepodařilo se zpravit data puzzle."
        }
    }
}

/// Načítání puzzlů přes `URLSession` (Lichess public API).
enum PuzzleService {
    private static let dailyURL = URL(string: "https://lichess.org/api/puzzle/daily")!

    private struct LichessDailyResponse: Decodable {
        let puzzle: PuzzlePayload
    }

    private struct PuzzlePayload: Decodable {
        let id: String
        let rating: Int?
        let fen: String?
        let solution: [String]?
        let themes: [String]?
    }

    /// `GET https://lichess.org/api/puzzle/daily`
    static func fetchDailyPuzzle(using session: URLSession = .shared) async throws -> DailyPuzzle {
        var req = URLRequest(url: dailyURL)
        req.timeoutInterval = 25
        req.setValue("application/json", forHTTPHeaderField: "Accept")
        let (data, response) = try await session.data(for: req)
        guard let http = response as? HTTPURLResponse, (200 ... 299).contains(http.statusCode) else {
            throw PuzzleServiceError.invalidHTTP
        }
        let decoded: LichessDailyResponse
        do {
            decoded = try JSONDecoder().decode(LichessDailyResponse.self, from: data)
        } catch {
            throw PuzzleServiceError.decodingFailed
        }
        guard let fen = decoded.puzzle.fen?.trimmingCharacters(in: .whitespacesAndNewlines), !fen.isEmpty else {
            throw PuzzleServiceError.missingFen
        }
        return DailyPuzzle(
            id: decoded.puzzle.id,
            fen: fen,
            rating: decoded.puzzle.rating,
            solutionMoves: decoded.puzzle.solution ?? [],
            themes: decoded.puzzle.themes ?? []
        )
    }
}
