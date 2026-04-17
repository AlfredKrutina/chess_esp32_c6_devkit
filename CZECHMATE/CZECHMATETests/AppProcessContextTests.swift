//
//  AppProcessContextTests.swift
//  CZECHMATETests — kouřový test: host proces naběhl (žádný pád v native init) + konzistence XCTest prostředí.
//

import XCTest
@testable import CZECHMATE

final class AppProcessContextTests: XCTestCase {
    func testXCTestHostEnvironmentDetected() {
        let isHost = AppProcessContext.isXCTestHostProcess
        AppDebugLog.staging("AppProcessContextTests xctestHost=\(isHost) sim=\(AppProcessContext.isSimulatorRuntime)")
        XCTAssertTrue(
            isHost,
            "Hosted unit testy musí běžet v .app s nastaveným XCTestBundlePath / XCTestConfigurationFilePath — jinak neplatí diagnostika startu."
        )
    }
}
