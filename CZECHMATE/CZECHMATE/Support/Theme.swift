//
//  Theme.swift
//  CZECHMATE — jednotný vizuální jazyk (karty, tlačítka, typografie).
//

import SwiftUI

enum Theme {
    /// Akcentová zelená pro primární akce (konzistence napříč aplikací).
    static let accent = Color(red: 0.22, green: 0.62, blue: 0.38)
    static let accentMuted = Color(red: 0.22, green: 0.62, blue: 0.38).opacity(0.14)

    // MARK: - Rozestupy (8pt grid)

    enum Spacing {
        static let xs: CGFloat = 4
        static let s: CGFloat = 8
        static let m: CGFloat = 12
        static let l: CGFloat = 16
        static let xl: CGFloat = 24
        static let xxl: CGFloat = 32
    }

    // MARK: - Sémantické barvy (text + jemné pozadí)

    enum Semantic {
        static let errorForeground = Color.red
        static let errorBackground = Color.red.opacity(0.1)
        static let warningForeground = Color.orange
        static let warningBackground = Color.orange.opacity(0.12)
        static let success = Color(red: 0.2, green: 0.78, blue: 0.35)
        static let successBackground = Color(red: 0.2, green: 0.78, blue: 0.35).opacity(0.14)
        static let infoForeground = Color.blue
        static let infoBackground = Color.blue.opacity(0.1)
        static let neutralFill = Color.secondary.opacity(0.08)
        static let purpleBackground = Color.purple.opacity(0.1)
    }

    /// Pozadí informačních pruhů (bannery) — sjednocuje Hru, Analýzu, …
    enum BannerStyle {
        case error
        case warning
        case info
        case success
        case neutral
        case accent
        case purple
        case mock

        var backgroundColor: Color {
            switch self {
            case .error: return Semantic.errorBackground
            case .warning: return Semantic.warningBackground
            case .info: return Semantic.infoBackground
            case .success: return Semantic.successBackground
            case .neutral: return Semantic.neutralFill
            case .accent: return Theme.accentMuted
            case .purple: return Semantic.purpleBackground
            case .mock: return Theme.accentMuted
            }
        }
    }

    // MARK: - Typografie (rounded — jednotně s existujícími obrazovkami)

    enum Typography {
        static func screenTitle() -> Font { .system(.largeTitle, design: .rounded).weight(.bold) }
        static func sectionTitle() -> Font { .system(.title3, design: .rounded).weight(.semibold) }
        static func cardTitle() -> Font { .system(.headline, design: .rounded) }
        static func subsection() -> Font { .system(.subheadline, design: .rounded).weight(.semibold) }
        static func body() -> Font { .system(.body, design: .rounded) }
        static func caption() -> Font { .system(.caption, design: .rounded) }
        static func caption2() -> Font { .system(.caption2, design: .rounded) }
    }
}

// MARK: - Karty

struct ThemeCardModifier: ViewModifier {
    var cornerRadius: CGFloat = 12

    func body(content: Content) -> some View {
        content
            .padding(16)
            .background {
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    #if os(iOS)
                    .fill(Color(uiColor: .secondarySystemGroupedBackground))
                    #elseif os(macOS)
                    .fill(Color(nsColor: .controlBackgroundColor))
                    #else
                    .fill(Color.gray.opacity(0.14))
                    #endif
                    .shadow(color: .black.opacity(0.06), radius: 8, y: 2)
            }
    }
}

extension View {
    func themeCard(cornerRadius: CGFloat = 12) -> some View {
        modifier(ThemeCardModifier(cornerRadius: cornerRadius))
    }
}

// MARK: - Primární tlačítko

struct ThemePrimaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(.headline, design: .rounded).weight(.semibold))
            .foregroundStyle(.white)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .fill(Theme.accent.opacity(configuration.isPressed ? 0.85 : 1))
            )
    }
}

struct ThemeSecondaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(.subheadline, design: .rounded).weight(.medium))
            .foregroundStyle(Theme.accent)
            .padding(.vertical, 10)
            .padding(.horizontal, 14)
            .background(
                RoundedRectangle(cornerRadius: 10, style: .continuous)
                    .fill(Theme.accentMuted.opacity(configuration.isPressed ? 1.2 : 1))
            )
    }
}

extension ButtonStyle where Self == ThemePrimaryButtonStyle {
    static var themePrimary: ThemePrimaryButtonStyle { ThemePrimaryButtonStyle() }
}

extension ButtonStyle where Self == ThemeSecondaryButtonStyle {
    static var themeSecondary: ThemeSecondaryButtonStyle { ThemeSecondaryButtonStyle() }
}

// MARK: - Toolbar: ikonová tlačítka (nápověda, připojení)

/// Kompaktní kulaté tlačítko v toolbaru — čitelnější než holý SF Symbol.
struct ThemeToolbarIconChip: View {
    let systemName: String
    var accessibilityLabel: String
    var tint: Color = Theme.accent
    var background: Color = Theme.accentMuted
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: systemName)
                .font(.system(size: 16, weight: .semibold))
                .foregroundStyle(tint)
                .frame(width: 34, height: 34)
                .background {
                    Circle()
                        .fill(background)
                }
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(accessibilityLabel)
    }
}
