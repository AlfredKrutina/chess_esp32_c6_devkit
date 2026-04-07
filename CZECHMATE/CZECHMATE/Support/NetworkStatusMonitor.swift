//
//  NetworkStatusMonitor.swift
//  CZECHMATE — dostupnost síťové cesty (pro Stockfish / chess-api).
//

import Foundation
import Network
import Observation

@MainActor
@Observable
final class NetworkStatusMonitor {
    private(set) var pathStatus: NWPath.Status = .requiresConnection
    /// Nízký datový režim / omezení (Stockfish může být nedostupný).
    private(set) var isConstrained: Bool = false
    private let monitor = NWPathMonitor()
    private let queue = DispatchQueue(label: "czechmate.nwpath")

    /// Pro běžné použití: spojení existuje (Wi‑Fi nebo mobilní data).
    var isPathSatisfied: Bool {
        pathStatus == .satisfied
    }

    /// Heuristika: chess-api / Stockfish — vyžaduje spokojenou cestu a ne „jen“ omezený režim.
    var isInternetLikelyForStockfish: Bool {
        pathStatus == .satisfied && !isConstrained
    }

    init() {
        monitor.pathUpdateHandler = { path in
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.pathStatus = path.status
                self.isConstrained = path.isConstrained
            }
        }
        monitor.start(queue: queue)
    }

    deinit {
        monitor.cancel()
    }
}
