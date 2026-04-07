//
//  TimerView.swift
//  CZECHMATE — nativní odpočet podle posledního snapshotu a času příjmu.
//

import CZECHMATEShared
import SwiftUI
#if os(iOS)
import UIKit
#endif

/// Zobrazí časy bílého/černého; s `whiteTimeMs`/`blackTimeMs` z `/api/timer` plynulý odpočet.
struct TimerView: View {
    let whiteBase: UInt32?
    let blackBase: UInt32?
    var whiteTimeMs: UInt32?
    var blackTimeMs: UInt32?
    let currentPlayer: String
    let gameActive: Bool
    let snapshotReceivedAt: Date?

    private var whiteIsActive: Bool {
        gameActive && currentPlayer.lowercased() == "white"
    }

    private var blackIsActive: Bool {
        gameActive && currentPlayer.lowercased() == "black"
    }

    var body: some View {
        TimelineView(.periodic(from: .now, by: 0.1)) { context in
            HStack(spacing: 16) {
                timerChip(
                    title: "Bílý",
                    base: whiteBase,
                    baseMs: whiteTimeMs,
                    isActive: whiteIsActive,
                    now: context.date
                )
                timerChip(
                    title: "Černý",
                    base: blackBase,
                    baseMs: blackTimeMs,
                    isActive: blackIsActive,
                    now: context.date
                )
            }
            .frame(maxWidth: .infinity)
        }
    }

    private func timerChip(title: String, base: UInt32?, baseMs: UInt32?, isActive: Bool, now: Date) -> some View {
        let rem: Double
        if let ms = baseMs {
            rem = BoardClockInterpolation.remainingSecondsFromMilliseconds(
                baseMilliseconds: ms,
                isActive: isActive,
                timerRunning: gameActive,
                clockReceivedAt: snapshotReceivedAt,
                now: now
            )
        } else {
            rem = BoardClockInterpolation.remainingSecondsStatic(fromSnapshotSeconds: base)
        }
        let display = ChessTimeFormat.displayClockInterpolated(rem)
        let lowTime = isActive && rem < 10
        let pulse = lowTime ? 0.08 + 0.1 * (0.5 + 0.5 * sin(now.timeIntervalSinceReferenceDate * 4.5)) : 0.0

        return VStack(spacing: 6) {
            Text(title)
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)
            Text(display)
                .font(.system(size: 28, weight: .semibold, design: .rounded))
                .monospacedDigit()
                .foregroundStyle(lowTime ? Color.red : (isActive ? Color.primary : Color.secondary))
                .padding(.horizontal, 20)
                .padding(.vertical, 12)
                .background(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .fill(
                            lowTime
                                ? Color.red.opacity(0.12 + pulse)
                                : (isActive ? Color.accentColor.opacity(0.18) : Color.secondary.opacity(0.14))
                        )
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .stroke(
                            lowTime ? Color.red.opacity(0.55) : (isActive ? Color.accentColor.opacity(0.5) : Color.clear),
                            lineWidth: 1.5
                        )
                )
        }
        .frame(maxWidth: .infinity)
    }
}

// MARK: - Kompaktní řádky nad / pod šachovnicí (chess.com styl: avatar + pilulka)

/// Jedna řada hodin a sebraných figurek (černý řádek nahoře, bílý dole).
struct BoardTimerLine: View {
    let isBlackRow: Bool
    let status: GameStatus
    /// Čas příjmu snímku (pro kompatibilitu; hodiny berou z `timerClock` + `timerClockReceivedAt`).
    let snapshotReceivedAt: Date?
    let board: [[String]]
    /// `GET /api/timer` — zbývající čas v ms (ne kumulativní údaj ze snapshotu).
    var timerClock: BoardTimerHTTPState?
    var timerClockReceivedAt: Date?

    private var baseSnapshot: UInt32? {
        isBlackRow ? status.blackTime : status.whiteTime
    }

    private var baseMs: UInt32? {
        guard let t = timerClock else { return nil }
        return isBlackRow ? t.blackTimeMs : t.whiteTimeMs
    }

    private var isActive: Bool {
        if let t = timerClock {
            let whiteTurn = t.isWhiteTurn
            guard t.timerRunning, !t.gamePaused, !status.isGameFinished else { return false }
            return isBlackRow ? !whiteTurn : whiteTurn
        }
        let whiteTurn = status.currentPlayer.lowercased() == "white"
        guard status.isTimerRunning else { return false }
        return isBlackRow ? !whiteTurn : whiteTurn
    }

