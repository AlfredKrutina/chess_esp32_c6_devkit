//
//  ProgressTabView.swift
//  Sloučená záložka Výuka + Statistiky (méně přeplněný tab bar).
//

import SwiftUI

struct ProgressTabView: View {
    private enum Segment: String, CaseIterable {
        case learn
        case stats

        var title: String {
            switch self {
            case .learn: return "Výuka"
            case .stats: return "Statistiky"
            }
        }

        var symbol: String {
            switch self {
            case .learn: return "graduationcap.fill"
            case .stats: return "chart.bar.fill"
            }
        }
    }

    @State private var segment: Segment = .learn

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                VStack(spacing: Theme.Spacing.s) {
                    Picker("Přehled", selection: $segment) {
                        Label(Segment.learn.title, systemImage: Segment.learn.symbol)
                            .tag(Segment.learn)
                        Label(Segment.stats.title, systemImage: Segment.stats.symbol)
                            .tag(Segment.stats)
                    }
                    .pickerStyle(.segmented)
                    .tint(Theme.accent)
                    .accessibilityLabel("Přepnout mezi výukou a statistikami")

                    Text(segment == .learn ? "Průvodci desky a AI trenér" : "Relace a přehledy z aplikace")
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.secondary)
                        .frame(maxWidth: .infinity)
                        .multilineTextAlignment(.center)
                }
                .padding(.horizontal, Theme.Spacing.l)
                .padding(.vertical, Theme.Spacing.m)
                .background {
                    Color(uiColor: .secondarySystemGroupedBackground)
                        .shadow(color: .black.opacity(0.04), radius: 6, y: 3)
                }

                Group {
                    switch segment {
                    case .learn:
                        LearnTabContent()
                    case .stats:
                        StatsTabContent()
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .navigationTitle("Pokrok")
            .navigationBarTitleDisplayMode(.large)
            .tint(Theme.accent)
        }
    }
}
