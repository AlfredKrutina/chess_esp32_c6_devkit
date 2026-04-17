//
//  CurrentWiFiSSIDReader.swift
//  CZECHMATE — aktuální SSID z iOS přes NetworkExtension.
//
//  Automatické načtení SSID vyžaduje placený Apple Developer Program a capability „Access WiFi Information“
//  v App ID + v projektu (entitlement `com.apple.developer.networking.wifi-info`). Osobní tým to nepodporuje —
//  bez něj `fetchCurrentSSID()` vrací nil a uživatel SSID zadá ručně (stejný výsledek).
//
//  Frekvenci pásma (2,4 vs 5 GHz) iOS obvykle neukáže — upozornění je v UI u zápisu na desku.

import Foundation

#if os(iOS)
import NetworkExtension
#endif

enum CurrentWiFiSSIDReader {
    /// Aktuální SSID Wi‑Fi, ke které je telefon připojený; `nil` bez oprávnění, na simulátoru nebo mimo iOS.
    static func fetchCurrentSSID() async -> String? {
        #if os(iOS)
        if #available(iOS 14.0, *) {
            return await withCheckedContinuation { cont in
                NEHotspotNetwork.fetchCurrent { network in
                    cont.resume(returning: network?.ssid)
                }
            }
        }
        #endif
        return nil
    }
}
