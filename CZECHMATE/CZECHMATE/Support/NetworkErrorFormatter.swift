//
//  NetworkErrorFormatter.swift
//  CZECHMATE — srozumitelné texty k NSURLError při LAN / ESP.
//

import Foundation

enum NetworkErrorFormatter {
    /// Čitelná zpráva pro `lastError` v UI (REST i obecné síťové chyby).
    static func userMessage(for error: Error) -> String {
        let ns = error as NSError
        guard ns.domain == NSURLErrorDomain else {
            return error.localizedDescription
        }
        // Klíče z NSURLError (Foundation je exportuje jinak podle platformy).
        let failString = ns.userInfo["NSErrorFailingURLStringKey"] as? String
        let host = failString.flatMap { URL(string: $0)?.host }
            ?? (ns.userInfo["NSErrorFailingURLKey"] as? URL)?.host
        let hostHint = host.map { " (\($0))" } ?? ""

        switch ns.code {
        case NSURLErrorCannotFindHost: // -1003
            return "Host nenalezen\(hostHint). Zkontroluj IP nebo název v URL."
        case NSURLErrorCannotConnectToHost: // -1004 (ECONNREFUSED apod.)
            return "Spojení odmítnuto nebo server neběží\(hostHint). Zkontroluj IP, že ESP má zapnutý Wi‑Fi režim a HTTP server."
        case NSURLErrorTimedOut: // -1001
            let mDns = (host?.contains(".local") == true)
                ? " Jména .local (mDNS) při prvním spojení někdy trvají déle — zkus znovu nebo použij přímo IP."
                : ""
            return "Časový limit\(hostHint). Zařízení neodpovídá — špatná síť, firewall, ESP v režimu spánku, nebo iPhone mimo stejnou Wi‑Fi.\(mDns)"
        case NSURLErrorNetworkConnectionLost:
            return "Síťové spojení spadlo. Zkus znovu načíst stav."
        case NSURLErrorNotConnectedToInternet:
            return "Není připojení k síti (Wi‑Fi / data)."
        case NSURLErrorSecureConnectionFailed:
            return "TLS selhalo — pro ESP používej http://, ne https://."
        default:
            return error.localizedDescription
        }
    }
}
