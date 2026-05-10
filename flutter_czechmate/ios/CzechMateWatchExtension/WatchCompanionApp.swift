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

final class WatchClockModel: NSObject, ObservableObject, WCSessionDelegate {
  @Published var phase = "no_game"
  @Published var whiteMs = 0
  @Published var blackMs = 0
  @Published var isWhiteTurn = true
  @Published var timerRunning = false
  @Published var gamePaused = false
  @Published var transport = ""
  @Published var phoneReachable = false

  override init() {
    super.init()
    guard WCSession.isSupported() else { return }
    let session = WCSession.default
    session.delegate = self
    session.activate()
  }

  func session(
    _ session: WCSession,
    activationDidCompleteWith activationState: WCSessionActivationState,
    error: Error?
  ) {
    DispatchQueue.main.async {
      self.phoneReachable = session.isReachable
    }
  }

  func sessionReachabilityDidChange(_ session: WCSession) {
    DispatchQueue.main.async {
      self.phoneReachable = session.isReachable
    }
  }

  func session(_ session: WCSession, didReceiveApplicationContext applicationContext: [String: Any]) {
    DispatchQueue.main.async {
      self.applyPayload(applicationContext)
    }
  }

  private func applyPayload(_ d: [String: Any]) {
    phase = d["phase"] as? String ?? "no_game"
    whiteMs = Self.intVal(d["whiteTimeMs"])
    blackMs = Self.intVal(d["blackTimeMs"])
    isWhiteTurn = d["isWhiteTurn"] as? Bool ?? true
    timerRunning = d["timerRunning"] as? Bool ?? false
    gamePaused = d["gamePaused"] as? Bool ?? false
    transport = d["transport"] as? String ?? ""
  }

  func sendPause() { sendCmd("pause") }
  func sendResume() { sendCmd("resume") }

  private func sendCmd(_ cmd: String) {
    guard WCSession.isSupported() else { return }
    let session = WCSession.default
    let payload: [String: Any] = ["cmd": cmd]
    if session.isReachable {
      session.sendMessage(
        payload,
        replyHandler: { _ in },
        errorHandler: { _ in session.transferUserInfo(payload) })
    } else {
      session.transferUserInfo(payload)
    }
  }

  private static func intVal(_ any: Any?) -> Int {
    if let i = any as? Int { return i }
    if let n = any as? NSNumber { return n.intValue }
    return 0
  }
}

private func czFormatMs(_ ms: Int) -> String {
  let s = max(0, ms / 1000)
  let m = s / 60
  let r = s % 60
  return String(format: "%d:%02d", m, r)
}

struct WatchRootView: View {
  @ObservedObject var model: WatchClockModel

  var body: some View {
    ScrollView {
      VStack(alignment: .leading, spacing: 10) {
        if !model.phoneReachable {
          Label("Zařízení nedostupné", systemImage: "wifi.slash")
            .font(.caption2)
            .foregroundStyle(.secondary)
        }

        if model.phase == "no_game" {
          Text("Žádná aktivní partie")
            .font(.headline)
        } else if model.phase == "no_timer" {
          Text("Čekání na časomíru")
            .font(.headline)
        } else {
          HStack {
            VStack(alignment: .leading) {
              Text("♔").font(.caption2).foregroundStyle(.secondary)
              Text(czFormatMs(model.whiteMs))
                .font(.title3.monospacedDigit())
                .fontWeight(model.isWhiteTurn ? .bold : .regular)
            }
            Spacer()
            VStack(alignment: .trailing) {
              Text("♚").font(.caption2).foregroundStyle(.secondary)
              Text(czFormatMs(model.blackMs))
                .font(.title3.monospacedDigit())
                .fontWeight(model.isWhiteTurn ? .regular : .bold)
            }
          }

          Text(model.transport.isEmpty ? "—" : model.transport.uppercased())
            .font(.caption2)
            .foregroundStyle(.secondary)

          HStack(spacing: 8) {
            Button("Pauza") {
              model.sendPause()
            }
            .buttonStyle(.bordered)
            .disabled(!model.phoneReachable || model.gamePaused)

            Button("Pokračovat") {
              model.sendResume()
            }
            .buttonStyle(.borderedProminent)
            .disabled(!model.phoneReachable || !model.gamePaused)
          }
          .padding(.top, 4)
        }
      }
      .padding(.horizontal, 4)
    }
  }
}
