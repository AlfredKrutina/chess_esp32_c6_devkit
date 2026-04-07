//
//  NewGameSetupSheet.swift
//  Nastavení času před novou hrou — parita s webovým `/api/timer/config` + nová hra.
//

import SwiftUI

struct NewGameSetupSheet: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.dismiss) private var dismiss

    @State private var selectedPreset: ChessTimeControlPreset? = .blitz5_0
    @State private var customMinutes: Int = 10
    @State private var customIncrement: Int = 5
    @State private var mode: SetupMode = .preset
    @State private var isWorking = false
    @State private var errorText: String?
    @State private var showConfirmNewGame = false

    private enum SetupMode: String, CaseIterable {
        case preset = "Předvolby"
        case custom = "Vlastní"
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    Picker("Režim", selection: $mode) {
                        ForEach(SetupMode.allCases, id: \.self) { m in
                            Text(m.rawValue).tag(m)
                        }
                    }
                    .pickerStyle(.segmented)

                    if mode == .preset {
                        presetGrid
                    } else {
                        customForm
                    }

                    if let errorText {
                        ThemedBanner(style: .error) {
                            Text(errorText)
                                .font(Theme.Typography.caption())
                                .foregroundStyle(Theme.Semantic.errorForeground)
                        }
                    }

                    Button {
                        if store.boardUIPrefsPayload.chess_confirm_new_game {
                            showConfirmNewGame = true
                        } else {
                            Task { await startGame() }
                        }
                    } label: {
                        if isWorking {
                            ProgressView()
                                .frame(maxWidth: .infinity)
                        } else {
                            Text("Začít hru")
                                .frame(maxWidth: .infinity)
                        }
                    }
                    .buttonStyle(.themePrimary)
                    .disabled(isWorking || !store.supportsWiFiRemoteCommands)
                }
                .padding(Theme.Spacing.l)
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .navigationTitle("Nová hra")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") { dismiss() }
                        .disabled(isWorking)
                }
            }
            .alert("Opravdu začít novou hru?", isPresented: $showConfirmNewGame) {
                Button("Zrušit", role: .cancel) {}
                Button("Začít", role: .destructive) {
                    Task { await startGame() }
                }
            } message: {
                Text("Aktuální partie na desce se přepíše novou hrou s vybraným časem.")
            }
        }
    }

    private var presetGrid: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            sectionHeader("Bez časomíry", systemImage: "infinity")
            LazyVGrid(columns: [GridItem(.flexible())], spacing: 10) {
                chipButton(.no_time_limit)
            }
            sectionHeader("Bullet", systemImage: "bolt.fill")
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                chipButton(.bullet1_0)
                chipButton(.bullet2_1)
            }
            sectionHeader("Blitz", systemImage: "hare.fill")
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                chipButton(.blitz3_0)
                chipButton(.blitz3_2)
                chipButton(.blitz5_0)
            }
            sectionHeader("Rapid", systemImage: "tortoise.fill")
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                chipButton(.rapid10_0)
                chipButton(.rapid15_10)
            }
        }
    }

    private func sectionHeader(_ title: String, systemImage: String) -> some View {
        Label(title, systemImage: systemImage)
            .font(.system(.subheadline, design: .rounded).weight(.semibold))
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.top, 4)
    }

    private func chipButton(_ p: ChessTimeControlPreset) -> some View {
        let on = selectedPreset == p && mode == .preset
        return Button {
            mode = .preset
            selectedPreset = p
        } label: {
            VStack(spacing: 6) {
                Image(systemName: p.systemImage)
                    .font(.title2)
                    .foregroundStyle(on ? Theme.accent : .secondary)
                Text(p.title)
                    .font(.system(.headline, design: .rounded).weight(.semibold))
                Text(p.subtitle)
                    .font(.system(.caption2, design: .rounded))
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .fill(on ? Theme.accentMuted : Color.secondary.opacity(0.08))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .stroke(on ? Theme.accent.opacity(0.55) : Color.clear, lineWidth: 2)
            )
        }
        .buttonStyle(.plain)
    }

    private var customForm: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            Text("Základní čas a inkrement se odešlou jako typ 14 (vlastní), stejně jako na webu.")
                .font(Theme.Typography.caption())
                .foregroundStyle(.secondary)
            Stepper("Minuty: \(customMinutes)", value: $customMinutes, in: 1 ... 180, step: 1)
            Stepper("Inkrement (s): \(customIncrement)", value: $customIncrement, in: 0 ... 60, step: 1)
        }
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(Color.secondary.opacity(0.06))
        )
    }

    private func startGame() async {
        isWorking = true
        errorText = nil
        defer { isWorking = false }
        let choice: ChessTimeControlChoice
        switch mode {
        case .preset:
            guard let p = selectedPreset else {
                errorText = "Vyber předvolbu."
                return
            }
            choice = p.choice
        case .custom:
            choice = .custom(minutes: customMinutes, incrementSeconds: customIncrement)
        }
        let ok = await store.startNewGameWithTimeControl(choice)
        if ok {
            dismiss()
        } else {
            errorText = store.lastError ?? "Akci se nepodařilo dokončit."
        }
    }
}
