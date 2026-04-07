//
//  SwiftDataAppSupport.swift
//  CZECHMATE — před prvním otevřením úložiště zajistíme existenci Application Support (jinak NSCocoaError 512).
//

import Foundation

enum SwiftDataAppSupport {
    /// Vytvoří `Library/Application Support` v sandboxu, pokud chybí.
    static func ensureApplicationSupportDirectoryExists() {
        let fm = FileManager.default
        guard let appSupport = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            return
        }
        var isDir: ObjCBool = false
        if fm.fileExists(atPath: appSupport.path, isDirectory: &isDir), isDir.boolValue {
            return
        }
        do {
            try fm.createDirectory(at: appSupport, withIntermediateDirectories: true, attributes: nil)
        } catch {
            #if DEBUG
            AppDebugLog.staging("SwiftDataAppSupport: createDirectory failed — \(error.localizedDescription)")
            #endif
        }
    }
}
