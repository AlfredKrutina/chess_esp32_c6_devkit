//
//  ChessLinkWidget.swift
//  Widget na plochu — odkaz do aplikace (bez síťových dat).
//

import SwiftUI
import WidgetKit

struct ChessLinkProvider: TimelineProvider {
    func placeholder(in _: Context) -> SimpleEntry {
        SimpleEntry(date: Date())
    }

    func getSnapshot(in _: Context, completion: @escaping (SimpleEntry) -> Void) {
        completion(SimpleEntry(date: Date()))
    }

    func getTimeline(in _: Context, completion: @escaping (Timeline<SimpleEntry>) -> Void) {
        let e = SimpleEntry(date: Date())
        completion(Timeline(entries: [e], policy: .never))
    }

    struct SimpleEntry: TimelineEntry {
        let date: Date
    }
}

struct ChessLinkWidgetEntryView: View {
    var entry: ChessLinkProvider.SimpleEntry

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("CZECHMATE")
                .font(.headline)
            Text("Otevřít hru")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
        .padding()
        .widgetURL(URL(string: "czechmate://game"))
    }
}

struct ChessLinkWidget: Widget {
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: "ChessLinkWidget", provider: ChessLinkProvider()) { entry in
            ChessLinkWidgetEntryView(entry: entry)
                .containerBackground(.fill.tertiary, for: .widget)
        }
        .configurationDisplayName("CZECHMATE")
        .description("Otevře záložku Hra.")
        .supportedFamilies([.systemSmall, .systemMedium])
    }
}
