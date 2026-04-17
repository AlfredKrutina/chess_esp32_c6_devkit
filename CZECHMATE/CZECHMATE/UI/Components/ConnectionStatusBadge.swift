//
//  ConnectionStatusBadge.swift
//  CZECHMATE — Compact toolbar connection indicator
//
//  Minimal visual indicator for toolbar use
//

import CZECHMATEShared
import SwiftUI

struct ConnectionStatusBadge: View {
    @Environment(BoardConnectionStore.self) private var store

    /// `nil` = jen indikátor (bez akce), např. náhledy.
    var onTap: (() -> Void)? = nil

    var body: some View {
        Group {
            if let onTap {
                Button(action: onTap) {
                    chipContent
                }
                .buttonStyle(.plain)
            } else {
                chipContent
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(onTap != nil ? .isButton : [])
        .accessibilityLabel(voiceOverConnectionSummary)
        .accessibilityHint(onTap != nil ? "Otevře vyhledání a připojení šachovnice." : "")
    }

    private var chipContent: some View {
        HStack(spacing: 6) {
            ConnectionDot(tier: store.boardLinkIndicatorTier)

            if store.boardLinkIndicatorTier == .live {
                Image(systemName: "checkerboard.rectangle")
                    .font(.caption)
            }
        }
        .padding(.horizontal, Theme.Spacing.s)
        .padding(.vertical, Theme.Spacing.xs + 2)
        .background(
            Capsule()
                .fill(backgroundColor)
        )
        .overlay(
            Capsule()
                .stroke(borderColor, lineWidth: 0.5)
        )
    }

    private var voiceOverConnectionSummary: String {
        switch store.boardLinkIndicatorTier {
        case .live:
            return "Živé spojení se šachovnicí"
        case .connecting:
            return "Navazuje se spojení se šachovnicí"
        case .offline:
            return "Šachovnice neodpovídá nebo není připojena"
        }
    }

    private var backgroundColor: Color {
        switch store.boardLinkIndicatorTier {
        case .live:
            return Theme.Semantic.successBackground
        case .connecting:
            return Color.orange.opacity(0.16)
        case .offline:
            return Theme.Semantic.neutralFill
        }
    }

    private var borderColor: Color {
        switch store.boardLinkIndicatorTier {
        case .live:
            return Theme.Semantic.success.opacity(0.22)
        case .connecting:
            return Color.orange.opacity(0.35)
        case .offline:
            return Color.primary.opacity(0.08)
        }
    }
}

struct ConnectionDot: View {
    var tier: BoardConnectionStore.BoardLinkIndicatorTier = .offline

    var body: some View {
        Circle()
            .fill(dotFill)
            .frame(width: 8, height: 8)
            .overlay(
                Circle()
                    .stroke(dotStroke, lineWidth: 2)
                    .scaleEffect(tier == .live ? 1.5 : 1.0)
                    .opacity(tier == .live ? 0.55 : 1.0)
            )
    }

    private var dotFill: Color {
        switch tier {
        case .live: return Theme.Semantic.success
        case .connecting: return Color.orange
        case .offline: return Color.secondary.opacity(0.45)
        }
    }

    private var dotStroke: Color {
        switch tier {
        case .live: return Theme.Semantic.success.opacity(0.35)
        case .connecting: return Color.orange.opacity(0.4)
        case .offline: return Color.clear
        }
    }
}

// MARK: - Preview

#Preview {
    HStack(spacing: 20) {
        ConnectionStatusBadge()
            .environment(BoardConnectionStore())
    }
    .padding()
}
