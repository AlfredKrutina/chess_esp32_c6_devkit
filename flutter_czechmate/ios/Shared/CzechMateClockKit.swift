// Shared by ChessLiveActivityExtension (Live Activity, Dynamic Island, Smart Stack)
// and CzechMateWatchExtension. Rules mirror lib/features/game/controls/player_clock.dart.
import SwiftUI

// MARK: - Language

enum CMLang: Equatable {
  case cs
  case en

  /// Payload `lang` wins (the app language picked in Settings); otherwise the device language.
  static func resolve(_ code: String?) -> CMLang {
    let raw: String
    if let code, !code.isEmpty {
      raw = code
    } else {
      raw = Locale.preferredLanguages.first ?? "en"
    }
    return raw.lowercased().hasPrefix("cs") ? .cs : .en
  }
}

/// Same wording as the iPhone app (`clock*`, `gameBar*` in lib/l10n/app_*.arb).
struct CMStrings {
  let lang: CMLang

  private func t(_ cs: String, _ en: String) -> String { lang == .cs ? cs : en }

  var white: String { t("Bílý", "White") }
  var black: String { t("Černý", "Black") }
  var onMove: String { t("Na tahu", "On move") }
  var lowTime: String { t("Málo času", "Low time") }
  var flagged: String { t("Čas vypršel", "Time is up") }
  var paused: String { t("Pauza", "Paused") }
  var offline: String { t("Bez spojení", "No connection") }
  var waiting: String { t("Čeká", "Waiting") }
  var noTimeControl: String { t("Bez časové kontroly", "No time control") }
  var gameFinished: String { t("Partie skončila", "Game over") }
  var noGameTitle: String { t("Žádná partie", "No game") }
  var noGameBody: String {
    t(
      "Začni partii na desce. Čas se tu ukáže, jakmile hodiny poběží.",
      "Start a game on the board. The clock shows up here once it runs.")
  }
  var waitingForPhoneTitle: String { t("Čekám na iPhone", "Waiting for iPhone") }
  var waitingForPhoneBody: String {
    t(
      "Otevři CzechMate na iPhonu vedle desky.",
      "Open CzechMate on the iPhone next to the board.")
  }
  var linkLive: String { t("Deska připojena", "Board connected") }
  var linkConnecting: String { t("Připojuji desku…", "Connecting board…") }
  var linkOffline: String { t("Deska nepřipojena", "Board not connected") }
  var phoneUnreachable: String { t("iPhone mimo dosah", "iPhone out of reach") }
  var atTheTable: String { t("U stolu", "At the table") }
  var pause: String { t("Pauza", "Pause") }
  var resume: String { t("Pokračovat", "Resume") }
  var confirmPauseTitle: String { t("Pozastavit hodiny?", "Pause the clock?") }
  var confirmResumeTitle: String { t("Spustit hodiny?", "Resume the clock?") }
  var confirmPause: String { t("Pozastavit", "Pause") }
  var confirmResume: String { t("Spustit", "Resume") }
  var cancel: String { t("Zrušit", "Cancel") }
  var commandSent: String { t("Odesláno na desku", "Sent to the board") }
  var commandFailed: String {
    t("Nepovedlo se. Použij iPhone.", "Didn't go through. Use the iPhone.")
  }
  var clockNotRunning: String { t("Hodiny neběží", "The clock is not running") }
  var needsPhone: String {
    t("Pauza jde jen s iPhonem v dosahu.", "Pause needs the iPhone in reach.")
  }

  func moves(_ n: Int) -> String { t("Tahy: \(n)", "Moves: \(n)") }

  func transport(_ raw: String) -> String {
    switch raw {
    case "ble": return "Bluetooth"
    case "wifi": return "Wi‑Fi"
    case "mock": return t("Ukázková deska", "Demo board")
    default: return ""
    }
  }

  func side(_ white: Bool) -> String { white ? self.white : black }
}

// MARK: - Clock state

enum CMLink: String {
  case live
  case connecting
  case offline
}

