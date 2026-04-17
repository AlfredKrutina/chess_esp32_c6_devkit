//
//  ChessPieceGlyph.swift
//  CZECHMATEShared — glyfy figurek (Unicode White Chess vs Black Chess, ne přebarvený jeden tvar).
//

import SwiftUI

/// Vykreslení figurek: **různé** Unicode znaky pro bílou a černou stranu (♙ vs ♟), ne jedna sada + výplň.
public enum ChessPieceGlyph {
    public static func isWhitePiece(_ fenChar: Character) -> Bool {
        fenChar.isUppercase && fenChar.isLetter
    }

    /// Názvy obrázků v `Assets.xcassets` (iOS / watchOS) — PNG jako na webu ESP (chess.com Staunton „neo“, 150 px).
    public static func assetImageName(for fenChar: Character) -> String? {
        switch fenChar {
        case "K": return "PieceWhiteKing"
        case "Q": return "PieceWhiteQueen"
        case "R": return "PieceWhiteRook"
        case "B": return "PieceWhiteBishop"
        case "N": return "PieceWhiteKnight"
        case "P": return "PieceWhitePawn"
        case "k": return "PieceBlackKing"
        case "q": return "PieceBlackQueen"
        case "r": return "PieceBlackRook"
        case "b": return "PieceBlackBishop"
        case "n": return "PieceBlackKnight"
        case "p": return "PieceBlackPawn"
        default: return nil
        }
    }

    /// Bílá armáda: U+2654…U+2659 (White Chess …), černá: U+265A…U+265F (Black Chess …).
    public static func symbol(for fenChar: Character) -> String {
        switch fenChar {
        case "K": return "\u{2654}" // ♔ White Chess King
        case "Q": return "\u{2655}" // ♕
        case "R": return "\u{2656}" // ♖
        case "B": return "\u{2657}" // ♗
        case "N": return "\u{2658}" // ♘
        case "P": return "\u{2659}" // ♙ White Chess Pawn
        case "k": return "\u{265A}" // ♚ Black Chess King
        case "q": return "\u{265B}" // ♛
        case "r": return "\u{265C}" // ♜
        case "b": return "\u{265D}" // ♝
        case "n": return "\u{265E}" // ♞
        case "p": return "\u{265F}" // ♟ Black Chess Pawn
        default: return ""
        }
    }

    /// Barva „těla“ figurky na poli (doplnění k tvaru znaku ve fontu).
    public static func fillColor(for fenChar: Character) -> Color {
        if isWhitePiece(fenChar) {
            return Color(red: 0.99, green: 0.98, blue: 0.96)
        }
        return Color(red: 0.07, green: 0.065, blue: 0.06)
    }

    public static func layerStyle(for fenChar: Character, isLightSquare: Bool) -> PieceLayerStyle {
        if isWhitePiece(fenChar) {
            let strength = isLightSquare ? 0.52 : 0.38
            return PieceLayerStyle(
                fill: fillColor(for: fenChar),
                shadowColor: Color.black.opacity(strength),
                shadowRadius: 1.35,
                shadowX: 0,
                shadowY: 1.05
            )
        } else {
            let strength = isLightSquare ? 0.22 : 0.42
            return PieceLayerStyle(
                fill: fillColor(for: fenChar),
                shadowColor: Color.white.opacity(strength),
                shadowRadius: 0.65,
                shadowX: 0,
                shadowY: -0.45
            )
        }
    }
}

public struct PieceLayerStyle {
    public let fill: Color
    public let shadowColor: Color
    public let shadowRadius: CGFloat
    public let shadowX: CGFloat
    public let shadowY: CGFloat

    public init(fill: Color, shadowColor: Color, shadowRadius: CGFloat, shadowX: CGFloat, shadowY: CGFloat) {
        self.fill = fill
        self.shadowColor = shadowColor
        self.shadowRadius = shadowRadius
        self.shadowX = shadowX
        self.shadowY = shadowY
    }
}

extension Text {
    /// Jednotný vzhled figurky (výplň + stín podle pole).
    public func chessPieceAppearance(fenChar: Character, isLightSquare: Bool, font: Font) -> some View {
        let s = ChessPieceGlyph.layerStyle(for: fenChar, isLightSquare: isLightSquare)
        return self
            .font(font)
            .foregroundStyle(s.fill)
            .shadow(color: s.shadowColor, radius: s.shadowRadius, x: s.shadowX, y: s.shadowY)
    }
}

/// Vykreslení figurky z asset katalogu (PNG); při chybě assetu fallback na Unicode glyf.
public struct ChessPieceArtView: View {
    public let fenChar: Character
    public let size: CGFloat
    public let isLightSquare: Bool

    public init(fenChar: Character, size: CGFloat, isLightSquare: Bool) {
        self.fenChar = fenChar
        self.size = size
        self.isLightSquare = isLightSquare
    }

    public var body: some View {
        if let name = ChessPieceGlyph.assetImageName(for: fenChar) {
            let s = ChessPieceGlyph.layerStyle(for: fenChar, isLightSquare: isLightSquare)
            Image(name, bundle: .main)
                .renderingMode(.original)
                .resizable()
                .interpolation(.high)
                .scaledToFit()
                .frame(width: size, height: size)
                .shadow(color: s.shadowColor, radius: s.shadowRadius, x: s.shadowX, y: s.shadowY)
        } else {
            Text(ChessPieceGlyph.symbol(for: fenChar))
                .chessPieceAppearance(
                    fenChar: fenChar,
                    isLightSquare: isLightSquare,
                    font: Font.system(size: size * 0.82, weight: .semibold)
                )
        }
    }
}
