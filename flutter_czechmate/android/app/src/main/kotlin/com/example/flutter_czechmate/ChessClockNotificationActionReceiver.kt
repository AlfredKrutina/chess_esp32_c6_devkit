package com.example.flutter_czechmate

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * Akce z ongoing notifikace — viz plán Fáze C (PendingIntent → Flutter přes [NotificationActionBridge]).
 */
class ChessClockNotificationActionReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context?, intent: Intent?) {
        when (intent?.action) {
            ACTION_PAUSE -> NotificationActionBridge.offer("pause")
            ACTION_RESUME -> NotificationActionBridge.offer("resume")
        }
    }

    companion object {
        const val ACTION_PAUSE = "com.example.flutter_czechmate.ACTION_CLOCK_PAUSE"
        const val ACTION_RESUME = "com.example.flutter_czechmate.ACTION_CLOCK_RESUME"
    }
}
