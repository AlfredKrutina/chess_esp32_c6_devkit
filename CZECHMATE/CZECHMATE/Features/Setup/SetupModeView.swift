//
//  SetupModeView.swift
//  Překryv nad Hrou — velká figurka, pole, postup, zrušení (průvodce postavením).
//

import CZECHMATEShared
import SwiftUI

struct SetupModeView: View {
    @Bindable var manager: BoardSetupManager
    @Environment(AppTabRouter.self) private var tabRouter

    var body: some View {
        ZStack {
            Color.black.opacity(0.52)
                .ignoresSafeArea()
                .allowsHitTesting(true)

            VStack(spacing: Theme.Spacing.m) {
                switch manager.phase {
                case .validatingFinish:
                    ProgressView()
                        .scaleEffect(1.2)
                    Text("Ověřuji desku…")
                        .font(.system(.headline, design: .rounded))
                case .completed:
                    Image(systemName: "checkmark.circle.fill")
                        .font(.system(size: 52))
                        .symbolRenderingMode(.palette)
                        .foregroundStyle(.green, .primary)
                    Text("Deska je připravena!")
                        .font(.system(.title2, design: .rounded).weight(.semibold))
                        .multilineTextAlignment(.center)
                    Text("Za chvíli přejdeme do záložky Hra.")
                        .font(Theme.Typography.caption())
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                case .failed(let msg):
                    Image(systemName: "exclamationmark.triangle.fill")
                        .font(.system(size: 40))
                        .foregroundStyle(.orange)
                    Text("Nelze dokončit")
                        .font(.system(.headline, design: .rounded))
                    Text(msg)
                        .font(Theme.Typography.body())
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.secondary)
                    Button("Zrušit průvodce") {
                        Task { await manager.cancel() }
                    }
                    .buttonStyle(.themeSecondary)
                default:
                    if let step = manager.currentStep {
                        ChessPieceArtView(fenChar: step.pieceChar, size: 96, isLightSquare: true)
                            .accessibilityHidden(true)
                        Text(manager.headlineInstruction)
                            .font(.system(.title2, design: .rounded).weight(.semibold))
                            .multilineTextAlignment(.center)
                            .minimumScaleFactor(0.85)
                        Text(step.labelLine)
                            .font(.system(.subheadline, design: .rounded))
                            .foregroundStyle(.secondary)
                            .multilineTextAlignment(.center)
                        ProgressView(value: {
                            let frac: Double = min(1, max(0, manager.progressFraction))
                            return frac
                        }())
                            .tint(Theme.accent)
                        Text(manager.progressLabel)
                            .font(Theme.Typography.caption())
                            .foregroundStyle(.secondary)
                        Text("Pokud pole nefunguje nebo deska nespolupracuje, můžeš krok přeskočit — figurku si doskladni podle obrázku.")
                            .font(Theme.Typography.caption2())
                            .foregroundStyle(.tertiary)
                            .multilineTextAlignment(.center)
                            .padding(.top, 4)
                    }
                    Button {
                        Task { await manager.skipCurrentStep() }
                    } label: {
                        Label("Přeskočit tento krok", systemImage: "arrow.forward.circle")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.themeSecondary)
                    .accessibilityHint("Pokračuje bez ověření fyzického pole.")
                    Button("Zrušit průvodce") {
                        Task { await manager.cancel() }
                    }
                    .buttonStyle(.themeSecondary)
                    .padding(.top, 4)
                }
            }
            .padding(Theme.Spacing.l)
            .frame(maxWidth: 420)
            .background(
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(.ultraThinMaterial)
            )
            .padding(Theme.Spacing.m)
        }
        .onChange(of: manager.phase) { _, newPhase in
            if newPhase == .completed {
                Task {
                    try? await Task.sleep(nanoseconds: 900_000_000)
                    await MainActor.run {
                        manager.acknowledgeCompletion()
                        tabRouter.selectedTab = .game
                    }
                }
            }
        }
    }
}
