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

/// Wi‑Fi transport má přednost; jinak poslední uložená URL (STA IP z předchozí session).
String? resolveBoardHttpBaseUrl({
  required bool wifiTransportActive,
  required String? sessionWifiBaseUrl,
  required String? prefsLastBoardBaseUrl,
}) {
  final wifi = normalizeBoardHttpBaseUrl(sessionWifiBaseUrl);
  if (wifiTransportActive && wifi != null) return wifi;
  return normalizeBoardHttpBaseUrl(prefsLastBoardBaseUrl);
}
