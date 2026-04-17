//
//  ContentView.swift
//  CZECHMATE_W Watch App — primárně přímé BLE k ESP; iPhone přes WatchConnectivity volitelně
//

import CZECHMATEShared
import SwiftUI

private enum WatchChessSquares {
    static let all: [String] = {
        var s: [String] = []
        for r in 1...8 {
            for f in ["a", "b", "c", "d", "e", "f", "g", "h"] {
                s.append("\(f)\(r)")
            }
        }
        return s
    }()
}

struct ContentView: View {
    @Environment(\.watchIntroReset) private var watchIntroReset
    @Environment(\.scenePhase) private var scenePhase

    @State private var model = WatchUnifiedSessionModel()
    @State private var boardFlipped = false
    @State private var fullScreenBoard = false
    @State private var pageIndex = 0

    @State private var moveFrom = "e2"
    @State private var moveTo = "e4"
    @State private var promotionChoice = "—"
    @State private var moveStatus: String?

    @State private var hintText: String?
    @State private var showingHint = false
    @State private var confirmNewGame = false

    var body: some View {
        TabView(selection: $pageIndex) {
            playPage
                .tag(0)
            boardPage
                .tag(1)
            movePage
                .tag(2)
            lampPage
                .tag(3)
            morePage
                .tag(4)
        }
        .tabViewStyle(.page(indexDisplayMode: .automatic))
        .onAppear {
            // `await Task.yield()` v Task { @MainActor } se na watchOS při .inactive často už nikdy nespustí → žádné BLE/WC.
            WatchAppLog.staging("ContentView onAppear — activate() synchronně")
            model.activate()
            WatchAppLog.staging("ContentView activate() hotovo")
        }
        // Shodit BLE jen když zmizí celé ContentView (např. reset úvodu), ne při .inactive / swipu TabView.
        .onDisappear {
            WatchAppLog.staging("ContentView onDisappear — deactivate()")
            model.deactivate()
        }
        .onChange(of: scenePhase) { _, phase in
            WatchAppLog.staging("scenePhase → \(String(describing: phase))")
            switch phase {
            case .background:
                WatchAppLog.staging("scenePhase.background — deactivate() (šetření BT, konzistence)")
                model.deactivate()
            case .active:
                WatchAppLog.staging("scenePhase.active — activate() (obnova po pozadí, idempotentní)")
                model.activate()
            default:
                break
            }
        }
        .fullScreenCover(isPresented: $fullScreenBoard) {
            fullScreenBoardView
                .watchWristAmbientDimming()
        }
        .confirmationDialog(
            "Nová hra?",
            isPresented: $confirmNewGame,
            titleVisibility: .visible
        ) {
            Button("Zahájit novou hru", role: .destructive) {
                runBoardAction { model.postNewGame(completion: $0) }
            }
            Button("Zrušit", role: .cancel) {}
        } message: {
            Text("Aktuální partie na desce se resetuje.")
        }
    }

    // MARK: - Partie (domů)

    private var playPage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text("Partie")
                    .font(.headline)
                    .frame(maxWidth: .infinity, alignment: .leading)

                if !canControlBoard {
                    connectionHintBanner
                }

                if !model.advantageSummary.isEmpty {
                    Text(model.advantageSummary)
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                }

