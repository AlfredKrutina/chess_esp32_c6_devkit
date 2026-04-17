//
//  HomeAssistantMqttSetupView.swift
//  Návod a zápis MQTT brokeru na desku (Home Assistant / Mosquitto).
//

import SwiftUI

struct HomeAssistantMqttSetupView: View {
    @Environment(BoardConnectionStore.self) private var store

    @State private var mqttHost = ""
    @State private var mqttPort = "1883"
    @State private var mqttUser = ""
    @State private var mqttPassword = ""
    @State private var mqttStatus: ESPMQTTStatusJSON?
    @State private var isBusy = false

    var body: some View {
        Form {
            Section {
                Text(
                    """
                    1. Home Assistant musí běžet ve stejné Wi‑Fi síti jako šachovnice (ESP připojené jako klient STA k routeru).

                    2. V Home Assistantu zapni MQTT broker — nejčastěji doplněk „Mosquitto broker“ (Settings → Add-ons). Poznamenej si port (obvykle 1883) a případné uživatelské jméno a heslo z konfigurace doplňku.

                    3. Zjisti IP adresu nebo hostname počítače / Raspberry Pi, kde HA běží (např. 192.168.1.42 nebo homeassistant.local). Šachovnice musí na tuto adresu v síti dosáhnout.

                    4. Níže zadej stejnou adresu jako „Broker (host)“ — jde o adresu MQTT serveru (u běžné instalace stejný stroj jako Home Assistant). Port nech 1883, pokud jsi v Mosquitto neměnil výchozí.

                    5. Uložením odešleš nastavení do firmware desky přes aktuální spojení (Wi‑Fi nebo Bluetooth). Aplikace CZECHMATE sama k brokeru nepřipojuje — data posílá jen ESP.
                    """
                )
                .font(.footnote)
                .foregroundStyle(.secondary)
            } header: {
                Text("Jak na Home Assistant")
            }

            Section {
                TextField("Broker (host), např. IP Home Assistant", text: $mqttHost)
                    .autocorrectionDisabled()
                    #if os(iOS)
                    .textInputAutocapitalization(.never)
                    .keyboardType(.URL)
                    #endif
                TextField("Port", text: $mqttPort)
                    #if os(iOS)
                    .keyboardType(.numberPad)
                    #endif
                TextField("Uživatel (volitelné)", text: $mqttUser)
                    .autocorrectionDisabled()
                SecureField("Heslo (volitelné)", text: $mqttPassword)
                Button {
                    Task {
                        isBusy = true
                        let p = Int(mqttPort.trimmingCharacters(in: .whitespaces)) ?? 1883
                        await store.postMQTTConfigToBoard(
                            host: mqttHost.trimmingCharacters(in: .whitespaces),
                            port: p,
                            username: mqttUser.isEmpty ? nil : mqttUser,
                            password: mqttPassword.isEmpty ? nil : mqttPassword
                        )
                        mqttStatus = await store.fetchMQTTStatusFromBoard()
                        isBusy = false
                    }
                } label: {
                    if isBusy {
                        ProgressView()
                    } else {
                        Label("Uložit na šachovnici", systemImage: "arrow.up.doc")
                    }
                }
                .disabled(!store.supportsOnDeviceSettingsWrite || mqttHost.trimmingCharacters(in: .whitespaces).isEmpty || isBusy)

                Button("Obnovit stav z desky") {
                    Task {
                        mqttStatus = await store.fetchMQTTStatusFromBoard()
                    }
                }
                .disabled(!store.supportsWiFiRemoteCommands)
            } header: {
                Text("MQTT broker")
            } footer: {
                Text(connectionFooter)
            }

            if let m = mqttStatus {
                Section("Stav na desce") {
                    Text("Režim: \(m.mode)")
                    Text("MQTT: \(m.mqttConnected ? "připojeno" : "nepřipojeno")")
                    Text("Wi‑Fi STA: \(m.wifiConnected ? "OK" : "bez spojení")")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .navigationTitle("Home Assistant a MQTT")
        #if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
        #endif
        .tint(Theme.accent)
        .onAppear {
            Task {
                mqttStatus = await store.fetchMQTTStatusFromBoard()
                if let m = mqttStatus {
                    mqttHost = m.host
                    mqttPort = String(m.port)
                    mqttUser = m.username
                }
            }
        }
    }

    private var connectionFooter: String {
        if store.useMockBoard {
            return "Ukázková šachovnice MQTT nepodporuje — připoj reálnou desku."
        }
        if !store.supportsOnDeviceSettingsWrite {
            return "Nejdřív připoj šachovnici (Bluetooth nebo Wi‑Fi), pak můžeš uložit broker."
        }
        if !store.supportsWiFiRemoteCommands {
            return "Broker uložíš i přes Bluetooth; stav z desky (tlačítko výše) se načte až při HTTP přes Wi‑Fi."
        }
        return "Pokud MQTT zůstane offline, zkontroluj firewall na HA, heslo Mosquitto a že ESP je ve stejné síti jako broker."
    }
}
