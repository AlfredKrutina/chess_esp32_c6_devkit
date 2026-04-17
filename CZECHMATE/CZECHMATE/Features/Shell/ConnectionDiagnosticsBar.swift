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
                Text("Lokální náhled — stejné rozhraní jako při živé hře. Nápověda a analýza Stockfish stejně potřebují internet.")
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
                        internetStockfishLabel,
                        systemImage: "network"
                    )
                    .font(.caption2)
                    .foregroundStyle(network.isInternetLikelyForStockfish ? Color.secondary : Color.orange)
                }
                if store.activeLinkKind == .bluetooth,
                   network.isInternetLikelyForStockfish,
                   network.isConstrained {
                    Text("Deska na Bluetooth — Stockfish přes síť může v úsporném režimu být pomalejší.")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
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
        let suffix: String = {
            switch store.boardLinkIndicatorTier {
            case .live: return " — živá data"
            case .connecting: return " — připojování"
            case .offline: return store.hasActiveConnection ? " — bez odezvy" : " — odpojeno"
            }
        }()
        switch store.activeLinkKind {
        case .bluetooth: return "Bluetooth\(suffix)"
        case .wifiLAN: return "Wi‑Fi\(suffix)"
        case .mock: return "Ukázka (bez desky)"
        }
    }

    private var internetStockfishLabel: String {
        guard network.isInternetLikelyForStockfish else {
            return "Stockfish: potřeba internetu"
        }
        if network.isConstrained {
            return "Internet (omezující režim)"
        }
        return "Internet (Stockfish OK)"
    }
}
