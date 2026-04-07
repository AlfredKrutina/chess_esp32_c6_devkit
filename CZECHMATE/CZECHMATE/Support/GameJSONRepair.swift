//
//  GameJSONRepair.swift
//  CZECHMATE
//
//  Dříve firmware vynechal `}` u `error_state.active == false` (game_task.c).
//  Na MCU opraveno; náhrada zůstává pro starší desky / rozbitý JSON.
//

import Foundation

enum GameJSONRepair {
    /// Opraví známý vzor a vrátí data vhodná pro JSONDecoder.
    static func repairStatusDataIfNeeded(_ data: Data) -> Data {
        guard var s = String(data: data, encoding: .utf8) else { return data }
        let broken = #""error_state":{"active":false,"restore_state""#
        let fixed = #""error_state":{"active":false},"restore_state""#
        if s.contains(broken) {
            s = s.replacingOccurrences(of: broken, with: fixed)
            return Data(s.utf8)
        }
        return data
    }
}
