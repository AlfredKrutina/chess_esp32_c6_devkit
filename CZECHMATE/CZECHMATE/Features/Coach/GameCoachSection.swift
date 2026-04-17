//
//  GameCoachSection.swift
//  CZECHMATE — vstup do AI trenéra (plán pozice + chat Gemma) na kartě Hra.
//

import SwiftUI

struct GameCoachSection: View {
    @Environment(BoardConnectionStore.self) private var store
    @EnvironmentObject private var learningModeManager: LearningModeManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager

    @Bindable var viewState: GameViewState

    var body: some View {
        Group {
            if learningModeManager.isLearningModeActive {
                VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                    HStack(spacing: Theme.Spacing.m) {
                        Image(systemName: "crown.fill")
                            .font(.title2)
                            .foregroundStyle(Theme.accent)
                        VStack(alignment: .leading, spacing: 4) {
                            Text("AI Trenér (Gemma)")
                                .font(Theme.Typography.cardTitle())
                            Text("Plán pozice: Stockfish přes internet určí nejlepší tah, Gemma v telefonu ho slovně vysvětlí. V chatu se můžeš ptát na šachy.")
                                .font(Theme.Typography.caption())
                                .foregroundStyle(.secondary)
                        }
                        Spacer(minLength: 0)
                    }

                    if !modelDownloadManager.isModelInstalled {
                        Text("Model trenéra zatím není stažený — v Ovládání hry otevři „Průvodce tréninkem“ nebo sekci Učení.")
                            .font(Theme.Typography.caption())
                            .foregroundStyle(Theme.Semantic.warningForeground)
                    }

                    if modelDownloadManager.isModelInstalled, !CoachMediaPipeBuildAvailability.isMediaPipeLinked {
                        Text(
                            "Model je v telefonu, ale v tomto buildu chybí MediaPipe (CocoaPods). Na Macu v adresáři CZECHMATE spusť „pod install“ a v Xcode otevři CZECHMATE.xcworkspace, ne jen .xcodeproj."
                        )
                        .font(Theme.Typography.caption())
                        .foregroundStyle(Theme.Semantic.warningForeground)
                    }

                    HStack(alignment: .center, spacing: Theme.Spacing.s) {
                        Button {
                            viewState.showCoachChat = true
                        } label: {
                            Label("Chat s Gemmou", systemImage: "bubble.left.and.bubble.right.fill")
                                .lineLimit(1)
                                .minimumScaleFactor(0.78)
                                .multilineTextAlignment(.center)
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.themePrimary)

                        Button {
                            viewState.showPositionCoach = true
                        } label: {
                            Label("Plán pozice", systemImage: "scope")
                                .lineLimit(1)
                                .minimumScaleFactor(0.78)
                                .multilineTextAlignment(.center)
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.themeOutline)
                        .disabled(store.snapshot == nil)
                    }
                }
                .padding(Theme.Spacing.m)
                .frame(maxWidth: .infinity, alignment: .leading)
                .themeCard()
            }
        }
    }
}
