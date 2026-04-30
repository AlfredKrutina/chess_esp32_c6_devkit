package com.example.flutter_czechmate

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat

/**
 * Ongoing notification — ekvivalent „viditelnosti“ časomíry mimo appku (viz plán Fáze C).
 * Na API 33+ je potřeba oprávnění POST_NOTIFICATIONS (uživatel ho musí povolit).
 */
object ChessClockNotificationHelper {
    private const val CHANNEL_ID = "czechmate_chess_clock"
    private const val NOTIF_ID = 74001

    fun notificationsAllowed(context: Context): Boolean {
        if (Build.VERSION.SDK_INT >= 33) {
            val granted = ContextCompat.checkSelfPermission(
                context,
                android.Manifest.permission.POST_NOTIFICATIONS,
            ) == PackageManager.PERMISSION_GRANTED
            if (!granted) return false
        }
        return NotificationManagerCompat.from(context).areNotificationsEnabled()
    }

    fun ensureChannel(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val ch = NotificationChannel(
            CHANNEL_ID,
            "Chess clock",
            NotificationManager.IMPORTANCE_LOW,
        ).apply {
            description = "Remaining time while a game is active"
            setShowBadge(false)
        }
        (context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager)
            .createNotificationChannel(ch)
    }

    fun sync(context: Context, args: Any?) {
        val map = args as? Map<*, *> ?: run {
            cancel(context)
            return
        }
        val phase = map["phase"] as? String ?: "no_game"
        if (phase == "no_game") {
            cancel(context)
            return
        }
        if (!notificationsAllowed(context)) {
            cancel(context)
            return
        }

        ensureChannel(context)

        val whiteMs = intVal(map, "whiteTimeMs")
        val blackMs = intVal(map, "blackTimeMs")
        val isWhiteTurn = map["isWhiteTurn"] as? Boolean ?: true
        val timerRunning = map["timerRunning"] as? Boolean ?: false
        val gamePaused = map["gamePaused"] as? Boolean ?: false
        val transport = map["transport"] as? String ?: ""
        val moves = intVal(map, "totalMoves")
        val tc = map["timeControlLabel"] as? String ?: ""

        val status = when {
            gamePaused -> "Pause"
            timerRunning -> "Running"
            else -> "Idle"
        }
        val turn = if (isWhiteTurn) "White to move" else "Black to move"
        val line1 = "♔ ${formatMs(whiteMs)}   ♚ ${formatMs(blackMs)}"
        val line2 = buildString {
            append(turn)
            append(" · ")
            append(status)
            if (moves > 0) append(" · Moves $moves")
            if (transport.isNotEmpty()) append(" · $transport")
        }
        val subtitle = if (tc.isNotEmpty()) "$tc · $line2" else line2

        val intent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        val piFlags = PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        val pending = PendingIntent.getActivity(context, 0, intent, piFlags)

        val pauseIntent = Intent(context, ChessClockNotificationActionReceiver::class.java).apply {
            action = ChessClockNotificationActionReceiver.ACTION_PAUSE
        }
        val resumeIntent = Intent(context, ChessClockNotificationActionReceiver::class.java).apply {
            action = ChessClockNotificationActionReceiver.ACTION_RESUME
        }
        val pausePi = PendingIntent.getBroadcast(
            context,
            1001,
            pauseIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val resumePi = PendingIntent.getBroadcast(
            context,
            1002,
            resumeIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )

        val builder = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_stat_chess_clock)
            .setContentTitle(line1)
            .setContentText(subtitle)
            .setStyle(NotificationCompat.BigTextStyle().bigText(subtitle))
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setContentIntent(pending)

        if (!gamePaused) {
            builder.addAction(R.drawable.ic_stat_chess_clock, "Pauza", pausePi)
        }
        if (gamePaused) {
            builder.addAction(R.drawable.ic_stat_chess_clock, "Pokračovat", resumePi)
        }

        val notif = builder.build()

        NotificationManagerCompat.from(context).notify(NOTIF_ID, notif)
    }

    fun cancel(context: Context) {
        NotificationManagerCompat.from(context).cancel(NOTIF_ID)
    }

    private fun intVal(map: Map<*, *>, key: String): Int {
        val v = map[key] ?: return 0
        return when (v) {
            is Int -> v
            is Long -> v.toInt()
            is Number -> v.toInt()
            else -> 0
        }
    }

    private fun formatMs(ms: Int): String {
        val s = kotlin.math.max(0, ms / 1000)
        val m = s / 60
        val r = s % 60
        return String.format("%d:%02d", m, r)
    }
}
