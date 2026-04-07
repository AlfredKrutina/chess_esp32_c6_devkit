//
//  AppUIComponents.swift
//  CZECHMATE — sdílené UI podle plánu (bannery, sekce, prázdné stavy).
//

import SwiftUI

// MARK: - Banner

/// Jednotný informační pruh s pozadím z `Theme.BannerStyle`.
struct ThemedBanner<Content: View>: View {
    let style: Theme.BannerStyle
    var cornerRadius: CGFloat = 12
    @ViewBuilder let content: () -> Content

    var body: some View {
        content()
            .padding(Theme.Spacing.m)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    .fill(style.backgroundColor)
            )
    }
}

// MARK: - Nadpis sekce

struct AppSectionHeader: View {
    let title: String
    var subtitle: String?
    var systemImage: String?

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: Theme.Spacing.s) {
            if let systemImage {
                Image(systemName: systemImage)
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundStyle(Theme.accent)
                    .accessibilityHidden(true)
            }
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(Theme.Typography.subsection())
                if let subtitle, !subtitle.isEmpty {
                    Text(subtitle)
                        .font(Theme.Typography.caption2())
                        .foregroundStyle(.secondary)
                }
            }
            Spacer(minLength: 0)
        }
        .accessibilityElement(children: .combine)
    }
}

// MARK: - Prázdný stav

struct AppEmptyState: View {
    let systemImage: String
    let title: String
    let message: String
    var primaryButtonTitle: String?
    var primaryAction: (() -> Void)?

    var body: some View {
        VStack(spacing: Theme.Spacing.l) {
            Image(systemName: systemImage)
                .font(.system(size: 40, weight: .light))
                .foregroundStyle(Theme.accent.opacity(0.85))
                .accessibilityHidden(true)
            VStack(spacing: Theme.Spacing.s) {
                Text(title)
                    .font(Theme.Typography.sectionTitle())
                    .multilineTextAlignment(.center)
                Text(message)
                    .font(Theme.Typography.body())
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
            if let primaryButtonTitle, let primaryAction {
                Button(primaryButtonTitle, action: primaryAction)
                    .buttonStyle(.themePrimary)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(Theme.Spacing.l)
    }
}
