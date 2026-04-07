//
//  LearnView.swift
//  Výuka — průvodce postavením figurek (parita s webovým tutoriálem).
//

import SwiftUI

struct LearnView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(AppTabRouter.self) private var tabRouter
    @State private var isStarting = false
    @State private var localError: String?

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    AppSectionHeader(
                        title: "Výuka",
                        subtitle: "Postav fyzickou desku podle LED — bez hledání do telefonu",
                        systemImage: "graduationcap.fill"
                    )

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
                            .font(.system(.headline, design: .rounded))
                        Text(
                            "Stejný postup jako na webu: deska se přepne do režimu výuky, "
                                + "LED ukazuje pole a senzory hlídají položení figurky. Na konci potvrdíš startovní pozici."
                        )
                        .font(Theme.Typography.body())
                        .foregroundStyle(.secondary)

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
                        .disabled(!store.supportsWiFiRemoteCommands || isStarting)
                    }
                    .themeCard()

                    if !store.supportsWiFiRemoteCommands {
                        ThemedBanner(style: .neutral) {
                            Text("Připoj šachovnici přes Wi‑Fi (Domácí síť), aby šly posílat příkazy na LED.")
                                .font(Theme.Typography.body())
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                .padding(Theme.Spacing.l)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .navigationTitle("Výuka")
            .navigationBarTitleDisplayMode(.inline)
        }
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
