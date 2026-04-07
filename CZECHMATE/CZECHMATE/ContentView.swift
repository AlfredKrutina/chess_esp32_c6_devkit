//
//  ContentView.swift
//  CZECHMATE
//

import SwiftUI

struct ContentView: View {
    @State private var boardStore = BoardConnectionStore()
    @State private var networkMonitor = NetworkStatusMonitor()
    @State private var tabRouter = AppTabRouter()
    @StateObject private var learningModeManager = LearningModeManager()
    @StateObject private var modelDownloadManager = ModelDownloadManager()
    @StateObject private var chessCoachManager = ChessCoachManager()
    @AppStorage("czechmate.onboarding.completed") private var onboardingCompleted = false
    @AppStorage("czechmate.appearance") private var appearanceRaw: String = AppAppearance.system.rawValue

    private var resolvedAppearance: AppAppearance {
        AppAppearance(rawValue: appearanceRaw) ?? .system
    }

    var body: some View {
        Group {
            if onboardingCompleted {
                MainTabView()
                    .environment(boardStore)
                    .environment(networkMonitor)
                    .environment(tabRouter)
                    .environment(\.czechmateOLEDBackground, resolvedAppearance.isOLED)
                    .environmentObject(learningModeManager)
                    .environmentObject(modelDownloadManager)
                    .environmentObject(chessCoachManager)
            } else {
                OnboardingView()
                    .environment(boardStore)
                    .environment(networkMonitor)
                    .environmentObject(learningModeManager)
                    .environmentObject(modelDownloadManager)
                    .environmentObject(chessCoachManager)
            }
        }
        .preferredColorScheme(resolvedAppearance.colorScheme)
        .background(
            Group {
                if resolvedAppearance.isOLED {
                    Color.black.ignoresSafeArea()
                }
            }
        )
        .onAppear {
            #if os(iOS)
            WatchConnectivityBridge.activateShared()
            #endif
        }
        #if os(iOS)
        .onOpenURL { url in
            handleDeepLink(url)
        }
        #endif
    }

    #if os(iOS)
    private func handleDeepLink(_ url: URL) {
        guard url.scheme?.lowercased() == "czechmate" else { return }
        let host = url.host?.lowercased()
        if host == "game" || url.path.lowercased().contains("game") {
            tabRouter.selectedTab = .game
        }
    }
    #endif
}

#Preview {
    ContentView()
        .environment(NetworkStatusMonitor())
        .environmentObject(LearningModeManager())
        .environmentObject(ModelDownloadManager())
        .environmentObject(ChessCoachManager())
}
