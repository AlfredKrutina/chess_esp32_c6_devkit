import Flutter
import Foundation
import WatchConnectivity

/// Forwards game state to Apple Watch and passes commands from the watch into Flutter.
final class WatchBridge: NSObject, WCSessionDelegate {
  static let shared = WatchBridge()

  private weak var flutterMessenger: FlutterBinaryMessenger?

  private override init() {
    super.init()
  }

  func attachMessenger(_ messenger: FlutterBinaryMessenger) {
    flutterMessenger = messenger
  }

  func activate() {
    guard WCSession.isSupported() else { return }
    let session = WCSession.default
    session.delegate = self
    session.activate()
  }

  /// Keys match the map from Flutter (`ChessLivePayload`).
  func mirrorGameState(_ payload: [String: Any]) {
    guard WCSession.isSupported() else { return }
    let session = WCSession.default
    guard session.activationState == .activated else {
      session.transferUserInfo(payload)
      return
    }
    if session.isPaired || session.isWatchAppInstalled {
      do {
        try session.updateApplicationContext(payload)
      } catch {
        session.transferUserInfo(payload)
      }
    }
  }

  /// `completion(true)` only when Flutter confirms the board accepted the command.
  private func forwardWatchCommandToFlutter(
    cmd: String, payload: [String: Any], completion: @escaping (Bool) -> Void
  ) {
    guard let messenger = flutterMessenger else {
      completion(false)
      return
    }
    let channel = FlutterMethodChannel(name: "czechmate/watch", binaryMessenger: messenger)
    channel.invokeMethod(
      "onWatchCommand",
      arguments: [
        "cmd": cmd,
        "payload": payload,
      ]
    ) { result in
      completion((result as? Bool) == true)
    }
  }

  func session(
    _ session: WCSession,
    activationDidCompleteWith activationState: WCSessionActivationState,
    error: Error?
  ) {}

  func sessionDidBecomeInactive(_ session: WCSession) {}

  func sessionDidDeactivate(_ session: WCSession) {
    session.activate()
  }

  func session(
    _ session: WCSession,
    didReceiveMessage message: [String: Any],
    replyHandler: @escaping ([String: Any]) -> Void
  ) {
    guard let cmd = message["cmd"] as? String else {
      replyHandler(["ok": false])
      return
    }
    DispatchQueue.main.async {
      self.forwardWatchCommandToFlutter(cmd: cmd, payload: message) { ok in
        replyHandler(["ok": ok])
      }
    }
  }

  /// Queued transfers can arrive long after the tap; clock commands are accepted
  /// only as live messages (see `didReceiveMessage`).
  func session(_ session: WCSession, didReceiveUserInfo userInfo: [String: Any] = [:]) {}
}
