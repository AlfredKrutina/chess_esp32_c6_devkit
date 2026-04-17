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
    /// Krátký výsledek po „Otestovat lokální síť“ (onboarding — oprávnění).
    @State private var localNetworkProbeHint: String?

    init(presentation: Presentation = .firstLaunch) {
        self.presentation = presentation
    }

    private var showContinueButton: Bool {
        switch presentation {
        case .firstLaunch:
            return page < 7
        case .fromSettings:
            return page < 6
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
                subtitle: "Aplikace pro fyzickou šachovnici CZECHMATE s řídicí deskou (ESP32). Připojíš ji Bluetoothem nebo přes Wi‑Fi; v telefonu uvidíš pozici, časovač a ovládání podle toho, co dané spojení podporuje.",
                systemImage: "checkerboard.rectangle"
            )
            .tag(0)

            onboardingPage(
                title: "Jak přichází stav z desky",
                subtitle: "Pozici a čas dostává aplikace průběžně — přes BLE notifikace nebo HTTP / WebSocket ve Wi‑Fi. Rychlost závisí na síti a na vytížení desky; při dobrém spojení bývá synchronizace svižná, ale není garantovaně okamžitá.",
                systemImage: "arrow.triangle.2.circlepath"
            )
            .tag(1)

            onboardingPage(
                title: "Nápověda, LED a učící mód",
                subtitle: "Nápověda k tahu ve hře počítá nejlepší tah přes internet (Stockfish / chess-api) — potřebuješ Wi‑Fi nebo mobilní data v telefonu; deska může zůstat jen na Bluetooth. Výsledek uvidíš v aplikaci a aplikace ho zkusí nasvítit na LED desce, pokud to spojení a firmware dovolí. Učící mód je navíc: po stažení modelu AI Trenéra (Gemma) běží slovní komentáře v telefonu a nejsou nutné k základní nápovědě tahu.",
                systemImage: "lightbulb.max.fill"
            )
            .tag(2)

            onboardingPage(
                title: "Bluetooth a Wi‑Fi",
                subtitle: "V záložce Hra otevři „Najít šachovnici“. Bluetooth slouží k přímému spojení a základním příkazům. Úpravy časovače, část nastavení a práce přes webové API vyžadují, aby byl telefon ve stejné Wi‑Fi jako deska (nebo měl správně nastavenou URL). Bez fyzické desky můžeš zapnout ukázkovou šachovnici.",
                systemImage: "link.circle.fill"
            )
            .tag(3)

            onboardingBulletsPage(
                title: "Záložky aplikace",
                systemImage: "square.grid.2x2",
                items: [
                    ("Hra", "Připojení, živá pozice, časovač, tahy z obrazovky, nápověda tahu přes internet a volitelný učící mód s AI Trenérem."),
                    ("Analýza", "Přehled partie, graf hodnocení tahů (když je zapnuté zhodnocení) a volná analýza — výpočty Stockfish přes internet."),
                    ("Puzzle", "Trénink z Lichess a vestavěné sady — podle dostupnosti sítě a dat."),
                    ("Pokrok", "Lokální přehledy a statistiky z aplikace."),
                    ("Nastavení", "Adresa desky, diagnostika, znovu tento průvodce, nápověda k funkcím."),
                ]
            )
            .tag(4)

            onboardingPage(
                title: "Apple Watch",
                subtitle: "Hodinky jsou doplněk: část příkazů jde Bluetoothem přímo na desku v dosahu, část přes iPhone. Pro spolehlivé příkazy přes telefon často musí být aplikace na iPhonu aktivní. iPad jako prostředník mezi hodinkami a telefonem nefunguje.",
                systemImage: "applewatch"
            )
            .tag(5)

            onboardingPage(
                title: "Tipy na závěr",
                subtitle: "Po dokončení úvodu se může zobrazit nabídka stažení AI Trenéra — je volitelná. Časovač v aplikaci vychází z dat desky a závisí na spojení. Úplný přehled funkcí najdeš v Nastavení → Nápověda a funkce.",
                systemImage: "questionmark.circle.fill"
            )
            .tag(6)

            permissionsPage
                .tag(7)
        }
    }

    private var settingsGuideTabView: some View {
        TabView(selection: $page) {
            onboardingLeadPage(
                title: "CZECHMATE",
                subtitle: "Aplikace pro fyzickou šachovnici CZECHMATE s řídicí deskou (ESP32). Připojíš ji Bluetoothem nebo přes Wi‑Fi; v telefonu uvidíš pozici, časovač a ovládání podle toho, co dané spojení podporuje.",
                systemImage: "checkerboard.rectangle"
            )
            .tag(0)

            onboardingPage(
                title: "Jak přichází stav z desky",
                subtitle: "Pozici a čas dostává aplikace průběžně — přes BLE notifikace nebo HTTP / WebSocket ve Wi‑Fi. Rychlost závisí na síti a na vytížení desky; při dobrém spojení bývá synchronizace svižná, ale není garantovaně okamžitá.",
                systemImage: "arrow.triangle.2.circlepath"
            )
            .tag(1)

            onboardingPage(
                title: "Nápověda, LED a učící mód",
                subtitle: "Nápověda k tahu ve hře počítá nejlepší tah přes internet (Stockfish / chess-api) — potřebuješ Wi‑Fi nebo mobilní data v telefonu; deska může zůstat jen na Bluetooth. Výsledek uvidíš v aplikaci a aplikace ho zkusí nasvítit na LED desce, pokud to spojení a firmware dovolí. Učící mód je navíc: po stažení modelu AI Trenéra (Gemma) běží slovní komentáře v telefonu a nejsou nutné k základní nápovědě tahu.",
                systemImage: "lightbulb.max.fill"
            )
            .tag(2)

            onboardingPage(
                title: "Bluetooth a Wi‑Fi",
                subtitle: "V záložce Hra otevři „Najít šachovnici“. Bluetooth slouží k přímému spojení a základním příkazům. Úpravy časovače, část nastavení a práce přes webové API vyžadují, aby byl telefon ve stejné Wi‑Fi jako deska (nebo měl správně nastavenou URL). Bez fyzické desky můžeš zapnout ukázkovou šachovnici.",
                systemImage: "link.circle.fill"
            )
            .tag(3)

            onboardingBulletsPage(
                title: "Záložky aplikace",
                systemImage: "square.grid.2x2",
                items: [
                    ("Hra", "Připojení, živá pozice, časovač, tahy z obrazovky, nápověda tahu přes internet a volitelný učící mód s AI Trenérem."),
                    ("Analýza", "Přehled partie, graf hodnocení tahů (když je zapnuté zhodnocení) a volná analýza — výpočty Stockfish přes internet."),
                    ("Puzzle", "Trénink z Lichess a vestavěné sady — podle dostupnosti sítě a dat."),
                    ("Pokrok", "Lokální přehledy a statistiky z aplikace."),
                    ("Nastavení", "Adresa desky, diagnostika, tento průvodce, nápověda k funkcím."),
                ]
            )
            .tag(4)

            onboardingPage(
                title: "Apple Watch",
                subtitle: "Hodinky jsou doplněk: část příkazů jde Bluetoothem přímo na desku v dosahu, část přes iPhone. Pro spolehlivé příkazy přes telefon často musí být aplikace aktivní. iPad jako prostředník nefunguje. Na Watch je po instalaci krátký vlastní úvod.",
                systemImage: "applewatch"
            )
            .tag(5)

            settingsClosingPage
                .tag(6)
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
                Text("Časovač v aplikaci vychází z dat desky a závisí na spojení. Bez šachovnice zapni ukázkovou desku v připojení. Apple Watch mají vlastní krátký úvod po instalaci. Úplný přehled funkcí je v Nastavení → Nápověda a funkce.")
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
                Text("Pro komunikaci se šachovnicí iOS typicky vyžádá Bluetooth. Pro HTTP a WebSocket v domácí Wi‑Fi se může objevit dotaz na přístup k místní síti — bez něj telefon často neuvidí desku na lokální IP.")
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
                    action: scheduleLocalNetworkProbe
                )

                if let localNetworkProbeHint {
                    Text(localNetworkProbeHint)
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.82))
                        .fixedSize(horizontal: false, vertical: true)
                }

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

    /// Mimo inline closure v `ViewBuilder` — vyhne se chybě „async call in a function that does not support concurrency“.
    private func scheduleLocalNetworkProbe() {
        let urlString = store.baseURLString
        Task {
            let outcome = await LocalNetworkPermissionProbe.probe(baseURLString: urlString)
            await MainActor.run {
                localNetworkProbeHint = Self.describeLocalNetworkProbeOutcome(outcome)
            }
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

    private static func describeLocalNetworkProbeOutcome(_ outcome: LocalNetworkPermissionProbe.Outcome) -> String {
        switch outcome {
        case .invalidURL:
            return "Neplatná nebo prázdná adresa — nejdřív nastav základní URL desky v připojení."
        case .tcpConnected(let host, let port):
            return "TCP k \(host):\(port) proběhl — lokální síť je povolená a host odpovídá."
        case .failed(let message):
            return message
        case .timedOut:
            return "Čas vypršel — pokud iOS nabídl dialog, povol „Místní síť“ pro CZECHMATE. Jinak ověř Wi‑Fi a IP desky."
        }
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
