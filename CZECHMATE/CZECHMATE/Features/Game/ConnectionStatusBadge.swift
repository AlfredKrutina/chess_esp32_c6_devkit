//
//  ConnectionStatusBadge.swift
//  CZECHMATE — kompaktní indikátor odkazu k desce (toolbar).
//

import SwiftUI

struct ConnectionStatusBadge: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(statusColor)
                .frame(width: 9, height: 9)
                .overlay(Circle().stroke(Color.primary.opacity(0.15), lineWidth: 0.5))
            Image(systemName: linkIcon)
                .font(.system(size: 15, weight: .medium))
                .foregroundStyle(.secondary)
                .symbolRenderingMode(.hierarchical)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel(accessibilitySummary)
    }

    private var statusColor: Color {
        if store.lastError != nil { return Theme.Semantic.errorForeground }
        if store.isPolling, store.snapshot != nil { return Theme.Semantic.success }
        if store.isPolling { return Theme.Semantic.warningForeground }
        return .secondary.opacity(0.55)
    }

    private var linkIcon: String {
        switch store.activeLinkKind {
        case .bluetooth: return "antenna.radiowaves.left.and.right"
        case .wifiLAN: return "wifi"
        case .mock: return "square.grid.3x3.square"
        }
    }

    private var accessibilitySummary: String {
        switch store.activeLinkKind {
        case .bluetooth: return "Připojení Bluetooth"
        case .wifiLAN: return "Připojení Wi-Fi"
        case .mock: return "Ukázkový režim"
        }
    }
}
