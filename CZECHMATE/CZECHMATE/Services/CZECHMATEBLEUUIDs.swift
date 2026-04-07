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
}