    private var timerRunningForInterpolation: Bool {
        if let t = timerClock {
            return t.timerRunning && !t.gamePaused && !status.isGameFinished
        }
        return status.isTimerRunning && !status.isGameFinished
    }

    private var capturedPieceChars: [Character] {
        isBlackRow
            ? CapturedPiecesBar.missingWhitePieceChars(board: board)
            : CapturedPiecesBar.missingBlackPieceChars(board: board)
    }

    private var playerGlyph: Character {
        isBlackRow ? "k" : "K"
    }

    private var tertiarySystemFillColor: Color {
        #if os(iOS)
        Color(uiColor: .tertiarySystemFill)
        #else
        Color.gray.opacity(0.22)
        #endif
    }

    private var secondaryGroupedBgColor: Color {
        #if os(iOS)
        Color(uiColor: .secondarySystemGroupedBackground)
        #else
        Color.gray.opacity(0.14)
        #endif
    }

    var body: some View {
        TimelineView(.periodic(from: .now, by: 0.1)) { context in
            HStack(alignment: .center, spacing: 8) {
                if isBlackRow {
                    playerChip
                    timeCapsule(now: context.date)
                    Spacer(minLength: 4)
                    capturedStrip
                } else {
                    capturedStrip
                    Spacer(minLength: 4)
                    timeCapsule(now: context.date)
                    playerChip
                }
            }
            .padding(.vertical, 4)
        }
    }

    private var playerChip: some View {
        ChessPieceArtView(fenChar: playerGlyph, size: 24, isLightSquare: true)
            .frame(width: 36, height: 36)
            .background(
                Circle()
                    .fill(tertiarySystemFillColor)
            )
            .overlay(
                Circle()
                    .stroke(Color.secondary.opacity(0.2), lineWidth: 1)
            )
    }

    private func timeCapsule(now: Date) -> some View {
        let rem = remainingDisplaySeconds(now: now)
        let t = ChessTimeFormat.displayClockInterpolated(rem)
        let lowTime = isActive && rem < 10
        let pulse = lowTime ? 0.07 + 0.09 * (0.5 + 0.5 * sin(now.timeIntervalSinceReferenceDate * 4.5)) : 0.0
        return Text(t)
            .font(.system(size: 17, weight: .semibold, design: .rounded))
            .monospacedDigit()
            .foregroundStyle(lowTime ? Color.red : (isActive ? Color.primary : Color.secondary))
            .padding(.horizontal, 14)
            .padding(.vertical, 7)
            .background(
                Capsule(style: .continuous)
                    .fill(
                        lowTime
                            ? Color.red.opacity(0.14 + pulse)
                            : secondaryGroupedBgColor
                    )
            )
            .overlay(
                Capsule(style: .continuous)
                    .stroke(
                        lowTime ? Color.red.opacity(0.5) : (isActive ? Theme.accent.opacity(0.45) : Color.clear),
                        lineWidth: 1.5
                    )
            )
    }

    /// Čas synchronizace s deskou: preferuj okamžik příjmu `/api/timer`, jinak čas snímku (bez toho zůstane statický čas = „hodiny nejdou“).
    private var effectiveClockSyncAt: Date? {
        timerClockReceivedAt ?? snapshotReceivedAt
    }

    private func remainingDisplaySeconds(now: Date) -> Double {
        if let ms = baseMs {
            return BoardClockInterpolation.remainingSecondsFromMilliseconds(
                baseMilliseconds: ms,
                isActive: isActive,
                timerRunning: timerRunningForInterpolation,
                clockReceivedAt: effectiveClockSyncAt,
                now: now
            )
        }
        return BoardClockInterpolation.remainingSecondsStatic(fromSnapshotSeconds: baseSnapshot)
    }

    @ViewBuilder
    private var capturedStrip: some View {
        if capturedPieceChars.isEmpty {
            Text(" ")
                .font(.caption2)
                .foregroundStyle(.clear)
                .frame(minWidth: 4)
        } else {
            HStack(spacing: 2) {
                ForEach(Array(capturedPieceChars.enumerated()), id: \.offset) { _, ch in
                    ChessPieceArtView(fenChar: ch, size: 18, isLightSquare: true)
                        .minimumScaleFactor(0.6)
                }
            }
        }
    }

}
