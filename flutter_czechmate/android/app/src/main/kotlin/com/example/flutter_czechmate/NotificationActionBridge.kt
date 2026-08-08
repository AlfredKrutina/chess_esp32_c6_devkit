package com.example.flutter_czechmate

import java.util.concurrent.atomic.AtomicReference

/// Pending command from the notification (Pause / Resume) — Flutter picks it up after returning to the foreground.
object NotificationActionBridge {
    private val pending = AtomicReference<String?>(null)

    fun offer(action: String) {
        pending.set(action)
    }

    fun consume(): String? = pending.getAndSet(null)
}
