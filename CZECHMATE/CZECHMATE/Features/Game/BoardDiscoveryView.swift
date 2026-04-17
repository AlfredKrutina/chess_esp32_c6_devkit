//
//  BoardDiscoveryView.swift
//  Jednoduché vyhledání šachovnice: Bluetooth nebo Wi‑Fi (Bonjour), bez ruční IP.
//

import SwiftUI

struct BoardDiscoveryView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(BoardConnectionStore.self) private var store
    @AppStorage("czechmate.developerModeUnlocked") private var developerModeUnlocked = false
    /// Stejný klíč jako v Nastavení — při připojení přes Bluetooth neprovádět automatický přechod na HTTP přes IP desky v LAN.
    @AppStorage(BoardConnectionStore.preferBluetoothOnlyKey) private var preferBluetoothOnly = false

    @State private var lanDiscovery = LANBoardDiscoveryService()
    @State private var bleTransport = BLEBoardTransport()
    @State private var phase: Phase = .choose
    @State private var bleDevices: [DiscoveredBoardDevice] = []
    @State private var isScanningBLE = false
    @State private var isBusy = false
    @State private var statusText: String?

    private enum Phase {
        case choose
        case bleList
        case wifiList
    }

    var body: some View {
        NavigationStack {
            Group {
                switch phase {
                case .choose:
                    chooseView
                case .bleList:
                    bleListView
                case .wifiList:
                    wifiListView
                }
            }
            .navigationTitle("Najít šachovnici")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") { dismiss() }
                }
            }
            .tint(Theme.accent)
        }
    }

    private var showAdvancedHTTPDeveloperSection: Bool {
        #if DEBUG
        return true
        #else
        return developerModeUnlocked
        #endif
    }

    private var chooseView: some View {
        ScrollView {
            VStack(spacing: 22) {
                VStack(spacing: 8) {
                    Image(systemName: "link.circle.fill")
                        .font(.system(size: 40))
                        .symbolRenderingMode(.hierarchical)
                        .foregroundStyle(Theme.accent)
                    Text("Připojení k desce")
                        .font(.title3.weight(.semibold))
                    Text("Bluetooth funguje bez Wi‑Fi v telefonu — stačí zapnuté Bluetooth. Wi‑Fi v domácnosti slouží jen k vyhledání desky přes Bonjour. Ruční adresa je v Nastavení.")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
                .padding(.horizontal)
                .padding(.top, 8)

                VStack(spacing: 12) {
                    Button {
                        phase = .bleList
                        startBLEScan()
                    } label: {
                        Label("Hledat přes Bluetooth", systemImage: "dot.radiowaves.left.and.right")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 4)
                    }
                    .buttonStyle(.borderedProminent)

                    Button {
                        phase = .wifiList
                        lanDiscovery.startSearch()
                    } label: {
                        Label("Hledat přes Wi‑Fi (domácí síť)", systemImage: "wifi")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 4)
                    }
                    .buttonStyle(.bordered)
                }
                .padding(.horizontal)

                VStack(alignment: .leading, spacing: 10) {
                    Toggle("Ukázková šachovnice (bez desky)", isOn: Bindable(store).useMockBoard)
                    if store.useMockBoard {
                        Text("Stejné rozhraní jako u živé hry — vhodné bez ESP.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                .padding()
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .fill(Color(uiColor: .secondarySystemGroupedBackground))
                )
                .padding(.horizontal)

                if showAdvancedHTTPDeveloperSection {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Ladění")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.secondary)
                        NavigationLink {
                            ConnectionSettingsView()
                                .environment(store)
                        } label: {
                            Label("HTTP, WebSocket a polling", systemImage: "wrench.and.screwdriver")
                        }
                        .padding(.vertical, 10)
                        .padding(.horizontal, 12)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(
                            RoundedRectangle(cornerRadius: 12, style: .continuous)
                                .fill(Color(uiColor: .tertiarySystemGroupedBackground))
                        )
                        Text("Úplná diagnostika a NVS je v Nastavení → Vývojář a diagnostika.")
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                    }
                    .padding(.horizontal)
                }

                if let statusText {
                    Text(statusText)
                        .font(.footnote)
                        .foregroundStyle(.red)
                        .padding(.horizontal)
                }
            }
            .padding(.vertical)
        }
    }

    private var bleListView: some View {
        List {
            Section {
                Toggle(isOn: $preferBluetoothOnly) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Jen Bluetooth (nepřepínat na Wi‑Fi)")
                        Text("Vhodné bez Wi‑Fi v telefonu nebo když chceš ovládat desku výhradně přes GATT — i když je deska zároveň v domácí síti.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            } header: {
                Text("Režim připojení")
            }

            Section {
                if bleDevices.isEmpty {
                    Text(isScanningBLE ? "Hledám šachovnici poblíž…" : "Žádná deska v dosahu. Zapni Bluetooth a měj šachovnici zapnutou.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(bleDevices) { dev in
                        Button {
                            Task { await pickBLE(dev) }
                        } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(dev.displayName)
                                    if let s = dev.signalDescription {
                                        Text(s).font(.caption).foregroundStyle(.secondary)
                                    }
                                }
                                Spacer()
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                    .foregroundStyle(.tint)
                            }
                        }
                        .disabled(isBusy)
                    }
                }
            } header: {
                Text("Bluetooth")
            }
        }
        .overlay {
            connectionProgressOverlay
        }
        .onAppear {
            if bleDevices.isEmpty { startBLEScan() }
        }
        .onDisappear {
            bleTransport.stopScanning()
        }
    }

    private var wifiListView: some View {
        List {
            Section {
                if lanDiscovery.devices.isEmpty {
                    Text(lanDiscovery.isSearching ? "Hledám šachovnice v síti…" : "Nic nenalezeno. Buď ve stejné Wi‑Fi jako šachovnice.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(lanDiscovery.devices) { dev in
                        Button {
                            Task { await pickWiFi(dev) }
                        } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(dev.displayName)
                                    Text("Domácí síť / Hotspot (mDNS)").font(.caption).foregroundStyle(.secondary)
                                }
                                Spacer()
                                Image(systemName: "wifi")
                                    .foregroundStyle(.tint)
                            }
                        }
                        .disabled(isBusy)
                    }
                }
            } header: {
                Text("Automaticky (mDNS Bonjour)")
            }

            Section {
                Button {
                    Task { await pickDirectHotspot() }
                } label: {
                    HStack {
                        VStack(alignment: .leading) {
                            Text("Připojit na 192.168.4.1")
                            Text("Použij, pokud jsi připojen přímo k Wi‑Fi hotspotu šachovnice a deska se nenalezla sama.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Image(systemName: "link")
                            .foregroundStyle(.tint)
                    }
                }
                .disabled(isBusy)
            } header: {
                Text("Přímé připojení na Hotspot ESP")
            }
        }
        .overlay {
            connectionProgressOverlay
        }
        .onDisappear {
            lanDiscovery.stopSearch()
        }
    }

    private var phaseLabelForConnectionOverlay: String {
        if let p = store.pairingPhaseMessage, !p.isEmpty { return p }
        return "Připojuji…"
    }

    @ViewBuilder
    private var connectionProgressOverlay: some View {
        if isBusy {
            ZStack {
                Color.black.opacity(0.22).ignoresSafeArea()
                VStack(spacing: 14) {
                    ProgressView()
                    Text(phaseLabelForConnectionOverlay)
                        .font(.footnote)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.primary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding(22)
                .frame(maxWidth: 320)
                .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            }
        }
    }

    private func startBLEScan() {
        isScanningBLE = true
        bleDevices = []
        store.prepareBluetoothForPermissions()
        bleTransport.preparePermissions()
        bleTransport.startScanningForBoards { dev in
            if !bleDevices.contains(where: { $0.id == dev.id }) {
                bleDevices.append(dev)
            }
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 12) {
            isScanningBLE = false
        }
    }

    private func pickBLE(_ dev: DiscoveredBoardDevice) async {
        isBusy = true
        statusText = nil
        defer { isBusy = false }
        bleTransport.stopScanning()
        await store.startBLETransport(device: dev)
        if store.lastError == nil {
            store.useMockBoard = false
            dismiss()
        } else {
            statusText = store.lastError
        }
    }

    private func pickWiFi(_ dev: DiscoveredBoardDevice) async {
        isBusy = true
        statusText = nil
        defer { isBusy = false }
        store.pairingPhaseMessage = "Ověřuji adresu šachovnice v síti (Bonjour)…"
        guard let url = await lanDiscovery.resolveURL(for: dev) else {
            store.pairingPhaseMessage = nil
            statusText = "Nepodařilo se najít adresu šachovnice v síti."
            return
        }
        store.pairingPhaseMessage = "Otevírám spojení s deskou přes Wi‑Fi…"
        store.startWiFiTransport(url: url)
        store.useMockBoard = false
        dismiss()
    }

    private func pickDirectHotspot() async {
        isBusy = true
        statusText = nil
        defer { isBusy = false }
        store.pairingPhaseMessage = "Připojuji k hotspotu desky (192.168.4.1)…"
        let url = URL(string: "http://192.168.4.1")!
        store.startWiFiTransport(url: url)
        store.useMockBoard = false
        dismiss()
    }
}
