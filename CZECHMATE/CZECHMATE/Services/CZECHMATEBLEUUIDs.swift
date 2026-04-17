//
//  CZECHMATEBLEUUIDs.swift
//  Musí odpovídat NimBLE GATT v components/ble_task (ESP32).
//

import CoreBluetooth
import Foundation

enum CZECHMATEBLEUUIDs {
    /// Vlastní služba CZECHMATE (128-bit UUID).
    static let service = CBUUID(string: "A0B40001-9267-4AB6-BDCC-E8336F8A8D9E")
    /// JSON snapshot (notify / read) — může být rozsekán na více notifikací.
    static let snapshotCharacteristic = CBUUID(string: "A0B40002-9267-4AB6-BDCC-E8336F8A8D9E")
    /// Příkazy z telefonu (write) — UTF-8 JSON např. {"cmd":"ping"}.
    static let commandCharacteristic = CBUUID(string: "A0B40003-9267-4AB6-BDCC-E8336F8A8D9E")
    /// Network info (read / notify) — IP adresa, SSID, online status.
    /// Obsah: {"sta_connected":true,"sta_ip":"192.168.1.45","sta_ssid":"WiFi","ap_ip":"192.168.4.1","online":true}
    static let networkCharacteristic = CBUUID(string: "A0B40004-9267-4AB6-BDCC-E8336F8A8D9E")
    /// Výsledek posledního zápisu na command (notify) — JSON s `channel` == `cmd_ack`.
    static let commandAckCharacteristic = CBUUID(string: "A0B40005-9267-4AB6-BDCC-E8336F8A8D9E")
}
