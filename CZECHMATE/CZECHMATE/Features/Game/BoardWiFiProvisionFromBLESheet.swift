//
//  BoardWiFiProvisionFromBLESheet.swift
//  Zápis domácí Wi‑Fi do ESP přes BLE (`wifi_sta_config`), poté přechod na HTTP v LAN.
//

import SwiftUI

struct BoardWiFiProvisionFromBLESheet: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.dismiss) private var dismiss

    @State private var ssid: String = ""
    @State private var password: String = ""
    @State private var busy = false
    @State private var localError: String?

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("SSID sítě", text: $ssid)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                    SecureField("Heslo Wi‑Fi", text: $password)
                } header: {
                    Text("Údaje pro desku")
                } footer: {
                    Text(
                        "ESP32 podporuje Wi‑Fi 2,4 GHz. Síť jen na 5 GHz se většinou nepřipojí — použij 2,4 GHz nebo smíšený režim routeru. Heslo z telefonu nelze přečíst automaticky."
                    )
                }

                if let localError {
                    Section {
                        Text(localError)
                            .foregroundStyle(.red)
                            .font(.footnote)
                    }
                }

                Section {
                    Button {
                        Task { await submit() }
                    } label: {
                        if busy {
                            ProgressView()
                        } else {
                            Text("Uložit na desku a přepnout na Wi‑Fi")
                        }
                    }
                    .disabled(busy || ssid.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
            .navigationTitle("Wi‑Fi pro šachovnici")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Později") {
                        store.dismissWiFiProvisionOffer()
                        dismiss()
                    }
                    .disabled(busy)
                }
            }
        }
        .task {
            if ssid.isEmpty, let s = await CurrentWiFiSSIDReader.fetchCurrentSSID() {
                ssid = s
            }
        }
    }

    @MainActor
    private func submit() async {
        localError = nil
        busy = true
        defer { busy = false }
        let s = ssid.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !s.isEmpty else { return }
        if let err = await store.provisionBoardWiFiOverBLE(ssid: s, password: password) {
            localError = err
            return
        }
        store.dismissWiFiProvisionOffer()
        dismiss()
    }
}
