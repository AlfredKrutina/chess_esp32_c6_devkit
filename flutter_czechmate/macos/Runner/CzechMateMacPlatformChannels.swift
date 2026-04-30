import FlutterMacOS

/// macOS nemá iOS Live Activities; kanály reagují, aby Dart neházel MissingPluginException.
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
