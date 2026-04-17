//
//  SandboxGameChrome.swift
//  CZECHMATE — jednotné UI pro režim Sandbox (vstup, aktivní panel, kompaktní lišta)
//

import SwiftUI

// MARK: - Vstup (řádek jako sekundární karta)

struct SandboxEnterRow: View {
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: Theme.Spacing.m) {
                ZStack {
                    Circle()
                        .fill(Theme.Sandbox.fillStrong)
                        .frame(width: 48, height: 48)
                    Image(systemName: "rectangle.on.rectangle.dashed")
                        .font(.system(size: 21, weight: .semibold))
                        .symbolRenderingMode(.hierarchical)
                        .foregroundStyle(Theme.Sandbox.accent)
                }
                VStack(alignment: .leading, spacing: 4) {
                    Text("Sandbox")
                        .font(.system(.subheadline, design: .rounded).weight(.semibold))
                        .foregroundStyle(.primary)
                    Text("Tahy jen na obrazovce — deska zůstane beze změny")
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.leading)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 4)
                Image(systemName: "chevron.right")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(.tertiary)
            }
            .padding(.vertical, Theme.Spacing.s)
            .padding(.horizontal, Theme.Spacing.m)
            .background {
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(Theme.Sandbox.fill)
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .stroke(Theme.Sandbox.stroke, lineWidth: 1)
            }
        }
        .buttonStyle(.plain)
        .accessibilityHint("Tahy se neodešlou na šachovnici. Po skončení synchronizuj pozici z desky.")
    }
}

// MARK: - Aktivní panel (pod deskou — v herním panelu / sidebaru)

struct SandboxModePanel: View {
    @Bindable var viewState: GameViewState
    var onSyncFromBoard: () async -> Void

    var body: some View {
        VStack(spacing: Theme.Spacing.m) {
            Capsule()
                .fill(
                    LinearGradient(
                        colors: [
                            Theme.Sandbox.accent.opacity(0.55),
                            Theme.Sandbox.accent.opacity(0.35)
                        ],
                        startPoint: .leading,
                        endPoint: .trailing
                    )
                )
                .frame(width: 40, height: 4)
                .accessibilityHidden(true)
            panelInner
        }
    }

    private var panelInner: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            HStack(alignment: .center, spacing: Theme.Spacing.s) {
                Image(systemName: "rectangle.on.rectangle.dashed")
                    .font(.title2)
                    .foregroundStyle(Theme.Sandbox.accent)
                    .accessibilityHidden(true)
                VStack(alignment: .leading, spacing: 2) {
                    Text("Sandbox je zapnutý")
                        .font(Theme.Typography.subsection())
                    Text("Figurky taháš jen v aplikaci — fyzická pozice se nemění.")
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
            }

            HStack(spacing: Theme.Spacing.s) {
                SandboxToolChip(
                    systemImage: "arrow.uturn.backward",
                    title: "Zpět",
                    badge: viewState.sandboxUndoCount > 0 ? "\(viewState.sandboxUndoCount)" : nil,
                    isEnabled: viewState.sandboxUndoCount > 0,
                    action: { viewState.sandboxUndoLastMove() }
                )
                SandboxToolChip(
                    systemImage: "arrow.counterclockwise",
                    title: "Na začátek",
                    badge: nil,
                    isEnabled: viewState.sandboxUndoCount > 0,
                    action: { viewState.sandboxUndoToEntryPosition() }
                )
            }

            Button {
                Task { await onSyncFromBoard() }
            } label: {
                Label("Načíst pozici z desky", systemImage: "arrow.triangle.2.circlepath.circle.fill")
                    .font(.system(.subheadline, design: .rounded).weight(.semibold))
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 4)
            }
            .buttonStyle(.borderedProminent)
            .tint(Theme.accent)
            .controlSize(.large)
            .accessibilityHint("Zavře sandbox a znovu načte skutečnou pozici z šachovnice.")
        }
    }
}

// MARK: - Kompaktní lišta (jen deska)

struct SandboxBoardOnlyStrip: View {
    @Bindable var viewState: GameViewState
    var onSyncFromBoard: () async -> Void

    var body: some View {
        HStack(spacing: Theme.Spacing.s) {
            Image(systemName: "rectangle.on.rectangle.dashed")
                .font(.body.weight(.semibold))
                .foregroundStyle(Theme.Sandbox.accent)
                .accessibilityLabel("Sandbox")

            SandboxIconChip(
                systemImage: "arrow.uturn.backward",
                accessibilityLabel: "Zpět jeden tah v sandboxu",
                badge: viewState.sandboxUndoCount > 0 ? viewState.sandboxUndoCount : nil,
                isEnabled: viewState.sandboxUndoCount > 0,
                action: { viewState.sandboxUndoLastMove() }
            )
            SandboxIconChip(
                systemImage: "arrow.counterclockwise",
                accessibilityLabel: "Zpět na začátek sandboxu",
                badge: nil,
                isEnabled: viewState.sandboxUndoCount > 0,
                action: { viewState.sandboxUndoToEntryPosition() }
            )

            Button {
                Task { await onSyncFromBoard() }
            } label: {
                Image(systemName: "arrow.triangle.2.circlepath.circle.fill")
                    .font(.body.weight(.semibold))
                    .foregroundStyle(.white)
                    .frame(width: 40, height: 40)
                    .background(Circle().fill(Theme.accent))
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Načíst pozici z desky")
        }
        .padding(.horizontal, Theme.Spacing.m)
        .padding(.vertical, Theme.Spacing.s)
        .background {
            Capsule()
                .fill(.ultraThinMaterial)
            Capsule()
                .stroke(Theme.Sandbox.stroke, lineWidth: 1)
        }
    }
}

// MARK: - dílčí prvky

private struct SandboxToolChip: View {
    let systemImage: String
    let title: String
    var badge: String?
    var isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 6) {
                Image(systemName: systemImage)
                    .font(.title3)
                    .foregroundStyle(isEnabled ? Theme.Sandbox.accent : .secondary)
                Text(title)
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(isEnabled ? .primary : .secondary)
                if let badge {
                    Text(badge)
                        .font(.caption2.monospacedDigit().weight(.bold))
                        .foregroundStyle(Theme.Sandbox.accent.opacity(isEnabled ? 1 : 0.45))
                }
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, Theme.Spacing.m)
            .padding(.horizontal, 4)
            .background {
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(Theme.Sandbox.fill.opacity(isEnabled ? 1 : 0.45))
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .stroke(Theme.Sandbox.stroke.opacity(isEnabled ? 1 : 0.35), lineWidth: 1)
            }
        }
        .buttonStyle(.plain)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.55)
    }
}

private struct SandboxIconChip: View {
    let systemImage: String
    var accessibilityLabel: String
    var badge: Int?
    var isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            ZStack(alignment: .topTrailing) {
                Image(systemName: systemImage)
                    .font(.body.weight(.semibold))
                    .foregroundStyle(isEnabled ? Theme.Sandbox.accent : .secondary)
                    .frame(width: 40, height: 40)
                    .background {
                        Circle()
                            .fill(Theme.Sandbox.fill.opacity(isEnabled ? 1 : 0.5))
                    }
                if let badge, badge > 0 {
                    Text("\(badge)")
                        .font(.system(size: 9, weight: .bold, design: .rounded))
                        .foregroundStyle(.white)
                        .frame(minWidth: 16, minHeight: 16)
                        .background(Circle().fill(Theme.Sandbox.accent))
                        .offset(x: 6, y: -6)
                }
            }
        }
        .buttonStyle(.plain)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.5)
        .accessibilityLabel(accessibilityLabel)
    }
}
