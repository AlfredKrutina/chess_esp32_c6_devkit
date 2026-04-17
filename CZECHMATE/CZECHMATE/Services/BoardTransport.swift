//
//  BoardTransport.swift
//  Jednotné rozhraní: Wi‑Fi (REST+WS), BLE (GATT), mock (UI vývoj).
//

import Foundation

/// Jak je uživatel připojen k logické šachovnici (pro štítek v UI).
enum BoardLinkKind: String, Sendable {
    case bluetooth
    case wifiLAN
    case mock
}

/// Položka z BLE scanu nebo Bonjour — bez zobrazení surové URL uživateli.
struct DiscoveredBoardDevice: Identifiable, Hashable, Sendable {
    let id: String
    /// Krátký název pro seznam (např. „CZECHMATE‑A3F2“).
    let displayName: String
    let transport: BoardLinkKind
    /// Interní `http://…` pro Wi‑Fi režim; u BLE může být prázdné.
    var resolvedBaseURL: URL?
    /// Volitelná RSSI pro BLE.
    var signalDescription: String?

    init(
        id: String,
        displayName: String,
        transport: BoardLinkKind,
        resolvedBaseURL: URL? = nil,
        signalDescription: String? = nil
    ) {
        self.id = id
        self.displayName = displayName
        self.transport = transport
        self.resolvedBaseURL = resolvedBaseURL
        self.signalDescription = signalDescription
    }
}

/// Abstrakce přenosu snímků hry a jednoduchých příkazů k desce.
@MainActor
protocol BoardTransport: AnyObject {
    var linkKind: BoardLinkKind { get }
    /// Lidsky: „Bluetooth“ / „Wi‑Fi (domácí síť)“ / „Ukázka“
    var connectionLabel: String { get }

    func startSnapshotUpdates(onSnapshot: @escaping @MainActor (GameSnapshot) -> Void) async throws
    func stopSnapshotUpdates() async

    /// Nápověda na desku přes firmware (stejné jako HTTP hint).
    func postHintHighlight(from: String, to: String) async throws
    /// Jen jedno pole (`to` v API) — slabší nápověda než plný tah.
    func postHintHighlightDestinationOnly(to square: String) async throws
    func postHintClear() async throws
    func postBrightness(percent: Int) async throws
    func fetchSnapshot(ifNoneMatch: String?) async throws -> GameSnapshot?
}
