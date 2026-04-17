//
//  CZECHMATE_WApp.swift
//  CZECHMATE_W Watch App
//
//  Created by Alfréd Krutina on 13.04.2026.
//

import SwiftUI

@main
struct CZECHMATE_W_Watch_AppApp: App {
    init() {
        WatchAppLog.staging("CZECHMATE_W App init")
    }

    var body: some Scene {
        WindowGroup {
            RootWatchView()
                .onAppear {
                    WatchAppLog.staging("WindowGroup RootWatchView onAppear")
                }
        }
    }
}
