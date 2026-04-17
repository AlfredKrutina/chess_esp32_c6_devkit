//
//  NetworkErrorFormatter.swift
//  CZECHMATE — srozumitelné texty k NSURLError při LAN / ESP.
//

import Foundation

/// Rozpoznání hostitele desky v LAN (pro lepší texty u NSURLError).
enum BoardLANHostHints {
    static func looksLikeLANOnlyBoardAddress(host: String?) -> Bool {
        guard let host else { return false }
        let h = host.lowercased()
        if h.hasSuffix(".local") { return true }
        let parts = h.split(separator: ".").compactMap { Int(String($0)) }
        guard parts.count == 4 else { return false }
        let a = parts[0], b = parts[1]
        if a == 10 { return true }
        if a == 192 && b == 168 { return true }
        if a == 172 && b >= 16 && b <= 31 { return true }
        if a == 169 && b == 254 { return true }
        return false
    }
}

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
        let isChessAPIHost = host == "chess-api.com" || (host?.hasSuffix(".chess-api.com") == true)

        switch ns.code {
        case NSURLErrorCannotFindHost: // -1003
            if isChessAPIHost {
                return "Služba Stockfish (chess-api.com) není dosažitelná — DNS nebo síť. Zkontroluj internet a mobilní data pro aplikaci."
            }
            return "Host nenalezen\(hostHint). Zkontroluj IP nebo název v URL."
        case NSURLErrorCannotConnectToHost: // -1004 (ECONNREFUSED apod.)
            if isChessAPIHost {
                return "Nelze se spojit se službou Stockfish (chess-api.com). Zkus znovu později nebo jinou síť."
            }
            return "Spojení odmítnuto nebo server neběží\(hostHint). Zkontroluj IP, že ESP má zapnutý Wi‑Fi režim a HTTP server."
        case NSURLErrorTimedOut: // -1001
            if isChessAPIHost {
                return "Časový limit u chess-api.com — slabší síť, úsporný režim dat, nebo server zaneprázdněný. Zkus znovu; deska na Bluetooth s tím nesouvisí."
            }
            let mDns = (host?.contains(".local") == true)
                ? " Jména .local (mDNS) při prvním spojení někdy trvají déle — zkus znovu nebo použij přímo IP."
                : ""
            return "Časový limit\(hostHint). Zařízení neodpovídá — špatná síť, firewall, ESP v režimu spánku, nebo iPhone mimo stejnou Wi‑Fi.\(mDns)"
        case NSURLErrorNetworkConnectionLost:
            if isChessAPIHost {
                return "Spojení se službou Stockfish spadlo — zkus nápovědu znovu."
            }
            return "Síťové spojení spadlo. Zkus znovu načíst stav."
        case NSURLErrorNotConnectedToInternet: // -1009 — u LAN IP často „mobilní data na soukromou adresu nejdou“
            if isChessAPIHost {
                return "Stockfish potřebuje internet v telefonu (Wi‑Fi nebo mobilní data). Deska může zůstat jen na Bluetooth."
            }
            if BoardLANHostHints.looksLikeLANOnlyBoardAddress(host: host) {
                return
                    "K desce\(hostHint) se telefon nepřipojil. Soukromé IP v LAN nejdou přes mobilní data — zapni Wi‑Fi ve stejné síti jako šachovnice, nebo znovu připoj Bluetooth (Najít šachovnici). Přepínač „Nepřepínat na Wi‑Fi po Bluetooth“ je v Nastavení → Připojení."
            }
            return "Není spojení s cílem\(hostHint). Pro domácí desku po IP často potřebuješ Wi‑Fi v LAN; pro internet (např. Stockfish) mohou stačit mobilní data."
        case NSURLErrorSecureConnectionFailed:
            return "TLS selhalo — pro ESP používej http://, ne https://."
        default:
            return error.localizedDescription
        }
    }
}
