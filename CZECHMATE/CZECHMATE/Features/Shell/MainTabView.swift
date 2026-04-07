//
//  MainTabView.swift
//  CZECHMATE
//

import SwiftData
import SwiftUI

struct MainTabView: View {
    @Environment(AppTabRouter.self) private var tabRouter
    @Environment(BoardConnectionStore.self) private var boardStore
    @Environment(\.modelContext) private var modelContext
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager
    @EnvironmentObject private var learningModeManager: LearningModeManager

    /// Jednorázová nabídka stažení AI Trenéra po vstupu do hlavních tabů (první spuštění po instalaci).
    @AppStorage("czechmate.coach.autoIntroConsumed") private var coachAutoIntroConsumed = false
    @State private var showCoachIntroFromLaunch = false

    var body: some View {
        TabView(selection: Binding(
            get: { tabRouter.selectedTab },
            set: { tabRouter.selectedTab = $0 }
        )) {
            GameView()
                .tabItem {
                    Label("Hra", systemImage: "checkerboard.rectangle")
                }
                .tag(AppMainTab.game)
            AnalysisView()
                .tabItem {
                    Label("Analýza", systemImage: "chart.line.uptrend.xyaxis")
                }
                .tag(AppMainTab.analysis)
            PuzzleView()
                .tabItem {
                    Label("Puzzle", systemImage: "puzzlepiece.extension")
                }
                .tag(AppMainTab.puzzle)
            LearnView()
                .tabItem {
                    Label("Výuka", systemImage: "graduationcap.fill")
                }
                .tag(AppMainTab.learn)
            StatsView()
                .tabItem {
                    Label("Statistiky", systemImage: "chart.bar.fill")
                }
                .tag(AppMainTab.stats)
            SettingsTabView()
                .tabItem {
                    Label("Nastavení", systemImage: "gearshape.fill")
                }
                .tag(AppMainTab.settings)
        }
        .tint(Theme.accent)
        .sheet(isPresented: $showCoachIntroFromLaunch) {
            CoachOnboardingView(
                modelDownload: modelDownloadManager,
                onFinished: {
                    learningModeManager.completePendingActivationIfNeeded(modelReady: modelDownloadManager.isModelInstalled)
                    showCoachIntroFromLaunch = false
                },
                onSkipForNow: {
                    learningModeManager.cancelPendingActivation()
                    showCoachIntroFromLaunch = false
                }
            )
        }
        .onAppear {
            // Spojení s deskou (Wi‑Fi / mock) hned po vstupu do aplikace — WebSocket zůstane aktivní i na jiných záložkách.
            boardStore.startPolling()
            if !coachAutoIntroConsumed {
                coachAutoIntroConsumed = true
                showCoachIntroFromLaunch = true
            }
        }
        .onChange(of: boardStore.isPolling) { _, on in
            if on {
                GameSessionTracker.sessionStarted(
                    linkKind: boardStore.activeLinkKind.rawValue,
                    modelContext: modelContext
                )
            } else {
                GameSessionTracker.sessionEnded(modelContext: modelContext)
            }
        }
        .onChange(of: boardStore.snapshot?.status.moveCount) { _, _ in
            if let n = boardStore.snapshot.map({ Int($0.status.moveCount) }) {
                GameSessionTracker.updatePeakMoveCount(n, modelContext: modelContext)
            }
        }
    }
}
