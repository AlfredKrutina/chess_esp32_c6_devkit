//
//  CZECHMATEApp.swift
//  CZECHMATE
//
//  Created by Alfréd Krutina on 06.04.2026.
//

import SwiftData
import SwiftUI

@main
struct CZECHMATEApp: App {
    init() {
        AppProcessContext.logBootstrap()
    }

    private var sharedModelContainer: ModelContainer = {
        SwiftDataAppSupport.ensureApplicationSupportDirectoryExists()
        let schema = Schema([PuzzleLibraryItem.self, GameSessionEntity.self])
        let config = ModelConfiguration(schema: schema, isStoredInMemoryOnly: false)
        do {
            return try ModelContainer(for: schema, configurations: [config])
        } catch {
            fatalError("SwiftData ModelContainer: \(error)")
        }
    }()

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .modelContainer(sharedModelContainer)
    }
}
