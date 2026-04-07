//
//  HelpView.swift
//  Shrnutí ovládání — parita s webovým ESP rozhraním.
//

import SwiftUI

struct HelpView: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack(spacing: 14) {
                    Image("AppLogo")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 56, height: 56)
                        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                    VStack(alignment: .leading, spacing: 4) {
                        Text("CZECHMATE")
                            .font(.title2.weight(.bold))
                        Text("iOS + šachovnice ESP32")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                helpSection(
                    icon: "wifi",
                    title: "Připojení",
                    body: "Wi‑Fi: zadej URL desky (např. http://192.168.4.1 nebo http://czechmate-xxxxxx.local). Aplikace stahuje GET /api/game/snapshot a volitelně WebSocket /ws.\n\nBluetooth: rychlý náhled + nápověda LED a jas — plné příkazy (nová hra, časovač, tah přes HTTP) nejsou v BLE profilu implementované; použij Wi‑Fi."
                )

                helpSection(
                    icon: "lock.fill",
                    title: "Web lock",
                    body: "Když je na ESP zapnutý zámek webu, server vrací 403 u nápovědy LED, časovače a vzdálených tahů. Odemkni přes UART na desce (stejně jako v dokumentaci firmware)."
                )

                helpSection(
                    icon: "hand.draw.fill",
                    title: "Tahy",
                    body: "Fyzické tahy na desce se promítají do aplikace přes snapshot.\n\nTahy z iPhonu (přepínač na kartě Hra) posílají POST /api/move — může se rozcházet s reálnými figurkami, pokud někdo hýbe na desce současně."
                )

                helpSection(
                    icon: "timer",
                    title: "Časovač",
                    body: "Zobrazení času vychází ze snapshotu. Ovládací tlačítka (pauza, pokračovat, reset) volají stejné endpointy jako web: /api/timer/pause, resume, reset. Konfigurace času (/api/timer/config) lze rozšířit v nastavení v další verzi."
                )

                helpSection(
                    icon: "sparkles",
                    title: "Nápověda Stockfish",
                    body: "Vyžaduje internet. Po výpočtu se pošle POST /api/game/hint_highlight na desku (LED)."
                )

                helpSection(
                    icon: "safari.fill",
                    title: "Web rozhraní desky",
                    body: "Plné UI (puzzle, demo, lampa Home Assistant, MQTT, …) zůstává v prohlížeči na IP AP 192.168.4.1 nebo na LAN URL. Tato aplikace pokrývá hlavní herní a diagnostické funkce přes stejné REST API."
                )
            }
            .padding()
        }
        .navigationTitle("Nápověda")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button("Hotovo") { dismiss() }
                    .fontWeight(.semibold)
            }
        }
    }

    private func helpSection(icon: String, title: String, body: String) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline, spacing: 12) {
                Image(systemName: icon)
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundStyle(Theme.accent)
                    .frame(width: 28, alignment: .center)
                    .accessibilityHidden(true)
                Text(title)
                    .font(.system(.headline, design: .rounded))
            }
            Text(body)
                .font(.system(.body, design: .rounded))
                .foregroundStyle(.primary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .background {
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(Theme.accentMuted)
        }
        .overlay {
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .strokeBorder(Color.primary.opacity(0.06), lineWidth: 1)
        }
    }
}

#Preview {
    NavigationStack {
        HelpView()
    }
}
