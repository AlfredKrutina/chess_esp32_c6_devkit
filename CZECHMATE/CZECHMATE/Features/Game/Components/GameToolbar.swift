//
//  GameToolbar.swift
//  CZECHMATE — Simplified game toolbar with contextual actions
//
//  Reduces cognitive load by grouping related actions
//

import CZECHMATEShared
import SwiftUI

struct GameToolbar: ToolbarContent {
    @Environment(BoardConnectionStore.self) private var store
    @EnvironmentObject private var learningModeManager: LearningModeManager
    
    @Bindable var viewState: GameViewState
    @Binding var isHintBusy: Bool
    
    var body: some ToolbarContent {
        // Leading: Connection status
        ToolbarItem(placement: .topBarLeading) {
            ConnectionStatusBadge(onTap: { viewState.showConnection = true })
        }
        
        // Principal: App branding
        ToolbarItem(placement: .principal) {
            HStack(spacing: Theme.Spacing.s) {
                Image(systemName: "checkerboard.rectangle")
                    .resizable()
                    .scaledToFit()
                    .frame(width: 26, height: 26)
                    .foregroundStyle(Theme.accent)
                    .accessibilityHidden(true)
                Text("CZECHMATE")
                    .font(.system(.headline, design: .rounded).weight(.semibold))
                    .foregroundStyle(.primary)
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("CZECHMATE")
        }
        
        // Trailing: Contextual actions
        ToolbarItemGroup(placement: .topBarTrailing) {
            if store.snapshot?.status.isGameFinished == true {
                ThemeToolbarIconChip(
                    systemName: "chart.bar.doc.horizontal",
                    accessibilityLabel: "Souhrn partie",
                    action: { viewState.showEndgameReport = true }
                )
            }
            LayoutModePicker(viewState: viewState)
            ThemeToolbarIconChip(
                systemName: "slider.horizontal.3",
                accessibilityLabel: "Ovládání hry",
                action: { viewState.showGameControls = true }
            )
        }
    }
}

// MARK: - Layout Mode Picker

struct LayoutModePicker: View {
    @Bindable var viewState: GameViewState
    
    var body: some View {
        Menu {
            ForEach(GameLayoutMode.allCases, id: \.self) { mode in
                Button {
                    withAnimation(.spring(duration: 0.3)) {
                        viewState.layoutMode = mode
                    }
                } label: {
                    Label(mode.label, systemImage: mode.icon)
                }
            }
        } label: {
            Image(systemName: viewState.layoutMode.icon)
                .font(.system(size: 18, weight: .semibold))
                .foregroundStyle(Theme.accent)
                .frame(width: 44, height: 44)
                .background {
                    Circle()
                        .fill(Theme.accentMuted)
                }
                .contentShape(Circle())
                .symbolRenderingMode(.hierarchical)
        }
        .accessibilityLabel("Režim zobrazení")
        .accessibilityHint("Otevře nabídku: jen deska, standard nebo široké rozložení.")
    }
}

// MARK: - Preview

#Preview {
    NavigationStack {
        Color.gray
            .toolbar {
                GameToolbar(
                    viewState: GameViewState(),
                    isHintBusy: .constant(false)
                )
            }
    }
    .environment(BoardConnectionStore())
    .environmentObject(LearningModeManager())
}
