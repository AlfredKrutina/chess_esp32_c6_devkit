//
//  PrePuzzleRestoreBanner.swift
//  Obnovení pozice před nahráním puzzle na desku (záložní FEN v BoardConnectionStore).
//

import SwiftUI

struct PrePuzzleRestoreBanner: View {
    @Environment(BoardConnectionStore.self) private var store
    @State private var isRestoring = false

    /// Při neúspěchu `restorePrePuzzleGame` (např. chyba desky).
    var onFailureMessage: (String) -> Void

    var body: some View {
        if store.canRestorePrePuzzleGame, store.supportsRemoteChessCommands {
            ThemedBanner(style: .neutral, cornerRadius: 10) {
                VStack(alignment: .leading, spacing: Theme.Spacing.xs) {
                    HStack(alignment: .center, spacing: Theme.Spacing.m) {
                        Label("Záloha partie před puzzle", systemImage: "arrow.uturn.backward.circle")
                            .font(Theme.Typography.subsection())
                        Spacer(minLength: 0)
                        Button {
                            Task { @MainActor in
                                isRestoring = true
                                let ok = await store.restorePrePuzzleGame()
                                isRestoring = false
                                if !ok {
                                    let raw = store.lastError?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
                                    onFailureMessage(raw.isEmpty ? "Obnovení pozice se nezdařilo." : raw)
                                }
                            }
                        } label: {
                            if isRestoring {
                                ProgressView()
                            } else {
                                Text("Obnovit na desce")
                            }
                        }
                        .buttonStyle(.themePrimary)
                        .disabled(isRestoring)
                    }
                    Text(
                        store.prePuzzleRestoreSubtitle
                            + " Po nahrání puzzle se vyhodnocení tahů v aplikaci vynuluje; tímto obnovíš uložený FEN."
                    )
                    .font(Theme.Typography.caption2())
                    .foregroundStyle(.secondary)
                }
            }
        }
    }
}
