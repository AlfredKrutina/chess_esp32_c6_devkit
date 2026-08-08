/// Parses CSV values ​​0-255 to filter IPv4 by 3rd octet (eg `88` → `192.168.88.*`).
Set<int> parseWifiBlockedThirdOctetsCsv(String csv) {
  final out = <int>{};
  for (final part in csv.split(',')) {
    final t = part.trim();
    if (t.isEmpty) continue;
    final v = int.tryParse(t);
    if (v != null && v >= 0 && v <= 255) {
      out.add(v);
    }
  }
  return out;
}

/// Returns the 3rd octet for pure IPv4 `a.b.c.d`, otherwise null (hostname, IPv6, void).
int? parseIpv4ThirdOctet(String host) {
  var h = host.trim();
  if (h.isEmpty) return null;
  if (h.startsWith('[') && h.endsWith(']')) {
    h = h.substring(1, h.length - 1);
  }
  final parts = h.split('.');
  if (parts.length != 4) return null;
  for (final p in parts) {
    final v = int.tryParse(p);
    if (v == null || v < 0 || v > 255) return null;
  }
  return int.parse(parts[2]);
}

bool wifiIpv4ThirdOctetIsBlocked(String host, Set<int> blockedThirdOctets) {
  if (blockedThirdOctets.isEmpty) return false;
  final third = parseIpv4ThirdOctet(host);
  if (third == null) return false;
  return blockedThirdOctets.contains(third);
}
