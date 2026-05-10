import 'package:network_info_plus/network_info_plus.dart';

/// Normalizace SSID z Android/iOS API (uvozovky, sentinel hodnoty).
String? normalizeWifiSsidRaw(String? raw) {
  if (raw == null) return null;
  var s = raw.trim();
  if (s.isEmpty) return null;
  final lower = s.toLowerCase();
  if (lower == '<unknown ssid>' ||
      lower == 'null' ||
      lower == 'ssid' ||
      s == '[SSID]') {
    return null;
  }
  if (s.length >= 2 && s.startsWith('"') && s.endsWith('"')) {
    s = s.substring(1, s.length - 1).trim();
  }
  return s.isEmpty ? null : s;
}

/// Aktuální SSID Wi‑Fi rozhraní tohoto zařízení (může být null na iOS bez oprávnění / mimo Wi‑Fi).
class PhoneWifiInfo {
  PhoneWifiInfo._();

  static Future<String?> tryCurrentWifiSsid() async {
    try {
      final name = await NetworkInfo().getWifiName();
      return normalizeWifiSsidRaw(name);
    } catch (_) {
      return null;
    }
  }
}
