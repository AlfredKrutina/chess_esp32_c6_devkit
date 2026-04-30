import Flutter
import Foundation

#if canImport(ActivityKit)
import ActivityKit

@available(iOS 16.2, *)
enum LiveActivityNativeController {
  private static var currentActivity: Activity<ChessLiveAttributes>?

  static func sync(arguments: Any?, result: @escaping FlutterResult) {
    guard let raw = arguments as? [String: Any] else {
      result(FlutterError(code: "badArgs", message: "Expected map", details: nil))
      return
    }

    let auth = ActivityAuthorizationInfo()
    guard auth.areActivitiesEnabled else {
      result(
        FlutterError(
          code: "disabled", message: "Live Activities disabled in Settings", details: nil))
      return
    }

    let phase = strVal(raw, "phase", default: "no_game")

    Task {
      do {
        if phase == "no_game" {
          await endAllAsync()
          await MainActor.run { result(nil) }
          return
        }

        let state = ChessLiveAttributes.ContentState(
          whiteTimeMs: intVal(raw, "whiteTimeMs"),
          blackTimeMs: intVal(raw, "blackTimeMs"),
          isWhiteTurn: boolVal(raw, "isWhiteTurn", default: true),
          timerRunning: boolVal(raw, "timerRunning"),
          gamePaused: boolVal(raw, "gamePaused"),
          timeExpired: boolVal(raw, "timeExpired"),
          transport: strVal(raw, "transport", default: ""),
          totalMoves: intVal(raw, "totalMoves"),
          timeControlLabel: strVal(raw, "timeControlLabel", default: ""),
          gameFinished: boolVal(raw, "gameFinished"),
          phase: phase
        )

        if currentActivity == nil {
          let attributes = ChessLiveAttributes(matchTitle: "czechmate")
          currentActivity = try Activity.request(
            attributes: attributes,
            contentState: state,
            pushType: nil
          )
        } else {
          await currentActivity?.update(using: state)
        }
        await MainActor.run { result(nil) }
      } catch {
        await MainActor.run {
          result(
            FlutterError(
              code: "activityError", message: error.localizedDescription, details: nil))
        }
      }
    }
  }

  static func endAll(result: @escaping FlutterResult) {
    Task {
      await endAllAsync()
      await MainActor.run { result(nil) }
    }
  }

  private static func endAllAsync() async {
    if let act = currentActivity {
      await act.end(dismissalPolicy: .immediate)
      currentActivity = nil
    }
  }

  private static func intVal(_ d: [String: Any], _ key: String, default def: Int = 0) -> Int {
    if let v = d[key] as? Int { return v }
    if let n = d[key] as? NSNumber { return n.intValue }
    return def
  }

  private static func boolVal(_ d: [String: Any], _ key: String, default def: Bool = false) -> Bool {
    if let v = d[key] as? Bool { return v }
    if let n = d[key] as? NSNumber { return n.boolValue }
    return def
  }

  private static func strVal(_ d: [String: Any], _ key: String, default def: String = "") -> String {
    if let s = d[key] as? String { return s }
    return def
  }
}

#endif

enum LiveActivityNativeBridge {
  static func sync(arguments: Any?, result: @escaping FlutterResult) {
    #if canImport(ActivityKit)
    if #available(iOS 16.2, *) {
      LiveActivityNativeController.sync(arguments: arguments, result: result)
    } else {
      result(nil)
    }
    #else
    result(nil)
    #endif
  }

  static func endAll(result: @escaping FlutterResult) {
    #if canImport(ActivityKit)
    if #available(iOS 16.2, *) {
      LiveActivityNativeController.endAll(result: result)
    } else {
      result(nil)
    }
    #else
    result(nil)
    #endif
  }
}
