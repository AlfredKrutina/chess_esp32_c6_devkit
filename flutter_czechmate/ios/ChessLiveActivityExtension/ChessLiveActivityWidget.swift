import ActivityKit
import SwiftUI
import WidgetKit

private extension CMClockSnapshot {
  init(_ s: ChessLiveAttributes.ContentState) {
    self.init(
      phase: Phase(rawValue: s.phase) ?? .noGame,
      whiteMs: s.whiteTimeMs,
      blackMs: s.blackTimeMs,
      isWhiteTurn: s.isWhiteTurn,
      timerRunning: s.timerRunning,
      gamePaused: s.gamePaused,
      timeExpired: s.timeExpired,
      gameFinished: s.gameFinished,
      lowTimeMs: s.lowTimeMs,
      anchor: CMClockSnapshot.clampedAnchor(s.clockAnchorMs, fallback: Date()),
      link: CMLink(rawValue: s.link) ?? .live,
      transport: s.transport,
      totalMoves: s.totalMoves,
      timeControlLabel: s.timeControlLabel,
      lang: CMLang.resolve(s.lang))
  }

  /// One-line status for headers: the most important thing that is not a clock value.
  var headline: (text: String, symbol: String, tint: (CMPalette) -> Color) {
    let s = strings
    if link == .offline { return (s.offline, "wifi.slash", { $0.expiredBackground }) }
    if link == .connecting { return (s.linkConnecting, "arrow.triangle.2.circlepath", { $0.muted }) }
    if timeExpired { return (s.flagged, "flag.fill", { $0.expiredBackground }) }
    if gameFinished { return (s.gameFinished, "checkmark.circle.fill", { $0.muted }) }
    if gamePaused { return (s.paused, "pause.fill", { $0.lowBackground }) }
    if !hasClock { return (s.noTimeControl, "clock", { $0.muted }) }
    return ("\(s.onMove): \(s.side(isWhiteTurn))", "arrowtriangle.right.fill", { $0.accent })
  }
}

extension WidgetConfiguration {
  /// Custom Smart Stack layout on Apple Watch (iOS 18+); older systems use the compact views.
  func czechMateSmartStack() -> some WidgetConfiguration {
    if #available(iOS 18.0, *) {
      return self.supplementalActivityFamilies([.small])
    } else {
      return self
    }
  }
}

struct ChessLiveActivityWidget: Widget {
  var body: some WidgetConfiguration {
    ActivityConfiguration(for: ChessLiveAttributes.self) { context in
      ChessActivityContentRouter(snapshot: CMClockSnapshot(context.state))
        .activityBackgroundTint(CMPalette.dark.background)
        .activitySystemActionForegroundColor(CMPalette.dark.idleForeground)
    } dynamicIsland: { context in
      let snap = CMClockSnapshot(context.state)
      let palette = CMPalette.dark
      return DynamicIsland {
        DynamicIslandExpandedRegion(.leading) {
          IslandSide(snapshot: snap, white: true, palette: palette)
        }
        DynamicIslandExpandedRegion(.trailing) {
          IslandSide(snapshot: snap, white: false, palette: palette)
        }
        DynamicIslandExpandedRegion(.bottom) {
          IslandStatusRow(snapshot: snap, palette: palette)
        }
      } compactLeading: {
        CompactSideMarker(snapshot: snap, palette: palette)
      } compactTrailing: {
        CompactRunningTime(snapshot: snap, palette: palette)
      } minimal: {
        MinimalView(snapshot: snap, palette: palette)
      }
      .keylineTint(palette.accent)
    }
    .czechMateSmartStack()
  }
}

// MARK: - Content routing (Lock Screen vs Apple Watch Smart Stack)

private struct ChessActivityContentRouter: View {
  let snapshot: CMClockSnapshot

  var body: some View {
    if #available(iOS 18.0, *) {
      FamilyAwareContent(snapshot: snapshot)
    } else {
      LockScreenView(snapshot: snapshot)
    }
  }
}

@available(iOS 18.0, *)
private struct FamilyAwareContent: View {
  let snapshot: CMClockSnapshot
  @Environment(\.activityFamily) private var family

  var body: some View {
    switch family {
    case .small:
      SmartStackView(snapshot: snapshot)
    default:
      LockScreenView(snapshot: snapshot)
    }
  }
}

// MARK: - Lock Screen

private struct LockScreenView: View {
  let snapshot: CMClockSnapshot
  @Environment(\.isLuminanceReduced) private var reduced

  private let palette = CMPalette.dark

