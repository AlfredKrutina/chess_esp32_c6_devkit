//
//  OnboardingView.swift
//  CZECHMATE — úvodní průvodce (první spuštění + znovu z Nastavení).
//

import SwiftUI

/// První spuštění: dokončení nastaví `czechmate.onboarding.completed`.
/// Z Nastavení: jen zavře přehled (bez resetu dokončení).
struct OnboardingView: View {
    enum Presentation {
        case firstLaunch
        case fromSettings
    }

    @Environment(BoardConnectionStore.self) private var store
    @Environment(\.dismiss) private var dismiss

    @AppStorage("czechmate.onboarding.completed") private var onboardingCompleted = false

    let presentation: Presentation

    @State private var page = 0

    init(presentation: Presentation = .firstLaunch) {
        self.presentation = presentation
    }

    private var showContinueButton: Bool {
        switch presentation {
        case .firstLaunch:
            return page < 6
        case .fromSettings:
            return page < 5
        }
    }

    var body: some View {
        Group {
            if presentation == .fromSettings {
                NavigationStack {
                    guideChrome {
                        settingsGuideTabView
                    }
                    .navigationTitle("Úvodní průvodce")
                    #if os(iOS)
                    .navigationBarTitleDisplayMode(.inline)
                    #endif
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Zavřít") { dismiss() }
                        }
                    }
                }
            } else {
                guideChrome {
                    firstLaunchGuideTabView
                }
            }
        }
    }

    @ViewBuilder
    private func guideChrome<Content: View>(@ViewBuilder content: () -> Content) -> some View {
        ZStack {
            LinearGradient(
                colors: [
                    Color(red: 0.12, green: 0.14, blue: 0.22),
                    Color(red: 0.06, green: 0.07, blue: 0.12),
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()

            VStack(spacing: 0) {
                content()
                    .tabViewStyle(.page(indexDisplayMode: .always))
                    .indexViewStyle(.page(backgroundDisplayMode: .always))

                if showContinueButton {
                    Button {
                        withAnimation { page += 1 }
                    } label: {
                        Text("Pokračovat")
                            .font(.headline)
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 14)
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Theme.accent)
                    .padding(.horizontal, 24)
                    .padding(.bottom, 28)
                    .accessibilityHint("Další stránka průvodce")
                }
            }
        }
    }

    private var firstLaunchGuideTabView: some View {
        TabView(selection: $page) {
            onboardingLeadPage(
                title: "CZECHMATE",
                subtitle: "Fyzická šachovnice propojená s telefonem — stav hry, časy a nápovědy v reálném čase.",
                systemImage: "checkerboard.rectangle"
            )
            .tag(0)

            onboardingPage(
                title: "Jedna hra, dva světy",
                subtitle: "Tahy na desce se okamžitě promítají do aplikace. Vidíš pozici, historii a hodiny stejně jako na šachovnici.",
                systemImage: "arrow.triangle.2.circlepath"
            )
            .tag(1)

            onboardingPage(
                title: "Nápovědy a LED",
                subtitle: "Stockfish navrhne tah a zvýrazní ho v aplikaci i na podsvícené desce — přehledně, bez nutnosti otevírat web v prohlížeči.",
                systemImage: "lightbulb.max.fill"
            )
            .tag(2)

            onboardingPage(
                title: "Jak připojit šachovnici",
                subtitle: "V záložce Hra klepni na „Najít šachovnici“. Můžeš hledat přes Bluetooth, nebo přes Wi‑Fi v domácí síti (Bonjour). Plné ovládání časovače a vzdálených tahů přes HTTP funguje přes Wi‑Fi k ESP.",
                systemImage: "link.circle.fill"
            )
            .tag(3)

            onboardingBulletsPage(
                title: "Záložky aplikace",
                systemImage: "square.grid.2x2",
                items: [
                    ("Hra", "Živý stav desky, časovač, šachovnice, nápověda Stockfish."),
                    ("Analýza", "Práce s pozicí a analýzou."),
                    ("Puzzle", "Trénink kombinací."),
                    ("Statistiky", "Přehled hraní a statistiky."),
                    ("Nastavení", "URL desky, diagnostika, nápověda a detaily připojení."),
                ]
            )
            .tag(4)

            onboardingPage(
                title: "Ještě pár tipů",
                subtitle: "Časovač v aplikaci odpovídá stavu z desky. Zapni „Ukázkovou šachovnici“ v Najít šachovnici, pokud chceš vyzkoumat rozhraní bez ESP. Podrobnosti vždy v Nastavení → Nápověda a funkce.",
                systemImage: "questionmark.circle.fill"
            )
            .tag(5)

            permissionsPage
                .tag(6)
        }
    }

    private var settingsGuideTabView: some View {
        TabView(selection: $page) {
            onboardingLeadPage(
                title: "CZECHMATE",
                subtitle: "Fyzická šachovnice propojená s telefonem — stav hry, časy a nápovědy v reálném čase.",
                systemImage: "checkerboard.rectangle"
            )
            .tag(0)

            onboardingPage(
                title: "Jedna hra, dva světy",
                subtitle: "Tahy na desce se okamžitě promítají do aplikace. Vidíš pozici, historii a hodiny stejně jako na šachovnici.",
                systemImage: "arrow.triangle.2.circlepath"
            )
            .tag(1)

            onboardingPage(
                title: "Nápovědy a LED",
                subtitle: "Stockfish navrhne tah a zvýrazní ho v aplikaci i na podsvícené desce — přehledně, bez nutnosti otevírat web v prohlížeči.",
                systemImage: "lightbulb.max.fill"
            )
            .tag(2)

            onboardingPage(
                title: "Jak připojit šachovnici",
                subtitle: "V záložce Hra klepni na „Najít šachovnici“. Můžeš hledat přes Bluetooth, nebo přes Wi‑Fi v domácí síti (Bonjour). Plné ovládání časovače a vzdálených tahů přes HTTP funguje přes Wi‑Fi k ESP.",
                systemImage: "link.circle.fill"
            )
            .tag(3)

            onboardingBulletsPage(
                title: "Záložky aplikace",
                systemImage: "square.grid.2x2",
                items: [
                    ("Hra", "Živý stav desky, časovač, šachovnice, nápověda Stockfish."),
                    ("Analýza", "Práce s pozicí a analýzou."),
                    ("Puzzle", "Trénink kombinací."),
                    ("Statistiky", "Přehled hraní a statistiky."),
                    ("Nastavení", "URL desky, diagnostika, nápověda a detaily připojení."),
                ]
            )
            .tag(4)

            settingsClosingPage
                .tag(5)
        }
    }

    private func onboardingLeadPage(title: String, subtitle: String, systemImage: String) -> some View {
        VStack(spacing: 24) {
            Spacer(minLength: 20)
            Image("AppLogo")
                .resizable()
                .scaledToFit()
                .frame(width: 96, height: 96)
                .clipShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
                .shadow(color: .black.opacity(0.35), radius: 12, y: 4)
            Image(systemName: systemImage)
                .font(.system(size: 44))
                .foregroundStyle(
                    LinearGradient(
                        colors: [.white.opacity(0.95), Color(red: 0.6, green: 0.85, blue: 0.65)],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
                .symbolRenderingMode(.hierarchical)
            Text(title)
                .font(.largeTitle.weight(.bold))
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
            Text(subtitle)
                .font(.body)
                .foregroundStyle(.white.opacity(0.85))
                .multilineTextAlignment(.center)
                .padding(.horizontal, 28)
            Spacer()
        }
    }

    private func onboardingPage(title: String, subtitle: String, systemImage: String) -> some View {
        VStack(spacing: 24) {
            Spacer(minLength: 20)
            Image(systemName: systemImage)
                .font(.system(size: 56))
                .foregroundStyle(
                    LinearGradient(
                        colors: [.white.opacity(0.95), Color(red: 0.6, green: 0.85, blue: 0.65)],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
                .symbolRenderingMode(.hierarchical)
            Text(title)
                .font(.title.weight(.bold))
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 12)
            Text(subtitle)
                .font(.body)
                .foregroundStyle(.white.opacity(0.85))
                .multilineTextAlignment(.center)
                .padding(.horizontal, 28)
            Spacer()
        }
    }

    private func onboardingBulletsPage(title: String, systemImage: String, items: [(String, String)]) -> some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack(spacing: 16) {
                    Image(systemName: systemImage)
                        .font(.system(size: 40))
                        .foregroundStyle(
                            LinearGradient(
                                colors: [.white.opacity(0.95), Color(red: 0.6, green: 0.85, blue: 0.65)],
                                startPoint: .top,
                                endPoint: .bottom
                            )
                        )
                    Text(title)
                        .font(.title.weight(.bold))
                        .foregroundStyle(.white)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.bottom, 4)

                ForEach(Array(items.enumerated()), id: \.offset) { _, row in
                    VStack(alignment: .leading, spacing: 6) {
                        Text(row.0)
                            .font(.headline)
                            .foregroundStyle(.white)
                        Text(row.1)
                            .font(.subheadline)
                            .foregroundStyle(.white.opacity(0.82))
                    }
                    .padding(14)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 14, style: .continuous).fill(.white.opacity(0.08)))
                }
            }
            .padding(28)
        }
    }

    private var settingsClosingPage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack(spacing: 16) {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.system(size: 40))
                        .foregroundStyle(Color(red: 0.45, green: 0.85, blue: 0.55))
                    Text("Vše připraveno")
                        .font(.title.weight(.bold))
                        .foregroundStyle(.white)
                }
                Text("Časovač v aplikaci sleduje stav z desky. Bez šachovnice zapni „Ukázkovou šachovnici“ v Najít šachovnici. Úplný popis funkcí je v Nastavení → Nápověda a funkce.")
                    .font(.body)
                    .foregroundStyle(.white.opacity(0.88))

                Button {
                    dismiss()
                } label: {
                    Text("Hotovo")
                        .font(.headline)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.borderedProminent)
                .tint(Theme.accent)
                .padding(.top, 8)
            }
            .padding(28)
        }
    }

    private var permissionsPage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                Text("Oprávnění")
                    .font(.largeTitle.weight(.bold))
                    .foregroundStyle(.white)
                Text("Aby aplikace mohla komunikovat se šachovnicí, systém potřebuje povolit přístup k Bluetooth a k zařízením v lokální síti.")
                    .foregroundStyle(.white.opacity(0.88))

                permissionBlock(
                    title: "Bluetooth",
                    text: "Slouží k přímému spojení s šachovnicí přes BLE.",
                    buttonTitle: "Připravit Bluetooth",
                    action: { store.prepareBluetoothForPermissions() }
                )

                permissionBlock(
                    title: "Lokální síť",
                    text: "HTTP a WebSocket běží v domácí Wi-Fi (např. 192.168.x.x). První spojení může vyvolat systémový dialog.",
                    buttonTitle: "Otestovat lokální síť",
                    action: {
                        LocalNetworkPermissionProbe.probe(baseURLString: store.baseURLString)
                    }
                )

                Button {
                    onboardingCompleted = true
                } label: {
                    Text("Začít používat CZECHMATE")
                        .font(.headline)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.borderedProminent)
                .tint(Theme.accent)
                .padding(.top, 8)
            }
            .padding(28)
        }
    }

    private func permissionBlock(
        title: String,
        text: String,
        buttonTitle: String,
        action: @escaping () -> Void
    ) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.headline)
                .foregroundStyle(.white)
            Text(text)
                .font(.subheadline)
                .foregroundStyle(.white.opacity(0.8))
            Button(action: action) {
                Text(buttonTitle)
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
            .tint(.white.opacity(0.9))
        }
        .padding(16)
        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(.white.opacity(0.08)))
    }
}

#Preview("První spuštění") {
    OnboardingView(presentation: .firstLaunch)
        .environment(BoardConnectionStore())
}

#Preview("Z Nastavení") {
    OnboardingView(presentation: .fromSettings)
        .environment(BoardConnectionStore())
}
