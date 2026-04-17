// Šablona pro watchOS target (přidej Watch App do Xcode a zkopíruj).
// Přijímá kontext z iPhonu přes WatchConnectivity (viz WatchConnectivityBridge na iOS).

import SwiftUI
import WatchConnectivity

struct ContentView: View {
    @State private var moveCount = 0
    @State private var currentPlayer = "—"

    var body: some View {
        VStack {
            Text("CZECHMATE")
                .font(.headline)
            Text("Tahy: \(moveCount)")
            Text(currentPlayer)
                .font(.caption)
        }
        .onAppear { startSession() }
    }

    private func startSession() {
        guard WCSession.isSupported() else { return }
        let s = WCSession.default
        s.delegate = SessionHolder.shared
        s.activate()
        if let ctx = s.receivedApplicationContext as? [String: Any],
           let mc = ctx["moveCount"] as? Int {
            moveCount = mc
        }
        if let ctx = s.receivedApplicationContext as? [String: Any],
           let cp = ctx["currentPlayer"] as? String {
            currentPlayer = cp
        }
    }
}

final class SessionHolder: NSObject, WCSessionDelegate {
    static let shared = SessionHolder()

    func session(_: WCSession, activationDidCompleteWith _: WCSessionActivationState, error _: Error?) {}
}
