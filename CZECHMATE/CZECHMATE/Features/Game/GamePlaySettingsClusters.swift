//
//  GamePlaySettingsClusters.swift
//  CZECHMATE — Sdílené sekce „Hra“: Nastavení záložka i list Ovládání při partii (stejná @AppStorage / store).
//

import SwiftUI

/// Preference záložky **Hra** — vlož do `List` nebo `Form`.
struct GamePlaySettingsClusters: View {
    @Environment(BoardConnectionStore.self) private var store
    @EnvironmentObject private var learningModeManager: LearningModeManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager

    @AppStorage("czechmate.game.layoutMode") private var layoutModeRaw: String = GameLayoutMode.standard.rawValue
    @AppStorage("czechmate.game.boardZoomStorage") private var boardZoomStorage = 1.0
    @AppStorage("czechmate.boardFlipped") private var boardFlipped = false
    @AppStorage("czechmate.showBoardCoordinates") private var showBoardCoordinates = true
    @AppStorage("czechmate.moveAnimationsEnabled") private var moveAnimationsEnabled = true
    @AppStorage("czechmate.remoteMovesEnabled") private var remoteMovesEnabled = false
    @AppStorage("czechmate.boardStyleRaw") private var boardStyleRaw: String = ChessBoardStyle.wooden.rawValue

    /// Z `GameControlsSheet` váže `sessionConfig` (+ zápis do UserDefaults); z Nastavení `$coachUserLevel` z `@AppStorage`.
    @Binding var coachUserLevel: Int

    /// V „Ovládání“ po otevření průvodce zavřít sheet (např. `dismiss`).
    private let onCoachGuideOpened: (() -> Void)?

    @State private var botPrefsBusy = false
    @State private var showCoachOnboarding = false

    init(
        coachUserLevel: Binding<Int>,
        onCoachGuideOpened: (() -> Void)? = nil
    ) {
        _coachUserLevel = coachUserLevel
        self.onCoachGuideOpened = onCoachGuideOpened
    }

    // MARK: - Rozložení a deska

    private var layoutAndBoardSection: some View {
        Section {
            Picker("Rozložení obrazovky", selection: $layoutModeRaw) {
                ForEach(GameLayoutMode.allCases, id: \.self) { mode in
                    Label(mode.label, systemImage: mode.icon).tag(mode.rawValue)
                }
            }
            .pickerStyle(.segmented)

            HStack {
                Text("Velikost desky")
                Spacer()
                HStack(spacing: 4) {
                    Button {
                        boardZoomStorage = max(boardZoomStorage - 0.1, 0.7)
                    } label: {
                        Image(systemName: "minus")
                    }
                    Text("\(Int(boardZoomStorage * 100))%")
                        .font(.caption.monospacedDigit())
                        .frame(minWidth: 44)
                    Button {
                        boardZoomStorage = min(boardZoomStorage + 0.1, 1.5)
                    } label: {
                        Image(systemName: "plus")
                    }
                }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
                .controlSize(.small)
            }

            Toggle("Otočit perspektivu (černé dole)", isOn: $boardFlipped)

            Picker("Vzhled polí šachovnice", selection: $boardStyleRaw) {
                ForEach(ChessBoardStyle.allCases) { style in
                    HStack(spacing: 10) {
                        RoundedRectangle(cornerRadius: 3, style: .continuous)
                            .fill(style.squareLight)
                            .frame(width: 14, height: 14)
                        RoundedRectangle(cornerRadius: 3, style: .continuous)
                            .fill(style.squareDark)
                            .frame(width: 14, height: 14)
                        Text(style.title)
                    }
                    .tag(style.rawValue)
                }
            }

            Toggle("Souřadnice na desce (a–h, 1–8)", isOn: $showBoardCoordinates)
            Toggle("Animace tahů figurek", isOn: $moveAnimationsEnabled)
            Toggle(GameRemoteMovesCopy.toggleTitle, isOn: $remoteMovesEnabled)
                .accessibilityHint(remoteMovesAccessibilityHint)

            Button("Obnovit výchozí zobrazení desky") {
                boardFlipped = false
                showBoardCoordinates = true
                moveAnimationsEnabled = true
                remoteMovesEnabled = false
                boardZoomStorage = 1.0
                layoutModeRaw = GameLayoutMode.standard.rawValue
                boardStyleRaw = ChessBoardStyle.wooden.rawValue
            }
            .foregroundStyle(.secondary)
        } header: {
            Text("Deska a rozložení")
        } footer: {
            Group {
                if remoteMovesEnabled {
                    if let reason = store.remoteChessFromAppBlockedReason {
                        Text(reason)
                    } else {
                        Text(GameRemoteMovesCopy.usageHint)
                    }
                } else {
                    Text("Rozložení a zoom platí v záložce Hra. Standard je telefon na výšku; široký režim využije iPad nebo iPhone na šířku.")
                }
            }
            .font(.footnote)
            .foregroundStyle(.secondary)
        }
    }

    // MARK: - Trénink

