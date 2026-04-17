//
//  StatsView.swift
//

import Charts
import SwiftData
import SwiftUI

/// Obsah statistik (sdíleno se záložkou Pokrok).
struct StatsTabContent: View {
    @Environment(BoardConnectionStore.self) private var store
    @Query(sort: \GameSessionEntity.startedAt, order: .reverse) private var sessions: [GameSessionEntity]
    @State private var exportText: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                Section {
                    VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                        AppSectionHeader(
                            title: "Lokální přehled",
                            subtitle: "Čísla z této instalace",
                            systemImage: "chart.bar.doc.horizontal"
                        )
                        LabeledContent("Aktuální počet tahů") {
                            Text("\(store.snapshot.map { Int($0.status.moveCount) } ?? 0)")
                        }
                        LabeledContent("Max. pozorovaný počet tahů") {
                            Text("\(StatsRecorder.peakMoveCount)")
                        }
                        LabeledContent("Aktualizací snapshotu (od instalace)") {
                            Text("\(StatsRecorder.snapshotUpdateCount)")
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .themeCard()
                }

                if sessions.isEmpty {
                    ThemedBanner(style: .neutral) {
                        Text("Zatím žádné uložené relace připojení — přibyde po spuštění pollingu na kartě Hra nebo v Nastavení.")
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                    }
                }

                if !sessions.isEmpty {
                    VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                        AppSectionHeader(
                            title: "Relace připojení",
                            subtitle: "SwiftData · start/stop pollingu",
                            systemImage: "clock.arrow.circlepath"
                        )
                        ForEach(sessions.prefix(12), id: \.id) { s in
                            VStack(alignment: .leading, spacing: 4) {
                                Text("\(s.linkKind) · tahů max \(s.peakMoveCount)")
                                    .font(.subheadline.weight(.medium))
                                Text(sessionLine(s))
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                            .padding(.vertical, 4)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .themeCard()
                }

                weeklyChartSection

                exportSection
            }
            .padding(Theme.Spacing.l)
        }
        .background(Color(uiColor: .systemGroupedBackground))
    }

    private func sessionLine(_ s: GameSessionEntity) -> String {
        let f = DateFormatter()
        f.dateStyle = .short
        f.timeStyle = .short
        let start = f.string(from: s.startedAt)
        if let e = s.endedAt {
            return "\(start) → \(f.string(from: e))"
        }
        return "\(start) → …"
    }

    @ViewBuilder
    private var weeklyChartSection: some View {
        let buckets = StatsWeeklyBuckets.build(from: sessions)
        if !buckets.isEmpty {
            VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                AppSectionHeader(title: "Relace za poslední týdny", systemImage: "chart.bar.xaxis")
                Chart(buckets) { b in
                    BarMark(
                        x: .value("Týden", b.label),
                        y: .value("Počet", b.count)
                    )
                    .foregroundStyle(Theme.accent)
                }
                .frame(height: 180)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .themeCard()
        }
    }

    @ViewBuilder
    private var exportSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            AppSectionHeader(title: "Export (JSON)", subtitle: "Bez serveru", systemImage: "square.and.arrow.up")
            Button("Připravit export") {
                exportText = StatsExportBuilder.buildJSON(
                    peakMoves: StatsRecorder.peakMoveCount,
                    snapshotUpdates: StatsRecorder.snapshotUpdateCount,
                    sessions: sessions
                )
            }
            .buttonStyle(.themePrimary)
            if let exportText {
                ShareLink(item: exportText, subject: Text("CZECHMATE statistiky")) {
                    Label("Sdílet", systemImage: "square.and.arrow.up")
                }
                .buttonStyle(.themeSecondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }
}

struct StatsView: View {
    var body: some View {
        NavigationStack {
            StatsTabContent()
                .navigationTitle("Statistiky")
                .navigationBarTitleDisplayMode(.inline)
                .tint(Theme.accent)
        }
    }
}

private enum StatsWeeklyBuckets {
    struct Week: Identifiable {
        let id: String
        let label: String
        let count: Int
    }

    static func build(from sessions: [GameSessionEntity]) -> [Week] {
        let cal = Calendar.current
        var counts: [String: Int] = [:]
        for s in sessions {
            let w = cal.component(.weekOfYear, from: s.startedAt)
            let y = cal.component(.yearForWeekOfYear, from: s.startedAt)
            let key = "\(y)-W\(w)"
            counts[key, default: 0] += 1
        }
        return counts.keys.sorted().suffix(8).map { k in
            Week(id: k, label: k, count: counts[k] ?? 0)
        }
    }
}

private enum StatsExportBuilder {
    struct Root: Codable {
        var peakMoveCount: Int
        var snapshotUpdateCount: Int
        var sessions: [SessionRow]
    }

    struct SessionRow: Codable {
        var id: String
        var linkKind: String
        var startedAt: String
        var endedAt: String?
        var peakMoveCount: Int
    }

    static func buildJSON(
        peakMoves: Int,
        snapshotUpdates: Int,
        sessions: [GameSessionEntity]
    ) -> String {
        let df = ISO8601DateFormatter()
        let rows = sessions.map {
            SessionRow(
                id: $0.id.uuidString,
                linkKind: $0.linkKind,
                startedAt: df.string(from: $0.startedAt),
                endedAt: $0.endedAt.map { df.string(from: $0) },
                peakMoveCount: $0.peakMoveCount
            )
        }
        let root = Root(
            peakMoveCount: peakMoves,
            snapshotUpdateCount: snapshotUpdates,
            sessions: rows
        )
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let data = try? enc.encode(root), let str = String(data: data, encoding: .utf8) else {
            return "{}"
        }
        return str
    }
}
