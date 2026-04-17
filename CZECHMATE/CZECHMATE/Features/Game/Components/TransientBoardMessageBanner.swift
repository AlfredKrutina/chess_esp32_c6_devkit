//
//  TransientBoardMessageBanner.swift
//  CZECHMATE — krátká zpětná vazba pod šachovnicí (např. zamítnutý vzdálený tah).
//

import SwiftUI

/// Zpráva pod deskou (např. selhání `postRemoteMove`) — mizí po timeoutu z `GameViewState.showTransientBoardMessage`.
struct TransientBoardMessageBanner: View {
    var message: String?
    /// Volitelné zavření před vypršením časovače (min. ~44 pt dotyk).
    var onDismiss: (() -> Void)? = nil

    var body: some View {
        Group {
            if let message, !message.isEmpty {
                HStack(alignment: .top, spacing: Theme.Spacing.s) {
                    Image(systemName: "exclamationmark.circle.fill")
                        .font(.body.weight(.semibold))
                        .foregroundStyle(Theme.Semantic.warningForeground)
                        .accessibilityHidden(true)
                    Text(message)
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.primary)
                        .fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: Theme.Spacing.s)
                    if let onDismiss {
                        Button {
                            onDismiss()
                        } label: {
                            Image(systemName: "xmark.circle.fill")
                                .font(.title3)
                                .symbolRenderingMode(.hierarchical)
                                .foregroundStyle(.secondary)
                        }
                        .buttonStyle(.plain)
                        .frame(minWidth: 44, minHeight: 44)
                        .contentShape(Rectangle())
                        .accessibilityLabel("Zavřít hlášení")
                    }
                }
                .padding(Theme.Spacing.m)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(Color.orange.opacity(0.12))
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .strokeBorder(Color.orange.opacity(0.28), lineWidth: 1)
                )
                .accessibilityElement(children: .combine)
                .accessibilityLabel("Hlášení u šachovnice: \(message)")
            }
        }
    }
}
