//
//  PuzzleView.swift
//  Denní puzzle (Lichess), knihovna SwiftData, trénink.
//

import SwiftData
import SwiftUI
#if os(iOS)
import UIKit
#endif

struct PuzzleView: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(AppTabRouter.self) private var tabRouter
    @Environment(\.modelContext) private var modelContext
    @Query(sort: \PuzzleLibraryItem.createdAt, order: .reverse) private var libraryItems: [PuzzleLibraryItem]

    @State private var selectedTab = 0
    @State private var isLoadingDaily = false
    @State private var dailyPuzzle: DailyPuzzle?
    @State private var errorText: String?
    @State private var showManualSetupDialog = false
    @State private var isSendingToBoard = false
    @State private var isGuidedSetupBusy = false

    @State private var newTitle = ""
    @State private var newFEN = ""
    @State private var newSolution = ""
    @State private var trainingSeconds: UInt = 120
    @State private var trainingDeadline: Date?

    var body: some View {
        NavigationStack {
            Group {
                switch selectedTab {
                case 0: dailyTab
                case 1: libraryTab
                default: trainingTab
                }
            }
            .navigationTitle("Puzzle")
            .toolbar {
                ToolbarItem(placement: .principal) {
                    Picker("", selection: $selectedTab) {
                        Text("Denní").tag(0)
                        Text("Knihovna").tag(1)
                        Text("Trénink").tag(2)
                    }
                    .pickerStyle(.segmented)
                }
            }
            .tint(Theme.accent)
            .alert("Postavení figurek na desce", isPresented: $showManualSetupDialog) {
                Button("Rozumím", role: .cancel) {}
            } message: {
                Text(
                    "Automatické nahrání této pozice na CZECHMATE přes síť se nepodařilo nebo není k dispozici. "
                        + "Nastavte figury na fyzické desce podle náhledu v aplikaci, případně přejděte do záložky Hra a použijte „Zkusit vyřešit na obrazovce“."
                )
            }
        }
    }

    // MARK: - Denní puzzle

    private var dailyTab: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                AppSectionHeader(
                    title: "Denní puzzle",
                    subtitle: "Lichess · tréninková úloha na dnes",
                    systemImage: "calendar"
                )

                if let p = dailyPuzzle {
                    VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                        HStack {
                            Spacer(minLength: 0)
                            MiniBoardFromFENView(fen: p.fen, cellSide: 40)
                            Spacer(minLength: 0)
                        }
                        .accessibilityElement(children: .combine)
                        .accessibilityLabel("Šachovnice, \(FENParser.sideToMoveDescription(fromFEN: p.fen))")

                        HStack(alignment: .firstTextBaseline, spacing: Theme.Spacing.m) {
                            if let r = p.rating {
                                Label {
                                    Text("\(r)")
                                        .font(.system(.title2, design: .rounded).weight(.semibold))
                                        .foregroundStyle(Theme.accent)
                                } icon: {
                                    Image(systemName: "chart.bar.fill")
                                        .foregroundStyle(Theme.accent.opacity(0.85))
                                }
                                .accessibilityLabel("Rating \(r)")
                            }
                            VStack(alignment: .leading, spacing: 4) {
                                Text(FENParser.sideToMoveDescription(fromFEN: p.fen))
                                    .font(.system(.headline, design: .rounded))
                                if !p.solutionMoves.isEmpty {
                                    Text("Řešení: \(p.solutionMoves.count) tahů")
                                        .font(Theme.Typography.caption())
                                        .foregroundStyle(.secondary)
                                }
                            }
                            Spacer(minLength: 0)
                        }

                        if !p.themes.isEmpty {
                            ScrollView(.horizontal, showsIndicators: false) {
                                HStack(spacing: 8) {
                                    ForEach(p.themes, id: \.self) { t in
                                        Text(themeChipTitle(t))
                                            .font(.system(.caption2, design: .rounded))
                                            .padding(.horizontal, 10)
                                            .padding(.vertical, 5)
                                            .background(
                                                Capsule(style: .continuous)
                                                    .fill(Color.secondary.opacity(0.14))
                                            )
                                    }
                                }
                            }
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .themeCard()

                    VStack(spacing: Theme.Spacing.m) {
                        Button {
                            store.loadPuzzlePosition(fromFEN: p.fen)
                            tabRouter.selectedTab = .game
                        } label: {
                            Text("Zkusit vyřešit na obrazovce")
                        }
                        .buttonStyle(.themePrimary)

                        Button {
                            Task { await sendPuzzleToBoard(fen: p.fen) }
                        } label: {
                            if isSendingToBoard {
                                ProgressView()
                                    .frame(maxWidth: .infinity)
                            } else {
                                Text("Poslat pozici na CZECHMATE desku")
                            }
                        }
                        .buttonStyle(.themeSecondary)
                        .disabled(isSendingToBoard)

                        Button {
                            Task { await startGuidedPhysicalSetup(fen: p.fen) }
                        } label: {
                            if isGuidedSetupBusy {
                                ProgressView()
                                    .frame(maxWidth: .infinity)
                            } else {
                                Text("Průvodce postavením na desce (LED)")
                            }
                        }
                        .buttonStyle(.themeSecondary)
                        .disabled(isGuidedSetupBusy || !store.supportsWiFiRemoteCommands)
                    }

                    Link(destination: URL(string: "https://lichess.org/training/daily")!) {
                        Label("Otevřít na Lichess", systemImage: "safari")
                            .font(.system(.subheadline, design: .rounded))
                            .frame(maxWidth: .infinity)
                    }
                    .padding(.top, 4)
                }

                if let errorText {
                    ThemedBanner(style: .error) {
                        Text(errorText)
                            .font(Theme.Typography.body())
                            .foregroundStyle(Theme.Semantic.errorForeground)
                    }
                }

                Button {
                    Task { await loadDaily() }
                } label: {
                    if isLoadingDaily {
                        ProgressView()
                    } else {
                        Text(dailyPuzzle == nil ? "Načíst denní puzzle" : "Obnovit denní úlohu")
                    }
                }
                .buttonStyle(.themeSecondary)
            }
            .padding(Theme.Spacing.l)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .task {
            await loadDaily()
        }
    }

    private func themeChipTitle(_ key: String) -> String {
        let map: [String: String] = [
            "opening": "Zahájení",
            "middlegame": "Střední hra",
            "endgame": "Koncovka",
            "mate": "Mat",
            "mateIn1": "Mat v 1",
            "mateIn2": "Mat ve 2",
            "mateIn3": "Mat ve 3",
            "advantage": "Výhoda",
            "crushing": "Drtivá výhoda",
            "equality": "Rovnováha",
            "defensive": "Obrana",
            "quietMove": "Tichý tah",
            "sacrifice": "Oběť",
            "long": "Dlouhá varianta",
            "short": "Krátká varianta",
            "oneMove": "Jeden tah",
        ]
        return map[key] ?? key.replacingOccurrences(of: "_", with: " ")
    }

    private func startGuidedPhysicalSetup(fen: String) async {
        isGuidedSetupBusy = true
        errorText = nil
        defer { isGuidedSetupBusy = false }
        let ok = await store.beginFenBoardSetupWizard(fen: fen, postNewGameWithFEN: true)
        if ok {
            tabRouter.selectedTab = .game
        } else {
            errorText = store.lastError ?? "Nepodařilo se spustit průvodce postavením."
        }
    }

    private func sendPuzzleToBoard(fen: String) async {
        isSendingToBoard = true
        errorText = nil
        defer { isSendingToBoard = false }
        guard store.supportsWiFiRemoteCommands else {
            showManualSetupDialog = true
            return
        }
        let ok = await store.postPuzzleFenToBoard(fen)
        if ok {
            store.clearPuzzlePosition()
            tabRouter.selectedTab = .game
        } else {
            errorText = store.lastError ?? "Nepodařilo se nahrát pozici na desku."
            showManualSetupDialog = true
        }
    }

    private func loadDaily() async {
        isLoadingDaily = true
        errorText = nil
        defer { isLoadingDaily = false }
        do {
            dailyPuzzle = try await PuzzleService.fetchDailyPuzzle()
        } catch {
            errorText = error.localizedDescription
        }
    }

    // MARK: - Knihovna

    private var libraryTab: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                AppSectionHeader(
                    title: "Knihovna",
                    subtitle: "Ukládáno lokálně (SwiftData)",
                    systemImage: "books.vertical.fill"
                )

                VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                    AppSectionHeader(title: "Nové puzzle", systemImage: "plus.square.fill")
                    TextField("Název", text: $newTitle)
                    TextField("FEN", text: $newFEN, axis: .vertical)
                        .font(.system(.caption, design: .monospaced))
                        .lineLimit(2 ... 4)
                    TextField("Řešení UCI (volitelné)", text: $newSolution)
                        .font(.caption)
                    Button("Uložit") {
                        savePuzzle()
                    }
                    .buttonStyle(.themePrimary)
                    .disabled(newTitle.trimmingCharacters(in: .whitespaces).isEmpty || newFEN.trimmingCharacters(in: .whitespaces).isEmpty)
                    #if os(iOS)
                    Button("Vložit FEN ze schránky") {
                        if let s = UIPasteboard.general.string {
                            newFEN = s
                        }
                    }
                    .buttonStyle(.themeSecondary)
                    #endif
                }
                .themeCard()

                if libraryItems.isEmpty {
                    ThemedBanner(style: .neutral) {
                        Text("Zatím žádná uložená puzzle — přidej FEN výše.")
                            .font(Theme.Typography.body())
                            .foregroundStyle(.secondary)
                    }
                } else {
                    ForEach(libraryItems) { item in
                        VStack(alignment: .leading, spacing: 6) {
                            Text(item.title)
                                .font(.headline)
                            Text(item.fen)
                                .font(.caption2.monospaced())
                                .lineLimit(2)
                            HStack {
                                Button("Hrát") {
                                    store.loadPuzzlePosition(fromFEN: item.fen)
                                    tabRouter.selectedTab = .game
                                }
                                .buttonStyle(.themeSecondary)
                                Button(role: .destructive) {
                                    modelContext.delete(item)
                                    try? modelContext.save()
                                } label: {
                                    Text("Smazat")
                                }
                            }
                        }
                        .padding()
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .themeCard()
                    }
                }
            }
            .padding(Theme.Spacing.l)
        }
    }

    private var trainingTab: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                AppSectionHeader(
                    title: "Trénink",
                    subtitle: "Náhodná z knihovny + čas",
                    systemImage: "timer"
                )
                Stepper("Čas: \(trainingSeconds) s", value: $trainingSeconds, in: 30 ... 600, step: 30)
                Button("Vybrat náhodnou a spustit") {
                    guard let p = libraryItems.randomElement() else { return }
                    store.loadPuzzlePosition(fromFEN: p.fen)
                    trainingDeadline = Date().addingTimeInterval(TimeInterval(trainingSeconds))
                    tabRouter.selectedTab = .game
                }
                .buttonStyle(.themePrimary)
                .disabled(libraryItems.isEmpty)
                if let trainingDeadline {
                    Text("Deadline: \(trainingDeadline.formatted(date: .omitted, time: .standard))")
                        .font(.caption)
                }
            }
            .padding(Theme.Spacing.l)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private func savePuzzle() {
        let t = newTitle.trimmingCharacters(in: .whitespacesAndNewlines)
        let f = newFEN.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty, !f.isEmpty else { return }
        let sol = newSolution.trimmingCharacters(in: .whitespacesAndNewlines)
        let item = PuzzleLibraryItem(
            title: t,
            fen: f,
            solutionUCI: sol,
            source: "ruční",
            notes: ""
        )
        modelContext.insert(item)
        try? modelContext.save()
        newTitle = ""
        newFEN = ""
        newSolution = ""
    }
}
