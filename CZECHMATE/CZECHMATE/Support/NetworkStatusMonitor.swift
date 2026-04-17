//
//  NetworkStatusMonitor.swift
//  CZECHMATE — dostupnost síťové cesty (pro Stockfish / chess-api).
//
//  Pozn.: chess-api běží přes HTTPS na internet — funguje na mobilních datech stejně jako na Wi‑Fi.
//  `isConstrained` (Low Data Mode) nesmí blokovat hru jen přes BLE k desce; požadavek jen upozorní v diagnostice.

import Foundation
import Network
import Observation

@MainActor
@Observable
final class NetworkStatusMonitor {
    private(set) var pathStatus: NWPath.Status = .requiresConnection
    /// Nízký datový režim / „constrained“ cesta — chess-api může být pomalejší nebo selhat, ale stále se zkusí.
    private(set) var isConstrained: Bool = false
    /// `true` když je v aktivní cestě rozhraní Wi‑Fi (typicky nutné pro HTTP na STA IP desky v LAN).
    private(set) var isWiFiInterfaceActive: Bool = false
    private let monitor = NWPathMonitor()
    private let queue = DispatchQueue(label: "czechmate.nwpath")
    /// Poslední známý stav Wi‑Fi rozhraní — první aktualizace nevolá callback (aby se při startu nespouštěl handoff).
    private var lastHandoffWifiActive: Bool?
    /// Voláno jen při **změně** `isWiFiInterfaceActive` (telefon přepnul Wi‑Fi / mobilní data).
    var onWiFiInterfaceForHandoffChanged: ((Bool) -> Void)?

    /// Pro běžné použití: spojení existuje (Wi‑Fi nebo mobilní data).
    var isPathSatisfied: Bool {
        pathStatus == .satisfied
    }

    /// chess-api / Stockfish — stačí dostupný internet (Wi‑Fi **nebo** mobilní data). Připojení k desce je oddělené (BLE / LAN).
    var isInternetLikelyForStockfish: Bool {
        pathStatus == .satisfied
    }

    init() {
        monitor.pathUpdateHandler = { [weak self] path in
            Task { @MainActor [weak self] in
                self?.ingest(path)
            }
        }
        monitor.start(queue: queue)
        queue.async { [weak self] in
            guard let self else { return }
            let path = self.monitor.currentPath
            Task { @MainActor [weak self] in
                self?.ingest(path)
            }
        }
    }

    private func ingest(_ path: NWPath) {
        pathStatus = path.status
        isConstrained = path.isConstrained
        let wifi = path.status == .satisfied && path.usesInterfaceType(.wifi)
        isWiFiInterfaceActive = wifi
        if let prev = lastHandoffWifiActive, prev != wifi {
            onWiFiInterfaceForHandoffChanged?(wifi)
        }
        lastHandoffWifiActive = wifi
    }

    deinit {
        monitor.cancel()
    }
}
