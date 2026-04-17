//
//  ConnectionStatusCard.swift
//  CZECHMATE — Status-first connection UI component
//
//  Shows connection state clearly with actionable context
//

import CZECHMATEShared
import SwiftUI

struct ConnectionStatusCard: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Status row
            HStack(spacing: 12) {
                StatusIndicator(connectionState: connectionState)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text(statusTitle)
                        .font(.headline)
                    
                    if let subtitle = statusSubtitle {
                        Text(subtitle)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                
                Spacer()
                
                // Connection icon
                Image(systemName: connectionIcon)
                    .font(.title2)
                    .foregroundStyle(connectionColor)
            }
            
            // Device info (if connected)
            if let device = connectedDevice {
                Divider()
                
                HStack {
                    Image(systemName: "checkerboard.rectangle")
                        .foregroundStyle(.secondary)
                    
                    VStack(alignment: .leading, spacing: 2) {
                        Text(device.name)
                            .font(.subheadline)
                        Text(device.connectionType)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    
                    Spacer()
                    
                    SignalStrengthIndicator(rssi: device.rssi)
                }
            }
        }
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(.ultraThinMaterial)
                .overlay(
                    RoundedRectangle(cornerRadius: 16)
                        .stroke(connectionColor.opacity(0.3), lineWidth: 1)
                )
        )
    }
    
    // MARK: - Computed Properties
    
    private var connectionState: ConnectionState {
        if store.isScanning {
            return .scanning
        }
        switch store.boardLinkIndicatorTier {
        case .live:
            return .connected
        case .connecting:
            return .connecting
        case .offline:
            if store.hasActiveConnection {
                if store.activeLinkKind == .bluetooth {
                    return .error(
                        "Deska dlouho neposílala žádný snímek přes Bluetooth — zkontroluj dosah a napájení, nebo znovu „Najít šachovnici“."
                    )
                }
                return .error("Deska dlouho neodpovídá — zkontroluj napájení, Wi‑Fi nebo zkus znovu Najít šachovnici.")
            }
            return .disconnected
        }
    }
    
    private var statusTitle: String {
        switch connectionState {
        case .connected:
            return store.activeLinkKind == .bluetooth ? "Připojeno přes Bluetooth" : "Připojeno"
        case .scanning:
            return "Hledání..."
        case .disconnected:
            return "Nepřipojeno"
        case .connecting:
            return "Připojování..."
        case .error:
            return "Spojení přerušeno"
        }
    }
    
    private var statusSubtitle: String? {
        switch connectionState {
        case .connected:
            if store.activeLinkKind == .bluetooth {
                return "Živá data z desky (GATT). Wi‑Fi v telefonu kvůli spojení s deskou nepotřebuješ."
            }
            return nil
        case .scanning:
            return "Klepněte pro výběr desky"
        case .disconnected:
            if store.activeLinkKind == .bluetooth {
                return "Zapni Bluetooth a připoj desku v „Najít šachovnici“ — stav Wi‑Fi v telefonu je pro tento režim vedlejší."
            }
            if UserDefaults.standard.bool(forKey: BoardConnectionStore.preferBluetoothOnlyKey) {
                return "Režim „jen Bluetooth“: připoj desku v „Najít šachovnici“ (Wi‑Fi k routeru u telefonu není nutná)."
            }
            return network.isPathSatisfied ? "Síť dostupná" : "Zkontrolujte připojení"
        case .connecting:
            if store.activeLinkKind == .bluetooth {
                return "Navazuji Bluetooth ke šachovnici…"
            }
            return "Čekám na první odpověď z desky…"
        case .error(let message):
            return message
        }
    }
    
    private var connectionIcon: String {
        switch connectionState {
        case .connected:
            return store.activeLinkKind == .bluetooth
                ? "dot.radiowaves.left.and.right"
                : "wifi"
        case .scanning:
            return "magnifyingglass"
        case .disconnected:
            return store.activeLinkKind == .bluetooth
                ? "antenna.radiowaves.left.and.right.slash"
                : "wifi.slash"
        case .connecting:
            return store.activeLinkKind == .bluetooth
                ? "dot.radiowaves.left.and.right"
                : "arrow.clockwise"
        case .error:
            return "exclamationmark.triangle"
        }
    }
    
    private var connectionColor: Color {
        switch connectionState {
        case .connected:
            return .green
        case .scanning:
            return .blue
        case .disconnected:
            return .gray
        case .connecting:
            return .orange
        case .error:
            return .red
        }
    }
    
    private var connectedDevice: DeviceInfo? {
        guard store.hasActiveConnection else { return nil }
        return DeviceInfo(
            name: store.connectionLabel ?? "Deska",
            connectionType: store.transport?.connectionLabel ?? "Wi‑Fi",
            rssi: -50 // Mock value, would come from actual BLE
        )
    }
}

// MARK: - Supporting Types

enum ConnectionState {
    case connected
    case scanning
    case disconnected
    case connecting
    case error(String)
}

struct DeviceInfo {
    let name: String
    let connectionType: String
    let rssi: Int
}

struct StatusIndicator: View {
    let connectionState: ConnectionState
    
    var body: some View {
        Circle()
            .fill(indicatorColor)
            .frame(width: 12, height: 12)
            .overlay(
                Circle()
                    .stroke(indicatorColor.opacity(0.3), lineWidth: 3)
                    .scaleEffect(pulseScale)
                    .opacity(pulseOpacity)
            )
    }
    
    private var indicatorColor: Color {
        switch connectionState {
        case .connected: return .green
        case .scanning: return .blue
        case .disconnected: return .gray
        case .connecting: return .orange
        case .error: return .red
        }
    }
    
    private var pulseScale: CGFloat {
        switch connectionState {
        case .scanning, .connecting: return 1.5
        default: return 1.0
        }
    }
    
    private var pulseOpacity: Double {
        switch connectionState {
        case .scanning, .connecting: return 0.5
        default: return 0
        }
    }
}

struct SignalStrengthIndicator: View {
    let rssi: Int
    
    var body: some View {
        HStack(spacing: 2) {
            ForEach(0..<4) { index in
                RoundedRectangle(cornerRadius: 1)
                    .fill(barColor(for: index))
                    .frame(width: 3, height: CGFloat(index + 1) * 3)
            }
        }
    }
    
    private func barColor(for index: Int) -> Color {
        let strength = (rssi + 100) / 25 // Convert RSSI to 0-4 scale
        return index < strength ? .green : .gray.opacity(0.3)
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 16) {
        ConnectionStatusCard()
            .environment(BoardConnectionStore())
            .environment(NetworkStatusMonitor())
    }
    .padding()
}
