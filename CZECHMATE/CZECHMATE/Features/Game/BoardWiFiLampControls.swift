//
//  BoardWiFiLampControls.swift
//  CZECHMATE — Wi‑Fi stav z ESP + ovládání jasu (lampa) v Hře i v Nastavení.
//

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif
#if os(macOS)
import AppKit
#endif

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

// MARK: - Lampa (jas LED) — rozhraní ve stylu Home / Hue

private enum BoardLampUI {
    static let brightnessPresets: [Int] = [0, 25, 50, 75, 100]

    static let colorPresets: [(name: String, color: Color)] = [
        ("Teplá bílá", Color(red: 1, green: 0.92, blue: 0.78)),
        ("Žlutá", Color(red: 1, green: 1, blue: 0.35)),
        ("Oranžová", Color(red: 1, green: 0.55, blue: 0.2)),
        ("Červená", Color(red: 1, green: 0.25, blue: 0.25)),
        ("Zelená", Color(red: 0.2, green: 0.85, blue: 0.35)),
        ("Modrá", Color(red: 0.25, green: 0.5, blue: 1)),
        ("Fialová", Color(red: 0.65, green: 0.35, blue: 1)),
    ]
}

struct BoardLampBlock: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.colorScheme) private var colorScheme
    /// Skryj v `Form`, když už máš `Section` header.
    var showTitle: Bool = true
    @State private var lampBrightness: Double = 50
    @State private var lampR: Double = 255
    @State private var lampG: Double = 200
    @State private var lampB: Double = 120
    @State private var lampPickColor = Color(red: 1, green: 200 / 255, blue: 120 / 255)
    @State private var isLampBusy = false

    private var lampLightOn: Bool {
        store.snapshot?.status.lightState == true
    }

    private var powerBinding: Binding<Bool> {
        Binding(
            get: { lampLightOn },
            set: { wantOn in
                guard wantOn != lampLightOn, !isLampBusy else { return }
                Task {
                    if wantOn {
                        applyRGBFromPickColor()
                        await applyLampRGB(on: true)
                    } else {
                        await applyLampRGB(on: false)
                    }
                }
            }
        )
    }

    var body: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.l) {
            if showTitle {
                Text("Světlo desky")
                    .font(Theme.Typography.subsection())
            }

            lampHeroRow

            lampBrightnessCard

            lampMetaLines

            lampColorCard

            lampGameModeRow
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .onAppear {
            syncBrightnessFromSnapshot()
            syncRGBFromSnapshotLights()
        }
        .onChange(of: store.snapshot?.status.brightness) { _, _ in
            syncBrightnessFromSnapshot()
        }
        .onChange(of: store.snapshot?.status.lightR) { _, _ in
            syncRGBFromSnapshotLights()
        }
        .onChange(of: store.snapshot?.status.lightG) { _, _ in
            syncRGBFromSnapshotLights()
        }
        .onChange(of: store.snapshot?.status.lightB) { _, _ in
            syncRGBFromSnapshotLights()
        }
    }

    private var hasActiveTransport: Bool { store.hasActiveBoardTransport }

    private var lampLiveColor: Color {
        Color(red: lampR / 255, green: lampG / 255, blue: lampB / 255)
    }

    private var lampHeroRow: some View {
        HStack(alignment: .center, spacing: Theme.Spacing.m) {
            ZStack {
                Circle()
                    .fill(lampHeroGradient)
                    .frame(width: 56, height: 56)
                    .shadow(color: lampLiveColor.opacity(lampLightOn ? 0.5 : 0), radius: lampLightOn ? 12 : 0, y: 0)
                Image(systemName: lampLightOn ? "lightbulb.max.fill" : "lightbulb.slash.fill")
                    .font(.system(size: 22, weight: .semibold))
                    .foregroundStyle(.white)
                    .shadow(color: .black.opacity(0.2), radius: 1, y: 1)
            }
            .accessibilityHidden(true)

            VStack(alignment: .leading, spacing: 4) {
                Text("Podsvětlení")
                    .font(Theme.Typography.body().weight(.semibold))
                Text(lampStatusSubtitle)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            }
            Spacer(minLength: 0)

            if store.supportsWiFiRemoteCommands {
                Toggle("", isOn: powerBinding)
                    .labelsHidden()
                    .tint(Theme.accent)
                    .disabled(isLampBusy || !hasActiveTransport)
                    .accessibilityLabel("Světlo desky")
            }
        }
        .padding(Theme.Spacing.m)
        .background {
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(Color(uiColor: .tertiarySystemGroupedBackground))
                .overlay {
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
                }
        }
    }

    private var lampHeroGradient: LinearGradient {
        if lampLightOn {
            LinearGradient(
                colors: [lampLiveColor, lampLiveColor.opacity(0.72)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
        } else {
            LinearGradient(
                colors: [Color.gray.opacity(0.5), Color.gray.opacity(0.32)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
        }
    }

    private var lampStatusSubtitle: String {
        if store.snapshot?.status.lightState == nil {
            return "Stav z desky se načítá…"
        }
        let b = Int(lampBrightness)
        return lampLightOn ? "Zapnuto · jas \(b) %" : "Vypnuto · naposledy \(b) %"
    }

    private var lampBrightnessCard: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            HStack(alignment: .firstTextBaseline) {
                Label("Jas", systemImage: "sun.max.fill")
                    .font(Theme.Typography.subsection())
                    .foregroundStyle(.primary)
                Spacer()
                Text("\(Int(lampBrightness)) %")
                    .font(.system(size: 26, weight: .semibold, design: .rounded))
                    .monospacedDigit()
                    .foregroundStyle(Theme.accent)
            }

            Slider(
                value: $lampBrightness,
                in: 0 ... 100,
                step: 1,
                label: { Text("Jas") },
                minimumValueLabel: {
                    Image(systemName: "sun.min")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                },
                maximumValueLabel: {
                    Image(systemName: "sun.max.fill")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                },
                onEditingChanged: { editing in
                    if !editing {
                        Task { await applyBrightness() }
                    }
                }
            )
            .tint(Theme.accent)
            .disabled(isLampBusy || !hasActiveTransport)

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: Theme.Spacing.s) {
                    ForEach(BoardLampUI.brightnessPresets, id: \.self) { p in
                        Button {
                            lampBrightness = Double(p)
                            Task { await applyBrightness() }
                        } label: {
                            Text("\(p) %")
                                .font(Theme.Typography.caption().weight(.semibold))
                                .monospacedDigit()
                                .padding(.horizontal, 14)
                                .padding(.vertical, 8)
                                .background {
                                    Capsule(style: .continuous)
                                        .fill(Int(lampBrightness) == p ? Theme.accent : Theme.accentMuted)
                                }
                                .foregroundStyle(Int(lampBrightness) == p ? Color.white : Theme.accent)
                        }
                        .buttonStyle(.plain)
                        .disabled(isLampBusy || !hasActiveTransport)
                    }
                }
            }

            Text("Puštění posuvníku nebo předvolba hned odešle jas na desku.")
                .font(Theme.Typography.caption2())
                .foregroundStyle(.tertiary)
        }
        .padding(Theme.Spacing.m)
        .background {
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(Color(uiColor: .tertiarySystemGroupedBackground))
                .overlay {
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
                }
        }
    }

    private var lampColorCard: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            Text("Barva")
                .font(Theme.Typography.subsection())

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 14) {
                    ForEach(Array(BoardLampUI.colorPresets.enumerated()), id: \.offset) { _, item in
                        let selected = lampColorMatchesPreset(item.color)
                        Button {
                            lampPickColor = item.color
                            applyRGBFromPickColor()
                            if lampLightOn {
                                Task { await applyLampRGB(on: true) }
                            }
                        } label: {
                            VStack(spacing: 6) {
                                ZStack {
                                    Circle()
                                        .fill(item.color)
                                        .frame(width: 44, height: 44)
                                        .shadow(color: item.color.opacity(0.4), radius: 5, y: 2)
                                    if selected {
                                        Circle()
                                            .strokeBorder(Color.white, lineWidth: 2.5)
                                            .frame(width: 48, height: 48)
                                        Image(systemName: "checkmark")
                                            .font(.caption.weight(.bold))
                                            .foregroundStyle(.white)
                                            .shadow(radius: 1)
                                    }
                                }
                                Text(item.name)
                                    .font(Theme.Typography.caption2())
                                    .foregroundStyle(.secondary)
                                    .lineLimit(2)
                                    .multilineTextAlignment(.center)
                                    .frame(width: 76)
                            }
                        }
                        .buttonStyle(.plain)
                        .disabled(isLampBusy || !hasActiveTransport || !store.supportsWiFiRemoteCommands)
                    }
                }
                .padding(.vertical, 4)
            }

            ColorPicker(selection: $lampPickColor, supportsOpacity: false) {
                HStack(spacing: Theme.Spacing.m) {
                    Image(systemName: "eyedropper.halffull")
                        .font(.body.weight(.medium))
                        .foregroundStyle(Theme.accent)
                        .frame(width: 28)
                    Text("Vlastní odstín")
                        .font(Theme.Typography.body())
                    Spacer()
                    Image(systemName: "chevron.up.chevron.down")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.tertiary)
                }
            }
            .disabled(!hasActiveTransport || !store.supportsWiFiRemoteCommands)
            .onChange(of: lampPickColor) { _, _ in
                applyRGBFromPickColor()
            }

            Text(
                lampLightOn
                    ? "Po výběru vlastního odstínu odešli změnu tlačítkem níže."
                    : "Vlastní barva se na desku odešle při zapnutí světla."
            )
            .font(Theme.Typography.caption2())
            .foregroundStyle(.tertiary)

            if lampLightOn {
                Button {
                    Task { await applyLampRGB(on: true) }
                } label: {
                    Label("Aktualizovat barvu na desce", systemImage: "arrow.triangle.2.circlepath")
                        .font(Theme.Typography.caption().weight(.semibold))
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .tint(Theme.accent)
                .disabled(isLampBusy || !hasActiveTransport || !store.supportsWiFiRemoteCommands)
            }
        }
        .padding(Theme.Spacing.m)
        .background {
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(Color(uiColor: .tertiarySystemGroupedBackground))
                .overlay {
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
                }
        }
    }

    private func lampColorMatchesPreset(_ c: Color) -> Bool {
        #if os(iOS)
        let u1 = UIColor(c)
        let u2 = UIColor(lampPickColor)
        var r1: CGFloat = 0, g1: CGFloat = 0, b1: CGFloat = 0, a1: CGFloat = 0
        var r2: CGFloat = 0, g2: CGFloat = 0, b2: CGFloat = 0, a2: CGFloat = 0
        guard u1.getRed(&r1, green: &g1, blue: &b1, alpha: &a1),
              u2.getRed(&r2, green: &g2, blue: &b2, alpha: &a2) else { return false }
        return abs(r1 - r2) < 0.035 && abs(g1 - g2) < 0.035 && abs(b1 - b2) < 0.035
        #else
        return false
        #endif
    }

    private var lampGameModeRow: some View {
        Button {
            Task {
                isLampBusy = true
                await store.postLightGameModeOnBoard()
                isLampBusy = false
            }
        } label: {
            HStack(alignment: .center, spacing: Theme.Spacing.m) {
                Image(systemName: "checkerboard.rectangle")
                    .font(.title2)
                    .foregroundStyle(Theme.accent)
                    .frame(width: 36)
                VStack(alignment: .leading, spacing: 4) {
                    Text("Herní režim LED")
                        .font(Theme.Typography.body().weight(.semibold))
                        .foregroundStyle(.primary)
                    Text("Deska znovu zobrazuje partii a nápovědy místo čistého podsvětlení.")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 0)
                Image(systemName: "arrow.forward.circle.fill")
                    .font(.title3)
                    .foregroundStyle(Theme.accent.opacity(0.65))
            }
            .padding(Theme.Spacing.m)
        }
        .buttonStyle(.plain)
        .background {
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(Theme.accentMuted.opacity(0.65))
                .overlay {
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .stroke(Theme.accent.opacity(0.28), lineWidth: 1)
                }
        }
        .disabled(isLampBusy || !hasActiveTransport || !store.supportsWiFiRemoteCommands)
    }

    @ViewBuilder
    private var lampMetaLines: some View {
        if let st = store.snapshot?.status {
            VStack(alignment: .leading, spacing: 4) {
                if let m = st.lightMode, !m.isEmpty {
                    Label("Režim: \(m)", systemImage: "waveform.path.ecg")
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.secondary)
                }
                if let t = st.autoLampTimeoutSec {
                    Label("Auto zhasnutí po \(t) s", systemImage: "moon.zzz")
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.tertiary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private func syncBrightnessFromSnapshot() {
        if let b = store.snapshot?.status.brightness {
            lampBrightness = Double(b)
        }
    }

    private func syncRGBFromSnapshotLights() {
        if let st = store.snapshot?.status,
           let r = st.lightR, let g = st.lightG, let b = st.lightB {
            lampR = Double(r)
            lampG = Double(g)
            lampB = Double(b)
        }
        syncPickColorFromRGB()
    }

    private func syncPickColorFromRGB() {
        lampPickColor = Color(
            red: lampR / 255,
            green: lampG / 255,
            blue: lampB / 255
        )
    }

    private func applyRGBFromPickColor() {
        #if os(iOS)
        let ui = UIColor(lampPickColor)
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        guard ui.getRed(&r, green: &g, blue: &b, alpha: &a) else { return }
        lampR = Double(r * 255).rounded(.towardZero)
        lampG = Double(g * 255).rounded(.towardZero)
        lampB = Double(b * 255).rounded(.towardZero)
        #elseif os(macOS)
        let ns = NSColor(lampPickColor)
        guard let rgb = ns.usingColorSpace(.deviceRGB) else { return }
        lampR = Double(rgb.redComponent * 255).rounded(.towardZero)
        lampG = Double(rgb.greenComponent * 255).rounded(.towardZero)
        lampB = Double(rgb.blueComponent * 255).rounded(.towardZero)
        #endif
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

// MARK: - Kompaktní pruh pod šachovnicí (jas + zap/vyp + barva v sheetu)

struct BoardLampQuickStrip: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.colorScheme) private var colorScheme
    @State private var lampBrightness: Double = 50
    @State private var lampR: Double = 255
    @State private var lampG: Double = 200
    @State private var lampB: Double = 120
    @State private var isLampBusy = false
    @State private var showLampSheet = false

    private var showStrip: Bool {
        store.supportsWiFiRemoteCommands && store.hasActiveBoardTransport
    }

    private var lampLightOn: Bool {
        store.snapshot?.status.lightState == true
    }

    private var hasActiveTransport: Bool { store.hasActiveBoardTransport }

    private var lampLiveColor: Color {
        Color(red: lampR / 255, green: lampG / 255, blue: lampB / 255)
    }

    private var stripPowerBinding: Binding<Bool> {
        Binding(
            get: { lampLightOn },
            set: { wantOn in
                guard wantOn != lampLightOn, !isLampBusy else { return }
                Task { await toggleLampPower(currentlyOn: lampLightOn, turnOn: wantOn) }
            }
        )
    }

    private var stripStatusSubtitle: String {
        if store.snapshot?.status.lightState == nil {
            return "Stav se načítá…"
        }
        let b = Int(lampBrightness)
        return lampLightOn ? "Zapnuto · \(b) %" : "Vypnuto · \(b) %"
    }

    var body: some View {
        Group {
            if showStrip {
                quickStripCard
            }
        }
    }

    @ViewBuilder
    private var quickStripCard: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            quickStripHeroRow

            Divider()
                .opacity(colorScheme == .dark ? 0.35 : 0.5)

            quickStripBrightnessBlock

            Divider()
                .opacity(colorScheme == .dark ? 0.35 : 0.5)

            Button {
                showLampSheet = true
            } label: {
                HStack(spacing: Theme.Spacing.m) {
                    Image(systemName: "paintpalette.fill")
                        .font(.title3)
                        .foregroundStyle(Theme.accent)
                        .frame(width: 32)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Barva a pokročilé")
                            .font(Theme.Typography.body().weight(.semibold))
                            .foregroundStyle(.primary)
                        Text("Scény, vlastní odstín, herní režim LED")
                            .font(Theme.Typography.caption2())
                            .foregroundStyle(.secondary)
                    }
                    Spacer(minLength: 0)
                    Image(systemName: "chevron.right")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.tertiary)
                }
                .padding(.vertical, 4)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Barva, předvolby a herní režim lampy")
        }
        .padding(Theme.Spacing.m)
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
        .sheet(isPresented: $showLampSheet) { lampColorSheet }
        .onAppear {
            syncBrightnessFromSnapshot()
            syncRGBFromSnapshotLights()
        }
        .onChange(of: store.snapshot?.status.brightness) { _, _ in
            syncBrightnessFromSnapshot()
        }
        .onChange(of: store.snapshot?.status.lightR) { _, _ in
            syncRGBFromSnapshotLights()
        }
        .onChange(of: store.snapshot?.status.lightG) { _, _ in
            syncRGBFromSnapshotLights()
        }
        .onChange(of: store.snapshot?.status.lightB) { _, _ in
            syncRGBFromSnapshotLights()
        }
    }

    private var quickStripHeroRow: some View {
        HStack(alignment: .center, spacing: Theme.Spacing.m) {
            ZStack {
                Circle()
                    .fill(quickStripHeroGradient)
                    .frame(width: 48, height: 48)
                    .shadow(color: lampLiveColor.opacity(lampLightOn ? 0.45 : 0), radius: lampLightOn ? 10 : 0, y: 0)
                Image(systemName: lampLightOn ? "lightbulb.max.fill" : "lightbulb.slash.fill")
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundStyle(.white)
                    .shadow(color: .black.opacity(0.2), radius: 1, y: 1)
            }
            .accessibilityHidden(true)

            VStack(alignment: .leading, spacing: 2) {
                Text("Světlo desky")
                    .font(Theme.Typography.cardTitle())
                Text(stripStatusSubtitle)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            }
            Spacer(minLength: 0)

            Toggle("", isOn: stripPowerBinding)
                .labelsHidden()
                .tint(Theme.accent)
                .disabled(isLampBusy || !hasActiveTransport)
                .accessibilityLabel("Světlo desky")
        }
    }

    private var quickStripHeroGradient: LinearGradient {
        if lampLightOn {
            LinearGradient(
                colors: [lampLiveColor, lampLiveColor.opacity(0.75)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
        } else {
            LinearGradient(
                colors: [Color.gray.opacity(0.48), Color.gray.opacity(0.3)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
        }
    }

    private var quickStripBrightnessBlock: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            HStack {
                Label("Jas", systemImage: "sun.max.fill")
                    .font(Theme.Typography.caption().weight(.semibold))
                    .foregroundStyle(.secondary)
                Spacer()
                Text("\(Int(lampBrightness)) %")
                    .font(.system(.title3, design: .rounded).weight(.semibold))
                    .monospacedDigit()
                    .foregroundStyle(Theme.accent)
            }

            Slider(
                value: $lampBrightness,
                in: 0 ... 100,
                step: 1,
                label: { Text("Jas") },
                minimumValueLabel: {
                    Image(systemName: "sun.min")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                },
                maximumValueLabel: {
                    Image(systemName: "sun.max.fill")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                },
                onEditingChanged: { editing in
                    if !editing {
                        Task { await applyBrightness() }
                    }
                }
            )
            .tint(Theme.accent)
            .disabled(isLampBusy || !hasActiveTransport)

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: Theme.Spacing.s) {
                    ForEach(BoardLampUI.brightnessPresets, id: \.self) { p in
                        Button {
                            lampBrightness = Double(p)
                            Task { await applyBrightness() }
                        } label: {
                            Text("\(p) %")
                                .font(Theme.Typography.caption2().weight(.semibold))
                                .monospacedDigit()
                                .padding(.horizontal, 12)
                                .padding(.vertical, 7)
                                .background {
                                    Capsule(style: .continuous)
                                        .fill(Int(lampBrightness) == p ? Theme.accent : Theme.accentMuted)
                                }
                                .foregroundStyle(Int(lampBrightness) == p ? Color.white : Theme.accent)
                        }
                        .buttonStyle(.plain)
                        .disabled(isLampBusy || !hasActiveTransport)
                    }
                }
            }

            Text("Puštění posuvníku nebo předvolba odešle jas na desku.")
                .font(Theme.Typography.caption2())
                .foregroundStyle(.tertiary)
        }
    }

    private var lampColorSheet: some View {
        NavigationStack {
            ScrollView {
                BoardLampBlock(showTitle: true)
                    .padding(Theme.Spacing.l)
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .navigationTitle("Světlo desky")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Hotovo") { showLampSheet = false }
                }
            }
        }
        .tint(Theme.accent)
    }

    private func syncBrightnessFromSnapshot() {
        if let b = store.snapshot?.status.brightness {
            lampBrightness = Double(b)
        }
    }

    private func syncRGBFromSnapshotLights() {
        if let st = store.snapshot?.status,
           let r = st.lightR, let g = st.lightG, let b = st.lightB {
            lampR = Double(r)
            lampG = Double(g)
            lampB = Double(b)
        }
    }

    private func applyBrightness() async {
        isLampBusy = true
        let p = Int(lampBrightness.rounded())
        await store.setBoardBrightness(percent: min(100, max(0, p)))
        #if DEBUG
        print("[CZECHMATE][staging] BoardLampQuickStrip brightness → \(p)%")
        #endif
        isLampBusy = false
    }

    private func toggleLampPower(currentlyOn: Bool, turnOn: Bool) async {
        guard turnOn != currentlyOn else { return }
        isLampBusy = true
        await store.postLightCommandToBoard(
            state: turnOn,
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