                HStack(alignment: .firstTextBaseline) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Na tahu")
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                        Text(displayPlayerName(model.currentPlayer))
                            .font(.title3.weight(.semibold))
                            .minimumScaleFactor(0.75)
                            .lineLimit(1)
                    }
                    Spacer(minLength: 4)
                    if model.inCheck {
                        Image(systemName: "exclamationmark.shield.fill")
                            .foregroundStyle(.red)
                            .font(.title3)
                            .accessibilityLabel("Šach")
                    }
                }

                watchTimerRow

                Text("Tah \(model.moveCount)")
                    .font(.caption2)
                    .foregroundStyle(.secondary)

                if model.gameEnded {
                    watchEndgameSummaryCard
                }

                gameActionButtons

                boardPreviewTap
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
        }
    }

    @ViewBuilder
    private var watchEndgameSummaryCard: some View {
        if let headline = model.watchEndgameHeadline, !headline.isEmpty {
            VStack(alignment: .leading, spacing: 6) {
                Text(headline)
                    .font(.headline.weight(.semibold))
                if let score = model.watchEndgameScoreLine, !score.isEmpty {
                    Text(score)
                        .font(.title3.monospacedDigit())
                        .fontWeight(.semibold)
                }
                if let reason = model.watchEndgameReason, !reason.isEmpty {
                    Text(reason)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
                if let stats = model.watchEndgameStats, !stats.isEmpty {
                    Text(stats)
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(Color.orange.opacity(0.18))
            )
            .accessibilityElement(children: .combine)
        } else {
            Text("Konec hry")
                .font(.caption.weight(.medium))
                .foregroundStyle(.orange)
        }
    }

    private var connectionHintBanner: some View {
        HStack(spacing: 8) {
            Image(systemName: connectionIcon)
                .font(.title3)
            Text(connectionBannerText)
                .font(.caption2)
                .multilineTextAlignment(.leading)
        }
        .foregroundStyle(connectionColor)
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 12).fill(connectionColor.opacity(0.18)))
        .accessibilityElement(children: .combine)
    }

    private var connectionBannerText: String {
        switch model.connectionMode {
        case .bleDirect:
            return model.bleConnectionState.description
        case .watchConnectivity:
            if model.connectionLost {
                return "Nespárované nebo WatchConnectivity neaktivní."
            }
            if !model.isPhoneBridgeReachable {
                return "Otevři CZECHMATE na iPhonu, aby šly posílat příkazy přes telefon."
            }
            return "Připojeno přes iPhone."
        case .determining:
            return "Zjišťuji připojení…"
        }
    }

    private var gameActionButtons: some View {
        VStack(spacing: 10) {
            Button {
                confirmNewGame = true
            } label: {
                Label("Nová hra", systemImage: "plus.circle.fill")
                    .frame(maxWidth: .infinity, minHeight: 44)
            }
            .buttonStyle(.borderedProminent)
            .tint(.green)
            .disabled(!canControlBoard)

            Button {
                runBoardAction { model.postUndo(completion: $0) }
            } label: {
                Label("Zpět tah", systemImage: "arrow.uturn.backward.circle.fill")
                    .frame(maxWidth: .infinity, minHeight: 44)
            }
            .buttonStyle(.bordered)
            .tint(.orange)
            .disabled(!canControlBoard || model.moveCount <= 0)

            Button {
                runBoardAction { model.postHintClear(completion: $0) }
            } label: {
                Label("Zhasnout nápovědu LED", systemImage: "lightbulb.slash.fill")
                    .frame(maxWidth: .infinity, minHeight: 40)
            }
            .buttonStyle(.bordered)
            .disabled(!canControlBoard)
        }
    }

    // MARK: - Deska

    private var boardPage: some View {
        ScrollView {
            VStack(spacing: 12) {
                Text("Deska")
                    .font(.headline)
                    .frame(maxWidth: .infinity, alignment: .leading)

                Toggle(isOn: $boardFlipped) {
                    Text("Pohled černého")
                        .font(.body)
                }
                .padding(.vertical, 2)

                if let b = boardFromFen(model.fen) {
                    SharedChessBoardView(
                        board: b,
                        flipped: boardFlipped,
                        highlightFrom: model.lastFrom,
                        highlightTo: model.lastTo,
                        rankLabelWidth: 14,
                        cellSize: nil
                    )
                    .frame(maxWidth: .infinity)
                    .frame(minHeight: 120)
                } else {
                    Text("Zatím není FEN ze desky — připoj se přes BLE k šachovnici, nebo přes iPhone.")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                        .frame(maxWidth: .infinity, minHeight: 100)
                }

                Button {
                    fullScreenBoard = true
                } label: {
                    Text("Celý displej")
                        .frame(maxWidth: .infinity, minHeight: 44)
                }
                .buttonStyle(.borderedProminent)
            }
            .padding(.horizontal, 8)
        }
    }

    // MARK: - Tah

    private var movePage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text("Tah")
                    .font(.headline)

                if let moveStatus {
                    Text(moveStatus)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text("Odkud")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Picker("Odkud", selection: $moveFrom) {
                        ForEach(WatchChessSquares.all, id: \.self) { s in
                            Text(s).tag(s)
                        }
                    }
                    .labelsHidden()
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text("Kam")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Picker("Kam", selection: $moveTo) {
                        ForEach(WatchChessSquares.all, id: \.self) { s in
                            Text(s).tag(s)
                        }
                    }
                    .labelsHidden()
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text("Promoce (volitelné)")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Picker("Promoce", selection: $promotionChoice) {
                        Text("—").tag("—")
                        Text("Dáma (q)").tag("q")
                        Text("Jezdec (n)").tag("n")
                        Text("Věž (r)").tag("r")
                        Text("Střelec (b)").tag("b")
                    }
                    .labelsHidden()
                }

                Button {
                    submitMove()
                } label: {
                    Label("Odeslat tah", systemImage: "paperplane.fill")
                        .frame(maxWidth: .infinity, minHeight: 48)
                }
                .buttonStyle(.borderedProminent)
                .disabled(!canControlBoard)

                Divider().padding(.vertical, 4)

                VStack(alignment: .leading, spacing: 6) {
                    Text("Poslední tah")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    if let f = model.lastFrom, let t = model.lastTo {
                        Text("\(f) → \(t)")
                            .font(.title3.monospaced())
                    } else {
                        Text("—")
                            .foregroundStyle(.secondary)
                    }
                }

                if case .watchConnectivity = model.connectionMode, model.isPhoneBridgeReachable {
                    Button {
                        requestHint()
                    } label: {
                        Label("Nápověda (iPhone)", systemImage: "lightbulb.fill")
                            .frame(maxWidth: .infinity, minHeight: 40)
                    }
                    .buttonStyle(.bordered)
                    .tint(.yellow)
                }
            }
            .padding(.horizontal, 8)
        }
        .sheet(isPresented: $showingHint) {
            hintSheet
                .watchWristAmbientDimming()
        }
    }

    // MARK: - Lampa (stav + jas + presety jako na iPhonu)

    private var lampPage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                Text("Lampa")
                    .font(.headline)

                if !canControlBoard {
                    Text("Pro lampu přímo z hodinek stačí přímé BLE k desce. Bez BLE můžeš použít iPhone s otevřenou CZECHMATE.")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }

                lampStatusCard

                Text("Jas podsvětlení")
                    .font(.subheadline.weight(.semibold))
                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())], spacing: 8) {
                    ForEach([0, 25, 50, 75, 100], id: \.self) { p in
                        Button("\(p)%") {
                            runBoardAction { model.postBrightness(percent: p, completion: $0) }
                        }
                        .buttonStyle(.bordered)
                        .disabled(!canControlBoard)
                    }
                }

                Text("Barvy (režim lampa)")
                    .font(.subheadline.weight(.semibold))
                    .padding(.top, 4)

                Button {
                    runBoardAction { model.postLightGameMode(completion: $0) }
                } label: {
                    Label("Zpět do hry (LED šachy)", systemImage: "checkerboard.rectangle")
                        .frame(maxWidth: .infinity, minHeight: 44)
                }
                .buttonStyle(.borderedProminent)
                .tint(.blue)
                .disabled(!canControlBoard)

                lampPresetButton(title: "Teplá", subtitle: "255·200·120", r: 255, g: 200, b: 120)
                lampPresetButton(title: "Studená", subtitle: "200·220·255", r: 200, g: 220, b: 255)
                lampPresetButton(title: "Bílá", subtitle: "255·255·255", r: 255, g: 255, b: 255)
                lampPresetButton(title: "Jemná červená", subtitle: "180·40·40", r: 180, g: 40, b: 40)

                Button {
                    runBoardAction { model.postLightCommand(state: false, r: 0, g: 0, b: 0, completion: $0) }
                } label: {
                    Label("Zhasnout lampu", systemImage: "lightbulb.slash.fill")
                        .frame(maxWidth: .infinity, minHeight: 44)
                }
                .buttonStyle(.bordered)
                .tint(.red)
                .disabled(!canControlBoard)

                Text("Stav přichází ze snapshotu desky (BLE notifikace). Přes iPhone jen v režimu „Přes iPhone“. Po změně může trvat okamžik.")
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
                    .padding(.top, 6)
            }
            .padding(.horizontal, 8)
        }
    }

    private var lampStatusCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Stav desky")
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)

            HStack {
                Text("Režim")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                Spacer()
                Text(lampModeLabel(model.gameState.lightMode))
                    .font(.caption.weight(.medium))
            }

            HStack {
                Text("Jas")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                Spacer()
                Text(model.gameState.brightness.map { "\($0) %" } ?? "—")
                    .font(.caption.monospacedDigit().weight(.medium))
            }

            HStack {
                Text("Lampa")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                Spacer()
                if let on = model.gameState.lightState {
                    Text(on ? "Zapnuto" : "Vypnuto")
                        .font(.caption.weight(.medium))
                        .foregroundStyle(on ? .green : .secondary)
                } else {
                    Text("—")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            if let r = model.gameState.lightR, let g = model.gameState.lightG, let b = model.gameState.lightB {
                HStack(spacing: 10) {
                    Text("RGB")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color(red: Double(r) / 255, green: Double(g) / 255, blue: Double(b) / 255))
                        .frame(width: 28, height: 28)
                        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(.secondary.opacity(0.4)))
                    Text("\(r)·\(g)·\(b)")
                        .font(.caption2.monospaced())
                }
            }

            if let t = model.gameState.autoLampTimeoutSec {
                HStack {
                    Text("Auto zhasnutí")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Text("\(t) s")
                        .font(.caption2.monospaced())
                }
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 14).fill(Color.secondary.opacity(0.12)))
    }

    private func lampModeLabel(_ raw: String?) -> String {
        guard let m = raw?.lowercased() else { return "—" }
        switch m {
        case "lamp": return "Lampa"
        case "game": return "Hra"
        default: return raw ?? "—"
        }
    }

    private func lampPresetButton(title: String, subtitle: String, r: Int, g: Int, b: Int) -> some View {
        Button {
            runBoardAction { model.postLightCommand(state: true, r: r, g: g, b: b, completion: $0) }
        } label: {
            HStack {
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color(red: Double(r) / 255, green: Double(g) / 255, blue: Double(b) / 255))
                    .frame(width: 24, height: 24)
                VStack(alignment: .leading, spacing: 2) {
                    Text(title)
                        .font(.body.weight(.medium))
                    Text(subtitle)
                        .font(.caption2.monospaced())
                        .foregroundStyle(.secondary)
                }
                Spacer()
            }
            .padding(.vertical, 6)
            .contentShape(Rectangle())
        }
        .buttonStyle(.bordered)
        .disabled(!canControlBoard)
    }

    // MARK: - Více (síť + nastavení)

    private var morePage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                Text("Více")
                    .font(.headline)

                Group {
                    Text("Připojení")
                        .font(.subheadline.weight(.semibold))
                    HStack {
                        Image(systemName: modeIcon)
                        Text(model.connectionMode.description)
                            .font(.caption)
                    }
                    .foregroundStyle(.secondary)

                    if case .bleDirect = model.connectionMode {
                        Text(model.bleConnectionState.description)
                            .font(.caption)
                            .foregroundStyle(connectionColor)
                    }

                    if canStartScanning {
                        Button {
                            model.startScanning()
                        } label: {
                            Label("Hledat šachovnici", systemImage: "dot.radiowaves.left.and.right")
                                .frame(maxWidth: .infinity, minHeight: 40)
                        }
                        .buttonStyle(.borderedProminent)
                    } else if isConnected, case .bleDirect = model.connectionMode {
                        Button {
                            model.disconnect()
                        } label: {
                            Label("Odpojit BLE", systemImage: "xmark.circle")
                                .frame(maxWidth: .infinity, minHeight: 40)
                        }
                        .buttonStyle(.bordered)
                        .tint(.red)
                    } else if isError {
                        Button {
                            model.disconnect()
                            model.startScanning()
                        } label: {
                            Label("Zkusit znovu", systemImage: "arrow.clockwise")
                                .frame(maxWidth: .infinity, minHeight: 40)
                        }
                        .buttonStyle(.borderedProminent)
                    }

                    Button(modeSwitchText) {
                        if case .bleDirect = model.connectionMode {
                            model.startScanning()
                        } else {
                            model.switchToBLE()
                        }
                    }
                    .buttonStyle(.bordered)
                    .disabled(model.connectionMode == .determining)
                }

                Text("Jas a lampa jsou na stránce „Lampa“.")
                    .font(.caption2)
                    .foregroundStyle(.secondary)

                Divider()

                Toggle("Vibrace při změně tahu", isOn: $model.hapticEnabled)
                    .font(.body)

                Button("Znovu úvodní průvodce") {
                    watchIntroReset()
                }
                .buttonStyle(.bordered)
                .font(.caption)

                Text("CZECHMATE Watch")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.top, 4)
            }
            .padding(.horizontal, 8)
        }
    }

    // MARK: - Celá deska

    private var fullScreenBoardView: some View {
        NavigationStack {
            Group {
                if let b = boardFromFen(model.fen) {
                    SharedChessBoardView(
                        board: b,
                        flipped: boardFlipped,
                        highlightFrom: model.lastFrom,
                        highlightTo: model.lastTo,
                        rankLabelWidth: 18,
                        cellSize: nil
                    )
                } else {
                    ContentUnavailableView("Žádná deska", systemImage: "checkerboard.rectangle")
                }
            }
            .navigationTitle("Deska")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") {
                        fullScreenBoard = false
                    }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        boardFlipped.toggle()
                    } label: {
                        Image(systemName: "arrow.up.arrow.down.circle")
                    }
                    .accessibilityLabel("Otočit desku")
                }
            }
        }
    }

    private var boardPreviewTap: some View {
        Button {
            fullScreenBoard = true
        } label: {
            Group {
                if let b = boardFromFen(model.fen) {
                    SharedChessBoardView(
                        board: b,
                        flipped: boardFlipped,
                        highlightFrom: model.lastFrom,
                        highlightTo: model.lastTo,
                        rankLabelWidth: 12,
                        cellSize: nil
                    )
                    .frame(height: 108)
                } else {
                    Text("Náhled desky — čekám na stav z BLE nebo iPhonu")
                        .font(.caption2)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.secondary)
                        .frame(height: 88)
                }
            }
            .frame(maxWidth: .infinity)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Otevřít šachovnici na celý displej")
    }

    private var hintSheet: some View {
        NavigationStack {
            ScrollView {
                Text(hintText ?? "Žádná nápověda")
                    .font(.body)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
            }
            .navigationTitle("Nápověda")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") {
                        showingHint = false
                    }
                }
            }
        }
    }

    // MARK: - Connection helpers (stejná logika jako dřív)

    private var canControlBoard: Bool {
        switch model.connectionMode {
        case .bleDirect:
            return model.bleConnectionState.isConnected
        case .watchConnectivity:
            return model.isPhoneBridgeReachable
        case .determining:
            return false
        }
    }

    private var isConnected: Bool {
        switch model.connectionMode {
        case .bleDirect:
            return model.bleConnectionState.isConnected
        case .watchConnectivity:
            return !model.connectionLost
        case .determining:
            return false
        }
    }

    private var canStartScanning: Bool {
        if case .bleDirect = model.connectionMode {
            return !model.bleConnectionState.isConnected && !model.bleConnectionState.isConnecting
        }
        return false
    }

    private var isError: Bool {
        if case .error = model.bleConnectionState { return true }
        return false
    }

    private var connectionIcon: String {
        switch model.bleConnectionState {
        case .idle: return "magnifyingglass.circle"
        case .bluetoothOff: return "bolt.slash.circle"
        case .unauthorized: return "lock.circle"
        case .scanning: return "dot.radiowaves.left.and.right"
        case .discovered: return "wifi.circle"
        case .connecting, .discoveringServices: return "arrow.clockwise.circle"
        case .connected: return "checkmark.circle.fill"
        case .reconnecting: return "arrow.clockwise.circle.fill"
        case .error: return "exclamationmark.triangle"
        }
    }

    private var connectionColor: Color {
        switch model.bleConnectionState {
        case .idle, .scanning: return .secondary
        case .bluetoothOff, .unauthorized: return .red
        case .discovered: return .yellow
        case .connecting, .discoveringServices: return .orange
        case .connected: return .green
        case .reconnecting: return .orange
        case .error: return .red
        }
    }

    private var modeIcon: String {
        switch model.connectionMode {
        case .determining: return "questionmark.circle"
        case .bleDirect: return "antenna.radiowaves.left.and.right"
        case .watchConnectivity: return "iphone"
        }
    }

    private var modeSwitchText: String {
        switch model.connectionMode {
        case .determining: return "Čekám…"
        case .bleDirect: return "Znovu hledat (BLE)"
        case .watchConnectivity: return "Přejít na BLE"
        }
    }

    private var watchTimerRow: some View {
        TimelineView(.periodic(from: .now, by: 0.5)) { context in
            let (w, b) = clocks(at: context.date)
            HStack(spacing: 6) {
                timeChip(label: "Bílý", sec: w, highlight: isWhiteTurn(model.currentPlayer))
                timeChip(label: "Černý", sec: b, highlight: !isWhiteTurn(model.currentPlayer))
            }
        }
    }

    private func isWhiteTurn(_ raw: String) -> Bool {
        let l = raw.lowercased()
        return l == "white" || l == "bílý"
    }

    private func displayPlayerName(_ raw: String) -> String {
        let l = raw.lowercased()
        if l == "white" || l == "bílý" { return "Bílý" }
        if l == "black" || l == "černý" { return "Černý" }
        return raw
    }

    private func clocks(at now: Date) -> (UInt32?, UInt32?) {
        guard model.isTimerRunning,
              let sync = model.clockSyncAt ?? model.lastUpdate
        else {
            return (model.whiteTime, model.blackTime)
        }
        let elapsed = UInt32(min(UInt64(Int.max), UInt64(max(0, now.timeIntervalSince(sync)))))
        let wTurn = isWhiteTurn(model.currentPlayer)
        if wTurn {
            let w = model.whiteTime.map { $0 > elapsed ? $0 - elapsed : 0 }
            return (w, model.blackTime)
        } else {
            let b = model.blackTime.map { $0 > elapsed ? $0 - elapsed : 0 }
            return (model.whiteTime, b)
        }
    }

    private func timeChip(label: String, sec: UInt32?, highlight: Bool) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
            Text(formatClock(sec))
                .font(.caption.monospacedDigit())
                .fontWeight(highlight ? .semibold : .regular)
                .foregroundStyle(highlight ? Color.primary : Color.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(8)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(highlight ? Color.green.opacity(0.22) : Color.secondary.opacity(0.12))
        )
    }

    private func formatClock(_ sec: UInt32?) -> String {
        guard let s = sec else { return "—" }
        let m = Int(s) / 60
        let r = Int(s) % 60
        return String(format: "%d:%02d", m, r)
    }

    private func boardFromFen(_ fen: String) -> [[String]]? {
        let t = fen.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return nil }
        return FENPlacementParser.boardFromFullFEN(t)
    }

    private func runBoardAction(_ work: (@escaping (Bool) -> Void) -> Void) {
        guard canControlBoard else {
            model.triggerHaptic(.failure)
            return
        }
        work { ok in
            if ok {
                model.triggerHaptic(.notification)
            } else {
                model.triggerHaptic(.failure)
            }
        }
    }

    private func submitMove() {
        guard canControlBoard else {
            moveStatus = "Není spojení na desku."
            model.triggerHaptic(.failure)
            return
        }
        let promo: String? = promotionChoice == "—" ? nil : promotionChoice
        let move = ChessMove(from: moveFrom, to: moveTo, promotion: promo)
        moveStatus = "Odesílám…"
        model.executeMove(move) { ok in
            moveStatus = ok ? "Tah odeslán." : "Tah se nepovedl (pravidla / spojení)."
            if ok {
                model.triggerHaptic(.click)
            } else {
                model.triggerHaptic(.failure)
            }
        }
    }

    private func requestHint() {
        model.requestHint { hint in
            if let hint {
                hintText = hint
                showingHint = true
            }
        }
    }
}

#Preview {
    ContentView()
}
