//
//  ConnectionSettingsView.swift
//  CZECHMATE
//

import SwiftUI

/// HTTP URL, WebSocket a polling — pro vývojáře / ladění; běžné připojení je `BoardDiscoveryView`.
struct ConnectionSettingsView: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        Form {
            Section {
                TextField("Základní URL", text: Bindable(store).baseURLString)
                    .autocorrectionDisabled()
                    #if os(iOS)
                    .textInputAutocapitalization(.never)
                    .keyboardType(.URL)
                    #endif
                Button("Uložit nastavení") {
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
            } header: {
                Text("HTTP / WebSocket")
            } footer: {
                Text("Typicky AP režim: http://192.168.4.1 — telefon ve stejné Wi‑Fi jako šachovnice.")
            }

            Section {
                Toggle("WebSocket + REST watchdog", isOn: Bindable(store).useWebSocket)
            } footer: {
                Text("Při živém WebSocket se REST dotazuje zřídka jako záloha.")
            }

            if let err = store.lastError {
                Section("Chyba") {
                    Text(err)
                        .foregroundStyle(.red)
                }
            }
        }
        .navigationTitle("HTTP a WebSocket")
        #if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
        #endif
        .tint(Theme.accent)
    }
}
