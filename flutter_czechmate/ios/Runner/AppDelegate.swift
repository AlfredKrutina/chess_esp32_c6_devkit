import Flutter
import UIKit
import WatchConnectivity
#if canImport(ActivityKit)
import ActivityKit
#endif

/// Method channels pro Live Activity (ActivityKit) a budoucí Watch bridge.
private enum CzechMatePlatformChannels {
  static func register(registry: FlutterPluginRegistry) {
    guard let registrar = registry.registrar(forPlugin: "com.czechmate.platform_channels") else {
      return
    }
    let messenger = registrar.messenger()

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
    if let registrar = engineBridge.pluginRegistry.registrar(forPlugin: "com.czechmate.platform_channels") {
      WatchBridge.shared.attachMessenger(registrar.messenger())
    }
    CzechMatePlatformChannels.register(registry: engineBridge.pluginRegistry)
  }
}
