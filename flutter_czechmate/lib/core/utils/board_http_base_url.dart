/// ESP default AP has gateway [192.168.4.1] on port 80.
/// The prefs sometimes accidentally stores the same host with the port from the application's local HTTP server on that device
/// (e.g. `:52340`) → `Connection refused` at `POST /api/system/ota`.
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

/// Normalizing the base URL of the Checkerboard (ESP) HTTP interface.
/// Prevents hostless relative URIs (`api/game/snapshot`).
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

/// Wi‑Fi transport takes precedence; otherwise, the last saved URL (STA IP from the previous session);
/// finally fallback from BLE notify (`sta_ip`) if the session is updated from GATT.
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
