//
//  LocalNetworkPermissionProbe.swift
//  CZECHMATE — první TCP k domácí šachovnici vyvolá Local Network Privacy dialog (iOS 14+).
//

import Darwin
import Foundation
import Network

enum LocalNetworkPermissionProbe {
    /// Výsledek pokusu o krátké TCP spojení k hostiteli z uložené základní URL.
    enum Outcome: Equatable {
        /// Neplatná nebo prázdná URL / hostitel.
        case invalidURL
        /// TCP handshake proběhl (deska nebo služba na portu odpovídá).
        case tcpConnected(host: String, port: UInt16)
        /// Síťové selhání (čitelná zpráva pro uživatele).
        case failed(message: String)
        /// Vypršel časový limit — typicky dialog iOS, nedostupná deska nebo špatná Wi‑Fi.
        case timedOut
    }

    /// Krátký pokus o TCP spojení; trvá nejvýše cca `timeoutSeconds`.
    static func probe(baseURLString: String, timeoutSeconds: TimeInterval = 3.0) async -> Outcome {
        let t = baseURLString.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let url = URL(string: t),
              let host = url.host,
              !host.isEmpty else {
            #if DEBUG
            print("[CZECHMATE][staging] LocalNetwork probe: neplatná URL „\(t)“")
            #endif
            return .invalidURL
        }
        let rawPort = UInt16(url.port ?? ((url.scheme?.lowercased() == "https") ? 443 : 80))
        guard let nwPort = NWEndpoint.Port(rawValue: rawPort) else {
            return .invalidURL
        }

        return await withCheckedContinuation { continuation in
            let conn = NWConnection(
                host: NWEndpoint.Host(host),
                port: nwPort,
                using: .tcp
            )
            let lock = NSLock()
            var finished = false

            func finish(_ outcome: Outcome) {
                lock.lock()
                defer { lock.unlock() }
                guard !finished else { return }
                finished = true
                conn.cancel()
                continuation.resume(returning: outcome)
            }

            conn.stateUpdateHandler = { state in
                switch state {
                case .ready:
                    finish(.tcpConnected(host: host, port: rawPort))
                case .failed(let error):
                    finish(.failed(message: userFacingNetworkError(error)))
                default:
                    break
                }
            }

            conn.start(queue: .global(qos: .userInitiated))

            DispatchQueue.global(qos: .utility).asyncAfter(deadline: .now() + timeoutSeconds) {
                finish(.timedOut)
            }
        }
    }

    private static func userFacingNetworkError(_ error: NWError) -> String {
        // `NWError.posix` používá `POSIXErrorCode` — spolehlivější je porovnat `rawValue` s errno (SDK se mezi verzemi liší).
        if case .posix(let code) = error {
            switch code.rawValue {
            case ECONNREFUSED:
                return "Spojení odmítnuto — na dané adrese pravděpodobně nic neposlouchá, nebo je špatný port."
            case EHOSTUNREACH, ENETUNREACH:
                return "Hostitel nedostupný — ověř, že telefon je ve stejné Wi‑Fi jako deska a že IP odpovídá šachovnici."
            case ECANCELED:
                return "Spojení bylo přerušeno."
            default:
                break
            }
        }
        let s = error.localizedDescription
        let lower = s.lowercased()
        if lower.contains("local network") || lower.contains("local network privacy") {
            return "Přístup k lokální síti byl zamítnut. V Nastavení iOS → CZECHMATE povol „Místní síť“ a zkus znovu."
        }
        return "Spojení se nezdařilo: \(s)"
    }
}
