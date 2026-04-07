//
//  StockfishAPIClient.swift
//  CZECHMATE
//
//  Kompatibilní s chess-api.com v1 (viz chess_app.js — fetchStockfishBestMove).
//

import Foundation

struct StockfishBestMove: Sendable {
    let from: String
    let to: String
    /// Eval ve „pěšácích“ z perspektivy API (bílý + / černý −), jako na webu.
    let evalPawns: Double?
    let text: String?
}

enum StockfishAPIError: Error, LocalizedError, Sendable {
    case invalidURL
    case badHTTP(Int)
    case invalidJSON
    case missingMove

    var errorDescription: String? {
        switch self {
        case .invalidURL: return "Neplatná URL Stockfish API."
        case .badHTTP(let c): return "Stockfish API HTTP \(c)."
        case .invalidJSON: return "Neplatná JSON odpověď od Stockfish API."
        case .missingMove: return "Odpověď neobsahuje tah (from/to ani move)."
        }
    }
}

actor StockfishAPIClient {
    private nonisolated let session: URLSession
    private let apiURL: URL

    init(session: URLSession = .shared, apiURL: URL? = nil) {
        self.session = session
        if let apiURL {
            self.apiURL = apiURL
        } else {
            guard let u = URL(string: "https://chess-api.com/v1") else {
                fatalError("chess-api base URL")
            }
            self.apiURL = u
        }
    }

    /// Hloubka 1–18 jako na webu (hint).
    func fetchBestMove(fen: String, depth: Int = 10) async throws -> StockfishBestMove {
        let d = min(18, max(1, depth))
        var req = URLRequest(url: apiURL)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.timeoutInterval = 15
        let body = ["fen": fen, "depth": d] as [String: Any]
        req.httpBody = try JSONSerialization.data(withJSONObject: body)

        #if DEBUG
        print("[staging] POST chess-api depth=\(d) fenLen=\(fen.count)")
        #endif
        let (data, response) = try await session.data(for: req)
        guard let http = response as? HTTPURLResponse else { throw StockfishAPIError.invalidURL }
        guard (200 ... 299).contains(http.statusCode) else { throw StockfishAPIError.badHTTP(http.statusCode) }

        guard let obj = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw StockfishAPIError.invalidJSON
        }

        let dataObj = (obj["data"] as? [String: Any])
            ?? (obj["result"] as? [String: Any])
            ?? (obj["bestMove"] as? [String: Any])
            ?? obj

        var from = dataObj["from"] as? String
        var to = dataObj["to"] as? String
        if from == nil || to == nil, let move = dataObj["move"] as? String, move.count >= 4 {
            let m = move.lowercased()
            from = String(m.prefix(2))
            to = String(m.dropFirst(2).prefix(2))
        }
        guard let f = from, let t = to, f.count == 2, t.count == 2 else {
            throw StockfishAPIError.missingMove
        }

        let evalPawns = parseEval(from: dataObj) ?? parseEval(from: obj)
        let text = dataObj["text"] as? String

        return StockfishBestMove(
            from: f.lowercased(),
            to: t.lowercased(),
            evalPawns: evalPawns,
            text: text
        )
    }

    /// Druhá varianta heuristicky: nižší hloubina může dát jiný nejlepší tah (chess-api často v jedné linii).
    func fetchPrimaryAndSecondary(fen: String, depth: Int) async throws -> (primary: StockfishBestMove, secondary: StockfishBestMove?) {
        let primary = try await fetchBestMove(fen: fen, depth: depth)
        let d2 = max(1, min(depth - 3, 18))
        guard d2 != depth else { return (primary, nil) }
        let alt = try? await fetchBestMove(fen: fen, depth: d2)
        guard let alt else { return (primary, nil) }
        if alt.from == primary.from && alt.to == primary.to { return (primary, nil) }
        return (primary, alt)
    }
}

private nonisolated func parseEval(from obj: [String: Any]) -> Double? {
    if let e = obj["eval"] as? Double { return e }
    if let e = obj["eval"] as? Int { return Double(e) }
    if let cp = obj["centipawns"] as? Int { return Double(cp) / 100.0 }
    if let cp = obj["centipawns"] as? Double { return cp / 100.0 }
    if let sc = obj["score"] as? Double { return sc }
    if let ev = (obj["evaluation"] as? [String: Any])?["eval"] as? Double { return ev }
    return nil
}
