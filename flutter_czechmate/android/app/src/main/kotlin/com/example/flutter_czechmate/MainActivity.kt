package com.example.flutter_czechmate

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        val messenger = flutterEngine.dartExecutor.binaryMessenger

        MethodChannel(messenger, "czechmate/live_activity").setMethodCallHandler { call, result ->
            when (call.method) {
                "isSupported" -> result.success(true)
                "areActivitiesEnabled" ->
                    result.success(ChessClockNotificationHelper.notificationsAllowed(this))
                "syncChessClock" -> {
                    ChessClockNotificationHelper.sync(this, call.arguments)
                    result.success(null)
                }
                "endAll" -> {
                    ChessClockNotificationHelper.cancel(this)
                    result.success(null)
                }
                "consumePendingClockAction" ->
                    result.success(NotificationActionBridge.consume())
                else -> result.notImplemented()
            }
        }

        MethodChannel(messenger, "czechmate/watch").setMethodCallHandler { call, result ->
            when (call.method) {
                "isSupported" -> result.success(true)
                "isReachable" -> result.success(false)
                "mirrorGameState" -> {
                    WearDataLayerMirror.sync(this, call.arguments)
                    result.success(null)
                }
                else -> result.notImplemented()
            }
        }
    }
}