  var body: some View {
    let s = snapshot.strings
    let head = snapshot.headline
    VStack(alignment: .leading, spacing: 10) {
      HStack(spacing: 8) {
        Text("CzechMate")
          .font(.subheadline.weight(.semibold))
          .foregroundStyle(palette.idleForeground)
        if !snapshot.timeControlLabel.isEmpty {
          Text(snapshot.timeControlLabel)
            .font(.subheadline.monospacedDigit())
            .foregroundStyle(palette.muted)
            .lineLimit(1)
        }
        Spacer(minLength: 4)
        Label(head.text, systemImage: head.symbol)
          .font(.subheadline.weight(.semibold))
          .foregroundStyle(head.tint(palette))
          .lineLimit(1)
          .minimumScaleFactor(0.8)
      }

      HStack(spacing: 8) {
        LockPlate(snapshot: snapshot, white: true, palette: palette, reduced: reduced)
        LockPlate(snapshot: snapshot, white: false, palette: palette, reduced: reduced)
      }
      .accessibilityElement(children: .contain)

      if snapshot.totalMoves > 0 {
        Text(s.moves(snapshot.totalMoves))
          .font(.footnote.monospacedDigit())
          .foregroundStyle(palette.muted)
      }
    }
    .padding(14)
  }
}

private struct LockPlate: View {
  let snapshot: CMClockSnapshot
  let white: Bool
  let palette: CMPalette
  let reduced: Bool

  /// Paused / offline: nobody is highlighted, but the side on move keeps an outline.
  private func outline(_ state: CMFaceState, filled: Bool) -> Color {
    if state.isHighlighted { return filled ? .clear : palette.tint(state) }
    let sideOnMove = !snapshot.gameFinished && snapshot.isWhiteTurn == white
    return sideOnMove ? palette.muted : .clear
  }

  var body: some View {
    let s = snapshot.strings
    let now = Date()
    let state = snapshot.face(white: white, at: now)
    let colors = palette.plate(state)
    let filled = state.isHighlighted && !reduced
    let fg = filled ? colors.foreground : (state.isHighlighted ? palette.tint(state) : colors.foreground)

    VStack(alignment: .leading, spacing: 2) {
      HStack(spacing: 6) {
        CMSideDot(white: white, size: 11)
        Text(s.side(white))
          .font(.subheadline.weight(.semibold))
          .lineLimit(1)
        Spacer(minLength: 0)
      }
      if snapshot.hasClock {
        CMClockDigits(snapshot: snapshot, white: white, now: now, preferSystemTimer: true)
          .font(.system(size: 34, weight: .semibold, design: .rounded).monospacedDigit())
          .lineLimit(1)
          .minimumScaleFactor(0.6)
          .contentTransition(.numericText(countsDown: true))
      }
      HStack(spacing: 4) {
        if let symbol = state.symbol {
          Image(systemName: symbol).imageScale(.small)
        }
        Text(state.label(s))
          .lineLimit(1)
          .minimumScaleFactor(0.8)
      }
      .font(.footnote.weight(.semibold))
      .opacity(state.isHighlighted ? 1 : 0)
      .accessibilityHidden(!state.isHighlighted)
    }
    .foregroundStyle(fg)
    .padding(.horizontal, 12)
    .padding(.vertical, 10)
    .frame(maxWidth: .infinity, alignment: .leading)
    .background(
      RoundedRectangle(cornerRadius: 12, style: .continuous)
        .fill(filled ? colors.background : palette.surface)
    )
    .overlay(
      RoundedRectangle(cornerRadius: 12, style: .continuous)
        .strokeBorder(outline(state, filled: filled), lineWidth: 2)
    )
    .accessibilityElement(children: .combine)
  }
}

// MARK: - Apple Watch Smart Stack (.small)

private struct SmartStackView: View {
  let snapshot: CMClockSnapshot
  @Environment(\.isLuminanceReduced) private var reduced

  private let palette = CMPalette.dark

  var body: some View {
    VStack(spacing: 4) {
      SmartStackRow(snapshot: snapshot, white: false, palette: palette, reduced: reduced)
      SmartStackRow(snapshot: snapshot, white: true, palette: palette, reduced: reduced)
    }
    .padding(6)
  }
}

private struct SmartStackRow: View {
  let snapshot: CMClockSnapshot
  let white: Bool
  let palette: CMPalette
  let reduced: Bool

  var body: some View {
    let s = snapshot.strings
    let now = Date()
    let state = snapshot.face(white: white, at: now)
    let colors = palette.plate(state)
    let filled = state.isHighlighted && !reduced
    let fg = filled ? colors.foreground : (state.isHighlighted ? palette.tint(state) : colors.foreground)

    HStack(spacing: 6) {
      CMSideDot(white: white, size: 10)
      if let symbol = state.symbol {
        Image(systemName: symbol)
          .font(.system(size: 12, weight: .bold))
          .accessibilityHidden(true)
      }
      Spacer(minLength: 2)
      if snapshot.hasClock {
        CMClockDigits(snapshot: snapshot, white: white, now: now, preferSystemTimer: true)
          .font(.system(size: 24, weight: .semibold, design: .rounded).monospacedDigit())
          .lineLimit(1)
          .minimumScaleFactor(0.6)
          .multilineTextAlignment(.trailing)
      } else {
        Text(state.label(s))
          .font(.system(size: 15, weight: .semibold))
          .lineLimit(1)
          .minimumScaleFactor(0.7)
      }
    }
    .foregroundStyle(fg)
    .padding(.horizontal, 10)
    .frame(maxWidth: .infinity, maxHeight: .infinity)
    .background(
      RoundedRectangle(cornerRadius: 10, style: .continuous)
        .fill(filled ? colors.background : palette.surface)
    )
    .overlay(
      RoundedRectangle(cornerRadius: 10, style: .continuous)
        .strokeBorder(state.isHighlighted && !filled ? palette.tint(state) : .clear, lineWidth: 1.5)
    )
    .accessibilityElement(children: .ignore)
    .accessibilityLabel(
      snapshot.hasClock
        ? "\(s.side(white)), \(cmFormatClock(snapshot.remainingMs(white: white, at: now))), \(state.label(s))"
        : "\(s.side(white)), \(state.label(s))")
  }
}

