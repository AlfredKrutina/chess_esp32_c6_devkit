//
//  MockBoardTransport.swift
//  Pro UI vývoj a náhled bez ESP.
//

import Foundation

@MainActor
final class MockBoardTransport: BoardTransport {
    let linkKind: BoardLinkKind = .mock
    let connectionLabel = "Ukázka"

    private var onSnapshot: (@MainActor (GameSnapshot) -> Void)?

    func startSnapshotUpdates(onSnapshot: @escaping @MainActor (GameSnapshot) -> Void) async throws {
        await stopSnapshotUpdates()
        self.onSnapshot = onSnapshot
        let initial = try GameSnapshot.mockStartingPosition()
        onSnapshot(initial)

        while !Task.isCancelled {
            try? await Task.sleep(nanoseconds: 2_500_000_000)
            guard !Task.isCancelled else { break }
            guard let on = self.onSnapshot else { break }
            if let s = try? GameSnapshot.mockStartingPosition() {
                on(s)
            }
        }
    }

    func stopSnapshotUpdates() async {
        onSnapshot = nil
    }

    func postHintHighlight(from: String, to: String) async throws {
        #if DEBUG
        print("[staging] MockBoardTransport hint \(from) → \(to)")
        #endif
    }

    func postHintClear() async throws {}

    func postBrightness(percent: Int) async throws {
        #if DEBUG
        print("[staging] MockBoardTransport brightness \(percent)")
        #endif
    }

    func fetchSnapshot(ifNoneMatch: String?) async throws -> GameSnapshot? {
        try GameSnapshot.mockStartingPosition()
    }
}
