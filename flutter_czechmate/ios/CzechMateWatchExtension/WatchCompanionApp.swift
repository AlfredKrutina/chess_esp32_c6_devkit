import SwiftUI
import WatchConnectivity

@main
struct CzechMateWatchApp: App {
  @StateObject private var model = WatchClockModel()

  var body: some Scene {
    WindowGroup {
      WatchRootView(model: model)
    }
  }
}

// MARK: - Model

final class WatchClockModel: NSObject, ObservableObject, WCSessionDelegate {
  enum CommandState: Equatable {
    case idle
    case sending
    case sent
    case failed
  }

  @Published private(set) var snapshot = CMClockSnapshot.empty
  @Published private(set) var hasPayload = false
  @Published private(set) var phoneReachable = false
  @Published private(set) var commandState: CommandState = .idle

  private var resetWorkItem: DispatchWorkItem?

  override init() {
    super.init()
    #if DEBUG
    if let demo = UserDefaults.standard.string(forKey: "cmDemo") {
      applyStagingDemo(demo)
      return
    }
    #endif
    guard WCSession.isSupported() else { return }
    let session = WCSession.default
    session.delegate = self
    session.activate()
  }

  var strings: CMStrings { snapshot.strings }

  var canSendClockCommand: Bool {
    phoneReachable && snapshot.canToggleClock && commandState != .sending
  }

  func session(
    _ session: WCSession,
    activationDidCompleteWith activationState: WCSessionActivationState,
    error: Error?
  ) {
    let context = session.receivedApplicationContext
    DispatchQueue.main.async {
      self.phoneReachable = session.isReachable
      if !context.isEmpty { self.apply(context) }
    }
  }

  func sessionReachabilityDidChange(_ session: WCSession) {
    DispatchQueue.main.async {
      self.phoneReachable = session.isReachable
    }
  }

  func session(_ session: WCSession, didReceiveApplicationContext applicationContext: [String: Any]) {
    DispatchQueue.main.async {
      self.apply(applicationContext)
    }
  }

  func session(_ session: WCSession, didReceiveUserInfo userInfo: [String: Any] = [:]) {
    guard userInfo["phase"] != nil else { return }
    DispatchQueue.main.async {
      self.apply(userInfo)
    }
  }

  private func apply(_ payload: [String: Any]) {
    snapshot = CMClockSnapshot(payload: payload)
    hasPayload = true
  }

  /// Live message only. A queued transfer could pause the clock minutes later, so
  /// an unreachable iPhone means the command is not sent at all.
  func sendClockCommand(pause: Bool) {
    guard WCSession.isSupported(), canSendClockCommand else { return }
    let session = WCSession.default
    guard session.isReachable else {
      finish(.failed)
      return
    }
    commandState = .sending
    session.sendMessage(
      ["cmd": pause ? "pause" : "resume"],
      replyHandler: { reply in
        let ok = reply["ok"] as? Bool ?? false
        DispatchQueue.main.async { self.finish(ok ? .sent : .failed) }
      },
      errorHandler: { _ in
        DispatchQueue.main.async { self.finish(.failed) }
      })
  }

  #if DEBUG
  /// Staging only: `-cmDemo clock|low|paused|offline|nophone|flag|nogame [-cmLang cs|en]` on launch
  /// renders a fixed state without a paired iPhone (simulator screenshots).
  private func applyStagingDemo(_ name: String) {
    var p: [String: Any] = [
      "phase": "clock", "whiteTimeMs": 272_000, "blackTimeMs": 191_000, "isWhiteTurn": true,
      "timerRunning": true, "gamePaused": false, "timeExpired": false, "gameFinished": false,
      "lowTimeMs": 30_000, "clockAnchorMs": Int(Date().timeIntervalSince1970 * 1000),
      "link": "live", "transport": "ble", "totalMoves": 24, "timeControlLabel": "5+3",
      "lang": UserDefaults.standard.string(forKey: "cmLang") ?? "cs",
    ]
    switch name {
    case "low": p["isWhiteTurn"] = false; p["blackTimeMs"] = 8_400
    case "paused": p["gamePaused"] = true; p["timerRunning"] = false
    case "offline": p["link"] = "offline"
    case "flag": p["timeExpired"] = true; p["timerRunning"] = false; p["whiteTimeMs"] = 0
    case "nogame": p = ["phase": "no_game", "link": "live", "lang": p["lang"] as Any]
    default: break
    }
    print("[staging][watch] demo payload: \(name)")
    apply(p)
    phoneReachable = name != "nophone"
  }
  #endif

