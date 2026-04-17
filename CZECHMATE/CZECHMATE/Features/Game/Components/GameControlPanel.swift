//
//  GameControlPanel.swift
//  CZECHMATE — Collapsible control panel for game actions
//
//  Reduces toolbar clutter by grouping secondary actions
//

import CZECHMATEShared
import SwiftUI

struct GameControlPanel: View {
    @Environment(BoardConnectionStore.self) private var store
    @Environment(NetworkStatusMonitor.self) private var network

    @Bindable var viewState: GameViewState
    @Binding var isHintBusy: Bool
    
    var body: some View {
        VStack(spacing: 12) {
            // Always visible: Primary actions
            primaryActionsRow

            sandboxEntryOrControls

            if store.snapshot?.status.isGameFinished == true {
                Button {
                    viewState.showEndgameReport = true
                } label: {
                    Label("Souhrn partie", systemImage: "flag.checkered")
                        .font(.subheadline.weight(.semibold))
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 10)
                }
                .buttonStyle(.borderedProminent)
                .tint(Theme.accent)
            }
            
            // Expandable: Secondary options
            if viewState.isControlPanelExpanded {
                Divider()
                secondaryOptionsGrid
                RemoteMovesHintBanner(remoteMovesEnabled: viewState.displayOptions.remoteMoves)
                    .padding(.top, 4)

                Divider()
                boardOptionsRow
            }
            
            // „Táhlo“ jako u systémových sheetů — širší zóna dotyku než samotná čára.
            Button {
                withAnimation(.spring(duration: 0.3)) {
                    viewState.isControlPanelExpanded.toggle()
                }
            } label: {
                VStack(spacing: 6) {
                    Capsule()
                        .fill(Color.secondary.opacity(0.45))
                        .frame(width: 44, height: 5)
                    Text(viewState.isControlPanelExpanded ? "Méně" : "Více možností")
                        .font(.caption2.weight(.medium))
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 10)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(
                viewState.isControlPanelExpanded
                    ? "Sbalit další možnosti ovládání hry"
                    : "Rozbalit další možnosti ovládání hry"
            )
        }
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(.ultraThinMaterial)
        )
    }
    
    // MARK: - Sandbox

    @ViewBuilder
    private var sandboxEntryOrControls: some View {
        if store.snapshot != nil, store.puzzleBoardPreview == nil, store.historyReviewMoveIndex == nil {
            if viewState.sandboxMode {
                SandboxModePanel(
                    viewState: viewState,
                    onSyncFromBoard: { await viewState.exitSandboxAndRefresh(store: store) }
                )
            } else {
                SandboxEnterRow {
                    guard let b = store.snapshot?.board else { return }
                    viewState.enterSandboxMode(board: b)
                }
            }
        }
    }

    // MARK: - Primary Actions
    
    private var primaryActionsRow: some View {
        HStack(spacing: 20) {
            // New Game
            PrimaryActionButton(
                icon: "plus.circle.fill",
                label: "Nová",
                color: .green
            ) {
                viewState.showNewGameSetup = true
            }
            
            // Undo
            PrimaryActionButton(
                icon: "arrow.uturn.backward.circle.fill",
                label: "Zpět",
                color: .orange,
                isEnabled: (store.snapshot?.status.moveCount ?? 0) > 0
                    && store.snapshot?.status.webLocked != true
            ) {
                Task { await store.requestUndo() }
            }
            
            // Hint
            PrimaryActionButton(
                icon: "lightbulb.circle.fill",
                label: "Nápověda",
                color: .yellow,
                isEnabled: !isHintBusy
                    && store.snapshot != nil
                    && store.snapshot?.status.webLocked != true
            ) {
                requestHint()
            }
            
            // Flip Board
            PrimaryActionButton(
                icon: "arrow.up.arrow.down.circle.fill",
                label: "Otočit",
                color: .blue
            ) {
                withAnimation {
                    viewState.displayOptions.boardFlipped.toggle()
                }
            }
        }
    }
    
    // MARK: - Secondary Options Grid
    
    private var secondaryOptionsGrid: some View {
        VStack(alignment: .leading, spacing: 12) {
            LazyVGrid(columns: [
                GridItem(.flexible()),
                GridItem(.flexible())
            ], spacing: 12) {
                ToggleOption(
                    icon: "graduationcap.fill",
                    label: "Učící mód",
                    isOn: $viewState.sessionConfig.learningMode
                )

                ToggleOption(
                    icon: "arrow.left.arrow.right",
                    label: GameRemoteMovesCopy.toggleTitle,
                    isOn: $viewState.displayOptions.remoteMoves,
                    accessibilityHint: controlPanelRemoteMovesAccessibilityHint
                )

                ToggleOption(
                    icon: "number",
                    label: "Souřadnice",
                    isOn: $viewState.displayOptions.showCoordinates
                )

                ToggleOption(
                    icon: "play.fill",
                    label: "Animace",
                    isOn: $viewState.displayOptions.moveAnimations
                )
            }

            VStack(alignment: .leading, spacing: 6) {
                Label("Úroveň nápovědy", systemImage: "lightbulb")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.secondary)
                Picker("", selection: Bindable(store).moveHintTier) {
                    ForEach(MoveHintTier.allCases) { tier in
                        Text(tier.shortTitle).tag(tier)
                    }
                }
                .pickerStyle(.segmented)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
    
    // MARK: - Board Options
    
    private var boardOptionsRow: some View {
        HStack(spacing: 16) {
            Button {
                withAnimation {
                    viewState.boardZoom = min(viewState.boardZoom + 0.2, 1.5)
                }
            } label: {
                Label("Zvětšit", systemImage: "plus.magnifyingglass")
                    .font(.caption)
            }
            
            Button {
                withAnimation {
                    viewState.boardZoom = 1.0
                }
            } label: {
                Text("100%")
                    .font(.caption2.monospaced())
                    .foregroundStyle(.secondary)
            }
            
            Button {
                withAnimation {
                    viewState.boardZoom = max(viewState.boardZoom - 0.2, 0.7)
                }
            } label: {
                Label("Zmenšit", systemImage: "minus.magnifyingglass")
                    .font(.caption)
            }
        }
        .buttonStyle(.borderless)
    }
    
    // MARK: - Actions
    
    private func requestHint() {
        Task {
            isHintBusy = true
            await store.requestHint(internetPathSatisfied: network.isInternetLikelyForStockfish)
            isHintBusy = false
        }
    }

    private var controlPanelRemoteMovesAccessibilityHint: String {
        var s = GameRemoteMovesCopy.usageHint
        if viewState.displayOptions.remoteMoves,
           let r = store.remoteChessFromAppBlockedReason?.trimmingCharacters(in: .whitespacesAndNewlines),
           !r.isEmpty {
            s += " " + r
        }
        return s
    }
}

// MARK: - Primary Action Button

struct PrimaryActionButton: View {
    let icon: String
    let label: String
    let color: Color
    var isEnabled: Bool = true
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            VStack(spacing: 4) {
                Image(systemName: icon)
                    .font(.title2)
                    .symbolRenderingMode(.multicolor)
                Text(label)
                    .font(.caption2)
            }
        }
        .foregroundStyle(color)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1.0 : 0.5)
    }
}

