//
//  CoachChatModels.swift
//  CZECHMATE — řádky chatu s lokálním modelem Gemma.
//

import Foundation

struct CoachChatLine: Identifiable, Equatable {
    let id: UUID
    let isUser: Bool
    var text: String
    var streaming: Bool
}
