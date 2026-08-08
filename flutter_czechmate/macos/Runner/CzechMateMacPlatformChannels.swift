import FlutterMacOS

/// macOS has no iOS Live Activities; channels still respond so Dart does not throw MissingPluginException.
enum CzechMateMacPlatformChannels {
  static func registerLiveActivityAndWatch(messenger: FlutterBinaryMessenger) {
    let live = FlutterMethodChannel(name: "czechmate/live_activity", binaryMessenger: messenger)
    live.setMethodCallHandler { call, result in
      switch call.method {
      case "isSupported", "areActivitiesEnabled":
        result(false)
      case "syncChessClock":
        result(nil)
      case "endAll":
        result(nil)
      default:
        result(FlutterMethodNotImplemented)
      }
    }

    let watch = FlutterMethodChannel(name: "czechmate/watch", binaryMessenger: messenger)
    watch.setMethodCallHandler { call, result in
      switch call.method {
      case "isSupported", "isReachable":
        result(false)
      case "mirrorGameState":
        result(nil)
      default:
        result(FlutterMethodNotImplemented)
      }
    }
  }
}
