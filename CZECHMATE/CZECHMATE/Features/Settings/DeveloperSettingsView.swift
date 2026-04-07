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
    @State private var showDevFENDisclosure = false

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
                Text("Evaluace tahů a nápověda v Hře. FEN je technický export aktuálního snímku.")
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
            } header: {
                Text("Síť a transport")
            } footer: {
                Text("Stejné jako ruční připojení v hlavním Nastavení — zde pro rychlé ladění bez opuštění vývojářské sekce.")
            }

            Section {
                NavigationLink {
                    AdvancedConnectionDiagnosticsView()
                } label: {
                    Label("Diagnostika připojení (REST / WS)", systemImage: "waveform.path.ecg")
                }
            } header: {
                Text("Logy a metriky")
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
                .disabled(espWiFiBusy || espWiFiSSID.isEmpty || espWiFiPassword.isEmpty || !store.supportsWiFiRemoteCommands)
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
            } header: {
                Text("Wi‑Fi na šachovnici (ESP)")
            } footer: {
                Text("REST /api/wifi/* — vyžaduje HTTP k desce.")
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
    }

    private func refreshEspWiFiLine() async {
        _ = await store.fetchESPWiFiStatus()
    }
}
