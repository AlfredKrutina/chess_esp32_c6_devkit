//
//  CoachSheets.swift
//  CZECHMATE — plán pozice (Gemma + Stockfish) a chat s trenérem.
//

import CZECHMATEShared
import SwiftUI

// MARK: - Akce (sdílená logika s legacy GameView)

enum GameCoachActions {
    @MainActor
    static func requestPositionCoach(
        store: BoardConnectionStore,
        network: NetworkStatusMonitor,
        chessCoachManager: ChessCoachManager,
        modelDownload: ModelDownloadManager,
        userLevel: Int
    ) async {
        chessCoachManager.advice = ""
        guard store.snapshot != nil else {
            AppDebugLog.coachTrace("GameCoachActions requestPositionCoach blocked — no snapshot")
            chessCoachManager.advice = "Nepodařilo se načíst pozici z desky."
            return
        }
        guard network.isInternetLikelyForStockfish else {
            AppDebugLog.coachTrace("GameCoachActions requestPositionCoach blocked — no internet for Stockfish")
            chessCoachManager.advice =
                "Pro analýzu Stockfish je potřeba internet (Wi‑Fi nebo mobilní data). Připojení k desce může zůstat jen přes Bluetooth."
            return
        }
        AppDebugLog.coachTrace("GameCoachActions requestPositionCoach → streamPositionPlanSummary userLevel=\(userLevel)")
        await chessCoachManager.streamPositionPlanSummary(
            store: store,
            modelDownload: modelDownload,
            userLevel: userLevel
        )
    }
}

// MARK: - Sheet: plán pozice

struct PositionCoachSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network
    @EnvironmentObject private var chessCoachManager: ChessCoachManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager
    let coachUserLevel: Int

    var body: some View {
        NavigationStack {
            ScrollView {
                CoachBubbleView(
                    isThinking: chessCoachManager.isThinking && chessCoachManager.advice.isEmpty,
                    fullText: chessCoachManager.advice.isEmpty ? " " : chessCoachManager.advice,
                    typewriterEnabled: !chessCoachManager.isStreamingLLM
                )
                .padding(Theme.Spacing.l)
            }
            .navigationTitle("Plán pozice")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") { dismiss() }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        Task {
                            await GameCoachActions.requestPositionCoach(
                                store: store,
                                network: network,
                                chessCoachManager: chessCoachManager,
                                modelDownload: modelDownloadManager,
                                userLevel: coachUserLevel
                            )
                        }
                    } label: {
                        Label("Obnovit", systemImage: "arrow.clockwise")
                    }
                    .disabled(store.snapshot == nil || chessCoachManager.isThinking || chessCoachManager.isStreamingLLM)
                }
            }
        }
        .tint(Theme.accent)
        .task {
            await GameCoachActions.requestPositionCoach(
                store: store,
                network: network,
                chessCoachManager: chessCoachManager,
                modelDownload: modelDownloadManager,
                userLevel: coachUserLevel
            )
        }
    }
}

// MARK: - Sheet: chat

