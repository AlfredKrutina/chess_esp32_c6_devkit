//
//  HelpView.swift
//  Nápověda pro hráče — bez technického žargonu.
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
                        Text("K fyzické šachovnici přes Bluetooth nebo Wi‑Fi")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                helpSection(
                    icon: "wifi",
                    title: "Připojení k desce",
                    body: "Nejjednodušší je vyhledání desky v záložce Hra (Bluetooth nebo Wi‑Fi). Ručně můžeš zadat adresu šachovnice v Nastavení — typicky ji najdeš v návodu k desce nebo v domácí síti (např. 192.168.x.x).\n\nAplikace zobrazuje pozici podle dat z desky; výchozí pohled odpovídá běžné šachové notaci (řada 8 nahoře, a vlevo). Bluetooth zvládne základní příkazy a náhled; úpravy časovače, část nastavení a spolehlivé HTTP příkazy obvykle potřebují, aby byl telefon ve stejné Wi‑Fi jako deska."
                )

                helpSection(
                    icon: "lock.fill",
                    title: "Když něco nejde nastavit",
                    body: "Některé šachovnice umí „zamknout“ ovládání z webu nebo aplikace — pak třeba nepůjde měnit LED nebo posílat tahy z telefonu. V takovém případě je potřeba desku odemknout podle návodu od výrobce (stejně jako u webového rozhraní)."
                )

                helpSection(
                    icon: "hand.draw.fill",
                    title: "Tahy",
                    body: "Tahy na fyzické desce se v aplikaci projeví podle spojení — obvykle po krátké prodlevě.\n\nZapneš-li tahy z obrazovky, aplikace odešle tah na desku (když to spojení a firmware dovolí). Vyhni se míchání fyzických tahů a tahů z telefonu v jednom okamžiku, ať zůstane pozice přehledná."
                )

                helpSection(
                    icon: "timer",
                    title: "Časovač",
                    body: "Čas v aplikaci vychází z údajů, které pošle šachovnice. Co přesně půjde měnit z telefonu (pauza, nová hra, předvolby), závisí na desce a na tom, jestli jsi přes Bluetooth, nebo přes Wi‑Fi s plným API."
                )

                helpSection(
                    icon: "sparkles",
                    title: "Nápověda a trenér",
                    body: "Nápověda k tahu ve hře používá engine Stockfish přes internet — potřebuješ Wi‑Fi nebo mobilní data v telefonu; samotná šachovnice může být připojená jen přes Bluetooth. Aplikace návrh zobrazí a zkusí ho nasvítit na LED, pokud to deska přijme. V Nastavení → Připojení můžeš zapnout „Nepřepínat na Wi‑Fi po Bluetooth“, když chceš desku vždy jen přes BLE a Stockfish přes mobilní síť.\n\nAI Trenér (volitelný stažený model Gemma) běží v telefonu a přidává slovní vysvětlení v učícím režimu — není potřeba k samotnému tlačítku nápovědy tahu."
                )

                helpSection(
                    icon: "safari.fill",
                    title: "Webové rozhraní desky",
                    body: "Část pokročilých funkcí (např. některá demo, MQTT, úpravy přes webový panel) zůstává v prohlížeči na adrese desky. Mobilní aplikace je zaměřená na hru, přehled partie a ovládání z telefonu; obojí můžeš podle potřeby kombinovat."
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
