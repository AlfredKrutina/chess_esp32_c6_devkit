//
//  SettingsTabView.swift
//  CZECHMATE — čisté uživatelské Nastavení; hardware a diagnostika pod Vývojář.
//

import SwiftUI

struct SettingsTabView: View {
    @Environment(BoardConnectionStore.self) private var store
    @AppStorage("czechmate.appearance") private var appearanceRaw: String = AppAppearance.system.rawValue
    @AppStorage("czechmate.moveAnimationsEnabled") private var moveAnimationsEnabled = true
    @AppStorage("czechmate.hapticsEnabled") private var hapticsEnabled = true
    @AppStorage("czechmate.soundEffectsEnabled") private var soundEffectsEnabled = true
    @AppStorage("czechmate.showBoardCoordinates") private var showBoardCoordinates = true
    @AppStorage("czechmate.boardStyleRaw") private var boardStyleRaw: String = ChessBoardStyle.wooden.rawValue
    @State private var showIntroGuide = false
    @State private var showBoardConnectionSheet = false

    var body: some View {
        NavigationStack {
            Form {
                gameAndUISection

                boardConnectionSection

                appSection

                aboutSection

                #if os(iOS)
                watchSection
                #endif

                privacySection

                iCloudSection

                developerSection

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
        }
    }

    // MARK: - Sekce: Hra a UI

    private var gameAndUISection: some View {
        Section {
            Picker("Vzhled aplikace", selection: $appearanceRaw) {
                ForEach(AppAppearance.allCases) { a in
                    Text(a.title).tag(a.rawValue)
                }
            }
            Toggle("Animace tahů figurek", isOn: $moveAnimationsEnabled)
            Toggle("Haptická odezva", isOn: $hapticsEnabled)
            Toggle("Zvuky šachovnice", isOn: $soundEffectsEnabled)
            Toggle("Zobrazit souřadnice na desce (a–h, 1–8)", isOn: $showBoardCoordinates)
            Picker("Grafický styl šachovnice", selection: $boardStyleRaw) {
                ForEach(ChessBoardStyle.allCases) { style in
                    Text(style.title).tag(style.rawValue)
                }
            }
        } header: {
            Text("Hra a UI")
        } footer: {
            Text("Styl šachovnice zatím připravujeme — volba se uloží pro budoucí aktualizaci vzhledu polí. OLED použije tmavé schéma aplikace s černým pozadím.")
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
        } header: {
            Text("Připojení k šachovnici (CZECHMATE)")
        } footer: {
            Text("Vyhledání desky přes Bluetooth nebo Wi‑Fi bez zadávání IP. Ruční adresa jen pokud to potřebuješ.")
        }
    }

    private var connectionStatusText: String {
        guard store.isPolling else { return "Odpojeno" }
        if store.snapshot == nil {
            return "Připojeno (načítám data)"
        }
        return "Připojeno"
    }

    private var connectionStatusColor: Color {
        guard store.isPolling else { return .secondary }
        return store.snapshot == nil ? Theme.Semantic.warningForeground : Theme.Semantic.success
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
                }
            }
        } header: {
            Text("O aplikaci")
        }
    }

    #if os(iOS)
    private var watchSection: some View {
        Section {
            Text("Živá aktivita na iPhonu (Lock Screen / Dynamic Island) se může zobrazit i na Apple Watch ve Smart Stacku. Vyžaduje povolení živých aktivit v systémovém Nastavení. Klepnutím na aktivitu se otevře záložka Hra (czechmate://game).")
                .font(.footnote)
                .foregroundStyle(.secondary)
        } header: {
            Text("Hodinky a Live Activity")
        }
    }
    #endif

    private var privacySection: some View {
        Section {
            Toggle("Telemetrie (připravuje se)", isOn: .constant(false))
                .disabled(true)
            Text("Budoucí opt-in anonymní statistiky a crash reporting — vždy vypnuté, dokud nebude implementováno.")
                .font(.footnote)
                .foregroundStyle(.secondary)
        } header: {
            Text("Soukromí a TestFlight")
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
            Text("Síťové logy, FEN, ping, WebSocket, NVS na desce a ruční URL — pro ladění, ne pro běžnou hru.")
        }
    }

    private var appVersionString: String {
        let v = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
        let b = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "?"
        return "Verze \(v) (\(b))"
    }
}
