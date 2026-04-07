//
//  LocalNetworkPermissionProbe.swift
//  CZECHMATE — první TCP k domácí šachovnici vyvolá Local Network Privacy dialog (iOS 14+).
//

import Foundation
import Network

enum LocalNetworkPermissionProbe {
    /// Krátké spojení k hostiteli z uložené základní URL — stačí na zobrazení systémového promptu.
    static func probe(baseURLString: String) {
        let t = baseURLString.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let url = URL(string: t), let host = url.host else {
            #if DEBUG
            print("[staging] LocalNetwork probe: neplatná URL „\(t)“")
            #endif
            return
        }
        let rawPort = UInt16(url.port ?? ((url.scheme?.lowercased() == "https") ? 443 : 80))
        guard let nwPort = NWEndpoint.Port(rawValue: rawPort) else { return }
        let conn = NWConnection(
            host: NWEndpoint.Host(host),
            port: nwPort,
            using: .tcp
        )
        conn.stateUpdateHandler = { _ in }
        conn.start(queue: .main)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) {
            conn.cancel()
        }
    }
}
