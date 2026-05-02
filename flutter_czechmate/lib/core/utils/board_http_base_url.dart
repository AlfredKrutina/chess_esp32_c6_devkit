/// Normalizace základní URL HTTP rozhraní šachovnice (ESP).
/// Zabraňuje relativním URI bez hostitele (`api/game/snapshot`).
String? normalizeBoardHttpBaseUrl(String? raw) {
  if (raw == null) return null;
  var u = raw.trim();
  if (u.isEmpty) return null;
  while (u.endsWith('/')) {
    u = u.substring(0, u.length - 1);
  }
  if (!u.contains('://')) {
    u = 'http://$u';
  }
  final parsed = Uri.parse(u);
  if (parsed.host.isEmpty) return null;
  return u;
}

/// Wi‑Fi transport má přednost; jinak poslední uložená URL (STA IP z předchozí session);
/// nakonec fallback z BLE notify (`sta_ip`), pokud je session aktualizovaná z GATT.
String? resolveBoardHttpBaseUrl({
  required bool wifiTransportActive,
  required String? sessionWifiBaseUrl,
  required String? prefsLastBoardBaseUrl,
  String? bleStaIp,
}) {
  final wifi = normalizeBoardHttpBaseUrl(sessionWifiBaseUrl);
  if (wifiTransportActive && wifi != null) return wifi;
  final fromPrefs = normalizeBoardHttpBaseUrl(prefsLastBoardBaseUrl);
  if (fromPrefs != null) return fromPrefs;
  if (bleStaIp != null && bleStaIp.trim().isNotEmpty) {
    return normalizeBoardHttpBaseUrl('http://${bleStaIp.trim()}');
  }
  return null;
}