  private func finish(_ state: CommandState) {
    commandState = state
    resetWorkItem?.cancel()
    let item = DispatchWorkItem { [weak self] in self?.commandState = .idle }
    resetWorkItem = item
    DispatchQueue.main.asyncAfter(deadline: .now() + 3, execute: item)
  }
}

// MARK: - Root

struct WatchRootView: View {
  @ObservedObject var model: WatchClockModel

  @State private var page = WatchRootView.initialPage

  private let palette = CMPalette.dark

  private static var initialPage: Int {
    #if DEBUG
    return UserDefaults.standard.integer(forKey: "cmPage")
    #else
    return 0
    #endif
  }

  var body: some View {
    TabView(selection: $page) {
      ClockPage(model: model)
        .containerBackground(palette.background.gradient, for: .tabView)
        .tag(0)
      TablePage(model: model)
        .containerBackground(palette.background.gradient, for: .tabView)
        .tag(1)
    }
    .tabViewStyle(.verticalPage)
    .tint(palette.accent)
    .dynamicTypeSize(...DynamicTypeSize.xxLarge)
  }
}

// MARK: - Page 1: both clocks

private struct ClockPage: View {
  @ObservedObject var model: WatchClockModel
  @Environment(\.isLuminanceReduced) private var reduced

  private let palette = CMPalette.dark

  var body: some View {
    let snap = model.snapshot
    let s = model.strings
    Group {
      if !model.hasPayload {
        EmptyStateView(
          symbol: "iphone", title: s.waitingForPhoneTitle, message: s.waitingForPhoneBody,
          palette: palette)
      } else if snap.phase == .noGame {
        EmptyStateView(
          symbol: "checkerboard.rectangle", title: s.noGameTitle, message: s.noGameBody,
          palette: palette)
      } else {
        TimelineView(.periodic(from: .now, by: tickInterval(snap))) { context in
          ClockFaces(
            snapshot: snap, now: context.date, reduced: reduced,
            phoneReachable: model.phoneReachable, palette: palette)
        }
      }
    }
  }

  /// Tenths under 10 s on the running side, otherwise once per second; nothing when idle.
  private func tickInterval(_ snap: CMClockSnapshot) -> TimeInterval {
    let white = snap.isWhiteTurn
    guard snap.isTicking(white: white) else { return 60 }
    if reduced { return 1 }
    return snap.remainingMs(white: white, at: Date()) < 11_000 ? 0.1 : 1
  }
}

private struct ClockFaces: View {
  let snapshot: CMClockSnapshot
  let now: Date
  let reduced: Bool
  let phoneReachable: Bool
  let palette: CMPalette

  var body: some View {
    let s = snapshot.strings
    GeometryReader { geo in
      let noticeHeight: CGFloat = notice == nil ? 0 : 24
      let plateHeight = max(0, (geo.size.height - noticeHeight - 6) / 2)
      let digits = min(max(plateHeight * 0.52, 30), 64)
      VStack(spacing: 6) {
        if let notice {
          Label(notice.text, systemImage: notice.symbol)
            .font(.system(size: 15, weight: .semibold))
            .foregroundStyle(notice.color)
            .lineLimit(1)
            .minimumScaleFactor(0.7)
            .frame(maxWidth: .infinity, minHeight: noticeHeight - 6, alignment: .leading)
            .accessibilityLabel(notice.text)
        }
        WatchPlate(
          snapshot: snapshot, white: false, now: now, reduced: reduced, digitSize: digits,
          palette: palette)
        WatchPlate(
          snapshot: snapshot, white: true, now: now, reduced: reduced, digitSize: digits,
          palette: palette)
      }
      // Gutter for the system page indicator on the trailing edge.
      .padding(.trailing, 6)
    }
    .accessibilityElement(children: .contain)
    .accessibilityLabel(s.atTheTable)
  }

  /// States shared by both clocks are said once, above the plates.
  private var notice: (text: String, symbol: String, color: Color)? {
    let s = snapshot.strings
    if !phoneReachable { return (s.phoneUnreachable, "iphone.slash", palette.lowBackground) }
    switch snapshot.link {
    case .offline: return (s.linkOffline, "wifi.slash", palette.expiredBackground)
    case .connecting: return (s.linkConnecting, "arrow.triangle.2.circlepath", palette.muted)
    case .live: break
    }
    if snapshot.timeExpired { return nil }
    if snapshot.gameFinished { return (s.gameFinished, "checkmark.circle.fill", palette.muted) }
    if snapshot.hasClock && snapshot.gamePaused { return (s.paused, "pause.fill", palette.lowBackground) }
    if !snapshot.hasClock { return (s.noTimeControl, "clock", palette.muted) }
    return nil
  }
}

