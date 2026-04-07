//
//  ESPGameStatusJSON.swift
//  Část `GET /api/status` — matrix senzory jen v režimu výuky / puzzle setup (firmware).
//

import Foundation

struct ESPGameStatusJSON: Decodable, Sendable {
    let boardSetupTutorial: Bool?
    let matrixOccupied: [Int]?

    enum CodingKeys: String, CodingKey {
        case boardSetupTutorial = "board_setup_tutorial"
        case matrixOccupied = "matrix_occupied"
    }
}
