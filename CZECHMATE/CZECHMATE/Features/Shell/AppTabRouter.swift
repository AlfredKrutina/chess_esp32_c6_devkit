//
//  AppTabRouter.swift
//  Výběr tabu (deep link z Live Activity / URL scheme).
//

import Foundation
import Observation

enum AppMainTab: Int, Hashable, CaseIterable {
    case game = 0
    case analysis = 1
    case puzzle = 2
    case progress = 3
    case settings = 4
}

@Observable
@MainActor
final class AppTabRouter {
    var selectedTab: AppMainTab = .game
}
