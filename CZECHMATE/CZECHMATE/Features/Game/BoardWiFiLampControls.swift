//
//  BoardWiFiLampControls.swift
//  CZECHMATE — Wi‑Fi stav z ESP + ovládání jasu (lampa) v Hře i v Nastavení.
//

import SwiftUI

// MARK: - Wi‑Fi (GET /api/wifi/status)

struct ESPWiFiStatusBlock: View {
    @Environment(BoardConnectionStore.self) private var store
    /// V `Form` sekci často stačí jen řádky — nadpis je v `Section` headeru.
    var showTitle: Bool = true
    @State private var isWiFiRefreshBusy = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .center) {
                if showTitle {
                    Text("Wi‑Fi na šachovnici")
                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                }
                Spacer()
                Button {
                    Task { await refreshWiFi() }
                } label: {
                    if isWiFiRefreshBusy {
                        ProgressView()
                    } else {
                        Text("Obnovit")
                            .font(.system(.caption, design: .rounded))
                    }
                }
                .disabled(isWiFiRefreshBusy || !store.supportsWiFiRemoteCommands)
            }

            if !store.supportsWiFiRemoteCommands {
                Text("Úplný přehled AP/STA je dostupný po HTTP připojení k desce (Wi‑Fi / LAN).")
                    .font(.system(.caption, design: .rounded))
                    .foregroundStyle(.secondary)
            } else if let s = store.espWiFiStatusCache {
                wifiDetailRows(s)
                if let t = store.espWiFiStatusUpdatedAt {
                    Text("Aktualizováno \(t.formatted(date: .abbreviated, time: .shortened))")
                        .font(.system(.caption2, design: .rounded))
                        .foregroundStyle(.tertiary)
                }
            } else {
                Text("Stav zatím nenačten — klepni na Obnovit.")
                    .font(.system(.caption, design: .rounded))
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    @ViewBuilder
    private func wifiDetailRows(_ s: ESPWiFiStatusJSON) -> some View {
        statusRow("Domácí síť (STA)", s.staSsid.isEmpty ? "—" : s.staSsid)
        statusRow("IP šachovnice", s.staIp.isEmpty ? "—" : s.staIp)
        HStack(spacing: 6) {
            Image(systemName: s.staConnected ? "checkmark.circle.fill" : "xmark.circle.fill")
                .foregroundStyle(s.staConnected ? Color.green : Color.orange)
            Text(s.staConnected ? "STA připojeno" : "STA nepřipojeno")
                .font(.system(.caption, design: .rounded))
        }
        statusRow("AP (SSID)", s.apSsid.isEmpty ? "—" : s.apSsid)
        statusRow("AP IP", s.apIp.isEmpty ? "—" : s.apIp)
        statusRow("Klienti na AP", "\(s.apClients)")
        HStack(spacing: 10) {
            Label(s.online ? "Online" : "Offline", systemImage: "circle.fill")
                .font(.system(.caption2, design: .rounded))
                .foregroundStyle(s.online ? Color.secondary : Color.orange)
            if s.locked {
                Label("Zámek", systemImage: "lock.fill")
                    .font(.system(.caption2, design: .rounded))
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func statusRow(_ title: String, _ value: String) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title)
                .font(.system(.caption, design: .rounded))
                .foregroundStyle(.secondary)
                .frame(minWidth: 130, alignment: .leading)
            Text(value)
                .font(.system(.caption, design: .rounded))
                .fontWeight(.medium)
            Spacer(minLength: 0)
        }
    }

    private func refreshWiFi() async {
        isWiFiRefreshBusy = true
        _ = await store.fetchESPWiFiStatus()
        isWiFiRefreshBusy = false
    }
}

// MARK: - Lampa (jas LED)

struct BoardLampBlock: View {
    @Environment(BoardConnectionStore.self) private var store
    /// Skryj v `Form`, když už máš `Section` header.
    var showTitle: Bool = true
    @State private var lampBrightness: Double = 50
    @State private var lampR: Double = 255
    @State private var lampG: Double = 200
    @State private var lampB: Double = 120
    @State private var isLampBusy = false

    private let presets: [Int] = [0, 25, 50, 75, 100]

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            if showTitle {
                Text("Lampa (LED podsvětlení)")
                    .font(.system(.subheadline, design: .rounded).weight(.semibold))
            }

            Slider(value: $lampBrightness, in: 0 ... 100, step: 1) {
                Text("Jas")
            }
            HStack {
                Text("\(Int(lampBrightness)) %")
                    .font(.system(.caption, design: .rounded).monospacedDigit())
                    .foregroundStyle(.secondary)
                Spacer()
            }

            LazyVGrid(columns: [GridItem(.adaptive(minimum: 52), spacing: 8)], spacing: 8) {
                ForEach(presets, id: \.self) { p in
                    Button("\(p) %") {
                        lampBrightness = Double(p)
                        Task { await applyBrightness() }
                    }
                    .buttonStyle(.bordered)
                    .font(.system(.caption2, design: .rounded))
                    .disabled(isLampBusy || !hasActiveTransport)
                }
            }

