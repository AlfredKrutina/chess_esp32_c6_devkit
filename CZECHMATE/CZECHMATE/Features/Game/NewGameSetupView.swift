//
//  NewGameSetupView.swift
//  CZECHMATE — Alias na plný sheet nové hry (předvolby + vlastní čas jako web desky).
//

import SwiftUI

/// Zachováno kvůli starším odkazům — obsah je `NewGameSetupSheet`.
struct NewGameSetupView: View {
    var body: some View {
        NewGameSetupSheet()
    }
}

// MARK: - Preview

#Preview {
    NewGameSetupView()
        .environment(BoardConnectionStore())
}