private struct WatchPlate: View {
  let snapshot: CMClockSnapshot
  let white: Bool
  let now: Date
  let reduced: Bool
  let digitSize: CGFloat
  let palette: CMPalette

  var body: some View {
    let s = snapshot.strings
    let state = snapshot.face(white: white, at: now)
    let colors = palette.plate(state)
    let filled = state.isHighlighted && !reduced
    let fg = filled ? colors.foreground : (state.isHighlighted ? palette.tint(state) : colors.foreground)
    // Paused / offline: nobody is highlighted, but the side on move keeps an outline.
    let sideOnMove = !snapshot.gameFinished && snapshot.isWhiteTurn == white
    let outline: Color =
      state.isHighlighted && !filled
      ? palette.tint(state) : (!state.isHighlighted && sideOnMove ? palette.muted : .clear)

    VStack(alignment: .leading, spacing: 0) {
      if state.isHighlighted {
        // 40–41 mm: the label replaces the name (the dot still marks the side),
        // so the digits keep their full height.
        ViewThatFits(in: .horizontal) {
          HStack(spacing: 5) {
            sideName(s)
            Spacer(minLength: 4)
            stateLabel(state, s)
          }
          HStack(spacing: 5) {
            CMSideDot(white: white, size: 11)
            stateLabel(state, s)
            Spacer(minLength: 0)
          }
        }
      } else {
        sideName(s)
      }
      if snapshot.hasClock {
        CMClockDigits(
          snapshot: snapshot, white: white, now: now, preferSystemTimer: reduced,
          showTenths: !reduced
        )
        .font(.system(size: digitSize, weight: .semibold, design: .rounded).monospacedDigit())
        .lineLimit(1)
        .minimumScaleFactor(0.45)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
      }
    }
    .foregroundStyle(fg)
    .padding(.horizontal, 10)
    .padding(.vertical, 6)
    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    .background(
      RoundedRectangle(cornerRadius: 14, style: .continuous)
        .fill(filled ? colors.background : palette.surface)
    )
    .overlay(
      RoundedRectangle(cornerRadius: 14, style: .continuous)
        .strokeBorder(outline, lineWidth: 2)
    )
    .accessibilityElement(children: .ignore)
    .accessibilityLabel(
      snapshot.hasClock
        ? "\(s.side(white)), \(cmFormatClock(snapshot.remainingMs(white: white, at: now), showTenths: false)), \(state.label(s))"
        : "\(s.side(white)), \(state.label(s))")
  }

  private func sideName(_ s: CMStrings) -> some View {
    HStack(spacing: 5) {
      CMSideDot(white: white, size: 11)
      Text(s.side(white))
        .font(.system(size: 15, weight: .semibold))
        .lineLimit(1)
        .fixedSize()
    }
  }

  private func stateLabel(_ state: CMFaceState, _ s: CMStrings) -> some View {
    HStack(spacing: 3) {
      if let symbol = state.symbol {
        Image(systemName: symbol)
          .font(.system(size: 11, weight: .bold))
      }
      Text(state.label(s))
        .font(.system(size: 14, weight: .semibold))
        .lineLimit(1)
        .fixedSize()
    }
  }
}

private struct EmptyStateView: View {
  let symbol: String
  let title: String
  let message: String
  let palette: CMPalette

  var body: some View {
    ScrollView {
      VStack(spacing: 8) {
        Image(systemName: symbol)
          .font(.system(size: 30, weight: .regular))
          .foregroundStyle(palette.accent)
          .accessibilityHidden(true)
        Text(title)
          .font(.headline)
          .foregroundStyle(palette.idleForeground)
          .multilineTextAlignment(.center)
        Text(message)
          .font(.system(size: 15))
          .foregroundStyle(palette.muted)
          .multilineTextAlignment(.center)
          .fixedSize(horizontal: false, vertical: true)
      }
      .frame(maxWidth: .infinity)
      .padding(.top, 8)
    }
  }
}

// MARK: - Page 2: connection and the one safe clock action

private struct TablePage: View {
  @ObservedObject var model: WatchClockModel
  @State private var confirming = false

  private let palette = CMPalette.dark

