//
//  LearnView.swift
//  Výuka — průvodce postavením figurek (parita s webovým tutoriálem).
//

import SwiftUI

/// Obsah výuky (sdíleno se záložkou Pokrok).
struct LearnTabContent: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(AppTabRouter.self) private var tabRouter
    @EnvironmentObject private var learningModeManager: LearningModeManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager
    @EnvironmentObject private var chessCoachManager: ChessCoachManager

    @AppStorage("czechmate.coach.userLevel") private var coachUserLevel = 4

    @State private var isStarting = false
    @State private var localError: String?
    @State private var showCoachOnboarding = false
    @State private var showCoachChat = false
    @State private var showPositionCoach = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                AppSectionHeader(
                    title: "Výuka",
                    subtitle: "LED průvodci a AI trenér (Gemma) na zařízení",
                    systemImage: "graduationcap.fill"
                )

                progressCoachHubCard

                if let localError {
                    ThemedBanner(style: .error) {
                        Text(localError)
                            .font(Theme.Typography.body())
                            .foregroundStyle(Theme.Semantic.errorForeground)
                    }
                } else if let err = store.lastError, !err.isEmpty {
                    ThemedBanner(style: .error) {
                        Text(err)
                            .font(Theme.Typography.body())
                            .foregroundStyle(Theme.Semantic.errorForeground)
                    }
                }

                VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                    Text("Základní postavení")
                        .font(Theme.Typography.cardTitle())
                    Text("Deska v režimu výuky: LED ukáže pole, senzory zkontrolují figury. Na konci potvrdíš výchozí pozici.")
                        .font(Theme.Typography.body())
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)

                    Button {
                        Task { await startStandard() }
                    } label: {
                        if isStarting {
                            ProgressView()
                                .frame(maxWidth: .infinity)
                        } else {
                            Label("Spustit průvodce — základní postavení", systemImage: "square.grid.3x3.fill")
                                .frame(maxWidth: .infinity)
                        }
                    }
                    .buttonStyle(.themePrimary)
                    .disabled(!store.supportsRemoteChessCommands || isStarting)
                }
                .themeCard()

                if !store.supportsRemoteChessCommands {
                    ThemedBanner(style: .neutral) {
                        Text("Pro LED průvodce připoj desku přes Wi‑Fi nebo Bluetooth.")
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .padding(Theme.Spacing.l)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .background(Color(uiColor: .systemGroupedBackground))
        .sheet(isPresented: $showCoachOnboarding) {
            CoachOnboardingView(
                modelDownload: modelDownloadManager,
                onFinished: {
                    learningModeManager.completePendingActivationIfNeeded(modelReady: modelDownloadManager.isModelInstalled)
                    showCoachOnboarding = false
                },
                onSkipForNow: {
                    learningModeManager.cancelPendingActivation()
                    showCoachOnboarding = false
                }
            )
        }
        .sheet(isPresented: $showCoachChat) {
            CoachChatSheet(coachUserLevel: coachUserLevel)
        }
        .sheet(isPresented: $showPositionCoach) {
            PositionCoachSheet(coachUserLevel: coachUserLevel)
        }
    }

    private var learningModeToggleBinding: Binding<Bool> {
        Binding(
            get: { learningModeManager.isLearningModeActive },
            set: { newValue in
                if newValue {
                    guard modelDownloadManager.isModelInstalled else {
                        learningModeManager.pendingActivateAfterModel = true
                        showCoachOnboarding = true
                        return
                    }
                    learningModeManager.isLearningModeActive = true
                } else {
                    learningModeManager.pendingActivateAfterModel = false
                    learningModeManager.isLearningModeActive = false
                }
            }
        )
    }

    private var progressCoachHubCard: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            HStack(alignment: .top, spacing: Theme.Spacing.m) {
                ZStack {
                    Circle()
                        .fill(Theme.accentMuted)
                        .frame(width: 48, height: 48)
                    Image(systemName: "crown.fill")
                        .font(.title2)
                        .foregroundStyle(Theme.accent)
                }
                .accessibilityHidden(true)

                VStack(alignment: .leading, spacing: 6) {
                    Text("AI trenér")
                        .font(Theme.Typography.cardTitle())
                    Text("Zapni učící mód na kartě Hra. Slovní výklad běží lokálně z modelu Gemma; nejlepší tah a eval pro plán pozice bere aplikace přes internet (Stockfish).")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 0)
            }

            Toggle("Učící mód", isOn: learningModeToggleBinding)
                .tint(Theme.accent)

            VStack(alignment: .leading, spacing: 6) {
                Text("Úroveň výkladu")
                    .font(Theme.Typography.caption().weight(.semibold))
                    .foregroundStyle(.secondary)
                Picker("Úroveň", selection: $coachUserLevel) {
                    Text("Začátečník").tag(1)
                    Text("Pokročilý").tag(2)
                    Text("Středně pokročilý").tag(3)
                    Text("Expert").tag(4)
                }
                .pickerStyle(.segmented)
            }

            if !modelDownloadManager.isModelInstalled {
                Text("Model Gemma ještě není stažený — po zapnutí učícího módu nabídneme průvodce stažením.")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(Theme.Semantic.warningForeground)
            }

            if modelDownloadManager.isModelInstalled, !CoachMediaPipeBuildAvailability.isMediaPipeLinked {
                Text("V buildu chybí MediaPipe — na Macu spusť „pod install“ a otevři .xcworkspace.")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(Theme.Semantic.warningForeground)
            }

            HStack(alignment: .center, spacing: Theme.Spacing.s) {
                Button {
                    showCoachChat = true
                } label: {
                    Label("Chat s Gemmou", systemImage: "bubble.left.and.bubble.right.fill")
                        .lineLimit(1)
                        .minimumScaleFactor(0.78)
                        .multilineTextAlignment(.center)
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.themePrimary)
                .disabled(!learningModeManager.isLearningModeActive)

                Button {
                    showPositionCoach = true
                } label: {
                    Label("Plán pozice", systemImage: "scope")
                        .lineLimit(1)
                        .minimumScaleFactor(0.78)
                        .multilineTextAlignment(.center)
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.themeOutline)
                .disabled(store.snapshot == nil || !learningModeManager.isLearningModeActive)
            }

            if !learningModeManager.isLearningModeActive {
                Text("Chat a plán pozice jsou dostupné po zapnutí učícího módu a (u plánu) po načtení pozice z karty Hra.")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.tertiary)
            } else if store.snapshot == nil {
                Text("Plán pozice: připoj desku a načti partii na kartě Hra.")
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.tertiary)
            }

            Button {
                showCoachOnboarding = true
            } label: {
                Label("Průvodce modelem / stáhnout Gemma", systemImage: "arrow.down.circle")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.themeSecondary)
        }
        .padding(Theme.Spacing.m)
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func startStandard() async {
        isStarting = true
        localError = nil
        defer { isStarting = false }
        let ok = await store.beginStandardBoardSetupWizard()
        if ok {
            tabRouter.selectedTab = .game
        } else if store.lastError == nil {
            localError = "Nepodařilo se spustit průvodce."
        }
    }
}

struct LearnView: View {
    var body: some View {
        NavigationStack {
            LearnTabContent()
                .navigationTitle("Výuka")
                .navigationBarTitleDisplayMode(.inline)
                .tint(Theme.accent)
        }
    }
}
