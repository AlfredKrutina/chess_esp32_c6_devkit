//
//  DeveloperSettingsView.swift
//  Diagnostika, Stockfish hloubka, FEN, NVS/MQTT, Wi‑Fi ESP — mimo běžné Nastavení.
//

import SwiftUI

struct DeveloperSettingsView: View {
    @Environment(BoardConnectionStore.self) private var store
    @State private var pingResult: String?
    @State private var isPingBusy = false
    @State private var espWiFiSSID = ""
    @State private var espWiFiPassword = ""
    @State private var espWiFiBusy = false
    @State private var showFactoryResetConfirm = false
    @State private var showDevFENDisclosure = false
    @AppStorage(BoardConnectionStore.preferBluetoothOnlyKey) private var preferBluetoothOnly = false
    @AppStorage(AppDebugLog.coachTraceLogsDefaultsKey) private var coachTraceLogs = false

    var body: some View {
        Form {
            Section {
                Toggle("Automatické zhodnocení tahů (Stockfish)", isOn: Bindable(store).moveEvaluationEnabled)
                Stepper("Hloubka Stockfish: \(store.hintDepth)", value: Bindable(store).hintDepth, in: 1 ... 18)
                if let s = store.snapshot,
                   let fen = FENBuilder.boardAndStatusToFEN(board: s.board, status: s.status, history: s.history) {
                    DisclosureGroup(isExpanded: $showDevFENDisclosure) {
                        Text(fen)
                            .font(.system(.caption, design: .monospaced))
                            .textSelection(.enabled)
                    } label: {
                        Label("Surový zápis pozice (FEN)", systemImage: "doc.plaintext")
                    }
                }
            } header: {
                Text("Stockfish a FEN")
            } footer: {
                Text("Automatické hodnocení tahů a tlačítko nápovědy ve hře používají Stockfish přes internet. FEN je technický zápis aktuální pozice.")
            }

            Section {
                TextField("Základní URL", text: Bindable(store).baseURLString)
                    .autocorrectionDisabled()
                    #if os(iOS)
                    .textInputAutocapitalization(.never)
                    .keyboardType(.URL)
                    #endif
                Button("Uložit URL") {
                    store.saveSettings()
                }
                HStack {
                    Button(store.isPolling ? "Polling…" : "Spustit připojení") {
                        store.startPolling()
                    }
                    .disabled(store.isPolling)
                    Button("Zastavit") {
                        store.stopPolling(notifyWatchDisconnect: true)
                    }
                }
                Button {
                    Task {
                        isPingBusy = true
                        pingResult = await store.pingBoardRTT()
                        isPingBusy = false
                    }
                } label: {
                    if isPingBusy {
                        ProgressView()
                    } else {
                        Text("Ping desky (GET snapshot)")
                    }
                }
                if let pingResult {
                    Text(pingResult)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                Toggle("WebSocket + REST watchdog", isOn: Bindable(store).useWebSocket)
                Toggle(
                    "Vynutit jen Bluetooth (nepřepínat na Wi‑Fi po STA)",
                    isOn: $preferBluetoothOnly
                )
            } header: {
                Text("Síť a transport")
            } footer: {
                Text(
                    "Stejný přepínač „Nepřepínat na Wi‑Fi“ je i v Nastavení → Připojení. Zde pro rychlé ladění bez opuštění vývojářské sekce. "
                        + "Volba „jen Bluetooth“ zabrání automatickému přechodu na HTTP, když deska nabídne IP v domácí síti — příkazy zůstanou na GATT."
                )
            }

            Section {
                Toggle("Podrobné logy (trenér / model)", isOn: $coachTraceLogs)
                NavigationLink {
                    AdvancedConnectionDiagnosticsView()
                } label: {
                    Label("Diagnostika připojení (REST / WS)", systemImage: "waveform.path.ecg")
                }
            } header: {
                Text("Logy a metriky")
            } footer: {
                Text(
                    "Při zapnutí se v konzoli Xcode nebo v zařízení (Console.app) tisknou značky [coach] u AI trenéra, stažení modelu a MediaPipe. V ladění se navíc vždy tiskne [staging:coach]."
                )
            }

            Section {
                NavigationLink {
                    BoardDeviceFeaturesView()
                        .environment(store)
                } label: {
                    Label("Deska — NVS, MQTT, demo", systemImage: "cpu")
                }
            } header: {
                Text("Firmware a příkazy")
            } footer: {
                Text("Parita s webovým panelem ESP: NVS, broker, demo.")
            }

            Section {
                BoardLampBlock(showTitle: false)
            } header: {
                Text("Deska — LED")
            }

            Section {
                TextField("SSID (domácí Wi‑Fi pro ESP)", text: $espWiFiSSID)
                    .autocorrectionDisabled()
                SecureField("Heslo", text: $espWiFiPassword)
                Button("Uložit a připojit ESP (STA)") {
                    Task {
                        espWiFiBusy = true
                        await store.saveESPWiFiCredentialsAndConnect(ssid: espWiFiSSID, password: espWiFiPassword)
                        espWiFiBusy = false
                        await refreshEspWiFiLine()
                    }
                }
                .disabled(espWiFiBusy || espWiFiSSID.isEmpty || espWiFiPassword.isEmpty || !store.canConfigureBoardWiFiCredentials)
                Button("Odpojit ESP od STA") {
                    Task {
                        espWiFiBusy = true
                        await store.disconnectESPWiFiSTA()
                        espWiFiBusy = false
                        await refreshEspWiFiLine()
                    }
                }
                .disabled(espWiFiBusy || !store.supportsWiFiRemoteCommands)
                Button("Smazat uložené WiFi z NVS") {
                    Task {
                        espWiFiBusy = true
                        await store.clearESPWiFiCredentials()
                        espWiFiBusy = false
                        await refreshEspWiFiLine()
                    }
                }
                .disabled(espWiFiBusy || !store.supportsWiFiRemoteCommands)
                ESPWiFiStatusBlock(showTitle: false)
                Button("Tovární reset desky (celá NVS)…", role: .destructive) {
                    showFactoryResetConfirm = true
                }
                .disabled(espWiFiBusy || !store.supportsFactoryReset)
            } header: {
                Text("Wi‑Fi na šachovnici (ESP)")
            } footer: {
                Text(
                    "Uložit STA: HTTP (`/api/wifi/*`) nebo Bluetooth (`wifi_sta_config`). "
                        + "Tovární reset smaže celý NVS oddíl (včetně Wi‑Fi, MQTT, partie) — `POST /api/system/factory_reset` nebo BLE `factory_reset` s potvrzením."
                )
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
        .navigationTitle("Vývojář a diagnostika")
        #if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
        #endif
        .tint(Theme.accent)
        .onAppear {
            Task { await refreshEspWiFiLine() }
        }
        .confirmationDialog(
            "Smazat celou NVS na desce a restartovat?",
            isPresented: $showFactoryResetConfirm,
            titleVisibility: .visible
        ) {
            Button("Zrušit", role: .cancel) {}
            Button("Smazat celou NVS", role: .destructive) {
                Task {
                    espWiFiBusy = true
                    await store.factoryResetBoard()
                    espWiFiBusy = false
                    await refreshEspWiFiLine()
                }
            }
        } message: {
            Text("Stejné jako na webu v záložce Nastavení → Síť. Po úspěchu deska zmizí z Wi‑Fi a naběhne znovu.")
        }
    }

    private func refreshEspWiFiLine() async {
        _ = await store.fetchESPWiFiStatus()
    }
}
