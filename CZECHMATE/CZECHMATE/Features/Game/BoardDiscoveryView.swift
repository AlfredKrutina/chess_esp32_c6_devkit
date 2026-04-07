//
//  BoardDiscoveryView.swift
//  Jednoduché vyhledání šachovnice: Bluetooth nebo Wi‑Fi (Bonjour), bez ruční IP.
//

import SwiftUI

struct BoardDiscoveryView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(BoardConnectionStore.self) private var store

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
        }
    }

    private var chooseView: some View {
        VStack(spacing: 20) {
            Text("Vyber, jak chceš desku najít. Doporučujeme Bluetooth — obvykle nemusíš nic nastavovat v routeru.")
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            Button {
                phase = .bleList
                startBLEScan()
            } label: {
                Label("Hledat přes Bluetooth", systemImage: "dot.radiowaves.left.and.right")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .padding(.horizontal)

            Button {
                phase = .wifiList
                lanDiscovery.startSearch()
            } label: {
                Label("Hledat přes Wi‑Fi v domácnosti", systemImage: "wifi")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
            .padding(.horizontal)

            Toggle("Ukázková šachovnice (bez ESP)", isOn: Bindable(store).useMockBoard)
                .padding()
            if store.useMockBoard {
                Text("Zobrazí se šachovnice, časovače a historie jako u živé hry (bez fyzické desky). Po zapnutí se stav načte hned v záložce Hra.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal)
            }

            if let statusText {
                Text(statusText)
                    .font(.footnote)
                    .foregroundStyle(.red)
            }
        }
        .padding(.vertical)
    }

    private var bleListView: some View {
        List {
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
            if isBusy { ProgressView("Připojuji…") }
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
            if isBusy { ProgressView("Připojuji…") }
        }
        .onDisappear {
            lanDiscovery.stopSearch()
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
        guard let url = await lanDiscovery.resolveURL(for: dev) else {
            statusText = "Nepodařilo se najít adresu šachovnice v síti."
            return
        }
        store.startWiFiTransport(url: url)
        store.useMockBoard = false
        dismiss()
    }

    private func pickDirectHotspot() async {
        isBusy = true
        statusText = nil
        defer { isBusy = false }
        let url = URL(string: "http://192.168.4.1")!
        store.startWiFiTransport(url: url)
        store.useMockBoard = false
        dismiss()
    }
}