enum CMFaceState: Equatable {
  case idle
  case onMove
  case lowTime
  case flagged
  case paused
  case offline
  case noTimeControl

  func label(_ s: CMStrings) -> String {
    switch self {
    case .idle: return s.waiting
    case .onMove: return s.onMove
    case .lowTime: return s.lowTime
    case .flagged: return s.flagged
    case .paused: return s.paused
    case .offline: return s.offline
    case .noTimeControl: return s.noTimeControl
    }
  }

  /// Icon next to the label so state never depends on colour alone.
  var symbol: String? {
    switch self {
    case .onMove: return "arrowtriangle.right.fill"
    case .lowTime: return "hourglass.bottomhalf.filled"
    case .flagged: return "flag.fill"
    case .paused: return "pause.fill"
    case .offline: return "wifi.slash"
    case .idle, .noTimeControl: return nil
    }
  }

  var isHighlighted: Bool {
    switch self {
    case .onMove, .lowTime, .flagged: return true
    default: return false
    }
  }
}

/// Snapshot of what the iPhone mirrors (`LiveActivityService._buildPayload` in Flutter).
struct CMClockSnapshot: Equatable {
  enum Phase: String {
    case noGame = "no_game"
    case noTimer = "no_timer"
    case clock
  }

  var phase: Phase
  var whiteMs: Int
  var blackMs: Int
  var isWhiteTurn: Bool
  var timerRunning: Bool
  var gamePaused: Bool
  var timeExpired: Bool
  var gameFinished: Bool
  var lowTimeMs: Int
  /// Moment the iPhone received the current clock values; local countdown starts here.
  var anchor: Date
  var link: CMLink
  var transport: String
  var totalMoves: Int
  var timeControlLabel: String
  var lang: CMLang

  static let empty = CMClockSnapshot(
    phase: .noGame, whiteMs: 0, blackMs: 0, isWhiteTurn: true, timerRunning: false,
    gamePaused: false, timeExpired: false, gameFinished: false, lowTimeMs: 20_000,
    anchor: Date(), link: .offline, transport: "", totalMoves: 0, timeControlLabel: "",
    lang: CMLang.resolve(nil))

  init(
    phase: Phase, whiteMs: Int, blackMs: Int, isWhiteTurn: Bool, timerRunning: Bool,
    gamePaused: Bool, timeExpired: Bool, gameFinished: Bool, lowTimeMs: Int, anchor: Date,
    link: CMLink, transport: String, totalMoves: Int, timeControlLabel: String, lang: CMLang
  ) {
    self.phase = phase
    self.whiteMs = whiteMs
    self.blackMs = blackMs
    self.isWhiteTurn = isWhiteTurn
    self.timerRunning = timerRunning
    self.gamePaused = gamePaused
    self.timeExpired = timeExpired
    self.gameFinished = gameFinished
    self.lowTimeMs = lowTimeMs
    self.anchor = anchor
    self.link = link
    self.transport = transport
    self.totalMoves = totalMoves
    self.timeControlLabel = timeControlLabel
    self.lang = lang
  }

  /// Older iPhone builds send no `link` / `lowTimeMs` / `clockAnchorMs`; defaults keep them readable.
  init(payload d: [String: Any], receivedAt: Date = Date()) {
    func int(_ k: String, _ def: Int = 0) -> Int {
      if let v = d[k] as? Int { return v }
      if let n = d[k] as? NSNumber { return n.intValue }
      return def
    }
    func bool(_ k: String, _ def: Bool = false) -> Bool {
      if let v = d[k] as? Bool { return v }
      if let n = d[k] as? NSNumber { return n.boolValue }
      return def
    }
    func str(_ k: String) -> String { d[k] as? String ?? "" }

    let anchorMs = int("clockAnchorMs")
    self.init(
      phase: Phase(rawValue: str("phase")) ?? .noGame,
      whiteMs: int("whiteTimeMs"),
      blackMs: int("blackTimeMs"),
      isWhiteTurn: bool("isWhiteTurn", true),
      timerRunning: bool("timerRunning"),
      gamePaused: bool("gamePaused"),
      timeExpired: bool("timeExpired"),
      gameFinished: bool("gameFinished"),
      lowTimeMs: int("lowTimeMs", 20_000),
      anchor: CMClockSnapshot.clampedAnchor(anchorMs, fallback: receivedAt),
      link: CMLink(rawValue: str("link")) ?? .live,
      transport: str("transport"),
      totalMoves: int("totalMoves"),
      timeControlLabel: str("timeControlLabel"),
      lang: CMLang.resolve(d["lang"] as? String))
  }

