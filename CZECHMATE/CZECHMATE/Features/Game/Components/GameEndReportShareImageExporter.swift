//
//  GameEndReportShareImageExporter.swift
//  CZECHMATE — rasterizace sdílecí karty (ImageRenderer), dynamická výška, volitelná průhlednost.
//
//  Kompiluje se jen tam, kde je UIKit (iOS / iPadOS / visionOS / Mac Catalyst).
//

import SwiftUI

#if canImport(UIKit)
import UIKit

enum GameEndReportShareImageExporter {
    /// Vykreslí kartu do bitmapy. `scale` 2–3 = ostrý export pro sociální sítě.
    @MainActor
    static func render(
        snapshot: GameSnapshot,
        model: GameEndReportModel,
        evalHistory: [MoveEvalHistoryEntry],
        layout: GameEndReportShareLayout,
        options: GameEndReportShareExportOptions,
        scale: CGFloat = 2
    ) -> UIImage? {
        let payload = GameEndReportSharePayload.build(
            snapshot: snapshot,
            model: model,
            evalHistory: evalHistory
        )
        let resolvedOpts = options.resolved(for: payload)
        let content = GameEndReportShareCardView(
            layout: layout,
            payload: payload,
            options: resolvedOpts
        )
        .environment(\.colorScheme, .dark)

        let targetWidth = GameEndReportShareLayout.exportWidth
        let measuredHeight = measureExportHeight(content: content, width: targetWidth)
        let exportHeight = max(320, measuredHeight)

        let renderer = ImageRenderer(content: content)
        renderer.scale = scale
        renderer.proposedSize = ProposedViewSize(width: targetWidth, height: exportHeight)
        if resolvedOpts.transparentBackground {
            renderer.isOpaque = false
        }

        let img = renderer.uiImage
        if img == nil {
            AppDebugLog.staging(
                "EndgameReportShareImageExporter: uiImage nil (layout=\(layout.rawValue) h=\(exportHeight))"
            )
        }
        return img
    }

    /// Spolehlivé měření výšky SwiftUI stromu před `ImageRenderer` (fixní šířka, vertikálně „co potřebuje“).
    @MainActor
    private static func measureExportHeight<Content: View>(content: Content, width: CGFloat) -> CGFloat {
        let host = UIHostingController(rootView: content)
        host.view.backgroundColor = .clear
        host.view.invalidateIntrinsicContentSize()

        let target = CGSize(width: width, height: UIView.layoutFittingExpandedSize.height)
        let size = host.view.systemLayoutSizeFitting(
            target,
            withHorizontalFittingPriority: .required,
            verticalFittingPriority: .fittingSizeLevel
        )
        let h = ceil(size.height)
        return h
    }
}

#endif
