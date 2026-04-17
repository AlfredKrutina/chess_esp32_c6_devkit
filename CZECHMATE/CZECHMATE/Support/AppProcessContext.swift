//
//  AppProcessContext.swift
//  CZECHMATE — kontext procesu (simulátor, XCTest host) + jednotné bootstrap logy [staging].
//

import Foundation

enum AppProcessContext {
    /// Spuštěno jako host pro unit testy (inject test bundle do .app).
    static var isXCTestHostProcess: Bool {
        let env = ProcessInfo.processInfo.environment
        if env["XCTestBundlePath"] != nil { return true }
        if env["XCTestConfigurationFilePath"] != nil { return true }
        if env["XCInjectBundleInto"] != nil { return true }
        return false
    }

    static var isSimulatorRuntime: Bool {
        #if targetEnvironment(simulator)
        true
        #else
        false
        #endif
    }

    /// Klíče z prostředí související s testy (jen pro log — neexponujeme hodnoty s cestami uživateli).
    private static func xctestRelatedEnvironmentSummary() -> String {
        let env = ProcessInfo.processInfo.environment
        let keys = env.keys.filter { key in
            key.localizedCaseInsensitiveContains("xctest")
                || key == "XCInjectBundleInto"
                || key == "DYLD_INSERT_LIBRARIES"
        }.sorted()
        if keys.isEmpty { return "none" }
        return keys.joined(separator: ", ")
    }

    private static func processArgumentHints() -> String {
        let args = ProcessInfo.processInfo.arguments
        let hints = args.filter { arg in
            arg.localizedCaseInsensitiveContains("xctest")
                || arg.hasSuffix(".xctest")
        }
        if hints.isEmpty { return "none" }
        return "\(hints.count) matching argv entries"
    }

    /// Volat z `CZECHMATEApp.init` — jednou při startu; v Release téměř zdarma (jen větve env).
    static func logBootstrap() {
        #if DEBUG
        let mpLinked = CoachMediaPipeBuildAvailability.isMediaPipeLinked
        AppDebugLog.staging(
            "bootstrap pid=\(ProcessInfo.processInfo.processIdentifier) sim=\(isSimulatorRuntime) xctestHost=\(isXCTestHostProcess) mediapipeLinked=\(mpLinked)"
        )
        AppDebugLog.staging(
            "bootstrap envKeys(\(xctestRelatedEnvironmentSummary())) argv(\(processArgumentHints()))"
        )
        #if os(iOS)
        if let model = ProcessInfo.processInfo.environment["SIMULATOR_MODEL_IDENTIFIER"] {
            AppDebugLog.staging("bootstrap SIMULATOR_MODEL_IDENTIFIER=\(model)")
        }
        #endif
        #endif
    }
}
