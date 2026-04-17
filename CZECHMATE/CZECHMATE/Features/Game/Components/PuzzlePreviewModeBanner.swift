//
//  PuzzlePreviewModeBanner.swift
//  CZECHMATE — banner náhledu puzzle + živý odpočet tréninku.
//

import SwiftUI

struct PuzzlePreviewModeBanner: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        if store.puzzleBoardPreview != nil {
            if let deadline = store.puzzleTrainingDeadline {
                LiveTrainingPuzzleBanner(deadline: deadline)
            } else {
                staticPuzzleBanner
            }
        }
    }

    private var staticPuzzleBanner: some View {
        ThemedBanner(style: .neutral) {
            VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                HStack {
                    Label("Puzzle — statická pozice", systemImage: "puzzlepiece.extension")
                        .font(Theme.Typography.subsection())
                    Spacer()
                    Button("Zavřít") {
                        store.clearPuzzlePosition()
                    }
                    .buttonStyle(.themeSecondary)
                }
                Text("Tahy nejdou odeslat na desku — jde o náhled pozice z aplikace.")
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
            }
        }
    }
}

// MARK: - Odpočet + haptika

private struct LiveTrainingPuzzleBanner: View {
    @Environment(BoardConnectionStore.self) private var store
    let deadline: Date
    @State private var expiryHapticFired = false

    var body: some View {
        TimelineView(.periodic(from: .now, by: 0.25)) { context in
            let remaining = deadline.timeIntervalSince(context.date)
            let expired = remaining <= 0
            ThemedBanner(style: expired ? .warning : .neutral, cornerRadius: 10) {
                VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                    HStack(alignment: .center, spacing: Theme.Spacing.m) {
                        VStack(alignment: .leading, spacing: 4) {
                            Label("Trénink puzzle", systemImage: "timer")
                                .font(Theme.Typography.subsection())
                            if expired {
                                Text("Čas vypršel — můžeš dál zkoušet nebo zavřít puzzle.")
                                    .font(Theme.Typography.caption())
                                    .foregroundStyle(Theme.Semantic.warningForeground)
                            } else {
                                Text("Vyřeš pozici dřív, než doběhne čas.")
                                    .font(Theme.Typography.caption())
                                    .foregroundStyle(.secondary)
                            }
                        }
                        Spacer(minLength: 8)
                        if !expired {
                            Text(formatCountdown(remaining))
                                .font(.system(size: 28, weight: .bold, design: .rounded))
                                .monospacedDigit()
                                .foregroundStyle(remaining < 30 ? Theme.Semantic.warningForeground : Theme.accent)
                                .accessibilityLabel("Zbývá \(Int(ceil(remaining))) sekund")
                        }
                        Button("Zavřít") {
                            store.clearPuzzlePosition()
                        }
                        .buttonStyle(.themeSecondary)
                    }
                }
            }
            .animation(.easeInOut(duration: 0.2), value: expired)
            .onChange(of: expired) { _, isExpired in
                if isExpired, !expiryHapticFired {
                    expiryHapticFired = true
                    HapticSettings.notificationErrorIfEnabled()
                }
            }
        }
        .onAppear {
            if deadline <= Date(), !expiryHapticFired {
                expiryHapticFired = true
                HapticSettings.notificationErrorIfEnabled()
            }
        }
        .onChange(of: deadline) { _, _ in
            expiryHapticFired = false
        }
    }

    private func formatCountdown(_ seconds: TimeInterval) -> String {
        let s = max(0, Int(ceil(seconds)))
        let m = s / 60
        let r = s % 60
        if m > 0 {
            return String(format: "%d:%02d", m, r)
        }
        return "\(r)s"
    }
}
