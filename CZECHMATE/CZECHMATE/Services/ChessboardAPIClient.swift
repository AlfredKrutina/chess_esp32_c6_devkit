//
//  ChessboardAPIClient.swift
//  CZECHMATE
//

import Foundation

/// Sdílená konfigurace pro HTTP i WebSocket k ESP v LAN (vč. `.local` / mDNS).
enum BoardLANURLSessionFactory {
    static func makeSession() -> URLSession {
        let cfg = URLSessionConfiguration.default
        // `.local` může při prvním dotazu déle řešit DNS; příliš krátký limit dává falešné -1001.
        cfg.timeoutIntervalForRequest = 35
        cfg.timeoutIntervalForResource = 120
        // U domácí Wi‑Fi nechceme čekat na „obnovu cesty“ — raději rychle selhat a znovu zkusit v poll smyčce.
        cfg.waitsForConnectivity = false
        // Na hotspotu ESP bez internetu iOS často označí Wi‑Fi jako „bez přístupu“ a zkusí LTE fallback
        // (`nw_endpoint_fallback` u lokální IP) → timeouty k 192.168.4.1. Deska musí jít jen přes Wi‑Fi.
        cfg.allowsCellularAccess = false
        cfg.multipathServiceType = .none
        return URLSession(configuration: cfg)
    }
}

/// Klient REST API ESP32; snapshot lze také přijímat přes WebSocket (`ChessboardWebSocketClient`).
actor ChessboardAPIClient {
    /// Sdíleno s `ChessboardAPIClient+Extras` (stejný modul).
    nonisolated let session: URLSession
    private var baseURL: URL

    init(baseURL: URL = URL(string: "http://192.168.4.1")!, session: URLSession? = nil) {
        self.baseURL = baseURL
        if let session {
            self.session = session
        } else {
            self.session = BoardLANURLSessionFactory.makeSession()
        }
    }

    func setBaseURL(_ url: URL) {
        baseURL = url
    }

    func currentBaseURL() -> URL {
        baseURL
    }

    /// `GET /api/game/snapshot`. Pro `If-None-Match` použij `fetchSnapshot(ifNoneMatch:)`.
    func fetchSnapshot() async throws -> GameSnapshot {
        guard let snap = try await fetchSnapshot(ifNoneMatch: nil) else {
            throw ChessboardAPIError.invalidResponse
        }
        return snap
    }

    /// `GET /api/game/snapshot` s volitelným `If-None-Match` (stejné jako ETag / `state_version`).
    /// Vrátí `nil` při **304 Not Modified**.
    func fetchSnapshot(ifNoneMatch: String?) async throws -> GameSnapshot? {
        let url = apiURL(path: "api/game/snapshot")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        if let tag = ifNoneMatch {
            req.setValue(tag, forHTTPHeaderField: "If-None-Match")
        }
        AppDebugLog.staging("GET \(url.absoluteString) ifNoneMatch=\(ifNoneMatch ?? "nil")")
        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await session.data(for: req)
        } catch {
            if url.host?.hasPrefix("192.168.4.") == true,
               let e = error as NSError?, e.domain == NSURLErrorDomain, e.code == NSURLErrorTimedOut {
                AppDebugLog.staging(
                    "HTTP hint: timeout k ESP AP — zkus Safari http://192.168.4.1; u sítě CZECHMATE vypni „Soukromou Wi‑Fi adresu“ a VPN; deska musí odpovídat na portu 80."
                )
            }
            AppDebugLog.stagingError("HTTP GET snapshot", error)
            throw error
        }
        guard let http = response as? HTTPURLResponse else {
            AppDebugLog.staging("HTTP snapshot: missing HTTPURLResponse")
            throw ChessboardAPIError.invalidResponse
        }
        if http.statusCode == 304 {
            return nil
        }
        try validateHTTP(response: response, data: data)
        return try decodeSnapshot(data)
    }

    /// `POST /api/game/hint_highlight` — body `{ "from": "e2", "to": "e4" }` (from volitelné na MCU, ale posíláme obě).
    func postHintHighlight(from: String, to: String) async throws {
        let url = apiURL(path: "api/game/hint_highlight")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let body: [String: String] = [
            "from": from.lowercased(),
            "to": to.lowercased(),
        ]
        req.httpBody = try JSONSerialization.data(withJSONObject: body)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    /// `POST /api/game/hint_clear` — zhasne hint LED (jako web).
    func postHintClear() async throws {
        let url = apiURL(path: "api/game/hint_clear")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
    }

    /// `POST /api/settings/brightness` — tělo `{"brightness": 0…100}` (jako webové UI).
    func postBrightness(percent: Int) async throws {
        let v = min(100, max(0, percent))
        let url = apiURL(path: "api/settings/brightness")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let body = #"{"brightness": \#(v)}"#
        req.httpBody = Data(body.utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
    }

    private func apiURL(path: String) -> URL {
        baseURL.appendingPathComponent(path)
    }

    /// Dekóduje tělo `GET /api/game/snapshot` / WS.
    private nonisolated func decodeSnapshot(_ data: Data) throws -> GameSnapshot {
        do {
            return try GameSnapshot.decodeFromBoardDataRepairingAndNormalizing(data)
        } catch {
            let fixed = GameJSONRepair.repairStatusDataIfNeeded(data)
            let preview = String(data: fixed.prefix(240), encoding: .utf8) ?? ""
            AppDebugLog.staging("JSON decode GameSnapshot failed: \(error) preview=\(preview)")
            throw ChessboardAPIError.decodeFailed(error.localizedDescription)
        }
    }

    /// Z JSON těla chyby od ESP (`message` / `error`) — stejný záměr jako BLE `cmd_ack.message`.
    private nonisolated static func extractESPJSONErrorMessage(from data: Data) -> String? {
        guard !data.isEmpty,
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else { return nil }
        if let m = obj["message"] as? String {
            let t = m.trimmingCharacters(in: .whitespacesAndNewlines)
            if !t.isEmpty { return t }
        }
        if let e = obj["error"] as? String {
            let t = e.trimmingCharacters(in: .whitespacesAndNewlines)
            if !t.isEmpty { return t }
        }
        return nil
    }

    nonisolated func validateHTTP(response: URLResponse, data: Data, treat403AsWebLock: Bool = false) throws {
        guard let http = response as? HTTPURLResponse else {
            throw ChessboardAPIError.invalidResponse
        }
        if http.statusCode == 403, treat403AsWebLock {
            AppDebugLog.staging("HTTP 403 — web locked")
            throw ChessboardAPIError.webLocked
        }
        guard (200 ... 299).contains(http.statusCode) else {
            let text = String(data: data, encoding: .utf8)
            let detail = Self.extractESPJSONErrorMessage(from: data)
                ?? text.map { String($0.prefix(200)) }
            let logLine = detail ?? ""
            AppDebugLog.staging("HTTP \(http.statusCode) — \(logLine)")
            throw ChessboardAPIError.httpStatus(http.statusCode, detail ?? text)
        }
    }
}
