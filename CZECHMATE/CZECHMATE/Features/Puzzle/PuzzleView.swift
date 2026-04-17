//
//  PuzzleView.swift
//  Denní puzzle (Lichess), knihovna SwiftData, trénink.
//

import SwiftData
import SwiftUI
#if os(iOS)
import UIKit
#endif

private enum PuzzleTrainingPoolMode: String, CaseIterable, Identifiable {
    case mixed
    case bundledOnly
    case libraryOnly

    var id: String { rawValue }

    var title: String {
        switch self {
        case .mixed: return "Mix"
        case .bundledOnly: return "Vestavěné"
        case .libraryOnly: return "Uložené"
        }
    }
}

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

    @State private var newTitle = ""
    @State private var newFEN = ""
    @State private var newSolution = ""
    @State private var trainingSeconds: UInt = 120
    @State private var librarySaveMessage: String?
    @AppStorage("czechmate.puzzleTrainingPoolMode") private var trainingPoolModeRaw: String = PuzzleTrainingPoolMode.mixed.rawValue
    @AppStorage("czechmate.puzzleTrainingSessionsStarted") private var trainingSessionsStarted: Int = 0

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
                    "Nahrání puzzle na desku se nepodařilo nebo není k dispozici připojení. "
                        + "Zkus znovu přes Wi‑Fi nebo Bluetooth, případně přejdi do záložky Hra a použij „Zkusit vyřešit na obrazovce“."
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
                    subtitle: "Lichess · úloha na dnes",
                    systemImage: "calendar"
                )

                if let librarySaveMessage {
                    ThemedBanner(style: .neutral) {
                        Text(librarySaveMessage)
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                    }
                }

                if let p = dailyPuzzle {
                    VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                        HStack(alignment: .top, spacing: Theme.Spacing.m) {
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
                                    .fixedSize(horizontal: false, vertical: true)
                                if !p.solutionMoves.isEmpty {
                                    Text(solutionMoveCountLabel(p.solutionMoves.count))
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

                        HStack {
                            Spacer(minLength: 0)
                            MiniBoardFromFENView(fen: p.fen, cellSide: 40)
                            Spacer(minLength: 0)
                        }
                        .accessibilityElement(children: .combine)
                        .accessibilityLabel("Šachovnice, \(FENParser.sideToMoveDescription(fromFEN: p.fen))")
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
                                Label("Nahrát na desku (LED, krok za krokem)", systemImage: "square.and.arrow.up")
                            }
                        }
                        .buttonStyle(.themeOutline)
                        .disabled(isSendingToBoard || !store.supportsRemoteChessCommands)

                        Button {
                            addDailyPuzzleToLibrary(p)
                        } label: {
                            Label(
                                isDailyPuzzleAlreadyInLibrary(p) ? "Už je v knihovně" : "Přidat do knihovny",
                                systemImage: "books.vertical.fill"
                            )
                        }
                        .buttonStyle(.themeSecondary)
                        .disabled(isDailyPuzzleAlreadyInLibrary(p))
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
        .onChange(of: librarySaveMessage) { _, new in
            guard let msg = new else { return }
            Task { @MainActor in
                try? await Task.sleep(nanoseconds: 5_000_000_000)
                if librarySaveMessage == msg {
                    librarySaveMessage = nil
                }
            }
        }
    }

    private func bundledPuzzleCard(_ item: BundledPuzzleItem) -> some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            VStack(alignment: .leading, spacing: 4) {
                Text(item.title)
                    .font(.system(.headline, design: .rounded))
                Text(item.subtitle)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                if !item.themes.isEmpty {
                    Text(item.themes.map { themeChipTitle($0) }.joined(separator: " · "))
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                        .lineLimit(2)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            HStack {
                Spacer(minLength: 0)
                MiniBoardFromFENView(fen: item.fen, cellSide: 38)
                Spacer(minLength: 0)
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("\(item.title), náhled pozice")

            VStack(spacing: Theme.Spacing.s) {
                Button {
                    store.loadPuzzlePosition(fromFEN: item.fen)
                    tabRouter.selectedTab = .game
                } label: {
                    Text("Zkusit vyřešit na obrazovce")
                }
                .buttonStyle(.themePrimary)

                Button {
                    Task { await sendPuzzleToBoard(fen: item.fen) }
                } label: {
                    if isSendingToBoard {
                        ProgressView()
                            .frame(maxWidth: .infinity)
                    } else {
                        Label("Nahrát na desku (LED, krok za krokem)", systemImage: "square.and.arrow.up")
                    }
                }
                .buttonStyle(.themeOutline)
                .disabled(isSendingToBoard || !store.supportsRemoteChessCommands)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .themeCard()
    }

    private func solutionMoveCountLabel(_ n: Int) -> String {
        switch n {
        case 1: return "Řešení: 1 tah"
        case 2 ... 4: return "Řešení: \(n) tahy"
        default: return "Řešení: \(n) tahů"
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
            "advancedPawn": "Postup pěšce",
        ]
        return map[key] ?? key.replacingOccurrences(of: "_", with: " ")
    }

    private func sendPuzzleToBoard(fen: String) async {
        isSendingToBoard = true
        errorText = nil
        defer { isSendingToBoard = false }
        guard store.supportsRemoteChessCommands else {
            showManualSetupDialog = true
            return
        }
        let ok = await store.beginFenBoardSetupWizard(fen: fen, loadFenWhenDone: true)
        if ok {
            store.clearPuzzlePosition()
            tabRouter.selectedTab = .game
        } else {
            errorText = store.lastError ?? "Nepodařilo se spustit nahrávání na desku."
            showManualSetupDialog = true
        }
    }

    private func loadDaily() async {
        isLoadingDaily = true
        errorText = nil
        defer { isLoadingDaily = false }
        do {
            dailyPuzzle = try await PuzzleService.fetchDailyPuzzle()
            librarySaveMessage = nil
        } catch {
            errorText = error.localizedDescription
        }
    }

    private func isDailyPuzzleAlreadyInLibrary(_ p: DailyPuzzle) -> Bool {
        let token = "lichess_id:\(p.id)"
        return libraryItems.contains { $0.notes.contains(token) }
    }

    private func addDailyPuzzleToLibrary(_ p: DailyPuzzle) {
        guard !isDailyPuzzleAlreadyInLibrary(p) else { return }
        let title = dailyLibraryTitle(for: p)
        let solution = p.solutionMoves.joined(separator: " ")
        var notes = "lichess_id:\(p.id)"
        if !p.themes.isEmpty {
            notes += "\n" + p.themes.map { themeChipTitle($0) }.joined(separator: " · ")
        }
        let item = PuzzleLibraryItem(
            title: title,
            fen: p.fen,
            solutionUCI: solution,
            source: "lichess-denní",
            notes: notes
        )
        modelContext.insert(item)
        try? modelContext.save()
        librarySaveMessage = "Úloha „\(title)“ je v záložce Knihovna."
    }

    private func dailyLibraryTitle(for p: DailyPuzzle) -> String {
        let df = DateFormatter()
        df.locale = Locale(identifier: "cs_CZ")
        df.setLocalizedDateFormatFromTemplate("dMMMM")
        let day = df.string(from: Date())
        if let r = p.rating {
            return "Denní Lichess · \(day) (\(r) Elo)"
        }
        return "Denní Lichess · \(day)"
    }

    // MARK: - Knihovna

    private var libraryTab: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                AppSectionHeader(
                    title: "Knihovna",
                    subtitle: "Vestavěné sady, vlastní FEN a uložené úlohy (SwiftData)",
                    systemImage: "books.vertical.fill"
                )

                AppSectionHeader(
                    title: "Vestavěné úlohy",
                    subtitle: "Offline v aplikaci — obrazovka nebo deska",
                    systemImage: "square.stack.3d.up.fill"
                )

                ForEach(BundledPuzzleCatalog.items) { item in
                    bundledPuzzleCard(item)
                }

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

                AppSectionHeader(
                    title: "Uložené puzzle",
                    subtitle: "Vlastní záznamy a denní úlohy přidané z Lichess",
                    systemImage: "folder.fill"
                )

                if libraryItems.isEmpty {
                    ThemedBanner(style: .neutral) {
                        Text("Zatím žádná vlastní uložená puzzle — přidej FEN výše nebo v záložce Denní ulož dnešní úlohu.")
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

    private var trainingPoolMode: PuzzleTrainingPoolMode {
        PuzzleTrainingPoolMode(rawValue: trainingPoolModeRaw) ?? .mixed
    }

    private var trainingPoolModeBinding: Binding<PuzzleTrainingPoolMode> {
        Binding(
            get: { PuzzleTrainingPoolMode(rawValue: trainingPoolModeRaw) ?? .mixed },
            set: { trainingPoolModeRaw = $0.rawValue }
        )
    }

    private var trainingTab: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                AppSectionHeader(
                    title: "Trénink",
                    subtitle: "Náhodná úloha, limit času a živý odpočet na kartě Hra",
                    systemImage: "timer"
                )

                if store.puzzleBoardPreview != nil, store.puzzleTrainingDeadline != nil {
                    ThemedBanner(style: .neutral) {
                        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                            Label("Probíhá trénink", systemImage: "figure.run")
                                .font(Theme.Typography.subsection())
                            Text("Odpočet a zavření puzzle je na kartě Hra nad šachovnicí.")
                                .font(Theme.Typography.caption())
                                .foregroundStyle(.secondary)
                            Button("Přejít na Hru") {
                                tabRouter.selectedTab = .game
                            }
                            .buttonStyle(.themeSecondary)
                        }
                    }
                }

                VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                    Text("Délka kola")
                        .font(Theme.Typography.caption().weight(.semibold))
                        .foregroundStyle(.secondary)
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: Theme.Spacing.s) {
                            ForEach([UInt(60), 120, 180, 300], id: \.self) { sec in
                                Button {
                                    trainingSeconds = sec
                                    HapticSettings.lightImpactIfEnabled()
                                } label: {
                                    Text("\(sec) s")
                                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                                        .frame(minWidth: 64)
                                }
                                .buttonStyle(.bordered)
                                .tint(trainingSeconds == sec ? Theme.accent : Color.secondary)
                            }
                        }
                    }
                    Stepper(value: $trainingSeconds, in: 30 ... 600, step: 15) {
                        Text("Nebo jemně: \(trainingSeconds) s")
                            .font(Theme.Typography.body())
                    }
                }
                .themeCard()

                VStack(alignment: .leading, spacing: Theme.Spacing.m) {
                    Text("Odkud brát úlohy")
                        .font(Theme.Typography.caption().weight(.semibold))
                        .foregroundStyle(.secondary)
                    Picker("Pool", selection: trainingPoolModeBinding) {
                        ForEach(PuzzleTrainingPoolMode.allCases) { mode in
                            Text(mode.title).tag(mode)
                        }
                    }
                    .pickerStyle(.segmented)
                    Text(trainingPoolFootnote)
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.tertiary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .themeCard()

                ThemedBanner(style: .neutral) {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("Tip")
                            .font(Theme.Typography.caption().weight(.semibold))
                        Text("Po startu vyber tah na desce v aplikaci stejně jako při hře — jde o náhled, tahy se na desku neposílají. Čas běží jen jako výzva.")
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }

                if trainingSessionsStarted > 0 {
                    Text("Spuštěných kol celkem: \(trainingSessionsStarted)")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }

                Button {
                    guard let fen = randomTrainingFEN() else { return }
                    let deadline = Date().addingTimeInterval(TimeInterval(trainingSeconds))
                    store.loadPuzzlePosition(fromFEN: fen, trainingDeadline: deadline)
                    trainingSessionsStarted += 1
                    HapticSettings.mediumImpactIfEnabled()
                    tabRouter.selectedTab = .game
                } label: {
                    Label("Náhodná úloha a start", systemImage: "play.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.themePrimary)
                .disabled(!hasTrainingPuzzlePool)
            }
            .padding(Theme.Spacing.l)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var trainingPoolFootnote: String {
        let b = BundledPuzzleCatalog.items.count
        let u = libraryItems.count
        switch trainingPoolMode {
        case .mixed:
            return "V poolu je \(b) vestavěných a \(u) uložených úloh — výběr je náhodný."
        case .bundledOnly:
            return b == 0 ? "Vestavěné sady nejsou k dispozici." : "Náhodně z \(b) vestavěných pozic."
        case .libraryOnly:
            return u == 0 ? "Nejdřív si ulož puzzle v Knihovně." : "Náhodně z \(u) tvých uložených pozic."
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

    private var hasTrainingPuzzlePool: Bool {
        switch trainingPoolMode {
        case .mixed:
            return !BundledPuzzleCatalog.items.isEmpty || !libraryItems.isEmpty
        case .bundledOnly:
            return !BundledPuzzleCatalog.items.isEmpty
        case .libraryOnly:
            return !libraryItems.isEmpty
        }
    }

    private func randomTrainingFEN() -> String? {
        switch trainingPoolMode {
        case .mixed:
            var pool = BundledPuzzleCatalog.items.map(\.fen)
            pool.append(contentsOf: libraryItems.map(\.fen))
            return pool.randomElement()
        case .bundledOnly:
            return BundledPuzzleCatalog.items.randomElement()?.fen
        case .libraryOnly:
            return libraryItems.randomElement()?.fen
        }
    }
}
