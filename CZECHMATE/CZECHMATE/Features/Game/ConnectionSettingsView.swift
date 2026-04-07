//
//  ConnectionSettingsView.swift
//  CZECHMATE
//

import SwiftUI

struct ConnectionSettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        NavigationStack {
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
                    Text("Typicky AP režim: http://192.168.4.1 — telefon ve stejné Wi-Fi jako šachovnice.")
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
            .navigationTitle("Připojení")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Hotovo") { dismiss() }
                }
            }
        }
    }
}
