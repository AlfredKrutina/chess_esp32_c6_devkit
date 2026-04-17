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

    /// Jemný obrys karet / bannerů podle režimu (oddělení od `systemGroupedBackground`).
    static func cardBorderColor(for colorScheme: ColorScheme) -> Color {
        colorScheme == .dark
            ? Color.white.opacity(0.09)
            : Color.black.opacity(0.06)
    }

    static func cardShadowOpacity(for colorScheme: ColorScheme) -> Double {
        colorScheme == .dark ? 0.35 : 0.06
    }

    // MARK: - Sandbox (lokální náhled — doplňuje zelený accent, ne ho nahrazuje)

    enum Sandbox {
        static let accent = Color(red: 0.52, green: 0.38, blue: 0.74)
        static let fill = accent.opacity(0.12)
        static let fillStrong = accent.opacity(0.18)
        static let stroke = accent.opacity(0.32)
    }
}

// MARK: - Karty

struct ThemeCardModifier: ViewModifier {
    var cornerRadius: CGFloat = 12
    @Environment(\.colorScheme) private var colorScheme

    func body(content: Content) -> some View {
        content
            .padding(Theme.Spacing.l)
            .background {
                RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                    #if os(iOS)
                    .fill(Color(uiColor: .secondarySystemGroupedBackground))
                    #elseif os(macOS)
                    .fill(Color(nsColor: .controlBackgroundColor))
                    #else
                    .fill(Color.gray.opacity(0.14))
                    #endif
                    .shadow(
                        color: .black.opacity(Theme.cardShadowOpacity(for: colorScheme)),
                        radius: colorScheme == .dark ? 10 : 8,
                        y: colorScheme == .dark ? 4 : 2
                    )
                    .overlay {
                        RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                            .stroke(Theme.cardBorderColor(for: colorScheme), lineWidth: 0.5)
                    }
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
                    .shadow(color: Theme.accent.opacity(configuration.isPressed ? 0.12 : 0.22), radius: 6, y: 3)
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

/// Sekundární akce na celou šířku — obrys (ghost), stejná výška jako primární.
struct ThemeOutlineButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(.headline, design: .rounded).weight(.semibold))
            .foregroundStyle(Theme.accent)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .stroke(Theme.accent, lineWidth: 2)
            )
            .opacity(configuration.isPressed ? 0.88 : 1)
    }
}

extension ButtonStyle where Self == ThemeOutlineButtonStyle {
    static var themeOutline: ThemeOutlineButtonStyle { ThemeOutlineButtonStyle() }
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
                .font(.system(size: 18, weight: .semibold))
                .foregroundStyle(tint)
                .frame(width: 44, height: 44)
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

// MARK: - Accessibility

extension View {
    /// Přidá VoiceOver hint jen když je neprázdný (prázdný řetězec by škodil).
    @ViewBuilder
    func accessibilityOptionalHint(_ hint: String?) -> some View {
        if let hint, !hint.isEmpty {
            self.accessibilityHint(hint)
        } else {
            self
        }
    }
}
