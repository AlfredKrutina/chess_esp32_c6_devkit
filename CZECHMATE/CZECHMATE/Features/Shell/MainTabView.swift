//
//  MainTabView.swift
//  CZECHMATE
//

import SwiftData
import SwiftUI

// MARK: - Game Views
// GameContainerView and related views are in the same module

struct MainTabView: View {
    @Environment(AppTabRouter.self) private var tabRouter
    @Environment(BoardConnectionStore.self) private var boardStore
    @Environment(NetworkStatusMonitor.self) private var networkMonitor
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
            GameContainerView()
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
            ProgressTabView()
                .tabItem {
                    Label("Pokrok", systemImage: "square.split.2x1")
                }
                .tag(AppMainTab.progress)
            SettingsTabView()
                .tabItem {
                    Label("Nastavení", systemImage: "gearshape.fill")
                }
                .tag(AppMainTab.settings)
        }
        .tint(Theme.accent)
        .toolbarBackground(.ultraThinMaterial, for: .tabBar)
        .toolbarBackground(.visible, for: .tabBar)
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
        #if os(iOS)
        .sheet(isPresented: Bindable(boardStore).offerWiFiProvisionSheet) {
            BoardWiFiProvisionFromBLESheet()
                .environment(boardStore)
        }
        #endif
        .onAppear {
            modelDownloadManager.reconcileWithDisk()
            boardStore.attachWifiPathHandoff(using: networkMonitor)
            AppDebugLog.coachTrace(
                "MainTabView onAppear coachIntro=\(!coachAutoIntroConsumed) modelInstalled=\(modelDownloadManager.isModelInstalled)"
            )
            // Obnovení spojení: poslední Bluetooth (nebo preferBluetoothOnly), jinak Wi‑Fi polling / mock.
            Task { await boardStore.resumeConnectionIfNeeded() }
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
