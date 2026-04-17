//
//  GameEndReportImageShareSheet.swift
//  CZECHMATE — předvolby, průhlednost, výběr bloků, náhled, sdílení obrázku a schránka.
//
//  UIKit je jen na iOS / iPadOS / visionOS / Catalyst; nativní macOS cíl používá placeholder.
//

import SwiftUI

#if canImport(UIKit)
import UIKit

struct GameEndReportImageShareSheet: View {
    let snapshot: GameSnapshot
    let model: GameEndReportModel
    let evalHistory: [MoveEvalHistoryEntry]

    @Environment(\.dismiss) private var dismiss
    @State private var selectedLayout: GameEndReportShareLayout = .classic
    @State private var exportOptions: GameEndReportShareExportOptions = .preset(.classic)
    @State private var previewImage: UIImage?
    @State private var isRendering = false
    @State private var renderToken = UUID()
    @State private var showSystemShare = false
    @State private var shareItems: [Any] = []
    @State private var copiedToast = false
    @State private var exportScale: Double = 2

    private var previewPayload: GameEndReportSharePayload {
        GameEndReportSharePayload.build(snapshot: snapshot, model: model, evalHistory: evalHistory)
    }

    private var resolvedOptions: GameEndReportShareExportOptions {
        exportOptions.resolved(for: previewPayload)
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: Theme.Spacing.l) {
                    layoutPickerSection
                    backgroundSection
                    contentTogglesSection
                    qualityPicker
                    previewSection
                    actionButtons
                }
                .padding(Theme.Spacing.l)
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .navigationTitle(EndgameReportCopy.imageShareTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(EndgameReportCopy.done) { dismiss() }
                }
            }
            .sheet(isPresented: $showSystemShare) {
                ShareActivityView(items: shareItems)
            }
            .onChange(of: selectedLayout) { _, newLayout in
                var o = GameEndReportShareExportOptions.preset(newLayout)
                o.transparentBackground = exportOptions.transparentBackground
                exportOptions = o
            }
            .onChange(of: exportOptions) { _, _ in
                renderToken = UUID()
            }
            .onChange(of: exportScale) { _, _ in
                renderToken = UUID()
            }
            .task(id: renderToken) {
                await renderPreview()
            }
        }
    }

    private var layoutPickerSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text(EndgameReportCopy.imageShareLayoutSection)
                .font(Theme.Typography.subsection())
            LazyVStack(spacing: Theme.Spacing.s) {
                ForEach(GameEndReportShareLayout.allCases) { lay in
                    Button {
                        selectedLayout = lay
                    } label: {
                        HStack(alignment: .top, spacing: Theme.Spacing.m) {
                            Image(systemName: lay == selectedLayout ? "checkmark.circle.fill" : "circle")
                                .font(.title3)
                                .foregroundStyle(lay == selectedLayout ? Theme.accent : .secondary)
                            VStack(alignment: .leading, spacing: 4) {
                                Text(EndgameReportCopy.imageShareLayoutTitle(lay))
                                    .font(Theme.Typography.cardTitle())
                                    .foregroundStyle(.primary)
                                Text(EndgameReportCopy.imageShareLayoutSubtitle(lay))
                                    .font(Theme.Typography.caption())
                                    .foregroundStyle(.secondary)
                                    .fixedSize(horizontal: false, vertical: true)
                            }
                            Spacer(minLength: 0)
                        }
                        .padding(Theme.Spacing.m)
                        .background {
                            RoundedRectangle(cornerRadius: 12, style: .continuous)
                                .fill(lay == selectedLayout ? Theme.accentMuted : Color.secondary.opacity(0.08))
                        }
                    }
                    .buttonStyle(.plain)
                }
            }
            Text(EndgameReportCopy.imageShareLayoutCustomizeHint)
                .font(Theme.Typography.caption2())
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var backgroundSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text(EndgameReportCopy.imageShareBackgroundSection)
                .font(Theme.Typography.subsection())
            Picker("", selection: transparentBinding) {
                Text(EndgameReportCopy.imageShareBackgroundGradient).tag(false)
                Text(EndgameReportCopy.imageShareBackgroundTransparent).tag(true)
            }
            .pickerStyle(.segmented)
            Text(EndgameReportCopy.imageShareBackgroundHint)
                .font(Theme.Typography.caption2())
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var transparentBinding: Binding<Bool> {
        Binding(
            get: { exportOptions.transparentBackground },
            set: { v in
                var o = exportOptions
                o.transparentBackground = v
                exportOptions = o
            }
        )
    }

    private var contentTogglesSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.m) {
            Text(EndgameReportCopy.imageShareContentSection)
                .font(Theme.Typography.subsection())
            Toggle(EndgameReportCopy.imageShareToggleHeader, isOn: optionBinding(\.includeBrandHeader))
            Toggle(EndgameReportCopy.imageShareToggleHero, isOn: optionBinding(\.includeHero))
            Toggle(EndgameReportCopy.imageShareToggleAveragesStrip, isOn: optionBinding(\.includeAveragesStrip))
                .disabled(!hasAverageData)
            Toggle(EndgameReportCopy.imageShareToggleBigStats, isOn: optionBinding(\.includeBigStatBlock))
                .disabled(!hasAverageData)
            Toggle(EndgameReportCopy.imageShareToggleChips, isOn: optionBinding(\.includeStatChips))
            Toggle(EndgameReportCopy.imageShareToggleCumulative, isOn: optionBinding(\.includeCumulativeChart))
                .disabled(previewPayload.cumulative.isEmpty)
            Toggle(EndgameReportCopy.imageShareToggleBar, isOn: optionBinding(\.includeBarChart))
                .disabled(previewPayload.barPoints.isEmpty)
            Toggle(EndgameReportCopy.imageShareToggleEval, isOn: optionBinding(\.includeEvalChart))
            Toggle(EndgameReportCopy.imageShareToggleClock, isOn: optionBinding(\.includeClockChart))
                .disabled(previewPayload.clock?.isTimeControlEnabled != true)
            Toggle(EndgameReportCopy.imageShareToggleFooter, isOn: optionBinding(\.includeFooterBrand))
            if !resolvedOptions.hasAnyContentBlock {
                Text(EndgameReportCopy.imageShareNoContentWarning)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(Theme.Semantic.warningForeground)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    private var hasAverageData: Bool {
        previewPayload.avgWhite != nil || previewPayload.avgBlack != nil || previewPayload.avgOverall != nil
    }

    private func optionBinding(_ keyPath: WritableKeyPath<GameEndReportShareExportOptions, Bool>) -> Binding<Bool> {
        Binding(
            get: { exportOptions[keyPath: keyPath] },
            set: { v in
                var o = exportOptions
                o[keyPath: keyPath] = v
                exportOptions = o
            }
        )
    }

    private var qualityPicker: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text(EndgameReportCopy.imageShareQuality)
                .font(Theme.Typography.subsection())
            Picker("", selection: $exportScale) {
                Text(EndgameReportCopy.imageShareQualityStandard).tag(2.0)
                Text(EndgameReportCopy.imageShareQualityHigh).tag(3.0)
            }
            .pickerStyle(.segmented)
            Text(EndgameReportCopy.imageShareQualityHint)
                .font(Theme.Typography.caption2())
                .foregroundStyle(.secondary)
        }
    }

    private var previewAspectRatio: CGFloat {
        guard let img = previewImage, img.size.width > 0, img.size.height > 0 else {
            return GameEndReportShareLayout.exportWidth / 1400
        }
        return img.size.width / img.size.height
    }

    private var previewSection: some View {
        VStack(alignment: .leading, spacing: Theme.Spacing.s) {
            Text(EndgameReportCopy.imageSharePreview)
                .font(Theme.Typography.subsection())
            ZStack {
                CheckerboardBackground(cell: 10)
                    .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
                if isRendering {
                    ProgressView(EndgameReportCopy.imageShareRendering)
                        .padding()
                } else if let img = previewImage {
                    Image(uiImage: img)
                        .resizable()
                        .interpolation(.high)
                        .scaledToFit()
                        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                        .padding(8)
                } else {
                    ContentUnavailableView(
                        EndgameReportCopy.imageShareNoPreview,
                        systemImage: "photo",
                        description: Text(EndgameReportCopy.imageShareNoPreviewDetail)
                    )
                    .padding()
                }
            }
            .frame(maxWidth: .infinity)
            .aspectRatio(previewAspectRatio, contentMode: .fit)
            .frame(maxHeight: 520)
        }
    }

    private var actionButtons: some View {
        VStack(spacing: Theme.Spacing.m) {
            Button {
                guard let img = previewImage else { return }
                shareItems = [img]
                showSystemShare = true
            } label: {
                Label(EndgameReportCopy.imageShareShareImage, systemImage: "square.and.arrow.up")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .tint(Theme.accent)
            .disabled(previewImage == nil || isRendering || !resolvedOptions.hasAnyContentBlock)

            Button {
                copyImageToPasteboard()
            } label: {
                Label(EndgameReportCopy.imageShareCopyClipboard, systemImage: "doc.on.doc")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
            .disabled(previewImage == nil || isRendering)

            if copiedToast {
                Text(EndgameReportCopy.imageShareCopied)
                    .font(Theme.Typography.caption())
                    .foregroundStyle(Theme.Semantic.success)
                    .frame(maxWidth: .infinity)
            }
        }
    }

    private func renderPreview() async {
        isRendering = true
        previewImage = nil
        let layout = selectedLayout
        let scale = CGFloat(exportScale)
        await Task.yield()
        let img = GameEndReportShareImageExporter.render(
            snapshot: snapshot,
            model: model,
            evalHistory: evalHistory,
            layout: layout,
            options: exportOptions,
            scale: scale
        )
        previewImage = img
        isRendering = false
    }

    private func copyImageToPasteboard() {
        guard let img = previewImage else { return }
        UIPasteboard.general.image = img
        UIImpactFeedbackGenerator(style: .medium).impactOccurred()
        copiedToast = true
        Task { @MainActor in
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            copiedToast = false
        }
    }
}

// MARK: - Šachovnicové pozadí náhledu (průhlednost)

private struct CheckerboardBackground: View {
    var cell: CGFloat = 8

    var body: some View {
        Canvas { context, size in
            let cols = Int(ceil(size.width / cell))
            let rows = Int(ceil(size.height / cell))
            for r in 0 ..< rows {
                for c in 0 ..< cols {
                    let isLight = (r + c) % 2 == 0
                    let rect = CGRect(
                        x: CGFloat(c) * cell,
                        y: CGFloat(r) * cell,
                        width: cell,
                        height: cell
                    )
                    context.fill(
                        Path(rect),
                        with: .color(isLight ? Color.gray.opacity(0.22) : Color.gray.opacity(0.38))
                    )
                }
            }
        }
    }
}

#else

/// Na macOS bez UIKit zatím jen zavřitelný placeholder (stejné API jako na iOS).
struct GameEndReportImageShareSheet: View {
    let snapshot: GameSnapshot
    let model: GameEndReportModel
    let evalHistory: [MoveEvalHistoryEntry]

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            ContentUnavailableView(
                EndgameReportCopy.imageShareTitle,
                systemImage: "photo",
                description: Text("Export souhrnu jako obrázek je v této verzi pro Mac zatím vypnutý. Použij iPhone nebo iPad.")
            )
            .navigationTitle(EndgameReportCopy.imageShareTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(EndgameReportCopy.done) { dismiss() }
                }
            }
        }
    }
}

#endif
