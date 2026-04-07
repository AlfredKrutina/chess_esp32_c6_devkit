//
//  LANBoardDiscoveryService.swift
//  Bonjour — `_http._tcp` v domácí síti (ESP mDNS).
//

import Foundation
import Observation

/// Přenos `NetService` do `Task { @MainActor }` bez Sendable varování (Swift 6).
private final class NetServiceSendableBox: @unchecked Sendable {
    let service: NetService
    init(_ service: NetService) { self.service = service }
}

@MainActor
@Observable
final class LANBoardDiscoveryService: NSObject {
    private var browser: NetServiceBrowser?
    /// Nalezené služby podle `DiscoveredBoardDevice.id`
    private var servicesById: [String: NetService] = [:]
    /// Drží delegáta při `resolve` (NetService má weak delegate).
    private var pendingResolve: NetResolveHelper?

    private(set) var devices: [DiscoveredBoardDevice] = []
    private(set) var isSearching = false
    private(set) var lastError: String?

    func startSearch() {
        stopSearch()
        lastError = nil
        devices = []
        servicesById = [:]
        AppDebugLog.staging("Bonjour: hledám _http._tcp v doméně local.")
        let b = NetServiceBrowser()
        browser = b
        b.delegate = self
        b.searchForServices(ofType: "_http._tcp", inDomain: "local.")
        isSearching = true
    }

    func stopSearch() {
        browser?.stop()
        browser = nil
        for s in servicesById.values {
            s.stop()
        }
        servicesById.removeAll()
        isSearching = false
    }

    func resolveURL(for device: DiscoveredBoardDevice) async -> URL? {
        guard let svc = servicesById[device.id] else { return nil }
        return await withCheckedContinuation { cont in
            let helper = NetResolveHelper(service: svc) { [weak self] host, port in
                self?.pendingResolve = nil
                let h = host.hasSuffix(".") ? String(host.dropLast()) : host
                guard !h.isEmpty else {
                    cont.resume(returning: nil)
                    return
                }
                let p = port > 0 ? port : 80
                cont.resume(returning: URL(string: "http://\(h):\(p)"))
            }
            pendingResolve = helper
            helper.start()
        }
    }
}

/// Jednorázové vyřešení hostname a portu z `NetService`.
private final class NetResolveHelper: NSObject, NetServiceDelegate {
    private let service: NetService
    private let done: (String, Int) -> Void

    init(service: NetService, done: @escaping (String, Int) -> Void) {
        self.service = service
        self.done = done
    }

    func start() {
        service.delegate = self
        service.resolve(withTimeout: 15)
    }

    func netServiceDidResolveAddress(_ sender: NetService) {
        /* Bonjour TXT z ESP:
         *  „ap_ip“  = stabilní IP na AP hotspotu (192.168.4.1) - obchází chybějící mDNS/DNS.
         *  „sta_ip“ = IP v domácí LAN síti.
         */
        if let txtData = sender.txtRecordData() {
            let dict = NetService.dictionary(fromTXTRecord: txtData)
            let port = sender.port > 0 ? sender.port : 80

            // 1. Zkusíme AP hotspot adresu (přímé připojení na ESP bez routeru)
            if let raw = dict["ap_ip"],
               let ip = String(data: raw, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines),
               !ip.isEmpty {
                done(ip, port)
                return
            }

            // 2. Zkusíme STA adresu (klasická domácí síť)
            if let raw = dict["sta_ip"],
               let ip = String(data: raw, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines),
               !ip.isEmpty {
                done(ip, port)
                return
            }
        }
        
        // 3. Fallback na hostname resolve (může selhat na AP síti bez internetu)
        let host = sender.hostName ?? ""
        done(host, sender.port)
    }

    func netService(_ sender: NetService, didNotResolve errorDict: [String: NSNumber]) {
        AppDebugLog.staging("Bonjour resolve selhalo: \(sender.name) err=\(errorDict)")
        done("", 0)
    }
}

extension LANBoardDiscoveryService: NetServiceBrowserDelegate {
    nonisolated func netServiceBrowser(
        _ browser: NetServiceBrowser,
        didFind service: NetService,
        moreComing: Bool
    ) {
        let box = NetServiceSendableBox(service)
        // _http._tcp je sdílené s kamerami, tiskárnami, NAS… Deska ESP má instance
        // name "CZECHMATE" (viz mdns_service_add ve web_server_task.c).
        let rawName = box.service.name.replacingOccurrences(of: "\\032", with: " ")
        guard rawName.localizedCaseInsensitiveContains("czechmate") else {
            return
        }
        let id = Self.makeId(box.service)
        Task { @MainActor in
            self.servicesById[id] = box.service
            let dev = DiscoveredBoardDevice(
                id: id,
                displayName: rawName,
                transport: .wifiLAN,
                resolvedBaseURL: nil,
                signalDescription: nil
            )
            if !self.devices.contains(where: { $0.id == id }) {
                self.devices.append(dev)
                AppDebugLog.staging("Bonjour: nalezeno \(rawName) (\(id.prefix(40))…)")
            }
        }
    }

    nonisolated func netServiceBrowser(
        _ browser: NetServiceBrowser,
        didRemove service: NetService,
        moreComing: Bool
    ) {
        let box = NetServiceSendableBox(service)
        let id = Self.makeId(box.service)
        Task { @MainActor in
            self.servicesById.removeValue(forKey: id)
            self.devices.removeAll { $0.id == id }
        }
    }

    nonisolated func netServiceBrowser(
        _ browser: NetServiceBrowser,
        didNotSearch errorDict: [String: NSNumber]
    ) {
        Task { @MainActor in
            AppDebugLog.staging("Bonjour prohlížeč didNotSearch: \(errorDict)")
            self.lastError = "Vyhledávání v síti selhalo."
            self.isSearching = false
        }
    }

    nonisolated func netServiceBrowserDidStopSearch(_ browser: NetServiceBrowser) {
        Task { @MainActor in
            self.isSearching = false
        }
    }

    private nonisolated static func makeId(_ s: NetService) -> String {
        "\(s.name)|\(s.type)|\(s.domain)"
    }
}