  static func clampedAnchor(_ epochMs: Int, fallback: Date) -> Date {
    guard epochMs > 0 else { return fallback }
    let date = Date(timeIntervalSince1970: TimeInterval(epochMs) / 1000)
    return min(date, Date())
  }

  var strings: CMStrings { CMStrings(lang: lang) }
  var connected: Bool { link == .live }
  var hasClock: Bool { phase == .clock }

  private func reportedMs(white: Bool) -> Int { max(0, white ? whiteMs : blackMs) }

  /// True while this side's time runs on the board, so the face may count down locally.
  func isTicking(white: Bool) -> Bool {
    hasClock && timerRunning && !gamePaused && !timeExpired && !gameFinished && connected
      && isWhiteTurn == white
  }

  /// Never below zero. Reaching zero locally does not flag; only the board does.
  func remainingMs(white: Bool, at now: Date) -> Int {
    let base = reportedMs(white: white)
    guard isTicking(white: white) else { return base }
    let elapsed = Int(now.timeIntervalSince(anchor) * 1000)
    return max(0, base - max(0, elapsed))
  }

  /// Wall-clock end of the running side, for system-driven `Text(timerInterval:)`.
  func runningInterval(white: Bool) -> ClosedRange<Date>? {
    guard isTicking(white: white) else { return nil }
    let end = anchor.addingTimeInterval(TimeInterval(reportedMs(white: white)) / 1000)
    let start = min(anchor, end)
    return start...end
  }

  func face(white: Bool, at now: Date) -> CMFaceState {
    if !hasClock {
      let onMove = !gameFinished && isWhiteTurn == white
      if !connected { return .offline }
      return onMove ? .onMove : .noTimeControl
    }
    let ms = remainingMs(white: white, at: now)
    let onMove = isWhiteTurn == white
    if timeExpired {
      return (reportedMs(white: white) <= 0 || onMove) ? .flagged : .idle
    }
    if !connected { return .offline }
    if gameFinished { return .idle }
    if gamePaused { return .paused }
    if !onMove { return .idle }
    return ms <= lowTimeMs ? .lowTime : .onMove
  }

  /// Which side is on move, for headers and the compact Dynamic Island.
  var sideToMoveIsWhite: Bool { isWhiteTurn }

  /// Same guard as the iPhone pause button (`_ActionBar.canToggleClock`).
  var canToggleClock: Bool {
    hasClock && connected && !gameFinished && !timeExpired && (timerRunning || gamePaused)
  }
}

// MARK: - Formatting

/// Matches Flutter `formatClockMs`: tenths under 10 s, h:mm:ss over an hour.
func cmFormatClock(_ ms: Int, showTenths: Bool = true) -> String {
  let clamped = max(0, ms)
  if showTenths && clamped < 10_000 {
    let tenths = clamped / 100
    return "\(tenths / 10).\(tenths % 10)"
  }
  let total = clamped / 1000
  let h = total / 3600
  let m = (total % 3600) / 60
  let s = total % 60
  if h > 0 { return String(format: "%d:%02d:%02d", h, m, s) }
  return String(format: "%d:%02d", m, s)
}

// MARK: - Palette ("Klidný stůl", ClockTokens in lib/core/theme/app_theme.dart)

extension Color {
  init(cmHex hex: UInt32) {
    self.init(
      red: Double((hex >> 16) & 0xFF) / 255,
      green: Double((hex >> 8) & 0xFF) / 255,
      blue: Double(hex & 0xFF) / 255)
  }
}

