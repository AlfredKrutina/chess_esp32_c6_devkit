/// Which saved path to try, in order, with no extra user input.
enum SavedReconnectKind { ble, wifi }

class SavedReconnectStep {
  const SavedReconnectStep.ble()
      : kind = SavedReconnectKind.ble,
        url = null;

  const SavedReconnectStep.wifi(this.url) : kind = SavedReconnectKind.wifi;

  final SavedReconnectKind kind;
  final String? url;
}

/// Identity persisted after a successful pair. Nothing here is invented:
/// BLE remote id, last HTTP base URL, and optional mDNS host from firmware.
class SavedBoardIdentity {
  const SavedBoardIdentity({
    this.bleRemoteId,
    this.httpBaseUrl,
    this.mdnsHost,
    this.lastLinkKind = 'none',
  });

  final String? bleRemoteId;
  final String? httpBaseUrl;

  /// `czechmate-aabbcc` without `.local`. Absent on firmware that does not send it.
  final String? mdnsHost;
  final String lastLinkKind;

  bool get hasBle {
    final id = bleRemoteId?.trim();
    return id != null && id.isNotEmpty;
  }
}

class SavedBoardReconnectPlan {
  const SavedBoardReconnectPlan(this.steps);

  final List<SavedReconnectStep> steps;

  bool get isEmpty => steps.isEmpty;
}

/// Firmware hostname is `czechmate-` plus three MAC bytes, lowercase hex.
String? normalizeMdnsHost(String? raw) {
  if (raw == null) return null;
  var host = raw.trim().toLowerCase();
  if (host.endsWith('.local')) {
    host = host.substring(0, host.length - '.local'.length);
  }
  if (host.endsWith('.')) {
    host = host.substring(0, host.length - 1);
  }
  if (!RegExp(r'^czechmate-[0-9a-f]{6}$').hasMatch(host)) return null;
  return host;
}

String? mdnsHttpBaseUrl(String? rawHost) {
  final host = normalizeMdnsHost(rawHost);
  if (host == null) return null;
  return 'http://$host.local';
}

String _canonUrl(String url) =>
    url.trim().toLowerCase().replaceAll(RegExp(r'/+$'), '');

/// Order of quiet reconnect attempts.
///
/// One path at a time. BLE is included only when this host can open GATT and
/// a remote id was saved. Wi‑Fi URLs on a private LAN are skipped when the
/// phone has no LAN (cellular) unless the user forced Wi‑Fi or this host has
/// no BLE (desktop). mDNS is a second Wi‑Fi URL, not a simultaneous radio.
SavedBoardReconnectPlan planSavedBoardReconnect({
  required bool bleHostSupported,
  required bool lanPathAvailable,
  required String mode,
  required SavedBoardIdentity identity,
  required bool Function(String url) urlLooksLikePrivateLan,
}) {
  final bleOk = bleHostSupported && identity.hasBle;
  final urls = <String>[];

  final http = identity.httpBaseUrl?.trim();
  if (http != null && http.isNotEmpty) {
    final allowPrivate = !urlLooksLikePrivateLan(http) ||
        lanPathAvailable ||
        mode == 'wifi_only' ||
        !bleHostSupported;
    if (allowPrivate) urls.add(http);
  }

  final mdns = mdnsHttpBaseUrl(identity.mdnsHost);
  if (mdns != null && !urls.any((u) => _canonUrl(u) == _canonUrl(mdns))) {
    final allowMdns =
        lanPathAvailable || !bleHostSupported || mode == 'wifi_only';
    if (allowMdns) urls.add(mdns);
  }

  final wifiSteps = urls.map(SavedReconnectStep.wifi).toList();
  final bleSteps =
      bleOk ? const <SavedReconnectStep>[SavedReconnectStep.ble()] : const <SavedReconnectStep>[];

  final preferBle = mode == 'ble_only' ||
      (mode != 'wifi_only' && identity.lastLinkKind == 'bluetooth');

  return SavedBoardReconnectPlan(<SavedReconnectStep>[
    if (preferBle) ...bleSteps,
    ...wifiSteps,
    if (!preferBle) ...bleSteps,
  ]);
}
