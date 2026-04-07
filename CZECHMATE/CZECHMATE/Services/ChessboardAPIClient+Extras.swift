//
//  ChessboardAPIClient+Extras.swift
//  Timer, tahy, nová hra, virtual_action, WiFi — parita s webovým ESP UI.
//
//  convertFromSnakeCase: jen pro BoardTimerHTTPState / ESPWiFiStatusJSON.
//  GameSnapshot vždy přes JSONDecoder.forGameSnapshot().
//

import Foundation

extension ChessboardAPIClient {
    func fetchTimerState() async throws -> BoardTimerHTTPState {
        let url = currentBaseURL().appendingPathComponent("api/timer")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
        let dec = JSONDecoder()
        dec.keyDecodingStrategy = .convertFromSnakeCase
        return try dec.decode(BoardTimerHTTPState.self, from: data)
    }

    /// `type` 0…14, u 14 vyplnit custom minuty a increment.
    func postTimerConfig(type: Int, customMinutes: Int?, customIncrement: Int?) async throws {
        let url = currentBaseURL().appendingPathComponent("api/timer/config")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let body: String
        if type == 14, let m = customMinutes, let inc = customIncrement {
            body = #"{"type":14,"custom_minutes":\#(m),"custom_increment":\#(inc)}"#
        } else {
            body = #"{"type":\#(type)}"#
        }
        req.httpBody = body.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postTimerPause() async throws {
        try await postTimerSimple(path: "api/timer/pause")
    }

    func postTimerResume() async throws {
        try await postTimerSimple(path: "api/timer/resume")
    }

    func postTimerReset() async throws {
        try await postTimerSimple(path: "api/timer/reset")
    }

    private func postTimerSimple(path: String) async throws {
        let url = currentBaseURL().appendingPathComponent(path)
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    /// Nová hra. Volitelný `fen` — pokud firmware podporuje, nastaví výchozí pozici.
    func postNewGame(fen: String? = nil) async throws {
        let url = currentBaseURL().appendingPathComponent("api/game/new")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        if let fen {
            req.setValue("application/json", forHTTPHeaderField: "Content-Type")
            let body: [String: Any] = ["fen": fen]
            req.httpBody = try JSONSerialization.data(withJSONObject: body)
        }
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postVirtualAction(action: String, square: String?, choice: String?) async throws {
        let url = currentBaseURL().appendingPathComponent("api/game/virtual_action")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        var parts: [String] = ["\"action\":\"\(action)\""]
        if let square {
            parts.append("\"square\":\"\(square.lowercased())\"")
        }
        if let choice {
            parts.append("\"choice\":\"\(choice.uppercased())\"")
        }
        let body = "{" + parts.joined(separator: ",") + "}"
        req.httpBody = body.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postGameMove(from: String, to: String, promotion: String?) async throws {
        let url = currentBaseURL().appendingPathComponent("api/move")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        var parts = [
            "\"from\":\"\(from.lowercased())\"",
            "\"to\":\"\(to.lowercased())\"",
        ]
        if let promotion {
            parts.append("\"promotion\":\"\(promotion.uppercased())\"")
        }
        req.httpBody = ("{" + parts.joined(separator: ",") + "}").data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        if let http = response as? HTTPURLResponse, http.statusCode == 403 {
            throw ChessboardAPIError.webLocked
        }
        if let http = response as? HTTPURLResponse, http.statusCode == 400 {
            if let msg = try? JSONDecoder().decode(ESPJSONSuccessMessage.self, from: data).message {
                throw ChessboardAPIError.httpStatus(400, msg)
            }
        }
        try validateHTTP(response: response, data: data, treat403AsWebLock: false)
    }

    func fetchWiFiStatus() async throws -> ESPWiFiStatusJSON {
        let url = currentBaseURL().appendingPathComponent("api/wifi/status")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
        let dec = JSONDecoder()
        dec.keyDecodingStrategy = .convertFromSnakeCase
        return try dec.decode(ESPWiFiStatusJSON.self, from: data)
    }

    func postWiFiConfig(ssid: String, password: String) async throws {
        let url = currentBaseURL().appendingPathComponent("api/wifi/config")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let escS = ssid.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
        let escP = password.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
        let body = #"{"ssid":"\#(escS)","password":"\#(escP)"}"#
        req.httpBody = body.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postWiFiConnect() async throws {
        try await postWiFiSimple(path: "api/wifi/connect")
    }

    func postWiFiDisconnect() async throws {
        try await postWiFiSimple(path: "api/wifi/disconnect")
    }

    func postWiFiClear() async throws {
        try await postWiFiSimple(path: "api/wifi/clear")
    }

    private func postWiFiSimple(path: String) async throws {
        let url = currentBaseURL().appendingPathComponent(path)
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }
}
