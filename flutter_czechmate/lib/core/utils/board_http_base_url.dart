/// ESP výchozí AP má gateway [192.168.4.1] na portu 80.
/// Do prefs se někdy omylem uloží stejný host s portem z lokálního HTTP serveru telefonu
/// (např. `:52340`) → `Connection refused` při `POST /api/system/ota`.
String? normalizeEspApGatewayBaseUrl(String? url) {
  if (url == null || url.trim().isEmpty) return null;
  final u = Uri.tryParse(url.trim());
  if (u == null || u.host.isEmpty) return url.trim();
  if (u.host == '192.168.4.1' && u.hasPort && u.port != 80) {
    final sch = u.scheme.isEmpty ? 'http' : u.scheme;
    return '$sch://${u.host}';
  }
  return url.trim();
}

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
  return normalizeEspApGatewayBaseUrl(u) ?? u;
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
