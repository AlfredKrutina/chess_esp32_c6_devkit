import ActivityKit
import SwiftUI
import WidgetKit

private func czFormatMs(_ ms: Int) -> String {
  let s = max(0, ms / 1000)
  let m = s / 60
  let r = s % 60
  return String(format: "%d:%02d", m, r)
}

private func transportLabel(_ raw: String) -> String {
  switch raw {
  case "ble": return "BLE"
  case "wifi": return "Wi‑Fi"
  case "mock": return "Mock"
  default: return raw.isEmpty ? "—" : raw
  }
}

struct ChessLiveActivityWidget: Widget {
  var body: some WidgetConfiguration {
    ActivityConfiguration(for: ChessLiveAttributes.self) { context in
      ChessLockScreenView(state: context.state, title: context.attributes.matchTitle)
    } dynamicIsland: { context in
      DynamicIsland {
        DynamicIslandExpandedRegion(.leading) {
          VStack(alignment: .leading) {
            Text("♔ Bílý")
              .font(.caption2)
              .foregroundStyle(.secondary)
            Text(czFormatMs(context.state.whiteTimeMs))
              .font(.title3.monospacedDigit())
              .fontWeight(.semibold)
          }
        }
        DynamicIslandExpandedRegion(.trailing) {
          VStack(alignment: .trailing) {
            Text("♚ Černý")
              .font(.caption2)
              .foregroundStyle(.secondary)
            Text(czFormatMs(context.state.blackTimeMs))
              .font(.title3.monospacedDigit())
              .fontWeight(.semibold)
          }
        }
        DynamicIslandExpandedRegion(.bottom) {
          HStack {
            Text(context.state.timeControlLabel.isEmpty ? "Časomíra" : context.state.timeControlLabel)
              .font(.caption)
            Spacer()
            Text("Tahy: \(context.state.totalMoves)")
              .font(.caption)
          }
          .padding(.top, 4)
        }
      } compactLeading: {
        Text(context.state.isWhiteTurn ? "♔" : "♚")
      } compactTrailing: {
        Text(
          czFormatMs(
            context.state.isWhiteTurn ? context.state.whiteTimeMs : context.state.blackTimeMs
          )
        )
        .font(.caption2.monospacedDigit())
      } minimal: {
        Text(context.state.isWhiteTurn ? "♔" : "♚")
      }
    }
  }
}

private struct ChessLockScreenView: View {
  let state: ChessLiveAttributes.ContentState
  let title: String

  var body: some View {
    VStack(alignment: .leading, spacing: 8) {
      HStack {
        Text(title)
          .font(.headline)
        Spacer()
        Text(transportLabel(state.transport))
          .font(.caption)
          .padding(.horizontal, 8)
          .padding(.vertical, 4)
          .background(.quaternary.opacity(0.35))
          .clipShape(Capsule())
      }

      if state.phase == "no_timer" {
        Text("Čekání na časomíru · tahů \(state.totalMoves)")
          .font(.subheadline)
          .foregroundStyle(.secondary)
      } else if state.gameFinished {
        Text("Partie ukončena")
          .font(.title3)
          .fontWeight(.semibold)
      } else {
        HStack(spacing: 16) {
          VStack(alignment: .leading, spacing: 4) {
            Text("♔ Bílý")
              .font(.caption)
              .foregroundStyle(.secondary)
            Text(czFormatMs(state.whiteTimeMs))
              .font(.title2.monospacedDigit())
              .fontWeight(state.isWhiteTurn ? .bold : .regular)
          }
          Spacer()
          VStack(alignment: .trailing, spacing: 4) {
            Text("♚ Černý")
              .font(.caption)
              .foregroundStyle(.secondary)
            Text(czFormatMs(state.blackTimeMs))
              .font(.title2.monospacedDigit())
              .fontWeight(state.isWhiteTurn ? .regular : .bold)
          }
        }

        HStack {
          if state.gamePaused {
            Label("Pauza", systemImage: "pause.circle.fill")
              .font(.caption)
          } else if state.timerRunning {
            Label("Běží", systemImage: "play.circle.fill")
              .font(.caption)
          } else {
            Label("Čeká", systemImage: "clock")
              .font(.caption)
          }
          Spacer()
          Text("Tahy: \(state.totalMoves)")
            .font(.caption)
            .foregroundStyle(.secondary)
        }
      }
    }
    .padding()
  }
}
