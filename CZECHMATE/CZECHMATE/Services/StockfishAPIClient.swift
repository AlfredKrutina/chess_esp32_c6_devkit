//
//  StockfishAPIClient.swift
//  CZECHMATE
//
//  Kompatibilní s chess-api.com v1 (viz chess_app.js — fetchStockfishBestMove).
//

import Foundation

/// Session vhodná pro chess-api přes mobilní síť (explicitně povolí cellular / expensive / constrained).
private let stockfishChessAPISession: URLSession = {
    let cfg = URLSessionConfiguration.ephemeral
    cfg.allowsCellularAccess = true
    cfg.allowsExpensiveNetworkAccess = true
    cfg.allowsConstrainedNetworkAccess = true
    cfg.waitsForConnectivity = true
    cfg.timeoutIntervalForRequest = 20
    cfg.timeoutIntervalForResource = 30
    return URLSession(configuration: cfg)
}()

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
    /// Jednou za život procesu: lehká kontrola dostupnosti hostitele před prvním POST (staging log v konzoli).
    private var ranReachabilityProbeThisProcess = false

    /// - Parameter session: výchozí je dedikovaná session s povolenými mobilními daty; pro testy lze předat vlastní.
    init(session: URLSession? = nil, apiURL: URL? = nil) {
        self.session = session ?? stockfishChessAPISession
        if let apiURL {
            self.apiURL = apiURL
        } else {
            guard let u = URL(string: "https://chess-api.com/v1") else {
                fatalError("chess-api base URL")
            }
            self.apiURL = u
        }
    }

    /// HEAD na kořen domény, při chybě krátký GET na `apiURL` — jen `[staging]` výpis, neblokuje POST.
    private func runReachabilityProbeOnceIfNeeded() async {
        guard !ranReachabilityProbeThisProcess else { return }
        ranReachabilityProbeThisProcess = true
        if let root = URL(string: "https://chess-api.com/") {
            var headReq = URLRequest(url: root)
            headReq.httpMethod = "HEAD"
            headReq.timeoutInterval = 5
            do {
                let (_, resp) = try await session.data(for: headReq)
                let code = (resp as? HTTPURLResponse)?.statusCode ?? -1
                AppDebugLog.staging("chess-api reachability: HEAD / → HTTP \(code)")
                return
            } catch {
                AppDebugLog.stagingError("chess-api reachability HEAD /", error)
            }
        }
        var getReq = URLRequest(url: apiURL)
        getReq.httpMethod = "GET"
        getReq.timeoutInterval = 5
        do {
            let (_, resp) = try await session.data(for: getReq)
            let code = (resp as? HTTPURLResponse)?.statusCode ?? -1
            AppDebugLog.staging("chess-api reachability: GET \(apiURL.absoluteString) → HTTP \(code)")
        } catch {
            AppDebugLog.stagingError("chess-api reachability GET v1", error)
        }
    }

    /// Hloubka 1–18 jako na webu (hint).
    func fetchBestMove(fen: String, depth: Int = 10) async throws -> StockfishBestMove {
        await runReachabilityProbeOnceIfNeeded()
        let d = min(18, max(1, depth))
        var req = URLRequest(url: apiURL)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue("application/json", forHTTPHeaderField: "Accept")
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
        if from == nil || to == nil, let move = dataObj["move"] as? String {
            let m = move.lowercased().filter { $0.isLetter || $0.isNumber }
            if m.count >= 4 {
                from = from ?? String(m.prefix(2))
                to = to ?? String(m.dropFirst(2).prefix(2))
            }
        }
        guard let f = from, let t = to, f.count == 2, t.count == 2 else {
            throw StockfishAPIError.missingMove
        }

        let evalPawns = parseEval(from: dataObj) ?? parseEval(from: obj) ?? nestedResultEval(from: dataObj) ?? nestedResultEval(from: obj)
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

private nonisolated func doubleFromJSON(_ v: Any?) -> Double? {
    switch v {
    case let d as Double: return d
    case let i as Int: return Double(i)
    case let n as NSNumber: return n.doubleValue
    case let s as String:
        let t = s.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return nil }
        return Double(t.replacingOccurrences(of: ",", with: "."))
    default: return nil
    }
}

/// Parsování evaluace v souladu s `parseEvalFromApiObject` ve `chess_app.js` (chess-api.com).
private nonisolated func parseEval(from obj: [String: Any]) -> Double? {
    if let e = doubleFromJSON(obj["eval"]) { return e }
    if let cp = doubleFromJSON(obj["centipawns"]) { return cp / 100.0 }
    if let cp = doubleFromJSON(obj["cp"]) { return cp / 100.0 }
    if let sc = doubleFromJSON(obj["score"]) { return sc }
    if let ev = obj["evaluation"] as? [String: Any], let e = doubleFromJSON(ev["eval"]) { return e }
    if let e = doubleFromJSON(obj["evaluation"]) { return e }
    return nil
}

private nonisolated func nestedResultEval(from obj: [String: Any]) -> Double? {
    guard let res = obj["result"] as? [String: Any] else { return nil }
    return parseEval(from: res)
}
