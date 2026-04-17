//
//  WatchOnboardingView.swift
//  CZECHMATE_W — krátký úvod bez scrollování (stránky = přejetí).
//

import SwiftUI

// MARK: - Znovu spustit úvod z „Více“

struct WatchIntroResetKey: EnvironmentKey {
    static let defaultValue: () -> Void = {}
}

extension EnvironmentValues {
    var watchIntroReset: () -> Void {
        get { self[WatchIntroResetKey.self] }
        set { self[WatchIntroResetKey.self] = newValue }
    }
}

// MARK: - Průvodce

struct WatchOnboardingView: View {
    let onFinished: () -> Void

    @State private var page = 0

    /// Stránky 0…2 — poslední má tlačítko „Začít“.
    private let lastPageWithContinueButton = 1

    var body: some View {
        ZStack {
            LinearGradient(
                colors: [
                    Color(red: 0.10, green: 0.12, blue: 0.20),
                    Color(red: 0.05, green: 0.06, blue: 0.11),
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()

            VStack(spacing: 0) {
                TabView(selection: $page) {
                    introPage(
                        icon: "checkerboard.rectangle",
                        title: "CZECHMATE",
                        lines: [
                            "Tahy a partie z ruky",
                            "Náhled desky, lampa",
                            "Swipuj mezi stránkami",
                        ]
                    )
                    .tag(0)

                    introPage(
                        icon: "link.circle",
                        title: "Jak to funguje",
                        lines: [
                            "Nejlepší je Bluetooth k desce",
                            "Nebo iPhone s aplikací CZECHMATE",
                            "Stav se obnovuje i na pozadí",
                        ]
                    )
                    .tag(1)

                    finalPage
                        .tag(2)
                }
                .tabViewStyle(.page(indexDisplayMode: .automatic))

                if page <= lastPageWithContinueButton {
                    Button {
                        withAnimation(.easeInOut(duration: 0.2)) { page += 1 }
                    } label: {
                        Text("Dál")
                            .font(.headline)
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 10)
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Color(red: 0.35, green: 0.72, blue: 0.48))
                    .padding(.horizontal, 12)
                    .padding(.bottom, 10)
                }
            }
        }
        .onAppear {
            WatchAppLog.staging("WatchOnboardingView onAppear")
        }
    }

    /// Jedna karta: bez ScrollView, jen krátké řádky.
    private func introPage(icon: String, title: String, lines: [String]) -> some View {
        VStack(spacing: 0) {
            Spacer(minLength: 4)

            Image(systemName: icon)
                .font(.system(size: 28, weight: .semibold))
                .foregroundStyle(
                    LinearGradient(
                        colors: [.white.opacity(0.95), Color(red: 0.55, green: 0.82, blue: 0.60)],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
                .padding(.bottom, 6)

            Text(title)
                .font(.title3.weight(.bold))
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
                .minimumScaleFactor(0.85)
                .padding(.bottom, 10)

            VStack(alignment: .leading, spacing: 6) {
                ForEach(Array(lines.enumerated()), id: \.offset) { _, line in
                    HStack(alignment: .firstTextBaseline, spacing: 6) {
                        Image(systemName: "circle.fill")
                            .font(.system(size: 5))
                            .foregroundStyle(.white.opacity(0.45))
                        Text(line)
                            .font(.caption2)
                            .foregroundStyle(.white.opacity(0.9))
                            .multilineTextAlignment(.leading)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 16)

            Spacer(minLength: 4)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var finalPage: some View {
        VStack(spacing: 0) {
            Spacer(minLength: 4)

            Image(systemName: "hand.wave.fill")
                .font(.system(size: 28, weight: .semibold))
                .foregroundStyle(Color(red: 0.55, green: 0.82, blue: 0.60))
                .padding(.bottom, 8)

            Text("Hotovo")
                .font(.title3.weight(.bold))
                .foregroundStyle(.white)
                .padding(.bottom, 8)

            Text("Časovač v detailu jen na iPhonu. První sken = souhlas s Bluetooth.")
                .font(.caption2)
                .foregroundStyle(.white.opacity(0.72))
                .multilineTextAlignment(.center)
                .padding(.horizontal, 14)
                .minimumScaleFactor(0.9)

            Spacer(minLength: 8)

            Button {
                onFinished()
            } label: {
                Text("Začít")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
            }
            .buttonStyle(.borderedProminent)
            .tint(Color(red: 0.35, green: 0.72, blue: 0.48))
            .padding(.horizontal, 12)
            .padding(.bottom, 10)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

#Preview {
    WatchOnboardingView(onFinished: {})
}