struct CoachChatSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(\.colorScheme) private var colorScheme
    @Environment(BoardConnectionStore.self) private var store
    @EnvironmentObject private var chessCoachManager: ChessCoachManager
    @EnvironmentObject private var modelDownloadManager: ModelDownloadManager
    let coachUserLevel: Int

    @State private var inputText = ""
    @FocusState private var inputFocused: Bool

    private var inputBlocked: Bool {
        chessCoachManager.coachChatIsSending || chessCoachManager.isStreamingLLM
    }

    var body: some View {
        NavigationStack {
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: Theme.Spacing.l) {
                        if chessCoachManager.coachChatLines.isEmpty {
                            chatIntro
                        }
                        ForEach(chessCoachManager.coachChatLines) { line in
                            chatRow(for: line)
                                .id(line.id)
                        }
                        Color.clear.frame(height: 1).id("chatBottom")
                    }
                    .padding(.horizontal, Theme.Spacing.m)
                    .padding(.vertical, Theme.Spacing.l)
                }
                .scrollDismissesKeyboard(.interactively)
                .background(chatThreadBackground)
                .onChange(of: chessCoachManager.coachChatLines.count) { _, _ in
                    scrollChatToBottom(proxy: proxy)
                }
                .onChange(of: chessCoachManager.coachChatLines.last?.text) { _, _ in
                    scrollChatToBottom(proxy: proxy, duration: 0.12)
                }
                .onChange(of: chessCoachManager.coachChatLines.last?.streaming) { _, _ in
                    scrollChatToBottom(proxy: proxy, duration: 0.12)
                }
            }
            .safeAreaInset(edge: .bottom, spacing: 0) {
                composerInset
            }
            .navigationTitle("Chat s trenérem")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") { dismiss() }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        chessCoachManager.clearCoachChat()
                    } label: {
                        Label("Vyčistit", systemImage: "trash")
                    }
                    .disabled(chessCoachManager.coachChatLines.isEmpty)
                }
            }
        }
        .tint(Theme.accent)
        .onAppear { inputFocused = true }
    }

    private var chatThreadBackground: some View {
        Color(uiColor: .systemGroupedBackground)
    }

    private func scrollChatToBottom(proxy: ScrollViewProxy, duration: Double = 0.2) {
        withAnimation(.easeOut(duration: duration)) {
            proxy.scrollTo("chatBottom", anchor: .bottom)
        }
    }

    private var trimmedInput: String {
        inputText.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private var chatIntro: some View {
        HStack(alignment: .top, spacing: Theme.Spacing.m) {
            CoachTrainerAvatarView(size: 40)
            VStack(alignment: .leading, spacing: Theme.Spacing.s) {
                Text("Lokální Gemma")
                    .font(Theme.Typography.cardTitle())
                Text(
                    "Zeptej se na pravidla, zahájení, taktiku nebo na pozici z připojené desky. "
                        + "Vše běží v telefonu — žádný cloudový chat."
                )
                .font(Theme.Typography.caption())
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
        }
        .padding(Theme.Spacing.l)
        .background {
            RoundedRectangle(cornerRadius: 20, style: .continuous)
                .fill(.ultraThinMaterial)
                .shadow(
                    color: .black.opacity(Theme.cardShadowOpacity(for: colorScheme)),
                    radius: colorScheme == .dark ? 12 : 8,
                    y: 3
                )
                .overlay {
                    RoundedRectangle(cornerRadius: 20, style: .continuous)
                        .stroke(
                            LinearGradient(
                                colors: [
                                    Theme.accent.opacity(0.35),
                                    Color.white.opacity(colorScheme == .dark ? 0.12 : 0.2),
                                ],
                                startPoint: .topLeading,
                                endPoint: .bottomTrailing
                            ),
                            lineWidth: 1
                        )
                }
        }
        .padding(.bottom, Theme.Spacing.s)
    }

    @ViewBuilder
    private func chatRow(for line: CoachChatLine) -> some View {
        if line.isUser {
            userBubble(line)
        } else {
            coachBubble(line)
        }
    }

    private func userBubble(_ line: CoachChatLine) -> some View {
        HStack(alignment: .top, spacing: 0) {
            Spacer(minLength: 56)
            VStack(alignment: .trailing, spacing: 6) {
                Text("Ty")
                    .font(.system(.caption2, design: .rounded).weight(.semibold))
                    .foregroundStyle(.secondary)
                Text(line.text)
                    .font(Theme.Typography.body())
                    .multilineTextAlignment(.trailing)
                    .foregroundStyle(.white)
                    .padding(.horizontal, Theme.Spacing.m)
                    .padding(.vertical, Theme.Spacing.m)
                    .background {
                        RoundedRectangle(cornerRadius: 20, style: .continuous)
                            .fill(
                                LinearGradient(
                                    colors: [
                                        Theme.accent,
                                        Theme.accent.opacity(0.82),
                                    ],
                                    startPoint: .topLeading,
                                    endPoint: .bottomTrailing
                                )
                            )
                            .shadow(color: Theme.accent.opacity(0.35), radius: 8, y: 3)
                    }
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Ty, zpráva")
        .accessibilityValue(line.text)
    }

    private func coachBubble(_ line: CoachChatLine) -> some View {
        HStack(alignment: .top, spacing: 10) {
            CoachTrainerAvatarView(size: 36)
            VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 6) {
                    Text("AI Trenér")
                        .font(.system(.caption, design: .rounded).weight(.semibold))
                        .foregroundStyle(.secondary)
                    if line.streaming && line.text.isEmpty {
                        CoachThinkingDotsView()
                    }
                }
                if !line.text.isEmpty || !line.streaming {
                    Text(line.text.isEmpty && line.streaming ? "…" : line.text)
                        .font(Theme.Typography.body())
                        .multilineTextAlignment(.leading)
                        .foregroundStyle(.primary)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(.horizontal, Theme.Spacing.m)
            .padding(.vertical, Theme.Spacing.m)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background {
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(coachBubbleSurface)
                    .shadow(color: .black.opacity(colorScheme == .dark ? 0.28 : 0.07), radius: 12, y: 4)
            }
            .overlay {
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
            }
            .overlay(alignment: .trailing) {
                if line.streaming && !line.text.isEmpty {
                    ProgressView()
                        .scaleEffect(0.75)
                        .padding(.trailing, 10)
                }
            }
            Spacer(minLength: 20)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("AI Trenér")
        .accessibilityValue(line.text.isEmpty && line.streaming ? "Přemýšlí" : line.text)
    }

    private var coachBubbleSurface: Color {
        #if os(iOS)
        Color(uiColor: .secondarySystemGroupedBackground)
        #else
        Color.gray.opacity(0.14)
        #endif
    }

    private var composerInset: some View {
        VStack(spacing: 0) {
            Divider().opacity(0.65)
            if showQuickPrompts {
                quickPromptsRow
                    .padding(.horizontal, Theme.Spacing.m)
                    .padding(.top, Theme.Spacing.s)
            }
            HStack(alignment: .bottom, spacing: Theme.Spacing.s) {
                HStack(alignment: .bottom, spacing: Theme.Spacing.s) {
                    TextField("Napiš otázku o šachu…", text: $inputText, axis: .vertical)
                        .font(Theme.Typography.body())
                        .lineLimit(1 ... 6)
                        .focused($inputFocused)
                        .submitLabel(.send)
                        .onSubmit { sendMessage() }
                        .padding(.leading, Theme.Spacing.m)
                        .padding(.vertical, 12)
                    Button {
                        sendMessage()
                    } label: {
                        Image(systemName: "arrow.up.circle.fill")
                            .font(.system(size: 34))
                            .symbolRenderingMode(.hierarchical)
                            .foregroundStyle(canSend ? Theme.accent : Color.secondary.opacity(0.45))
                    }
                    .disabled(!canSend)
                    .padding(.trailing, 8)
                    .padding(.bottom, 6)
                    .accessibilityLabel("Odeslat")
                }
                .background {
                    RoundedRectangle(cornerRadius: 24, style: .continuous)
                        .fill(.ultraThinMaterial)
                        .overlay {
                            RoundedRectangle(cornerRadius: 24, style: .continuous)
                                .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
                        }
                }
            }
            .padding(.horizontal, Theme.Spacing.m)
            .padding(.vertical, Theme.Spacing.m)
            .background(.bar)
        }
    }

    private var showQuickPrompts: Bool {
        chessCoachManager.coachChatLines.count <= 1 && !inputBlocked
    }

    private var quickPromptsRow: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: Theme.Spacing.s) {
                ForEach(Self.quickPrompts, id: \.self) { prompt in
                    Button {
                        sendMessage(with: prompt)
                    } label: {
                        Text(prompt)
                            .font(Theme.Typography.caption())
                            .fontWeight(.medium)
                            .foregroundStyle(Theme.accent)
                            .padding(.horizontal, 14)
                            .padding(.vertical, 10)
                            .background {
                                Capsule(style: .continuous)
                                    .fill(Theme.accentMuted)
                                    .overlay {
                                        Capsule(style: .continuous)
                                            .stroke(Theme.accent.opacity(0.22), lineWidth: 1)
                                    }
                            }
                    }
                    .buttonStyle(.plain)
                    .disabled(inputBlocked)
                }
            }
            .padding(.bottom, Theme.Spacing.s)
        }
    }

    private var canSend: Bool {
        !trimmedInput.isEmpty && !inputBlocked
    }

    private static let quickPrompts: [String] = [
        "Vysvětli mi tuto pozici",
        "Jaké jsou hlavní plány?",
        "Co je fork?",
        "Tip na zahájení pro začátečníka",
    ]

    private func sendMessage() {
        sendMessage(with: trimmedInput)
    }

    private func sendMessage(with raw: String) {
        let t = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return }
        inputText = ""
        inputFocused = true
        Task {
            await chessCoachManager.sendChessChatMessage(
                t,
                store: store,
                modelDownload: modelDownloadManager,
                userLevel: coachUserLevel
            )
        }
    }
}
