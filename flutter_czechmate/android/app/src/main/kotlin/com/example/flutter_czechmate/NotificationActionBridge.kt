package com.example.flutter_czechmate

import java.util.concurrent.atomic.AtomicReference

/// Pending příkaz z notifikace (Pauza / Pokračovat) — Flutter si ho vyzvedne po návratu do popředí.
object NotificationActionBridge {
    private val pending = AtomicReference<String?>(null)

    fun offer(action: String) {
        pending.set(action)
    }

    fun consume(): String? = pending.getAndSet(null)
}
