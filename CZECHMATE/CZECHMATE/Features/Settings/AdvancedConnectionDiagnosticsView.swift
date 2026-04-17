//
//  AdvancedConnectionDiagnosticsView.swift
//  CZECHMATE — technické údaje o připojení (mimo hlavní Hra).
//

import SwiftUI

struct AdvancedConnectionDiagnosticsView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network

    var body: some View {
        Form {
            Section {
                LabeledContent("Režim") {
                    Text(linkTitle)
                }
                if store.activeLinkKind != .bluetooth {
                    LabeledContent("URL") {
                        Text(store.baseURLString.trimmingCharacters(in: .whitespacesAndNewlines))
                            .font(.caption)
                            .textSelection(.enabled)
                    }
                }
                LabeledContent("WebSocket") {
                    Text(store.isWebSocketLive ? "živý" : "neaktivní")
                }
                if let ms = store.lastRestRoundTripMs {
                    LabeledContent("REST RTT") {
                        Text("\(ms) ms")
                    }
                }
                if let t = store.lastRestFetchAt {
                    LabeledContent("Poslední REST") {
                        Text(t, style: .time)
                    }
                }
                LabeledContent("Internet (Stockfish)") {
                    Text(network.isInternetLikelyForStockfish ? "ano (Wi‑Fi nebo mobilní data)" : "nejasné / nedostupné")
                }
                LabeledContent("Wi‑Fi rozhraní (LAN desky)") {
                    Text(network.isWiFiInterfaceActive ? "aktivní — HTTP na 192.168… typicky možné" : "ne — k STA IP přes HTTP často jen přes Bluetooth")
                }
                LabeledContent("Omezující síť") {
                    Text(network.isConstrained ? "ano (Low Data / constrained — chess-api se stejně zkusí)" : "ne")
                }
                #if DEBUG
                LabeledContent("WS snímků (dbg)") {
                    Text("\(store.webSocketFramesForDiagnostics)")
                }
                #endif
            } header: {
                Text("Stav spojení")
            } footer: {
                Text("Tyto údaje slouží k řešení problémů v síti. Na kartě Hra se zobrazuje jen stručný stav.")
            }

            if store.activeLinkKind == .mock {
                Section {
                    Text("Ukázkový režim běží lokálně — stejné rozhraní jako při živé hře, bez fyzické desky.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .navigationTitle("Diagnostika připojení")
        #if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
        #endif
    }

    private var linkTitle: String {
        switch store.activeLinkKind {
        case .bluetooth: return "Bluetooth"
        case .wifiLAN: return "Wi‑Fi"
        case .mock: return "Ukázka"
        }
    }
}