// MARK: - Dynamic Island

private struct IslandSide: View {
  let snapshot: CMClockSnapshot
  let white: Bool
  let palette: CMPalette

  var body: some View {
    let s = snapshot.strings
    let now = Date()
    let state = snapshot.face(white: white, at: now)
    VStack(alignment: white ? .leading : .trailing, spacing: 2) {
      HStack(spacing: 5) {
        if white { CMSideDot(white: true, size: 10) }
        Text(s.side(white))
          .font(.caption.weight(.semibold))
          .foregroundStyle(palette.muted)
        if !white { CMSideDot(white: false, size: 10) }
      }
      if snapshot.hasClock {
        CMClockDigits(snapshot: snapshot, white: white, now: now, preferSystemTimer: true)
          .font(.system(size: 28, weight: .semibold, design: .rounded).monospacedDigit())
          .foregroundStyle(state.isHighlighted ? palette.tint(state) : palette.idleForeground)
          .lineLimit(1)
          .minimumScaleFactor(0.6)
          .multilineTextAlignment(white ? .leading : .trailing)
      }
    }
    .padding(.horizontal, 4)
    .accessibilityElement(children: .combine)
  }
}

private struct IslandStatusRow: View {
  let snapshot: CMClockSnapshot
  let palette: CMPalette

  var body: some View {
    let head = snapshot.headline
    HStack(spacing: 8) {
      Label(head.text, systemImage: head.symbol)
        .font(.subheadline.weight(.semibold))
        .foregroundStyle(head.tint(palette))
        .lineLimit(1)
      Spacer(minLength: 4)
      if snapshot.totalMoves > 0 {
        Text(snapshot.strings.moves(snapshot.totalMoves))
          .font(.footnote.monospacedDigit())
          .foregroundStyle(palette.muted)
      }
    }
    .padding(.horizontal, 4)
    .padding(.top, 2)
  }
}

private struct CompactSideMarker: View {
  let snapshot: CMClockSnapshot
  let palette: CMPalette

  var body: some View {
    let state = snapshot.face(white: snapshot.sideToMoveIsWhite, at: Date())
    HStack(spacing: 4) {
      CMSideDot(white: snapshot.sideToMoveIsWhite, size: 12)
      if state == .paused || state == .flagged || state == .offline, let symbol = state.symbol {
        Image(systemName: symbol)
          .font(.system(size: 11, weight: .bold))
          .foregroundStyle(palette.tint(state == .paused ? .lowTime : state))
      }
    }
    .accessibilityLabel(snapshot.headline.text)
  }
}

private struct CompactRunningTime: View {
  let snapshot: CMClockSnapshot
  let palette: CMPalette

  var body: some View {
    let white = snapshot.sideToMoveIsWhite
    let state = snapshot.face(white: white, at: Date())
    if snapshot.hasClock {
      CMClockDigits(snapshot: snapshot, white: white, now: Date(), preferSystemTimer: true)
        .font(.system(size: 15, weight: .semibold, design: .rounded).monospacedDigit())
        .foregroundStyle(state.isHighlighted ? palette.tint(state) : palette.idleForeground)
        .lineLimit(1)
        .multilineTextAlignment(.trailing)
        .frame(maxWidth: 56)
    } else {
      Image(systemName: "clock")
        .foregroundStyle(palette.muted)
    }
  }
}

private struct MinimalView: View {
  let snapshot: CMClockSnapshot
  let palette: CMPalette

  var body: some View {
    let white = snapshot.sideToMoveIsWhite
    let state = snapshot.face(white: white, at: Date())
    if snapshot.isTicking(white: white) {
      CMClockDigits(snapshot: snapshot, white: white, now: Date(), preferSystemTimer: true)
        .font(.system(size: 11, weight: .semibold, design: .rounded).monospacedDigit())
        .foregroundStyle(palette.tint(state))
        .lineLimit(1)
        .minimumScaleFactor(0.5)
        .frame(maxWidth: 36)
    } else if let symbol = state.symbol, state != .onMove {
      Image(systemName: symbol)
        .foregroundStyle(palette.tint(state == .paused ? .lowTime : state))
    } else {
      CMSideDot(white: white, size: 12)
    }
  }
}
