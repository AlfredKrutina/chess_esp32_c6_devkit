package com.example.flutter_czechmate

import android.appwidget.AppWidgetManager
import android.content.Context
import android.content.SharedPreferences
import android.view.View
import android.widget.RemoteViews
import es.antonborri.home_widget.HomeWidgetLaunchIntent
import es.antonborri.home_widget.HomeWidgetProvider

class ChessBoardHomeWidgetProvider : HomeWidgetProvider() {

    override fun onUpdate(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetIds: IntArray,
        widgetData: SharedPreferences,
    ) {
        appWidgetIds.forEach { widgetId ->
            val views = RemoteViews(context.packageName, R.layout.chess_board_home_widget)

            val pending =
                HomeWidgetLaunchIntent.getActivity(
                    context,
                    MainActivity::class.java,
                )
            views.setOnClickPendingIntent(R.id.chess_home_widget_root, pending)

            val phase = widgetData.getString("cz_phase", "no_game") ?: "no_game"
            val transportRaw = widgetData.getString("cz_transport", "") ?: ""
            val transport =
                when (transportRaw) {
                    "ble" -> "BLE"
                    "wifi" -> "Wi‑Fi"
                    "mock" -> "Demo"
                    else -> transportRaw.ifEmpty { "—" }
                }
            views.setTextViewText(R.id.chess_home_transport, transport)

            when (phase) {
                "no_game" -> {
                    views.setViewVisibility(R.id.chess_home_clocks_row, View.GONE)
                    views.setTextViewText(
                        R.id.chess_home_phase_line,
                        context.getString(R.string.chess_home_widget_phase_no_game),
                    )
                    views.setTextViewText(R.id.chess_home_footer, "")
                }
                "no_timer" -> {
                    views.setViewVisibility(R.id.chess_home_clocks_row, View.GONE)
                    val moves = widgetData.getInt("cz_total_moves", 0)
                    val finished = widgetData.getBoolean("cz_game_finished", false)
                    val line =
                        if (finished) {
                            context.getString(R.string.chess_home_widget_finished)
                        } else {
                            context.getString(R.string.chess_home_widget_moves, moves)
                        }
                    views.setTextViewText(R.id.chess_home_phase_line, line)
                    views.setTextViewText(R.id.chess_home_footer, "")
                }
                "clock" -> {
                    views.setViewVisibility(R.id.chess_home_clocks_row, View.VISIBLE)
                    val wMs = widgetData.getInt("cz_white_ms", 0)
                    val bMs = widgetData.getInt("cz_black_ms", 0)
                    val whiteTurn = widgetData.getBoolean("cz_white_turn", true)
                    val paused = widgetData.getBoolean("cz_game_paused", false)
                    val running = widgetData.getBoolean("cz_timer_running", false)
                    val finished = widgetData.getBoolean("cz_game_finished", false)
                    val moves = widgetData.getInt("cz_total_moves", 0)
                    val tc = widgetData.getString("cz_tc_label", "") ?: ""

                    views.setTextViewText(R.id.chess_home_white_time, formatMs(wMs))
                    views.setTextViewText(R.id.chess_home_black_time, formatMs(bMs))

                    val line =
                        buildString {
                            if (tc.isNotEmpty()) {
                                append(tc)
                                append(" · ")
                            }
                            append(context.getString(R.string.chess_home_widget_moves, moves))
                        }
                    views.setTextViewText(R.id.chess_home_phase_line, line)

                    val footer =
                        when {
                            finished -> context.getString(R.string.chess_home_widget_finished)
                            paused -> context.getString(R.string.chess_home_widget_paused)
                            running -> context.getString(R.string.chess_home_widget_running)
                            else -> context.getString(R.string.chess_home_widget_waiting)
                        }
                    val turnHint =
                        if (!finished) {
                            " · " + if (whiteTurn) "♔" else "♚"
                        } else {
                            ""
                        }
                    views.setTextViewText(R.id.chess_home_footer, footer + turnHint)
                }
                else -> {
                    views.setViewVisibility(R.id.chess_home_clocks_row, View.GONE)
                    views.setTextViewText(R.id.chess_home_phase_line, phase)
                    views.setTextViewText(R.id.chess_home_footer, "")
                }
            }

            appWidgetManager.updateAppWidget(widgetId, views)
        }
    }

    private fun formatMs(ms: Int): String {
        val s = maxOf(0, ms / 1000)
        val m = s / 60
        val r = s % 60
        return String.format("%d:%02d", m, r)
    }
}
