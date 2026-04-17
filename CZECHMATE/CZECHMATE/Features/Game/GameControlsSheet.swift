//
//  GameControlsSheet.swift
//  CZECHMATE — Ovládání při partii (stejný obsah jako Nastavení → Záložka Hra).
//

import SwiftUI

struct GameControlsSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(BoardConnectionStore.self) private var store
    @Bindable var viewState: GameViewState

    private static let coachUserDefaultsKey = "czechmate.coach.userLevel"

    var body: some View {
        NavigationStack {
            List {
                GamePlaySettingsClusters(
                    coachUserLevel: coachUserLevelBinding,
                    onCoachGuideOpened: { dismiss() }
                )

                Section {
                    Button {
                        viewState.showConnection = true
                        dismiss()
                    } label: {
                        Label("Správa připojení", systemImage: "wifi")
                    }
                } header: {
                    Text("Připojení")
                } footer: {
                    Text("Kompletní stav spojení a Bluetooth je také v záložce Nastavení. Novou partii spustíš tlačítkem níže.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .tint(Theme.accent)
            .safeAreaInset(edge: .bottom, spacing: 0) {
                newGameFloatingBar
            }
            .navigationTitle("Ovládání hry")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Hotovo") {
                        dismiss()
                    }
                }
            }
        }
    }

    private var coachUserLevelBinding: Binding<Int> {
        Binding(
            get: { viewState.sessionConfig.coachUserLevel },
            set: { v in
                var cfg = viewState.sessionConfig
                cfg.coachUserLevel = v
                viewState.sessionConfig = cfg
                UserDefaults.standard.set(v, forKey: Self.coachUserDefaultsKey)
            }
        )
    }

    private var newGameFloatingBar: some View {
        VStack(spacing: 0) {
            Divider()
                .opacity(0.4)
            Button {
                viewState.showNewGameSetup = true
                dismiss()
            } label: {
                Label("Nová hra", systemImage: "arrow.counterclockwise")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.themePrimary)
            .padding(.horizontal, Theme.Spacing.l)
            .padding(.top, Theme.Spacing.m)
            .padding(.bottom, Theme.Spacing.m)
            .frame(maxWidth: .infinity)
        }
        .background {
            Color(uiColor: .systemGroupedBackground)
                .ignoresSafeArea(edges: .bottom)
        }
    }
}

// MARK: - Preview

#Preview {
    GameControlsSheet(viewState: GameViewState())
        .environment(BoardConnectionStore())
        .environmentObject(LearningModeManager())
        .environmentObject(ModelDownloadManager())
        .environmentObject(ChessCoachManager())
}