  var body: some View {
    let snap = model.snapshot
    let s = model.strings
    let pauseNext = !snap.gamePaused

    ScrollView {
      VStack(alignment: .leading, spacing: 8) {
        Text(s.atTheTable)
          .font(.system(size: 15, weight: .semibold))
          .foregroundStyle(palette.muted)

        StatusRow(
          symbol: linkSymbol(snap), color: linkColor(snap), title: linkTitle(snap, s),
          detail: model.hasPayload ? s.transport(snap.transport) : "", palette: palette)

        if !model.phoneReachable {
          StatusRow(
            symbol: "iphone.slash", color: palette.lowBackground, title: s.phoneUnreachable,
            detail: "", palette: palette)
        }

        if snap.phase != .noGame && model.hasPayload {
          let meta = [snap.timeControlLabel, snap.totalMoves > 0 ? s.moves(snap.totalMoves) : ""]
            .filter { !$0.isEmpty }
            .joined(separator: " · ")
          if !meta.isEmpty {
            Text(meta)
              .font(.system(size: 14).monospacedDigit())
              .foregroundStyle(palette.muted)
              .lineLimit(2)
          }
        }

        if snap.hasClock {
          Button {
            confirming = true
          } label: {
            HStack(spacing: 6) {
              if model.commandState == .sending {
                ProgressView()
                  .frame(width: 20, height: 20)
              } else {
                Image(systemName: pauseNext ? "pause.fill" : "play.fill")
              }
              Text(pauseNext ? s.pause : s.resume)
                .lineLimit(1)
            }
            .font(.system(size: 17, weight: .semibold))
            .frame(maxWidth: .infinity, minHeight: 44)
          }
          .buttonStyle(.borderedProminent)
          .tint(pauseNext ? palette.surface : palette.accent)
          .foregroundStyle(pauseNext ? palette.idleForeground : palette.activeForeground)
          .disabled(!model.canSendClockCommand)
          .opacity(model.canSendClockCommand || model.commandState == .sending ? 1 : 0.45)
          .padding(.top, 4)
          .confirmationDialog(
            pauseNext ? s.confirmPauseTitle : s.confirmResumeTitle,
            isPresented: $confirming,
            titleVisibility: .visible
          ) {
            Button(pauseNext ? s.confirmPause : s.confirmResume) {
              model.sendClockCommand(pause: pauseNext)
            }
            Button(s.cancel, role: .cancel) {}
          }

          if let feedback = feedback(snap, s) {
            Text(feedback.text)
              .font(.system(size: 14, weight: .medium))
              .foregroundStyle(feedback.color)
              .fixedSize(horizontal: false, vertical: true)
              .accessibilityAddTraits(.updatesFrequently)
          }
        }
      }
      .frame(maxWidth: .infinity, alignment: .leading)
    }
  }

  private func feedback(_ snap: CMClockSnapshot, _ s: CMStrings) -> (text: String, color: Color)? {
    switch model.commandState {
    case .sent: return (s.commandSent, palette.accent)
    case .failed: return (s.commandFailed, palette.expiredBackground)
    case .sending: return nil
    case .idle: break
    }
    if !model.phoneReachable { return (s.needsPhone, palette.muted) }
    if !snap.canToggleClock { return (s.clockNotRunning, palette.muted) }
    return nil
  }

  private func linkTitle(_ snap: CMClockSnapshot, _ s: CMStrings) -> String {
    guard model.hasPayload else { return s.waitingForPhoneTitle }
    switch snap.link {
    case .live: return s.linkLive
    case .connecting: return s.linkConnecting
    case .offline: return s.linkOffline
    }
  }

  private func linkSymbol(_ snap: CMClockSnapshot) -> String {
    guard model.hasPayload else { return "hourglass" }
    switch snap.link {
    case .live: return "checkmark.circle.fill"
    case .connecting: return "arrow.triangle.2.circlepath"
    case .offline: return "wifi.slash"
    }
  }

  private func linkColor(_ snap: CMClockSnapshot) -> Color {
    guard model.hasPayload else { return palette.muted }
    switch snap.link {
    case .live: return palette.accent
    case .connecting: return palette.lowBackground
    case .offline: return palette.expiredBackground
    }
  }
}

private struct StatusRow: View {
  let symbol: String
  let color: Color
  let title: String
  let detail: String
  let palette: CMPalette

  var body: some View {
    HStack(alignment: .firstTextBaseline, spacing: 8) {
      Image(systemName: symbol)
        .font(.system(size: 15, weight: .semibold))
        .foregroundStyle(color)
        .frame(width: 20)
        .accessibilityHidden(true)
      VStack(alignment: .leading, spacing: 1) {
        Text(title)
          .font(.system(size: 16, weight: .semibold))
          .foregroundStyle(palette.idleForeground)
          .lineLimit(2)
          .minimumScaleFactor(0.8)
        if !detail.isEmpty {
          Text(detail)
            .font(.system(size: 14))
            .foregroundStyle(palette.muted)
            .lineLimit(1)
        }
      }
    }
    .accessibilityElement(children: .combine)
  }
}
