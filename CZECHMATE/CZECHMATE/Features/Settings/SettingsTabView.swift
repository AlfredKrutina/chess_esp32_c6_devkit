//
//  SettingsTabView.swift
//  CZECHMATE — čisté uživatelské Nastavení; hardware a diagnostika pod Vývojář.
//

import SwiftUI
#if os(iOS)
import UIKit
#endif

struct SettingsTabView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(AppTabRouter.self) private var tabRouter
    @EnvironmentObject private var learningModeManager: LearningModeManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager

    @AppStorage("czechmate.appearance") private var appearanceRaw: String = AppAppearance.system.rawValue
    @AppStorage("czechmate.hapticsEnabled") private var hapticsEnabled = true
    @AppStorage("czechmate.soundEffectsEnabled") private var soundEffectsEnabled = true
    @AppStorage("czechmate.endgameReportAutoOpen") private var endgameReportAutoOpen = true
    @AppStorage("czechmate.coach.userLevel") private var coachUserLevel = 4
    @AppStorage("czechmate.developerModeUnlocked") private var developerModeUnlocked = false
    @AppStorage(BoardConnectionStore.preferBluetoothOnlyKey) private var preferBluetoothOnly = false
    @AppStorage(MatchLiveActivityManager.liveActivityEnabledDefaultsKey) private var liveActivityEnabled = true
    @State private var showIntroGuide = false
    @State private var showBoardConnectionSheet = false
    @State private var bleReconnectBusy = false
    @State private var showBoardFactoryResetConfirm = false
    @State private var boardFactoryResetBusy = false
    @State private var developerTapCount = 0
    @State private var lastDeveloperTapAt: Date?

    var body: some View {
        NavigationStack {
            Form {
                orientationSection

                GamePlaySettingsClusters(coachUserLevel: $coachUserLevel)

                appearanceAndFeedbackSection

                boardConnectionSection

                homeAssistantSection

                appSection

                aboutSection

                #if os(iOS)
                watchSection
                #endif

                privacySection

                iCloudSection

                if developerModeUnlocked {
                    developerSection
                }

                if let err = store.lastError {
                    Section {
                        ThemedBanner(style: .error) {
                            Text(err)
                                .font(Theme.Typography.body())
                                .foregroundStyle(Theme.Semantic.errorForeground)
                        }
                    }
                }
            }
            .navigationTitle("Nastavení")
            .tint(Theme.accent)
            .fullScreenCover(isPresented: $showIntroGuide) {
                OnboardingView(presentation: .fromSettings)
                    .environment(store)
            }
            .sheet(isPresented: $showBoardConnectionSheet) {
                BoardDiscoveryView()
                    .environment(store)
            }
            .confirmationDialog(
                "Smazat celou paměť desky (NVS) a restartovat?",
                isPresented: $showBoardFactoryResetConfirm,
                titleVisibility: .visible
            ) {
                Button("Zrušit", role: .cancel) {}
                Button("Smazat NVS a restartovat", role: .destructive) {
                    Task {
                        boardFactoryResetBusy = true
                        await store.factoryResetBoard()
                        boardFactoryResetBusy = false
                    }
                }
            } message: {
                Text("Data v telefonu zůstanou. Deska po restartu bude jako po prvním zapnutí — znovu ji připoj v Najít šachovnici.")
            }
        }
    }

    // MARK: - Orientace v nastavení

    private var orientationSection: some View {
        Section {
            VStack(alignment: .leading, spacing: 12) {
                Text(
                    "Sekce „Záložka Hra“ je stejná jako okno Ovládání přímo u partie (ikona posuvníků vpravo nahoře v Hře). "
                        + "Můžeš tedy vše upravit tady nebo během hry — změny se okamžitě projeví v obou místech."
                )
                .font(.footnote)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)

                Button {
                    tabRouter.selectedTab = .game
                } label: {
                    Label("Přejít na záložku Hra", systemImage: "checkerboard.rectangle")
                }
            }
        } header: {
            Text("Přehled")
        }
    }

    // MARK: - Vzhled a odezva (celá aplikace)

    private var appearanceAndFeedbackSection: some View {
        Section {
            Picker("Vzhled aplikace", selection: $appearanceRaw) {
                ForEach(AppAppearance.allCases) { a in
                    Text(a.title).tag(a.rawValue)
                }
            }
            Toggle("Haptická odezva", isOn: $hapticsEnabled)
            Toggle("Zvuky šachovnice", isOn: $soundEffectsEnabled)
            Toggle("Po skončení partie automaticky otevřít souhrn", isOn: $endgameReportAutoOpen)
        } header: {
            Text("Vzhled a odezva")
        } footer: {
            Text("Vzhled aplikace (včetně OLED) platí pro celý CZECHMATE. Animace, souřadnice, styl polí a rozložení desky jsou v sekci „Záložka Hra“ výše.")
        }
    }

    // MARK: - Home Assistant / MQTT

    private var homeAssistantSection: some View {
        Section {
            NavigationLink {
                HomeAssistantMqttSetupView()
                    .environment(store)
            } label: {
                Label("Home Assistant a MQTT", systemImage: "house.and.flag.fill")
            }
        } header: {
            Text("Chytrá domácnost")
        } footer: {
            Text(
                "Návod pro Mosquitto v Home Assistantu a zápis brokeru do šachovnice. Telefon jen předá nastavení na ESP — k brokeru se připojuje deska ve stejné Wi‑Fi."
            )
        }
    }

    // MARK: - Připojení k šachovnici

    private var boardConnectionSection: some View {
        Section {
            LabeledContent("Stav") {
                Text(connectionStatusText)
                    .foregroundStyle(connectionStatusColor)
            }
            LabeledContent("Typ spojení") {
                Text(linkKindTitle)
            }
            Button {
                showBoardConnectionSheet = true
            } label: {
                Label("Spravovat šachovnici", systemImage: "link.circle")
            }
            if let dev = store.lastSavedBLEBoardForReconnect,
               !(store.isPolling && store.activeLinkKind == .bluetooth) {
                Button {
                    Task {
                        bleReconnectBusy = true
                        await store.reconnectLastSavedBLEBoard()
                        bleReconnectBusy = false
                    }
                } label: {
                    if bleReconnectBusy {
                        ProgressView()
                    } else {
                        Label("Znovu „\(dev.displayName)“ (Bluetooth)", systemImage: "dot.radiowaves.left.and.right")
                    }
                }
                .disabled(bleReconnectBusy)
            }
            Toggle(
                "Nepřepínat na Wi‑Fi po Bluetooth",
                isOn: $preferBluetoothOnly
            )
            if store.isPolling {
                Button(role: .destructive) {
                    store.stopPolling(notifyWatchDisconnect: true)
                } label: {
                    Label("Odpojit", systemImage: "wifi.slash")
                }
            }
            NavigationLink {
                ManualConnectionSettingsView()
                    .environment(store)
            } label: {
                Label("Ruční nastavení připojení", systemImage: "keyboard")
            }
            if store.supportsFactoryReset {
                Button("Tovární reset desky (smazat NVS)…", role: .destructive) {
                    showBoardFactoryResetConfirm = true
                }
                .disabled(boardFactoryResetBusy)
            }
        } header: {
            Text("Připojení k šachovnici (CZECHMATE)")
        } footer: {
            Text(
                "Vyhledání desky přes Bluetooth nebo Wi‑Fi bez zadávání IP. Ruční adresa jen pokud to potřebuješ. "
                    + "Volba „Nepřepínat na Wi‑Fi“ je vhodná, když nechceš v telefonu zapínat Wi‑Fi, nebo když máš doma Wi‑Fi a přesto chceš výhradně Bluetooth k desce (stejný přepínač je i u „Hledat přes Bluetooth“ v Najít šachovnici). "
                    + "Stockfish v aplikaci používá internet v telefonu — nezávisle na tom, zda jdeš na desku přes BLE nebo HTTP. "
                    + "Tlačítkem „Znovu … Bluetooth“ obnovíš poslední desku bez skenování. "
                    + "Tovární reset vymaže celou NVS na ESP (Wi‑Fi, MQTT, partie) a desku restartuje — stejné jako na webu v Nastavení → Síť."
            )
        }
    }

    private var connectionStatusText: String {
        guard store.isPolling else { return "Odpojeno" }
        switch store.boardLinkIndicatorTier {
        case .live:
            return "Připojeno"
        case .connecting:
            return "Připojování…"
        case .offline:
            return store.transport != nil ? "Bez odezvy desky" : "Odpojeno"
        }
    }

    private var connectionStatusColor: Color {
        guard store.isPolling else { return .secondary }
        switch store.boardLinkIndicatorTier {
        case .live:
            return Theme.Semantic.success
        case .connecting:
            return Theme.Semantic.warningForeground
        case .offline:
            return Theme.Semantic.errorForeground
        }
    }

    private var linkKindTitle: String {
        switch store.activeLinkKind {
        case .wifiLAN: return "Wi‑Fi"
        case .bluetooth: return "Bluetooth"
        case .mock: return "Ukázka (offline)"
        }
    }

    // MARK: - Aplikace

    private var appSection: some View {
        Section {
            NavigationLink {
                HelpView()
            } label: {
                Label {
                    Text("Nápověda a funkce")
                } icon: {
                    Image(systemName: "book.pages.fill")
                        .foregroundStyle(Theme.accent)
                }
            }
            Button {
                showIntroGuide = true
            } label: {
                Label("Úvodní průvodce", systemImage: "hand.wave.fill")
            }
        } header: {
            Text("Aplikace")
        }
    }

    private var aboutSection: some View {
        Section {
            HStack(spacing: 12) {
                Image("AppLogo")
                    .resizable()
                    .scaledToFit()
                    .frame(width: 48, height: 48)
                    .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
                VStack(alignment: .leading, spacing: 4) {
                    Text("CZECHMATE")
                        .font(.headline)
                    Text(appVersionString)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .contentShape(Rectangle())
                        .onTapGesture {
                            registerDeveloperModeTap()
                        }
                }
            }
        } header: {
            Text("O aplikaci")
        }
    }

    #if os(iOS)
    private var watchSection: some View {
        Section {
            Toggle("Živá aktivita partie (Lock Screen / Dynamic Island)", isOn: $liveActivityEnabled)
                .onChange(of: liveActivityEnabled) { _, isOn in
                    if !isOn {
                        Task { await MatchLiveActivityManager.shared.dismissForUserPreferenceOff() }
                    }
                }
            Text("Živá aktivita na iPhonu se může zobrazit i na Apple Watch ve Smart Stacku. Kromě tohoto přepínače je potřeba povolit živé aktivity v systémovém Nastavení iOS. Klepnutím na aktivitu se otevře záložka Hra (czechmate://game).")
                .font(.footnote)
                .foregroundStyle(.secondary)
        } header: {
            Text("Hodinky a Live Activity")
        }
    }
    #endif

    private var privacySection: some View {
        Section {
            Text("Tato verze neodesílá telemetrii ani analytiku. Data zůstávají v zařízení, pokud je sám neexportuješ.")
                .font(.footnote)
                .foregroundStyle(.secondary)
        } header: {
            Text("Soukromí")
        }
    }

    private var iCloudSection: some View {
        Section {
            Text("Synchronizace puzzle přes iCloud bude volitelná v budoucí verzi (CloudKit). Data zůstávají lokálně na zařízení.")
                .font(.footnote)
                .foregroundStyle(.secondary)
        } header: {
            Text("iCloud")
        }
    }

    // MARK: - Vývojář

    private var developerSection: some View {
        Section {
            NavigationLink {
                DeveloperSettingsView()
                    .environment(store)
            } label: {
                Label("Pokročilé a vývojář", systemImage: "ladybug")
            }
        } header: {
            Text("Diagnostika")
        } footer: {
            Text(
                "Síťové logy, FEN, ping, WebSocket, NVS na desce a ruční URL — pro ladění, ne pro běžnou hru. "
                    + "Sekci odemkneš 7× klepnutím na řádek verze v „O aplikaci“ výše."
            )
        }
    }

    private var appVersionString: String {
        let v = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
        let b = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "?"
        return "Verze \(v) (\(b))"
    }

    /// Sedm rychlých klepnutí na řádek verze (do ~0,6 s mezi klepnutími) odemkne sekci vývojáře.
    private func registerDeveloperModeTap() {
        let now = Date()
        if let last = lastDeveloperTapAt, now.timeIntervalSince(last) > 0.6 {
            developerTapCount = 0
        }
        developerTapCount += 1
        lastDeveloperTapAt = now
        guard developerTapCount >= 7 else { return }
        developerTapCount = 0
        developerModeUnlocked = true
        #if os(iOS)
        UIImpactFeedbackGenerator(style: .medium).impactOccurred()
        #endif
    }
}
