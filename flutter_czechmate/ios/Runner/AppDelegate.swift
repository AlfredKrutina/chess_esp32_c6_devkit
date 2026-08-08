import Flutter
import UIKit
import WatchConnectivity
#if canImport(ActivityKit)
import ActivityKit
#endif

/// Method channels for Live Activity (ActivityKit) and Watch bridge.
///
/// With the implicit Flutter engine (`FlutterImplicitEngineDelegate`) there is no plugin
/// `com.czechmate.platform_channels` — `registrar(forPlugin:)` would return nil and the channels
/// would never be registered. We take the messenger from `FlutterApplicationRegistrar`.
private enum CzechMatePlatformChannels {
  static func register(messenger: FlutterBinaryMessenger) {
    let live = FlutterMethodChannel(name: "czechmate/live_activity", binaryMessenger: messenger)
    live.setMethodCallHandler { call, result in
      switch call.method {
      case "isSupported":
        if #available(iOS 16.2, *) {
          result(true)
        } else {
          result(false)
        }
      case "areActivitiesEnabled":
        #if canImport(ActivityKit)
        if #available(iOS 16.2, *) {
          result(ActivityAuthorizationInfo().areActivitiesEnabled)
        } else {
          result(false)
        }
        #else
        result(false)
        #endif
      case "syncChessClock":
        LiveActivityNativeBridge.sync(arguments: call.arguments, result: result)
      case "endAll":
        LiveActivityNativeBridge.endAll(result: result)
      default:
        result(FlutterMethodNotImplemented)
      }
    }

    let watch = FlutterMethodChannel(name: "czechmate/watch", binaryMessenger: messenger)
    watch.setMethodCallHandler { call, result in
      switch call.method {
      case "isSupported":
        result(WCSession.isSupported())
      case "isReachable":
        guard WCSession.isSupported() else {
          result(false)
          return
        }
        result(WCSession.default.isReachable)
      case "mirrorGameState":
        if let map = call.arguments as? [String: Any] {
          WatchBridge.shared.mirrorGameState(map)
        }
        result(nil)
      default:
        result(FlutterMethodNotImplemented)
      }
    }
  }
}

@main
@objc class AppDelegate: FlutterAppDelegate, FlutterImplicitEngineDelegate {
  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    WatchBridge.shared.activate()
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }

  func didInitializeImplicitFlutterEngine(_ engineBridge: FlutterImplicitEngineBridge) {
    GeneratedPluginRegistrant.register(with: engineBridge.pluginRegistry)
    let messenger = engineBridge.applicationRegistrar.messenger()
    WatchBridge.shared.attachMessenger(messenger)
    CzechMatePlatformChannels.register(messenger: messenger)
  }
}
