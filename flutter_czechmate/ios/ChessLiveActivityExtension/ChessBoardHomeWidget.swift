import SwiftUI
import UIKit
import WidgetKit

private let kWidgetAppGroup = "group.com.example.flutterCzechmate.widget"

private func czFmtMs(_ ms: Int) -> String {
  let s = max(0, ms / 1000)
  let m = s / 60
  let r = s % 60
  return String(format: "%d:%02d", m, r)
}

private func transportChip(_ raw: String) -> String {
  switch raw {
  case "ble": return "BLE"
  case "wifi": return "Wi‑Fi"
  case "mock": return "Demo"
  default: return raw.isEmpty ? "—" : raw
  }
}

private func defaultsInt(_ d: UserDefaults?, _ key: String) -> Int {
  (d?.object(forKey: key) as? NSNumber)?.intValue ?? 0
}

private func defaultsBool(_ d: UserDefaults?, _ key: String) -> Bool {
  d?.bool(forKey: key) ?? false
}

struct ChessBoardHomeEntry: TimelineEntry {
  let date: Date
  let phase: String
  let transport: String
  let whiteMs: Int
  let blackMs: Int
  let whiteTurn: Bool
  let timerRunning: Bool
  let gamePaused: Bool
  let totalMoves: Int
  let tcLabel: String
  let gameFinished: Bool
}

struct ChessBoardHomeProvider: TimelineProvider {
  func placeholder(in context: Context) -> ChessBoardHomeEntry {
    ChessBoardHomeEntry(
      date: Date(),
      phase: "clock",
      transport: "Wi‑Fi",
      whiteMs: 300_000,
      blackMs: 300_000,
      whiteTurn: true,
      timerRunning: true,
      gamePaused: false,
      totalMoves: 0,
      tcLabel: "Blitz",
      gameFinished: false)
  }

  func getSnapshot(in context: Context, completion: @escaping (ChessBoardHomeEntry) -> Void) {
    completion(loadEntry())
  }

  func getTimeline(in context: Context, completion: @escaping (Timeline<ChessBoardHomeEntry>) -> Void) {
    let e = loadEntry()
    let next =
      Calendar.current.date(byAdding: .minute, value: 30, to: e.date)
        ?? e.date.addingTimeInterval(1800)
    completion(Timeline(entries: [e], policy: .after(next)))
  }

  private func loadEntry() -> ChessBoardHomeEntry {
    let d = UserDefaults(suiteName: kWidgetAppGroup)
    let phase = d?.string(forKey: "cz_phase") ?? "no_game"
    let transport = transportChip(d?.string(forKey: "cz_transport") ?? "")
    return ChessBoardHomeEntry(
      date: Date(),
      phase: phase,
      transport: transport,
      whiteMs: defaultsInt(d, "cz_white_ms"),
      blackMs: defaultsInt(d, "cz_black_ms"),
      whiteTurn: defaultsBool(d, "cz_white_turn"),
      timerRunning: defaultsBool(d, "cz_timer_running"),
      gamePaused: defaultsBool(d, "cz_game_paused"),
      totalMoves: defaultsInt(d, "cz_total_moves"),
      tcLabel: d?.string(forKey: "cz_tc_label") ?? "",
      gameFinished: defaultsBool(d, "cz_game_finished"))
  }
}

struct ChessBoardHomeWidgetEntryView: View {
  @Environment(\.widgetFamily) var family
  var entry: ChessBoardHomeProvider.Entry

  var body: some View {
    switch entry.phase {
    case "no_game":
      noGameView
    case "no_timer":
      noTimerView
    case "clock":
      clockView
    default:
      Text(entry.phase)
        .font(.caption)
    }
  }

  private var noGameView: some View {
    VStack(alignment: .leading, spacing: 6) {
      header
      Text("Otevři aplikaci a připoj desku.")
        .font(.subheadline)
        .foregroundStyle(.secondary)
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
    .padding()
  }

  private var noTimerView: some View {
    VStack(alignment: .leading, spacing: 6) {
      header
      if entry.gameFinished {
        Text("Partie ukončena")
          .font(.headline)
      } else {
        Text("Tahů: \(entry.totalMoves)")
          .font(.subheadline)
          .foregroundStyle(.secondary)
      }
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
    .padding()
  }

  private var clockView: some View {
    VStack(alignment: .leading, spacing: 8) {
      header
      if entry.gameFinished {
        Text("Partie ukončena")
          .font(.title3)
          .fontWeight(.semibold)
      } else if family == .systemSmall {
        VStack(alignment: .leading, spacing: 4) {
          Text(entry.whiteTurn ? "Na tahu ♔" : "Na tahu ♚")
            .font(.caption)
            .foregroundStyle(.secondary)
          Text(czFmtMs(entry.whiteTurn ? entry.whiteMs : entry.blackMs))
            .font(.system(.title, design: .monospaced))
            .fontWeight(.bold)
          secondaryLine
        }
      } else {
        HStack {
          VStack(alignment: .leading, spacing: 4) {
            Text("♔")
              .font(.caption2)
              .foregroundStyle(.secondary)
            Text(czFmtMs(entry.whiteMs))
              .font(.title2.monospacedDigit())
              .fontWeight(entry.whiteTurn && !entry.gameFinished ? .bold : .regular)
          }
          Spacer()
          VStack(alignment: .trailing, spacing: 4) {
            Text("♚")
              .font(.caption2)
              .foregroundStyle(.secondary)
            Text(czFmtMs(entry.blackMs))
              .font(.title2.monospacedDigit())
              .fontWeight(entry.whiteTurn || entry.gameFinished ? .regular : .bold)
          }
        }
        secondaryLine
      }
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
    .padding()
  }

  private var header: some View {
    HStack {
      Text("CzechMate")
        .font(.headline)
      Spacer()
      Text(entry.transport)
        .font(.caption2)
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
        .background(.quaternary.opacity(0.4))
        .clipShape(Capsule())
    }
  }

  @ViewBuilder
  private var secondaryLine: some View {
    HStack {
      if entry.gamePaused {
        Label("Pauza", systemImage: "pause.circle.fill")
          .font(.caption2)
      } else if entry.timerRunning {
        Label("Běží", systemImage: "play.circle.fill")
          .font(.caption2)
      } else {
        Label("Čeká", systemImage: "clock")
          .font(.caption2)
      }
      Spacer()
      if !entry.tcLabel.isEmpty {
        Text(entry.tcLabel)
          .font(.caption2)
          .foregroundStyle(.secondary)
      }
      Text("· \(entry.totalMoves)")
        .font(.caption2)
        .foregroundStyle(.secondary)
    }
  }
}

struct ChessBoardHomeWidget: Widget {
  let kind: String = "ChessBoardHomeWidget"

  var body: some WidgetConfiguration {
    StaticConfiguration(kind: kind, provider: ChessBoardHomeProvider()) { entry in
      Group {
        if #available(iOSApplicationExtension 17.0, *) {
          ChessBoardHomeWidgetEntryView(entry: entry)
            .containerBackground(.fill.tertiary, for: .widget)
        } else {
          ChessBoardHomeWidgetEntryView(entry: entry)
            .padding()
            .background(Color(UIColor.secondarySystemGroupedBackground))
        }
      }
    }
    .configurationDisplayName("CzechMate")
    .description("Stav partie a časomíry na desce.")
    .supportedFamilies([.systemSmall, .systemMedium])
  }
}
