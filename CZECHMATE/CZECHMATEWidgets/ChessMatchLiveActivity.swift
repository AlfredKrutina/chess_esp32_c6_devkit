//
//  ChessMatchLiveActivity.swift
//

import ActivityKit
import CZECHMATEShared
import SwiftUI
import WidgetKit

struct ChessMatchLiveActivity: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: ChessMatchAttributes.self) { context in
            VStack(alignment: .leading, spacing: 6) {
                HStack(alignment: .firstTextBaseline) {
                    Text("CZECHMATE")
                        .font(.system(.headline, design: .rounded))
                    Spacer(minLength: 0)
                    Text("\(context.state.moveCount) tahů")
                        .font(.system(.caption, design: .rounded))
                        .foregroundStyle(.secondary)
                }
                if !context.state.advantageSummary.isEmpty {
                    Text(context.state.advantageSummary)
                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                        .foregroundStyle(.primary)
                }
                Text(context.state.subtitle)
                    .font(.system(.caption, design: .rounded))
                    .foregroundStyle(.secondary)
                    .lineLimit(2)
                HStack {
                    Text("Na tahu")
                        .font(.system(.caption2, design: .rounded))
                        .foregroundStyle(.secondary)
                    Text(playerLabel(context.state.currentPlayer))
                        .font(.system(.caption, design: .rounded).weight(.semibold))
                    Spacer(minLength: 0)
                    if context.state.inCheck {
                        Image(systemName: "exclamationmark.shield.fill")
                            .foregroundStyle(.red)
                    }
                }
                if context.state.timerActive,
                   let w = context.state.whiteTimeSeconds,
                   let b = context.state.blackTimeSeconds
                {
                    HStack(spacing: 12) {
                        timerPill(label: "Bílý", seconds: w, active: isWhiteTurn(context.state.currentPlayer))
                        timerPill(label: "Černý", seconds: b, active: !isWhiteTurn(context.state.currentPlayer))
                    }
                }
            }
            .padding(12)
            .activityBackgroundTint(Color(red: 0.12, green: 0.45, blue: 0.38).opacity(0.25))
            .widgetURL(URL(string: "czechmate://game"))
        } dynamicIsland: { context in
            DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    VStack(alignment: .leading, spacing: 4) {
                        if !context.state.advantageSummary.isEmpty {
                            Text(context.state.advantageSummary)
                                .font(.system(.caption, design: .rounded).weight(.semibold))
                        }
                        Text(ChessPieceGlyph.symbol(for: turnPawnChar(context.state.currentPlayer)))
                            .chessPieceAppearance(
                                fenChar: turnPawnChar(context.state.currentPlayer),
                                isLightSquare: false,
                                font: .title2.weight(.semibold)
                            )
                    }
                }
                DynamicIslandExpandedRegion(.trailing) {
                    VStack(alignment: .trailing, spacing: 2) {
                        Text(playerLabel(context.state.currentPlayer))
                            .font(.system(.caption, design: .rounded).weight(.semibold))
                        if context.state.timerActive,
                           let w = context.state.whiteTimeSeconds,
                           let b = context.state.blackTimeSeconds
                        {
                            Text("\(formatClock(w)) · \(formatClock(b))")
                                .font(.system(.caption2, design: .rounded).monospacedDigit())
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                DynamicIslandExpandedRegion(.bottom) {
                    Text(context.state.subtitle)
                        .font(.system(.caption2, design: .rounded))
                        .lineLimit(2)
                }
            } compactLeading: {
                Image(systemName: "crown.fill")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.yellow.opacity(0.9))
            } compactTrailing: {
                if context.state.timerActive, let w = context.state.whiteTimeSeconds {
                    Text(formatClock(w))
                        .font(.system(.caption2, design: .rounded).monospacedDigit())
                } else {
                    Text("\(context.state.moveCount)")
                        .font(.caption.monospacedDigit())
                }
            } minimal: {
                Text(ChessPieceGlyph.symbol(for: turnPawnChar(context.state.currentPlayer)))
                    .chessPieceAppearance(
                        fenChar: turnPawnChar(context.state.currentPlayer),
                        isLightSquare: false,
                        font: .caption.weight(.semibold)
                    )
            }
        }
    }

    private func turnPawnChar(_ raw: String) -> Character {
        isWhiteTurn(raw) ? "P" : "p"
    }

    private func playerLabel(_ raw: String) -> String {
        let l = raw.lowercased()
        if l == "white" || l == "bílý" { return "Bílý" }
        if l == "black" || l == "černý" { return "Černý" }
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
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.system(.caption2, design: .rounded))
                .foregroundStyle(.secondary)
            Text(formatClock(seconds))
                .font(.system(.body, design: .rounded).weight(.semibold))
                .monospacedDigit()
                .foregroundStyle(active ? .primary : .secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(8)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(active ? Color.green.opacity(0.2) : Color.secondary.opacity(0.12))
        )
    }
}
