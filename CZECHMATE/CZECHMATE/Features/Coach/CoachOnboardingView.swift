//
//  CoachOnboardingView.swift
//  CZECHMATE — První spuštění / stažení AI Trenéra (URLSession → Application Support).
//

import SwiftUI

struct CoachOnboardingView: View {
    @ObservedObject var modelDownload: ModelDownloadManager
    var onFinished: () -> Void
    var onSkipForNow: (() -> Void)?

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            ZStack {
                LinearGradient(
                    colors: [
                        Color(red: 0.06, green: 0.09, blue: 0.14),
                        Color(red: 0.12, green: 0.22, blue: 0.18),
                        Theme.accent.opacity(0.35)
                    ],
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
                .ignoresSafeArea()

                ScrollView {
                    VStack(spacing: 28) {
                        hero
                        copyBlock
                        downloadCard
                        if modelDownload.state == .completed || modelDownload.isModelInstalled {
                            Button {
                                onFinished()
                                dismiss()
                            } label: {
                                Text("Pokračovat do učícího módu")
                            }
                            .buttonStyle(.themePrimary)
                        }
                    }
                    .padding(.horizontal, 22)
                    .padding(.vertical, 28)
                }
            }
            .navigationTitle("AI Trenér")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Zavřít") {
                        onSkipForNow?()
                        dismiss()
                    }
                    .foregroundStyle(.white.opacity(0.9))
                }
            }
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarBackground(Color.black.opacity(0.2), for: .navigationBar)
        }
        .preferredColorScheme(.dark)
    }

    private var hero: some View {
        VStack(spacing: 16) {
            ZStack {
                Circle()
                    .fill(.ultraThinMaterial)
                    .frame(width: 112, height: 112)
                Image(systemName: "brain.head.profile")
                    .font(.system(size: 48, weight: .medium))
                    .symbolRenderingMode(.hierarchical)
                    .foregroundStyle(.white)
            }
            Text("Nauč se přemýšlet jako velmistr")
                .font(.system(.title2, design: .rounded).weight(.bold))
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
        }
    }

    private var copyBlock: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(
                "Pro nejlepší zážitek si stáhněte AI Trenéra. Běží 100 % offline přímo ve vašem telefonu a pomůže vám pochopit každý tah."
            )
            .font(.system(.body, design: .rounded))
            .foregroundStyle(.white.opacity(0.92))
            .fixedSize(horizontal: false, vertical: true)

            Label("Soukromí: žádná data neodcházejí na server", systemImage: "lock.shield.fill")
                .font(.system(.caption, design: .rounded))
                .foregroundStyle(.white.opacity(0.75))
        }
        .padding(18)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(.white.opacity(0.08))
        )
    }

    private var downloadCard: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Text("Balíček modelu")
                    .font(.system(.subheadline, design: .rounded).weight(.semibold))
                    .foregroundStyle(.white)
                Spacer()
                Text(ModelDownloadManager.displayedFileSizeDescription)
                    .font(.system(.caption, design: .rounded).weight(.medium))
                    .foregroundStyle(.white.opacity(0.75))
            }

            ProgressView(value: modelDownload.progress, total: 1.0)
                .tint(Theme.accent)
                .scaleEffect(x: 1, y: 1.35, anchor: .center)

            HStack {
                Text(progressPercentText)
                    .font(.system(.caption, design: .rounded).monospacedDigit())
                    .foregroundStyle(.white.opacity(0.85))
                Spacer()
                statusChip
            }

            if modelDownload.state == .failed, let err = modelDownload.lastErrorDescription {
                Text(err)
                    .font(.system(.caption, design: .rounded))
                    .foregroundStyle(.orange.opacity(0.95))
                    .fixedSize(horizontal: false, vertical: true)
            }

            HStack(spacing: 12) {
                if modelDownload.state == .downloading || modelDownload.state == .paused {
                    Button {
                        if modelDownload.isPaused {
                            modelDownload.resume()
                        } else {
                            modelDownload.pause()
                        }
                    } label: {
                        Label(
                            modelDownload.isPaused ? "Pokračovat" : "Pozastavit",
                            systemImage: modelDownload.isPaused ? "play.fill" : "pause.fill"
                        )
                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                        .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(.white.opacity(0.22))
                }

                if modelDownload.state == .idle || modelDownload.state == .failed {
                    Button {
                        modelDownload.startDownload()
                    } label: {
                        Text(modelDownload.state == .failed ? "Zkusit znovu" : "Stáhnout AI Trenéra")
                            .font(.system(.subheadline, design: .rounded).weight(.semibold))
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.themePrimary)
                }
            }
        }
        .padding(20)
        .background(
            RoundedRectangle(cornerRadius: 20, style: .continuous)
                .fill(.ultraThinMaterial)
                .shadow(color: .black.opacity(0.25), radius: 20, y: 8)
        )
    }

    private var progressPercentText: String {
        let p = Int((modelDownload.progress * 100).rounded())
        return "\(p) %"
    }

    @ViewBuilder
    private var statusChip: some View {
        switch modelDownload.state {
        case .idle:
            Text("Připraveno ke stažení")
                .font(.caption2.weight(.semibold))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(Capsule().fill(.white.opacity(0.12)))
                .foregroundStyle(.white.opacity(0.9))
        case .downloading:
            Text("Stahuje se…")
                .font(.caption2.weight(.semibold))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(Capsule().fill(Theme.accent.opacity(0.35)))
                .foregroundStyle(.white)
        case .paused:
            Text("Pozastaveno")
                .font(.caption2.weight(.semibold))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(Capsule().fill(.orange.opacity(0.4)))
                .foregroundStyle(.white)
        case .completed:
            Text("Hotovo")
                .font(.caption2.weight(.semibold))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(Capsule().fill(Theme.accent.opacity(0.45)))
                .foregroundStyle(.white)
        case .failed:
            Text("Chyba")
                .font(.caption2.weight(.semibold))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(Capsule().fill(Color.red.opacity(0.4)))
                .foregroundStyle(.white)
        }
    }
}

#Preview {
    CoachOnboardingView(modelDownload: ModelDownloadManager(), onFinished: {}, onSkipForNow: {})
}
