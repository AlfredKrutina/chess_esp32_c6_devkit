//
//  RemoteMovesHintBanner.swift
//  CZECHMATE — banner pod deskou: blokace tahů z aplikace (`TransientBoardMessageBanner` je v samostatném souboru).
//

import SwiftUI

struct RemoteMovesHintBanner: View {
    @Environment(BoardConnectionStore.self) private var store
    var remoteMovesEnabled: Bool

    var body: some View {
        Group {
            if remoteMovesEnabled, let reason = store.remoteChessFromAppBlockedReason {
                HStack(alignment: .top, spacing: Theme.Spacing.s) {
                    Image(systemName: "hand.tap")
                        .font(.body.weight(.semibold))
                        .foregroundStyle(Theme.Semantic.warningForeground)
                        .accessibilityHidden(true)
                    Text(reason)
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.primary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding(Theme.Spacing.m)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(Color.orange.opacity(0.14))
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .strokeBorder(Color.orange.opacity(0.35), lineWidth: 1)
                )
                .accessibilityElement(children: .combine)
                .accessibilityLabel("\(GameRemoteMovesCopy.toggleTitle): \(reason)")
            }
        }
    }
}
