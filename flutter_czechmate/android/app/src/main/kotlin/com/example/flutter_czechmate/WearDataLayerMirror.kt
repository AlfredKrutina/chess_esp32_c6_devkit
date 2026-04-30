package com.example.flutter_czechmate

import android.content.Context
import android.util.Log
import com.google.android.gms.wearable.PutDataMapRequest
import com.google.android.gms.wearable.Wearable
import org.json.JSONObject

/**
 * Wear OS Data Layer — stejný payload jako Live Activity mirror (plán §6).
 * Neprovádí nic závažného, pokud nejsou spárované hodinky / Play Services.
 */
object WearDataLayerMirror {
    private const val TAG = "WearDataLayerMirror"
    private const val PATH = "/czechmate/chess_clock"

    fun sync(context: Context, args: Any?) {
        val map = args as? Map<*, *> ?: return
        try {
            val json = mapToJson(map)
            val req = PutDataMapRequest.create(PATH).apply {
                dataMap.putString("json", json)
                dataMap.putLong("ts", System.currentTimeMillis())
            }.asPutDataRequest()
            Wearable.getDataClient(context.applicationContext).putDataItem(req)
        } catch (e: Exception) {
            Log.d(TAG, "wear mirror skip: ${e.message}")
        }
    }

    private fun mapToJson(map: Map<*, *>): String {
        val j = JSONObject()
        for ((rawK, v) in map) {
            val k = rawK?.toString() ?: continue
            when (v) {
                null -> j.put(k, JSONObject.NULL)
                is Boolean -> j.put(k, v)
                is Int -> j.put(k, v)
                is Long -> j.put(k, v)
                is Double -> j.put(k, v)
                is Float -> j.put(k, v.toDouble())
                is Number -> j.put(k, v.toDouble())
                else -> j.put(k, v.toString())
            }
        }
        return j.toString()
    }
}
