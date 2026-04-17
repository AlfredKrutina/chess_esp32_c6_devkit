// Šablona pro watchOS target — po přidání Watch App do Xcode zkopíruj do projektu.
// CoreBluetooth + stejné GATT UUID jako na iPhonu (CZECHMATEBLEUUIDs).

import SwiftUI

struct WatchContentView: View {
    @State private var moveCount = 0
    @State private var currentPlayer = "—"

    var body: some View {
        VStack(spacing: 8) {
            Text("CZECHMATE")
                .font(.headline)
            Text("Tahy: \(moveCount)")
            Text(currentPlayer)
                .font(.caption2)
            Text("Najít šachovnici přes Bluetooth")
                .font(.caption2)
                .multilineTextAlignment(.center)
        }
        .padding()
    }
}

#Preview {
    WatchContentView()
}