struct CMPalette {
  let background: Color
  let surface: Color
  let idleBackground: Color
  let idleForeground: Color
  let activeBackground: Color
  let activeForeground: Color
  let lowBackground: Color
  let lowForeground: Color
  let expiredBackground: Color
  let expiredForeground: Color
  let muted: Color
  let accent: Color
  let outline: Color

  static let dark = CMPalette(
    background: Color(cmHex: 0x111714),
    surface: Color(cmHex: 0x1A221E),
    idleBackground: Color(cmHex: 0x1A221E),
    idleForeground: Color(cmHex: 0xD5DCD6),
    activeBackground: Color(cmHex: 0xE9E4D4),
    activeForeground: Color(cmHex: 0x13201A),
    lowBackground: Color(cmHex: 0xF3C766),
    lowForeground: Color(cmHex: 0x241A00),
    expiredBackground: Color(cmHex: 0xFFB4AB),
    expiredForeground: Color(cmHex: 0x410002),
    muted: Color(cmHex: 0xA9B4AC),
    accent: Color(cmHex: 0x9CCBB2),
    outline: Color(cmHex: 0x3A4540))

  static let light = CMPalette(
    background: Color(cmHex: 0xF6F5F0),
    surface: Color(cmHex: 0xEAE9E3),
    idleBackground: Color(cmHex: 0xE6E5DF),
    idleForeground: Color(cmHex: 0x1F2420),
    activeBackground: Color(cmHex: 0x233A30),
    activeForeground: Color(cmHex: 0xFFFFFF),
    lowBackground: Color(cmHex: 0x6B4700),
    lowForeground: Color(cmHex: 0xFFFFFF),
    expiredBackground: Color(cmHex: 0x93000A),
    expiredForeground: Color(cmHex: 0xFFFFFF),
    muted: Color(cmHex: 0x4A524C),
    accent: Color(cmHex: 0x2E4A3E),
    outline: Color(cmHex: 0xC9CCC5))

  static func of(_ scheme: ColorScheme) -> CMPalette { scheme == .light ? .light : .dark }

  /// Plate fill and text colour for a face state.
  func plate(_ state: CMFaceState) -> (background: Color, foreground: Color) {
    switch state {
    case .onMove: return (activeBackground, activeForeground)
    case .lowTime: return (lowBackground, lowForeground)
    case .flagged: return (expiredBackground, expiredForeground)
    case .idle, .paused, .offline, .noTimeControl: return (idleBackground, idleForeground)
    }
  }

  /// Text-only tint for black system backgrounds (Dynamic Island, reduced luminance).
  func tint(_ state: CMFaceState) -> Color {
    switch state {
    case .onMove: return activeBackground
    case .lowTime: return lowBackground
    case .flagged: return expiredBackground
    case .idle, .paused, .offline, .noTimeControl: return muted
    }
  }
}

// MARK: - Small shared views

/// Side marker drawn in the current text colour: outline = White, filled = Black
/// (the ♔ / ♚ convention), so it reads the same on light and dark plates.
struct CMSideDot: View {
  let white: Bool
  var size: CGFloat = 12

  var body: some View {
    Group {
      if white {
        Circle().strokeBorder(lineWidth: max(1.5, size / 6))
      } else {
        Circle()
      }
    }
    .frame(width: size, height: size)
    .accessibilityHidden(true)
  }
}

/// Clock digits. A running side uses the system timer so it keeps counting
/// between updates and on reduced-luminance displays.
struct CMClockDigits: View {
  let snapshot: CMClockSnapshot
  let white: Bool
  let now: Date
  var preferSystemTimer: Bool = false
  var showTenths: Bool = true

  var body: some View {
    if preferSystemTimer, let interval = snapshot.runningInterval(white: white) {
      Text(timerInterval: interval, countsDown: true)
    } else {
      Text(cmFormatClock(snapshot.remainingMs(white: white, at: now), showTenths: showTenths))
    }
  }
}