    private var trainingSectionContent: some View {
        Section {
            Toggle("Učící mód (AI trenér)", isOn: learningModeToggleBinding)

            if learningModeManager.isLearningModeActive || learningModeManager.pendingActivateAfterModel {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Úroveň výkladu")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Picker("Úroveň výkladu", selection: $coachUserLevel) {
                        Text("Začátečník").tag(1)
                        Text("Pokročilý").tag(2)
                        Text("Středně pokročilý").tag(3)
                        Text("Expert").tag(4)
                    }
                    .pickerStyle(.segmented)
                }

                Button {
                    showCoachOnboarding = true
                    onCoachGuideOpened?()
                } label: {
                    Label("Průvodce tréninkem a modelem", systemImage: "book.fill")
                }
            }
        } header: {
            Text("Trénink")
        } footer: {
            Text("Učící mód potřebuje stažený model Gemma (jednorázově). Nápověda tahu Stockfish je samostatná a funguje i bez trenéra.")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    // MARK: - Bot

    private var botSection: some View {
        Section {
            Picker("Režim hry na desce", selection: botModeBinding) {
                Text("Člověk vs člověk").tag("pvp")
                Text("Proti botovi").tag("bot")
            }
            if store.boardUIPrefsPayload.botSettings.isBotMode {
                Picker("Strana bota", selection: botSideBinding) {
                    Text("Bot hraje bílé").tag("white")
                    Text("Bot hraje černé").tag("black")
                }
                Picker("Síla bota", selection: botStrengthBinding) {
                    ForEach(["6", "8", "10", "12", "14", "16"], id: \.self) { s in
                        Text(s).tag(s)
                    }
                }
                Text(BotStrengthCopy.approximateEloDescription(strengthDigits: store.boardUIPrefsPayload.botSettings.strength))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Toggle("LED návrh až po zvednutí figurky", isOn: botLedAfterLiftBinding)
            }

            Button {
                Task {
                    botPrefsBusy = true
                    await store.pushCurrentPrefsToBoard()
                    botPrefsBusy = false
                }
            } label: {
                if botPrefsBusy {
                    ProgressView()
                } else {
                    Label("Uložit režim a nastavení do desky (NVS)", systemImage: "arrow.up.doc")
                }
            }
            .disabled(!store.supportsWiFiRemoteCommands || botPrefsBusy || store.useMockBoard)
        } header: {
            Text("Bot a pravidla na šachovnici")
        } footer: {
            Text(botSectionFooter)
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    @ViewBuilder
    private var lampSectionIfNeeded: some View {
        if store.supportsWiFiRemoteCommands, store.hasActiveBoardTransport {
            Section {
                BoardLampQuickStrip()
                    .listRowInsets(EdgeInsets(top: 10, leading: 16, bottom: 10, trailing: 16))
                    .listRowBackground(Color.clear)
                    .listRowSeparator(.hidden)
            } header: {
                Text("Světlo desky")
            } footer: {
                Text("Jas a vypínač; barvy a herní režim LED v podrobnostech.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: - Bindings

    private var learningModeToggleBinding: Binding<Bool> {
        Binding(
            get: { learningModeManager.isLearningModeActive },
            set: { wantsOn in
                if wantsOn {
                    guard modelDownloadManager.isModelInstalled else {
                        learningModeManager.pendingActivateAfterModel = true
                        showCoachOnboarding = true
                        onCoachGuideOpened?()
                        return
                    }
                    if !learningModeManager.isLearningModeActive {
                        learningModeManager.isLearningModeActive = true
                    }
                } else {
                    learningModeManager.cancelPendingActivation()
                    if learningModeManager.isLearningModeActive {
                        learningModeManager.isLearningModeActive = false
                    }
                }
            }
        )
    }

    private var botModeBinding: Binding<String> {
        Binding(
            get: { store.boardUIPrefsPayload.botSettings.mode },
            set: { v in
                var p = store.boardUIPrefsPayload
                p.botSettings.mode = v
                store.boardUIPrefsPayload = p
            }
        )
    }

    private var botSideBinding: Binding<String> {
        Binding(
            get: { store.boardUIPrefsPayload.botSettings.side },
            set: { v in
                var p = store.boardUIPrefsPayload
                p.botSettings.side = v
                store.boardUIPrefsPayload = p
            }
        )
    }

    private var botStrengthBinding: Binding<String> {
        Binding(
            get: { store.boardUIPrefsPayload.botSettings.strength },
            set: { v in
                var p = store.boardUIPrefsPayload
                p.botSettings.strength = v
                store.boardUIPrefsPayload = p
            }
        )
    }

    private var botLedAfterLiftBinding: Binding<Bool> {
        Binding(
            get: { store.boardUIPrefsPayload.chessBotLedTargetOnlyAfterLift },
            set: { v in
                var p = store.boardUIPrefsPayload
                p.chessBotLedTargetOnlyAfterLift = v
                store.boardUIPrefsPayload = p
            }
        )
    }

    private var botSectionFooter: String {
        if store.useMockBoard {
            return "Ukázková šachovnice nepodporuje zápis na desku."
        }
        if store.supportsWiFiRemoteCommands {
            return "Zápis uloží do NVS i režim Člověk vs člověk (`pvp`), nejen bota. Návrhy z telefonu deska respektuje i před uložením."
        }
        return "Bez Wi‑Fi k desce lze režim měnit v aplikaci; zápis do NVS vyžaduje HTTP přes Wi‑Fi."
    }

    private var remoteMovesAccessibilityHint: String {
        var s = GameRemoteMovesCopy.usageHint
        if remoteMovesEnabled,
           let r = store.remoteChessFromAppBlockedReason?.trimmingCharacters(in: .whitespacesAndNewlines),
           !r.isEmpty {
            s += " " + r
        }
        return s
    }

    var body: some View {
        Group {
            layoutAndBoardSection
            trainingSectionContent
            botSection
            lampSectionIfNeeded
        }
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
    }
}