            Button {
                Task { await applyBrightness() }
            } label: {
                if isLampBusy {
                    ProgressView()
                        .frame(maxWidth: .infinity)
                } else {
                    Text("Aplikovat jas na desku")
                        .frame(maxWidth: .infinity)
                }
            }
            .buttonStyle(.themePrimary)
            .disabled(isLampBusy || !hasActiveTransport)

            lampMetaLines

            Text("Režim lampa (RGB) — jako web")
                .font(.system(.caption, design: .rounded).weight(.semibold))
                .padding(.top, 8)
            HStack {
                Text("R").frame(width: 16, alignment: .leading)
                Slider(value: $lampR, in: 0 ... 255, step: 1)
                Text("\(Int(lampR))")
                    .font(.caption2.monospacedDigit())
                    .frame(width: 34, alignment: .trailing)
            }
            HStack {
                Text("G").frame(width: 16, alignment: .leading)
                Slider(value: $lampG, in: 0 ... 255, step: 1)
                Text("\(Int(lampG))")
                    .font(.caption2.monospacedDigit())
                    .frame(width: 34, alignment: .trailing)
            }
            HStack {
                Text("B").frame(width: 16, alignment: .leading)
                Slider(value: $lampB, in: 0 ... 255, step: 1)
                Text("\(Int(lampB))")
                    .font(.caption2.monospacedDigit())
                    .frame(width: 34, alignment: .trailing)
            }
            HStack(spacing: 10) {
                Button("Zapnout lampu (RGB)") {
                    Task { await applyLampRGB(on: true) }
                }
                .buttonStyle(.borderedProminent)
                .disabled(isLampBusy || !hasActiveTransport || !store.supportsWiFiRemoteCommands)
                Button("Vypnout") {
                    Task { await applyLampRGB(on: false) }
                }
                .buttonStyle(.bordered)
                .disabled(isLampBusy || !hasActiveTransport || !store.supportsWiFiRemoteCommands)
            }
            Button("Zpět do herního režimu LED") {
                Task {
                    isLampBusy = true
                    await store.postLightGameModeOnBoard()
                    isLampBusy = false
                }
            }
            .buttonStyle(.themeSecondary)
            .disabled(isLampBusy || !hasActiveTransport || !store.supportsWiFiRemoteCommands)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .onAppear { syncBrightnessFromSnapshot() }
        .onChange(of: store.snapshot?.status.brightness) { _, _ in
            syncBrightnessFromSnapshot()
        }
    }

    private var hasActiveTransport: Bool { store.hasActiveBoardTransport }

    @ViewBuilder
    private var lampMetaLines: some View {
        if let st = store.snapshot?.status {
            if let m = st.lightMode, !m.isEmpty {
                Text("Režim světla: \(m)")
                    .font(.system(.caption2, design: .rounded))
                    .foregroundStyle(.secondary)
            }
            if let on = st.lightState {
                Text(on ? "Světlo: zapnuto" : "Světlo: vypnuto")
                    .font(.system(.caption2, design: .rounded))
                    .foregroundStyle(.secondary)
            }
            if let r = st.lightR, let g = st.lightG, let b = st.lightB {
                Text("RGB: \(r), \(g), \(b)")
                    .font(.system(.caption2, design: .rounded))
                    .foregroundStyle(.tertiary)
            }
            if let t = st.autoLampTimeoutSec {
                Text("Auto zhasnutí: \(t) s")
                    .font(.system(.caption2, design: .rounded))
                    .foregroundStyle(.tertiary)
            }
        }
    }

    private func syncBrightnessFromSnapshot() {
        if let b = store.snapshot?.status.brightness {
            lampBrightness = Double(b)
        }
    }

    private func applyBrightness() async {
        isLampBusy = true
        let p = Int(lampBrightness.rounded())
        await store.setBoardBrightness(percent: min(100, max(0, p)))
        #if DEBUG
        print("[CZECHMATE][staging] Board brightness POST → \(p)%")
        #endif
        isLampBusy = false
    }

    private func applyLampRGB(on: Bool) async {
        isLampBusy = true
        await store.postLightCommandToBoard(
            state: on,
            r: Int(lampR.rounded()),
            g: Int(lampG.rounded()),
            b: Int(lampB.rounded())
        )
        isLampBusy = false
    }
}

// MARK: - Karta v pokročilých ovládáních (Hra)

struct BoardWiFiLampSection: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Síť a lampa")
                .font(.system(.subheadline, design: .rounded).weight(.semibold))
            ESPWiFiStatusBlock()
            BoardLampBlock()
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
        .task {
            if store.supportsWiFiRemoteCommands {
                _ = await store.fetchESPWiFiStatus()
            }
        }
    }
}
