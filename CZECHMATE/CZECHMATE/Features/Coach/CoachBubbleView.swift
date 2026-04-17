//
//  CoachBubbleView.swift
//  CZECHMATE — Bublina trenéra: avatar, „Přemýšlí…“, typewriter text.
//

import Combine
import SwiftUI

struct CoachBubbleView: View {
    var isThinking: Bool
    /// Plný text — při změně se spustí typewriter od začátku.
    var fullText: String
    var typewriterEnabled: Bool = true
    /// Znaků za sekundu (typewriter).
    var typewriterCharactersPerSecond: Double = 42

    @State private var displayedLength = 0
    @State private var typewriterTask: Task<Void, Never>?

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            CoachTrainerAvatarView()
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 6) {
                    Text("AI Trenér")
                        .font(.system(.caption, design: .rounded).weight(.semibold))
                        .foregroundStyle(.secondary)
                    if isThinking {
                        CoachThinkingDotsView()
                    }
                }
                Text(displayedSubstring)
                    .font(.system(.body, design: .rounded))
                    .foregroundStyle(.primary)
                    .multilineTextAlignment(.leading)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 0)
        }
        .padding(14)
        .background(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .fill(coachBubbleFill)
                .shadow(color: .black.opacity(0.07), radius: 12, y: 4)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .stroke(Color.primary.opacity(0.06), lineWidth: 1)
        )
        .onChange(of: fullText) { _, new in
            if typewriterEnabled {
                restartTypewriter(for: new)
            } else {
                typewriterTask?.cancel()
                displayedLength = new.count
            }
        }
        .onChange(of: isThinking) { _, thinking in
            if thinking {
                typewriterTask?.cancel()
                displayedLength = 0
            } else if typewriterEnabled {
                restartTypewriter(for: fullText)
            }
        }
        .onAppear {
            if !isThinking, typewriterEnabled {
                restartTypewriter(for: fullText)
            } else {
                displayedLength = fullText.count
            }
        }
        .onDisappear {
            typewriterTask?.cancel()
        }
    }

    private var displayedSubstring: String {
        let c = Array(fullText)
        let n = min(displayedLength, c.count)
        return String(c.prefix(n))
    }

    private func restartTypewriter(for text: String) {
        typewriterTask?.cancel()
        displayedLength = 0
        guard typewriterEnabled, !text.isEmpty, !isThinking else {
            displayedLength = text.count
            return
        }
        let interval = 1.0 / max(8, typewriterCharactersPerSecond)
        typewriterTask = Task { @MainActor in
            for i in 0 ... text.count {
                if Task.isCancelled { return }
                displayedLength = i
                try? await Task.sleep(nanoseconds: UInt64(interval * 1_000_000_000))
            }
        }
    }

    private var coachBubbleFill: Color {
        #if os(iOS)
        Color(uiColor: .secondarySystemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .controlBackgroundColor)
        #else
        Color.gray.opacity(0.14)
        #endif
    }

}

// MARK: - Avatar (sdílený chat + plán pozice)

struct CoachTrainerAvatarView: View {
    var size: CGFloat = 44

    private var iconSize: CGFloat { max(14, size * 0.45) }

    var body: some View {
        ZStack {
            Circle()
                .fill(
                    LinearGradient(
                        colors: [
                            Theme.accent.opacity(0.95),
                            Color(red: 0.12, green: 0.45, blue: 0.55)
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
                .frame(width: size, height: size)
            Image(systemName: "crown.fill")
                .font(.system(size: iconSize, weight: .semibold))
                .foregroundStyle(.white)
        }
        .accessibilityHidden(true)
    }
}

// MARK: - iMessage-style tečky

struct CoachThinkingDotsView: View {
    @State private var phase = 0
    private let timer = Timer.publish(every: 0.45, on: .main, in: .common).autoconnect()

    var body: some View {
        HStack(spacing: 3) {
            ForEach(0 ..< 3, id: \.self) { i in
                Circle()
                    .fill(Color.secondary.opacity(0.55))
                    .frame(width: 5, height: 5)
                    .offset(y: (phase % 3) == i ? -3 : 0)
                    .animation(.easeInOut(duration: 0.35), value: phase)
            }
        }
        .accessibilityLabel("Přemýšlí")
        .onReceive(timer) { _ in
            phase += 1
        }
    }
}

#Preview {
    CoachBubbleView(isThinking: false, fullText: "Krátká rada trenéra pro tuto pozici.")
        .padding()
}
