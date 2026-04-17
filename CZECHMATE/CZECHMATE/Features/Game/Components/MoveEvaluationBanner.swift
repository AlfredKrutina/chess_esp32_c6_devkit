//
//  MoveEvaluationBanner.swift
//  Jednotné zobrazení Stockfish zhodnocení tahu ve všech herních layoutech.
//

import SwiftUI

extension MoveGrade {
    var feedbackForeground: Color {
        switch self {
        case .best, .good: Theme.Semantic.success
        case .inaccuracy: Color.yellow
        case .mistake: Theme.Semantic.warningForeground
        case .blunder: Theme.Semantic.errorForeground
        case .error: Color.secondary
        }
    }
}

/// Zobrazí poslední `lastMoveEvaluation`, pokud je zapnuté automatické hodnocení.
struct MoveEvaluationBanner: View {
    @Environment(BoardConnectionStore.self) private var store

    var compact: Bool = false

    var body: some View {
        if store.moveEvaluationEnabled, let ev = store.lastMoveEvaluation {
            VStack(alignment: .leading, spacing: compact ? 4 : Theme.Spacing.s) {
                if !compact {
                    AppSectionHeader(title: "Zhodnocení tahu")
                } else {
                    Label("Zhodnocení tahu", systemImage: "chart.line.uptrend.xyaxis")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                }
                Text(ev.message)
                    .font(compact ? Theme.Typography.caption() : Theme.Typography.body())
                    .foregroundStyle(ev.grade.feedbackForeground)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(compact ? Theme.Spacing.m : Theme.Spacing.l)
            .background(
                RoundedRectangle(cornerRadius: compact ? 12 : 14, style: .continuous)
                    .fill(compact ? Color.secondary.opacity(0.08) : Color.clear)
            )
            .modifier(NonCompactCardModifier(isCompact: compact))
            .accessibilityElement(children: .combine)
            .accessibilityLabel("Zhodnocení tahu: \(ev.message)")
        }
    }
}

private struct NonCompactCardModifier: ViewModifier {
    let isCompact: Bool

    func body(content: Content) -> some View {
        if isCompact {
            content
        } else {
            content.themeCard()
        }
    }
}

/// Banner při prohlížení pozice z historie (replay).
/// Připomínka, že je zapnutý režim hry proti botovi (NVS / návrhy z aplikace).
struct BotModeHintBanner: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        if store.boardUIPrefsPayload.botSettings.isBotMode, store.snapshot != nil {
            ThemedBanner(style: .accent, cornerRadius: 10) {
                Text("Režim bot: návrhy tahů na LED (Stockfish). Hraj fyzicky za stranu bota.")
                    .font(Theme.Typography.caption())
            }
        }
    }
}

struct HistoryReviewStatusBanner: View {
    @Environment(BoardConnectionStore.self) private var store

    var body: some View {
        if store.historyReviewMoveIndex != nil, store.puzzleBoardPreview == nil {
            ThemedBanner(style: .neutral, cornerRadius: 10) {
                HStack {
                    Label("Náhled pozice z historie", systemImage: "clock.arrow.circlepath")
                        .font(Theme.Typography.caption())
                    Spacer()
                    Button("Živá hra") {
                        store.historyReviewMoveIndex = nil
                    }
                    .buttonStyle(.themeSecondary)
                }
            }
        }
    }
}
