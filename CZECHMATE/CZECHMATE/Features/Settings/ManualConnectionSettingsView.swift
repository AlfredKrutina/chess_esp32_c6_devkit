//
//  ManualConnectionSettingsView.swift
//  Ruční URL / hostname — pouze z hlavního Nastavení přes „Ruční nastavení připojení“.
//

import SwiftUI

struct ManualConnectionSettingsView: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        Form {
            Section {
                TextField("URL nebo hostname desky", text: Bindable(store).baseURLString)
                    .autocorrectionDisabled()
                    #if os(iOS)
                    .textInputAutocapitalization(.never)
                    .keyboardType(.URL)
                    #endif
                Button("Uložit a použít") {
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
                Text("Adresa desky")
            } footer: {
                Text("Typicky http://192.168.x.x nebo http://czechmate.local — po uložení se použije pro HTTP a WebSocket.")
            }
        }
        .navigationTitle("Ruční připojení")
        #if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
        #endif
        .tint(Theme.accent)
    }
}
