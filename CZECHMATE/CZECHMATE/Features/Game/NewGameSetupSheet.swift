//
//  NewGameSetupSheet.swift
//  Nastavení času před novou hrou — parita s webovým `/api/timer/config` + nová hra.
//

import SwiftUI

struct NewGameSetupSheet: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.dismiss) private var dismiss

    @State private var selectedPreset: ChessTimeControlPreset?
    @State private var customMinutes: Int
    @State private var customIncrement: Int
    @State private var mode: SetupMode
    @State private var isWorking = false
    @State private var errorText: String?
    @State private var showConfirmNewGame = false

    private enum SetupMode: String, CaseIterable {
        case preset = "Předvolby"
        case custom = "Vlastní"
    }

    init() {
        if let saved = LastNewGameTimeSelection.load() {
            switch saved {
            case .presetChoice(let p):
                _selectedPreset = State(initialValue: p)
                _mode = State(initialValue: .preset)
                _customMinutes = State(initialValue: 10)
                _customIncrement = State(initialValue: 5)
            case .customChoice(let m, let inc):
                _selectedPreset = State(initialValue: .no_time_limit)
                _mode = State(initialValue: .custom)
                _customMinutes = State(initialValue: m)
                _customIncrement = State(initialValue: inc)
            }
        } else {
            _selectedPreset = State(initialValue: .no_time_limit)
            _mode = State(initialValue: .preset)
            _customMinutes = State(initialValue: 10)
            _customIncrement = State(initialValue: 5)
        }
        _isWorking = State(initialValue: false)
        _errorText = State(initialValue: nil)
        _showConfirmNewGame = State(initialValue: false)
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    Text("Stejné předvolby času a vlastní nastavení (typ 14) jako na webovém rozhraní šachovnice — odešle se na desku a spustí novou partii.")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)

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

                    if store.isBluetoothOnlyBoardPath {
                        ThemedBanner(style: .neutral) {
                            Text(
                                "Přes Bluetooth se pošle nastavení času i nová hra (stejná logika jako přes Wi‑Fi). "
                                    + "Hodiny v aplikaci berou čas z posledního snímku (`clock` v JSON z desky), ne z samostatného HTTP — u novějšího firmwaru běží odpočet i bez Wi‑Fi k šachovnici."
                            )
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                        }
                    }
                }
                .padding(Theme.Spacing.l)
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .safeAreaInset(edge: .bottom, spacing: 0) {
                startGameFloatingBar
            }
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
        .task {
            await store.cancelBoardSetupWizardIfActive()
        }
    }

    /// Přilepený spodek sheetu — „Začít hru“ je vždy po ruce bez rolování dlouhého obsahu.
    private var startGameFloatingBar: some View {
        VStack(spacing: 0) {
            Divider()
                .opacity(0.4)
            VStack(spacing: Theme.Spacing.s) {
                Button {
                    if store.boardUIPrefsPayload.chess_confirm_new_game {
                        showConfirmNewGame = true
                    } else {
                        Task { await startGame() }
                    }
                } label: {
                    if isWorking {
                        ProgressView()
                            .tint(.white)
                            .frame(maxWidth: .infinity)
                    } else {
                        Text("Začít hru")
                            .frame(maxWidth: .infinity)
                    }
                }
                .buttonStyle(.themePrimary)
                .disabled(isWorking || !store.supportsRemoteChessCommands)
            }
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
