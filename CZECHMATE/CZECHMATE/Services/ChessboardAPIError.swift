//
//  ChessboardAPIError.swift
//  CZECHMATE
//

import Foundation

enum ChessboardAPIError: Error, LocalizedError, Sendable {
    case invalidBaseURL
    case invalidResponse
    case httpStatus(Int, String?)
    case webLocked
    case decodeFailed(String)
    case emptyBody

    var errorDescription: String? {
        switch self {
        case .invalidBaseURL:
            return "Neplatná základní URL šachovnice."
        case .invalidResponse:
            return "Neplatná odpověď serveru."
        case .httpStatus(let code, let detail):
            if let detail, !detail.isEmpty { return "HTTP \(code): \(detail)" }
            return "HTTP chyba \(code)."
        case .webLocked:
            return "Webové rozhraní je uzamčeno (web lock). Odemkni přes UART."
        case .decodeFailed(let msg):
            return "Parsování JSON: \(msg)"
        case .emptyBody:
            return "Prázdná odpověď."
        }
    }
}
