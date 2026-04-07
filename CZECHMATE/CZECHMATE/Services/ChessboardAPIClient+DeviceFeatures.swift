//
//  ChessboardAPIClient+DeviceFeatures.swift
//  /api/settings/ui, MQTT, demo, lampa, LED guidance — parita s webovým ESP UI.
//

import Foundation

extension ChessboardAPIClient {
    // MARK: - UI prefs (NVS)

    func fetchUISettingsEnvelope() async throws -> BoardUISettingsEnvelope {
        let url = currentBaseURL().appendingPathComponent("api/settings/ui")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
        let dec = JSONDecoder()
        return try dec.decode(BoardUISettingsEnvelope.self, from: data)
    }

    /// Uloží stejný formát jako web (`version` + `prefs`).
    func postUISettingsEnvelope(_ envelope: BoardUISettingsEnvelope) async throws {
        let url = currentBaseURL().appendingPathComponent("api/settings/ui")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let enc = JSONEncoder()
        enc.outputFormatting = [.sortedKeys]
        req.httpBody = try enc.encode(envelope)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    // MARK: - LED / lamp

    func postAutoLampTimeout(seconds: Int) async throws {
        let v = min(7200, max(5, seconds))
        let url = currentBaseURL().appendingPathComponent("api/settings/auto_lamp_timeout")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = #"{"seconds": \#(v)}"#.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postGuidedHints(enabled: Bool) async throws {
        let url = currentBaseURL().appendingPathComponent("api/settings/guided_hints")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let body = #"{"enabled": \#(enabled ? "true" : "false")}"#
        req.httpBody = body.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postLedGuidance(level: Int) async throws {
        let v = min(5, max(1, level))
        let url = currentBaseURL().appendingPathComponent("api/settings/led_guidance")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = #"{"level": \#(v)}"#.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    /// Režim „lampa“ — RGB na desce (jako web).
    func postLightCommand(state: Bool, r: Int, g: Int, b: Int) async throws {
        let url = currentBaseURL().appendingPathComponent("api/light/command")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let rs = min(255, max(0, r)), gs = min(255, max(0, g)), bs = min(255, max(0, b))
        let body = #"{"state":\#(state ? "true" : "false"),"r":\#(rs),"g":\#(gs),"b":\#(bs)}"#
        req.httpBody = body.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
    }

    func postLightGameMode() async throws {
        let url = currentBaseURL().appendingPathComponent("api/light/game_mode")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
    }

    // MARK: - Demo

    func fetchDemoStatus() async throws -> ESPDemoStatusJSON {
        let url = currentBaseURL().appendingPathComponent("api/demo/status")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
        return try JSONDecoder().decode(ESPDemoStatusJSON.self, from: data)
    }

    func postDemoConfig(enabled: Bool, speedMs: UInt32?) async throws {
        let url = currentBaseURL().appendingPathComponent("api/demo/config")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        var parts = ["\"enabled\":\(enabled ? "true" : "false")"]
        if let speedMs {
            parts.append("\"speed_ms\":\(speedMs)")
        }
        req.httpBody = ("{" + parts.joined(separator: ",") + "}").data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    func postDemoStart() async throws {
        let url = currentBaseURL().appendingPathComponent("api/demo/start")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    // MARK: - MQTT

    func fetchMQTTStatus() async throws -> ESPMQTTStatusJSON {
        let url = currentBaseURL().appendingPathComponent("api/mqtt/status")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
        let dec = JSONDecoder()
        dec.keyDecodingStrategy = .convertFromSnakeCase
        return try dec.decode(ESPMQTTStatusJSON.self, from: data)
    }

    func postMQTTConfig(host: String, port: Int, username: String?, password: String?) async throws {
        let url = currentBaseURL().appendingPathComponent("api/mqtt/config")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let escH = escapeJSON(host)
        var parts = ["\"host\":\"\(escH)\"", "\"port\":\(port)"]
        if let u = username, !u.isEmpty {
            parts.append("\"username\":\"\(escapeJSON(u))\"")
        }
        if let p = password, !p.isEmpty {
            parts.append("\"password\":\"\(escapeJSON(p))\"")
        }
        req.httpBody = ("{" + parts.joined(separator: ",") + "}").data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    private nonisolated func escapeJSON(_ s: String) -> String {
        s.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
    }
}
