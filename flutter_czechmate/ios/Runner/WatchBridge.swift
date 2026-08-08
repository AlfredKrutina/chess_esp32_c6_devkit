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

  private func forwardWatchCommandToFlutter(cmd: String, payload: [String: Any]) {
    guard let messenger = flutterMessenger else { return }
    let channel = FlutterMethodChannel(name: "czechmate/watch", binaryMessenger: messenger)
    channel.invokeMethod(
      "onWatchCommand",
      arguments: [
        "cmd": cmd,
        "payload": payload,
      ])
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
      self.forwardWatchCommandToFlutter(cmd: cmd, payload: message)
      replyHandler(["ok": true])
    }
  }

  func session(_ session: WCSession, didReceiveUserInfo userInfo: [String: Any] = [:]) {
    guard let cmd = userInfo["cmd"] as? String else { return }
    DispatchQueue.main.async {
      self.forwardWatchCommandToFlutter(cmd: cmd, payload: userInfo)
    }
  }
}
