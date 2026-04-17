//
//  ChessMatchLiveActivity.swift
//  CZECHMATE — Live Activity (Lock Screen, Dynamic Island, Smart Stack náhled).
//

import ActivityKit
import CZECHMATEShared
import SwiftUI
import WidgetKit

// MARK: - Paleta (shodná s aplikací `Theme.accent` — widget target nemá Theme.swift)

private enum LiveActivityPalette {
    static let accent = Color(red: 0.22, green: 0.62, blue: 0.38)
    static let accentSoft = Color(red: 0.22, green: 0.62, blue: 0.38).opacity(0.22)
    static let checkRed = Color.red
}

struct ChessMatchLiveActivity: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: ChessMatchAttributes.self) { context in
            lockScreenCard(context: context)
                .activityBackgroundTint(LiveActivityPalette.accentSoft)
                .widgetURL(URL(string: "czechmate://game?source=liveactivity"))
        } dynamicIsland: { context in
            dynamicIslandContent(context: context)
        }
    }

    // MARK: - Lock Screen

    @ViewBuilder
    private func lockScreenCard(context: ActivityViewContext<ChessMatchAttributes>) -> some View {
        let state = context.state
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .center, spacing: 8) {
                Image(systemName: "checkerboard.rectangle")
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundStyle(LiveActivityPalette.accent)
                VStack(alignment: .leading, spacing: 2) {
                    Text("CZECHMATE")
                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                    Text("\(state.moveCount) tahů")
                        .font(.system(.caption2, design: .rounded))
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("CZECHMATE, \(state.moveCount) tahů")

            Divider()
                .opacity(0.35)

            HStack(alignment: .center, spacing: 8) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(String(localized: "Na tahu", comment: "Live Activity: whose turn"))
                        .font(.system(.caption2, design: .rounded))
                        .foregroundStyle(.secondary)
                    HStack(spacing: 6) {
                        Text(playerLabel(state.currentPlayer))
                            .font(.system(.body, design: .rounded).weight(.semibold))
                        if state.inCheck {
                            Image(systemName: "exclamationmark.shield.fill")
                                .font(.system(size: 14, weight: .semibold))
                                .foregroundStyle(LiveActivityPalette.checkRed)
                                .accessibilityLabel(String(localized: "Šach", comment: "Live Activity: in check"))
                        }
                    }
                }
                Spacer(minLength: 0)
                Text(ChessPieceGlyph.symbol(for: turnPawnChar(state.currentPlayer)))
                    .chessPieceAppearance(
                        fenChar: turnPawnChar(state.currentPlayer),
                        isLightSquare: false,
                        font: .title.weight(.semibold)
                    )
                    .accessibilityHidden(true)
            }
            .padding(.vertical, 4)
            .padding(.horizontal, 10)
            .background(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(Color.primary.opacity(0.06))
            )

            if !state.advantageSummary.isEmpty {
                Label {
                    Text(state.advantageSummary)
                        .font(.system(.subheadline, design: .rounded).weight(.medium))
                } icon: {
                    Image(systemName: "scalemass")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(LiveActivityPalette.accent)
                }
                .labelStyle(.titleAndIcon)
            }

            Text(state.subtitle)
                .font(.system(.caption, design: .rounded))
                .foregroundStyle(.secondary)
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)

            if state.timerActive,
               let w = state.whiteTimeSeconds,
               let b = state.blackTimeSeconds
            {
                HStack(spacing: 10) {
                    timerPill(label: String(localized: "Bílý", comment: "White player"), seconds: w, active: isWhiteTurn(state.currentPlayer))
                    timerPill(label: String(localized: "Černý", comment: "Black player"), seconds: b, active: !isWhiteTurn(state.currentPlayer))
                }
            }
        }
        .padding(14)
    }

    // MARK: - Dynamic Island

    private func dynamicIslandContent(context: ActivityViewContext<ChessMatchAttributes>) -> DynamicIsland {
        let state = context.state
        return DynamicIsland {
            DynamicIslandExpandedRegion(.leading) {
                VStack(alignment: .leading, spacing: 6) {
                    if !state.advantageSummary.isEmpty {
                        Text(state.advantageSummary)
                            .font(.system(.caption, design: .rounded).weight(.semibold))
                            .lineLimit(2)
                            .minimumScaleFactor(0.85)
                    }
                    HStack(spacing: 8) {
                        Text(ChessPieceGlyph.symbol(for: turnPawnChar(state.currentPlayer)))
                            .chessPieceAppearance(
                                fenChar: turnPawnChar(state.currentPlayer),
                                isLightSquare: false,
                                font: .title2.weight(.semibold)
                            )
                        if state.inCheck {
                            Image(systemName: "exclamationmark.shield.fill")
                                .foregroundStyle(LiveActivityPalette.checkRed)
                                .font(.title3)
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            DynamicIslandExpandedRegion(.trailing) {
                VStack(alignment: .trailing, spacing: 4) {
                    Text(playerLabel(state.currentPlayer))
                        .font(.system(.caption, design: .rounded).weight(.semibold))
                    if state.timerActive,
                       let w = state.whiteTimeSeconds,
                       let b = state.blackTimeSeconds
                    {
                        let active = isWhiteTurn(state.currentPlayer) ? w : b
                        Text(formatClock(active))
                            .font(.system(.title3, design: .rounded).weight(.bold))
                            .monospacedDigit()
                        Text("\(formatClock(w)) · \(formatClock(b))")
                            .font(.system(.caption2, design: .rounded))
                            .monospacedDigit()
                            .foregroundStyle(.secondary)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .trailing)
            }
            DynamicIslandExpandedRegion(.bottom) {
                HStack(spacing: 8) {
                    if state.inCheck {
                        Label(String(localized: "Šach", comment: "Live Activity: check banner"), systemImage: "exclamationmark.shield.fill")
                            .font(.system(.caption2, design: .rounded).weight(.semibold))
                            .foregroundStyle(LiveActivityPalette.checkRed)
                    }
                    Text(state.subtitle)
                        .font(.system(.caption2, design: .rounded))
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                        .minimumScaleFactor(0.9)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        } compactLeading: {
            Image(systemName: "checkerboard.rectangle")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(LiveActivityPalette.accent)
                .accessibilityLabel(String(localized: "CZECHMATE šachy", comment: "Live Activity compact leading"))
        } compactTrailing: {
            compactTrailingView(state: state)
        } minimal: {
            ZStack(alignment: .topTrailing) {
                Text(ChessPieceGlyph.symbol(for: turnPawnChar(state.currentPlayer)))
                    .chessPieceAppearance(
                        fenChar: turnPawnChar(state.currentPlayer),
                        isLightSquare: false,
                        font: .caption2.weight(.semibold)
                    )
                if state.inCheck {
                    Circle()
                        .fill(LiveActivityPalette.checkRed)
                        .frame(width: 5, height: 5)
                        .offset(x: 2, y: -2)
                }
            }
            .accessibilityLabel(minimalAccessibilityLabel(state: state))
        }
    }

    @ViewBuilder
    private func compactTrailingView(state: ChessMatchAttributes.ContentState) -> some View {
        if state.timerActive,
           let active = activePlayerSeconds(state)
        {
            Text(formatClock(active))
                .font(.system(.caption2, design: .rounded).weight(.semibold))
                .monospacedDigit()
                .accessibilityLabel("Čas na tahu \(playerLabel(state.currentPlayer)): \(formatClock(active))")
        } else {
            Text("\(state.moveCount)")
                .font(.caption.weight(.semibold).monospacedDigit())
                .accessibilityLabel("Počet tahů \(state.moveCount)")
        }
    }

    private func minimalAccessibilityLabel(state: ChessMatchAttributes.ContentState) -> String {
        let who = playerLabel(state.currentPlayer)
        if state.inCheck {
            return "Na tahu \(who), šach"
        }
        return "Na tahu \(who)"
    }

    private func activePlayerSeconds(_ state: ChessMatchAttributes.ContentState) -> UInt32? {
        guard state.timerActive,
              let w = state.whiteTimeSeconds,
              let b = state.blackTimeSeconds else { return nil }
        return isWhiteTurn(state.currentPlayer) ? w : b
    }

    private func turnPawnChar(_ raw: String) -> Character {
        isWhiteTurn(raw) ? "P" : "p"
    }

    private func playerLabel(_ raw: String) -> String {
        let l = raw.lowercased()
        if l == "white" || l == "bílý" { return String(localized: "Bílý", comment: "White") }
        if l == "black" || l == "černý" { return String(localized: "Černý", comment: "Black") }
        return raw
    }

    private func isWhiteTurn(_ raw: String) -> Bool {
        let l = raw.lowercased()
        return l == "white" || l == "bílý"
    }

    private func formatClock(_ sec: UInt32) -> String {
        let m = Int(sec) / 60
        let r = Int(sec) % 60
        return String(format: "%d:%02d", m, r)
    }

    private func timerPill(label: String, seconds: UInt32, active: Bool) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.system(.caption2, design: .rounded))
                .foregroundStyle(.secondary)
            Text(formatClock(seconds))
                .font(.system(.body, design: .rounded).weight(.semibold))
                .monospacedDigit()
                .foregroundStyle(active ? .primary : .secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(10)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(active ? LiveActivityPalette.accent.opacity(0.18) : Color.primary.opacity(0.06))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .strokeBorder(active ? LiveActivityPalette.accent.opacity(0.45) : Color.clear, lineWidth: 1)
        )
    }
}