// MARK: - Toggle Option

struct ToggleOption: View {
    let icon: String
    let label: String
    @Binding var isOn: Bool
    var accessibilityHint: String? = nil

    var body: some View {
        Button {
            isOn.toggle()
        } label: {
            HStack(spacing: 8) {
                Image(systemName: icon)
                    .foregroundStyle(isOn ? Color.accentColor : .secondary)
                
                Text(label)
                    .font(.caption)
                    .foregroundStyle(isOn ? .primary : .secondary)
                
                Spacer()
                
                Image(systemName: isOn ? "checkmark.circle.fill" : "circle")
                    .foregroundStyle(isOn ? Color.accentColor : .secondary)
                    .imageScale(.small)
            }
            .padding(8)
            .background(
                RoundedRectangle(cornerRadius: 8)
                    .fill(isOn ? Color.accentColor.opacity(0.1) : .clear)
            )
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(.isToggle)
        .accessibilityValue(isOn ? "Zapnuto" : "Vypnuto")
        .accessibilityOptionalHint(accessibilityHint)
    }
}

// MARK: - Preview

#Preview {
    GameControlPanel(
        viewState: GameViewState(),
        isHintBusy: .constant(false)
    )
    .environment(BoardConnectionStore())
    .environment(NetworkStatusMonitor())
    .padding()
}
