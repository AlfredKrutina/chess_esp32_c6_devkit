package com.example.flutter_czechmate.wear

import android.app.Activity
import android.os.Bundle
import android.widget.ScrollView
import android.widget.TextView
import com.google.android.gms.wearable.DataClient
import com.google.android.gms.wearable.DataEvent
import com.google.android.gms.wearable.DataEventBuffer
import com.google.android.gms.wearable.DataMapItem
import com.google.android.gms.wearable.Wearable

/**
 * Minimal Wear OS companion — zobrazí JSON payload z Data Layer (`/czechmate/chess_clock`).
 * Spárované zařízení (telefon nebo tablet) posílá data z [com.example.flutter_czechmate.WearDataLayerMirror].
 */
class MainActivity : Activity(), DataClient.OnDataChangedListener {

    private lateinit var textView: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        textView = TextView(this).apply {
            textSize = 13f
            setPadding(24, 24, 24, 24)
            text = "czechmate\n\nČekám na spárované zařízení…"
        }
        val scroll = ScrollView(this)
        scroll.addView(textView)
        setContentView(scroll)
        Wearable.getDataClient(this).addListener(this)
    }

    override fun onDestroy() {
        Wearable.getDataClient(this).removeListener(this)
        super.onDestroy()
    }

    override fun onDataChanged(events: DataEventBuffer) {
        events.use { buffer ->
            for (event in buffer) {
                if (event.type != DataEvent.TYPE_CHANGED) continue
                val path = event.dataItem.uri.path ?: continue
                if (path != "/czechmate/chess_clock") continue
                val json =
                    DataMapItem.fromDataItem(event.dataItem).dataMap.getString("json") ?: continue
                runOnUiThread {
                    textView.text = "czechmate\n\n$json"
                }
            }
        }
    }
}
