//
//  ChessboardAPIClient+SetupWizard.swift
//  Průvodce postavením figurek — parita s webem (hint na cílové pole, status, setup_tutorial).
//

import Foundation

enum SetupTutorialAPIAction: String, Encodable {
    case start
    case cancel
    case finish
}

extension ChessboardAPIClient {
    /// Jako webové `hint_highlight` s jedním polem `to` — LED na cílovém poli.
    func postHintHighlightDestinationOnly(to square: String) async throws {
        let url = currentBaseURL().appendingPathComponent("api/game/hint_highlight")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let body: [String: String] = ["to": square.lowercased()]
        req.httpBody = try JSONSerialization.data(withJSONObject: body)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }

    /// `GET /api/status` — dekódujeme jen pole potřebná průvodci (ostatní klíče ignorovány).
    func fetchGameStatus() async throws -> ESPGameStatusJSON {
        let url = currentBaseURL().appendingPathComponent("api/status")
        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data)
        return try JSONDecoder().decode(ESPGameStatusJSON.self, from: data)
    }

    /// `POST /api/game/setup_tutorial` — `start` / `cancel` / `finish` (jako web).
    func postSetupTutorial(action: SetupTutorialAPIAction) async throws {
        let url = currentBaseURL().appendingPathComponent("api/game/setup_tutorial")
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let body = #"{"action":"\#(action.rawValue)"}"#
        req.httpBody = body.data(using: .utf8)
        let (data, response) = try await session.data(for: req)
        try validateHTTP(response: response, data: data, treat403AsWebLock: true)
    }
}
