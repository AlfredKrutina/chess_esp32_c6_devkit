//
//  BoardDeviceFeaturesView.swift
//  Parita s webem: NVS prefs přes HTTP; demo / MQTT / auto lampa / LED guidance i přes BLE (`supportsOnDeviceSettingsWrite`).
//

import SwiftUI

struct BoardDeviceFeaturesView: View {
    @Environment(BoardConnectionStore.self) private var store
    @State private var isBusy = false
    @State private var mqttHost = ""
    @State private var mqttPort = "1883"
    @State private var mqttUser = ""
    @State private var mqttPassword = ""
    @State private var mqttStatus: ESPMQTTStatusJSON?
    @State private var demoEnabled = false
    @State private var demoSpeedMs: Double = 800
    @State private var demoStatus: ESPDemoStatusJSON?
    @State private var autoLampSeconds: Double = 300

    var body: some View {
        Form {
            Section {
                Button {
                    Task {
                        isBusy = true
                        await store.syncBoardUIPrefsFromDevice()
                        isBusy = false
                    }
                } label: {
                    if isBusy {
                        ProgressView()
                    } else {
                        Label("Načíst z desky (GET)", systemImage: "arrow.down.circle")
                    }
                }
                .disabled(!store.supportsWiFiRemoteCommands || isBusy)

                Button {
                    Task {
                        isBusy = true
                        await store.pushCurrentPrefsToBoard()
                        isBusy = false
                    }
                } label: {
                    Label("Uložit do desky (POST)", systemImage: "arrow.up.circle.fill")
                }
                .disabled(!store.supportsWiFiRemoteCommands || isBusy)
            } header: {
                Text("Synchronizace NVS")
            } footer: {
                Text(
                    store.supportsWiFiRemoteCommands
                        ? "Stejná data jako webové UI (`/api/settings/ui`). Po načtení se sloučí hloubka nápovědy a eval s tímto iPhonem."
                        : "Hromadné GET/POST `/api/settings/ui` potřebuje HTTP (Wi‑Fi) k desce. Při spojení jen přes Bluetooth použij jednotlivé přepínače a tlačítka v této obrazovce (GATT), případně dočasně přepni na Wi‑Fi v Najít šachovnici."
                )
            }

            Section {
                Picker("Síla nápovědy v aplikaci", selection: Bindable(store).moveHintTier) {
                    ForEach(MoveHintTier.allCases) { tier in
                        Text(tier.shortTitle).tag(tier)
                    }
                }
                Stepper("Hloubka nápovědy: \(store.boardUIPrefsPayload.chessHintDepth)", value: Bindable(store).boardUIPrefsPayload.chessHintDepth, in: 1 ... 18)
                Toggle("Zhodnocení tahu (eval)", isOn: Bindable(store).boardUIPrefsPayload.chessEvaluateMove)
                Stepper("Limit nápověd / stranu (0 = neomezeno): \(store.boardUIPrefsPayload.chessHintLimit)", value: Bindable(store).boardUIPrefsPayload.chessHintLimit, in: 0 ... 99)
                Toggle("Odměna +1 za nejlepší tah", isOn: Bindable(store).boardUIPrefsPayload.chessHintAwardBest)
                Toggle("Odměna +1 za dobrý tah", isOn: Bindable(store).boardUIPrefsPayload.chessHintAwardGood)
                Toggle("Odměna +1 za braní", isOn: Bindable(store).boardUIPrefsPayload.chessHintAwardCapture)
                Toggle("Zobrazovat statistiky (výukový přehled)", isOn: Bindable(store).boardUIPrefsPayload.chessShowHintStats)
                Toggle("Potvrdit novou hru", isOn: Bindable(store).boardUIPrefsPayload.chess_confirm_new_game)
                Toggle("Výuky / tutoriály (stav na desce)", isOn: Bindable(store).boardUIPrefsPayload.chessTutorialsEnabled)
                Toggle("Bot: LED cíl jen po zvednutí figurky", isOn: Bindable(store).boardUIPrefsPayload.chessBotLedTargetOnlyAfterLift)
                Picker("Režim hry", selection: Bindable(store).boardUIPrefsPayload.botSettings.mode) {
                    Text("Člověk vs člověk").tag("pvp")
                    Text("Proti botu (návrh tahu)").tag("bot")
                }
                Picker("Síla bota", selection: Bindable(store).boardUIPrefsPayload.botSettings.strength) {
                    ForEach(["6", "8", "10", "12", "14", "16"], id: \.self) { s in
                        Text(s).tag(s)
                    }
                }
                Picker("Strana bota", selection: Bindable(store).boardUIPrefsPayload.botSettings.side) {
                    Text("Bílý").tag("white")
                    Text("Černý").tag("black")
                }
            } header: {
                Text("Šachová pravidla v NVS")
            } footer: {
                Text("H1–H3 je jen v aplikaci (cíl / výchozí pole / celý tah). Ostatní změny ulož tlačítkem „Uložit do desky“. Hloubka výpočtu z Nastavení → Diagnostika → Stockfish a FEN se při POST sloučí.")
            }

            Section {
                Stepper("LED nápověda (1–5): \(Int(store.boardUIPrefsPayload.ledGuidanceLevelFallback))", value: ledGuidanceBinding, in: 1 ... 5)
                Toggle("Navedení při braní (guided capture)", isOn: guidedCaptureBinding)
            } header: {
                Text("LED na desce")
            } footer: {
                Text("Odesílá POST na `/api/settings/led_guidance` a `/api/settings/guided_hints`. Aktuální stav je ve snapshotu.")
            }

            Section {
                Slider(value: $autoLampSeconds, in: 5 ... 7200, step: 1) {
                    Text("Auto zhasnutí")
                }
                Text("\(Int(autoLampSeconds)) s")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Button("Aplikovat auto zhasnutí") {
                    Task {
                        isBusy = true
                        await store.postAutoLampTimeoutToBoard(seconds: Int(autoLampSeconds.rounded()))
                        isBusy = false
                    }
                }
                .disabled(!store.supportsOnDeviceSettingsWrite || isBusy)
            } header: {
                Text("Lampa — časovač nečinnosti")
            }

            Section {
                TextField("Broker (host)", text: $mqttHost)
                    .autocorrectionDisabled()
                TextField("Port", text: $mqttPort)
                    #if os(iOS)
                    .keyboardType(.numberPad)
                    #endif
                TextField("Uživatel (volitelné)", text: $mqttUser)
                SecureField("Heslo (volitelné)", text: $mqttPassword)
                Button("Uložit MQTT a zkusit reconnect") {
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
                }
                .disabled(!store.supportsOnDeviceSettingsWrite || mqttHost.isEmpty || isBusy)
                if let m = mqttStatus {
                    Text("Stav: \(m.mode) · MQTT \(m.mqttConnected ? "OK" : "offline") · Wi‑Fi \(m.wifiConnected ? "OK" : "bez STA")")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                Button("Obnovit stav MQTT") {
                    Task {
                        mqttStatus = await store.fetchMQTTStatusFromBoard()
                    }
                }
                .disabled(!store.supportsWiFiRemoteCommands)
            } header: {
                Text("MQTT (Home Assistant / broker)")
            } footer: {
                Text("Krok za krokem a uživatelský návod: Nastavení → Chytrá domácnost → Home Assistant a MQTT.")
            }

            Section {
                Toggle("Demo zapnuto (konfig)", isOn: $demoEnabled)
                Slider(value: $demoSpeedMs, in: 200 ... 3000, step: 50) {
                    Text("Rychlost")
                }
                Text("\(Int(demoSpeedMs)) ms")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Button("Odeslat konfiguraci demo") {
                    Task {
                        isBusy = true
                        await store.postDemoConfigToBoard(enabled: demoEnabled, speedMs: UInt32(demoSpeedMs.rounded()))
                        demoStatus = await store.fetchDemoStatusFromBoard()
                        isBusy = false
                    }
                }
                .disabled(!store.supportsOnDeviceSettingsWrite || isBusy)
                Button("Spustit demo") {
                    Task {
                        isBusy = true
                        await store.postDemoStartOnBoard()
                        demoStatus = await store.fetchDemoStatusFromBoard()
                        isBusy = false
                    }
                }
                .disabled(!store.supportsOnDeviceSettingsWrite || isBusy)
                if let d = demoStatus {
                    Text("Demo aktivní: \(d.enabled ? "ano" : "ne")")
                        .font(.footnote)
                }
            } header: {
                Text("Demo režim")
            }
        }
        .navigationTitle("Deska (web parity)")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            store.hintDepth = store.boardUIPrefsPayload.chessHintDepth
            store.moveEvaluationEnabled = store.boardUIPrefsPayload.chessEvaluateMove
            if let t = store.snapshot?.status.autoLampTimeoutSec {
                autoLampSeconds = Double(t)
            }
            Task {
                mqttStatus = await store.fetchMQTTStatusFromBoard()
                demoStatus = await store.fetchDemoStatusFromBoard()
                if let m = mqttStatus {
                    mqttHost = m.host
                    mqttPort = String(m.port)
                    mqttUser = m.username
                }
            }
        }
    }

    private var ledGuidanceBinding: Binding<Int> {
        Binding(
            get: { store.snapshot?.status.ledGuidanceLevel ?? store.boardUIPrefsPayload.ledGuidanceLevelFallback },
            set: { store.boardUIPrefsPayload.ledGuidanceLevelFallback = $0 }
        )
    }

    private var guidedCaptureBinding: Binding<Bool> {
        Binding(
            get: { store.snapshot?.status.guidedCaptureHintsEnabled ?? false },
            set: { store.boardUIPrefsPayload.guidedCapturePreferred = $0 }
        )
    }
}

private extension BoardUIPrefsPayload {
    /// Vedlejší úložiště pro slider, dokud nepošleme POST (status nemusí mít led level v prefs).
    var ledGuidanceLevelFallback: Int {
        get {
            UserDefaults.standard.integer(forKey: "czechmate.ledGuidanceLevel")
        }
        set {
            UserDefaults.standard.set(newValue, forKey: "czechmate.ledGuidanceLevel")
        }
    }

    var guidedCapturePreferred: Bool {
        get { UserDefaults.standard.bool(forKey: "czechmate.guidedCapturePref") }
        set { UserDefaults.standard.set(newValue, forKey: "czechmate.guidedCapturePref") }
    }
}
