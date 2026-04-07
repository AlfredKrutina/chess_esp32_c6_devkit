//
//  ConnectionDiagnosticsBar.swift
//  CZECHMATE
//

import SwiftUI

/// Kompaktní řádek: URL, WS, REST RTT, internet (Stockfish).
struct ConnectionDiagnosticsBar: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(linkTitle)
                    .font(.caption.weight(.semibold))
                Spacer()
            }
            if store.activeLinkKind == .mock {
                Text("Lokální náhled — stejné rozhraní jako u živé hry (časy, deska, nápověda Stockfish).")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            } else {
                if store.activeLinkKind != .bluetooth {
                    Text(store.baseURLString.trimmingCharacters(in: .whitespacesAndNewlines))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                }
                HStack(spacing: 12) {
                    Label(
                        store.isWebSocketLive ? "WS živý" : "WS —",
                        systemImage: "antenna.radiowaves.left.and.right"
                    )
                    .font(.caption.weight(.medium))
                    if let ms = store.lastRestRoundTripMs {
                        Text("REST \(ms) ms")
                            .font(.caption.monospacedDigit())
                            .foregroundStyle(.secondary)
                    }
                    Spacer(minLength: 0)
                    Label(
                        network.isInternetLikelyForStockfish ? "Internet" : "Stockfish: potřeba sítě",
                        systemImage: "network"
                    )
                    .font(.caption2)
                    .foregroundStyle(network.isInternetLikelyForStockfish ? Color.secondary : Color.orange)
                }
                #if DEBUG
                Text("WS snímků (dbg): \(store.webSocketFramesForDiagnostics)")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
                #endif
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(10)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(Color.secondary.opacity(0.12))
        )
    }

    private var linkTitle: String {
        switch store.activeLinkKind {
        case .bluetooth: return "Připojeno — Bluetooth"
        case .wifiLAN: return "Připojeno — Wi‑Fi"
        case .mock: return "Ukázka (bez desky)"
        }
    }
}
