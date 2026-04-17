//
//  NetworkErrorFormatterTests.swift
//  CZECHMATETests — copy pro Stockfish / chess-api při mobilních datech + BLE.
//

import XCTest
@testable import CZECHMATE

final class NetworkErrorFormatterTests: XCTestCase {
    private func urlError(code: Int, urlString: String) -> NSError {
        // Stejné klíče jako `NetworkErrorFormatter` (NSURLError userInfo).
        NSError(
            domain: NSURLErrorDomain,
            code: code,
            userInfo: ["NSErrorFailingURLStringKey": urlString]
        )
    }

    func testChessAPI_NotConnectedToInternet_MentionsStockfishAndBluetooth() {
        let err = urlError(code: NSURLErrorNotConnectedToInternet, urlString: "https://chess-api.com/v1")
        let msg = NetworkErrorFormatter.userMessage(for: err)
        XCTAssertTrue(msg.localizedCaseInsensitiveContains("stockfish"), msg)
        XCTAssertTrue(msg.localizedCaseInsensitiveContains("bluetooth"), msg)
    }

    func testChessAPI_TimedOut_MentionsChessApiAndBluetooth() {
        let err = urlError(code: NSURLErrorTimedOut, urlString: "https://chess-api.com/v1")
        let msg = NetworkErrorFormatter.userMessage(for: err)
        XCTAssertTrue(msg.contains("chess-api.com"), msg)
        XCTAssertTrue(msg.localizedCaseInsensitiveContains("bluetooth"), msg)
    }

    func testLANIP_NotConnectedToInternet_MentionsBluetoothReconnect() {
        let err = urlError(code: NSURLErrorNotConnectedToInternet, urlString: "http://192.168.1.50/api/game/snapshot")
        let msg = NetworkErrorFormatter.userMessage(for: err)
        XCTAssertTrue(msg.localizedCaseInsensitiveContains("bluetooth"), msg)
        XCTAssertTrue(msg.contains("Nastavení"), msg)
    }

    func testBoardLANHostHints_PrivateIPv4() {
        XCTAssertTrue(BoardLANHostHints.looksLikeLANOnlyBoardAddress(host: "192.168.0.1"))
        XCTAssertTrue(BoardLANHostHints.looksLikeLANOnlyBoardAddress(host: "10.0.0.1"))
        XCTAssertTrue(BoardLANHostHints.looksLikeLANOnlyBoardAddress(host: "board.local"))
        XCTAssertFalse(BoardLANHostHints.looksLikeLANOnlyBoardAddress(host: "chess-api.com"))
    }
}
