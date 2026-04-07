//
//  JSONDecoder+GameSnapshot.swift
//  Jednotná politika dekódování snapshotu z ESP (HTTP / WebSocket / BLE GATT).
//

import Foundation

extension JSONDecoder {
    /// Dekodér pro `GameSnapshot` a vnořené typy s explicitními `CodingKeys`.
    ///
    /// **Nepoužívat** `keyDecodingStrategy = .convertFromSnakeCase` — koliduje s
    /// `GameStatus` (`game_state`, …) a způsobí `keyNotFound(game_state)`.
    static func forGameSnapshot() -> JSONDecoder {
        JSONDecoder()
    }
}
