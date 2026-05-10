/// Parita `GET /api/system/firmware`, `GET /api/system/ota/status` a hostovaného `version.json`.
class BoardFirmwareInfo {
  const BoardFirmwareInfo({
    required this.version,
    this.projectName,
    this.idfVersion,
    this.otaSupported,
    this.otaLastBootFailed,
    this.otaFailedFirmwareVersion,
    this.otaFailedSlot,
  });

  final String version;
  final String? projectName;
  final String? idfVersion;

  /// From `GET /api/system/firmware` (`ota_supported`). Null if absent (older firmware).
  final bool? otaSupported;

  /// `GET /api/system/firmware` — bootloader vrátil předchozí slot po pádu nového obrazu.
  final bool? otaLastBootFailed;

  /// Semver z hlavičky neúspěšného slotu (`ota_failed_firmware_version`).
  final String? otaFailedFirmwareVersion;

  /// Např. `ota_0` / `ota_1` (`ota_failed_slot`).
  final String? otaFailedSlot;

  factory BoardFirmwareInfo.fromJson(Map<String, dynamic> json) {
    final rawOta = json['ota_supported'];
    final rawFail = json['ota_last_boot_failed'];
    return BoardFirmwareInfo(
      version: (json['version'] as String?)?.trim() ?? '',
      projectName: json['project_name'] as String?,
      idfVersion: json['idf'] as String?,
      otaSupported: rawOta is bool ? rawOta : null,
      otaLastBootFailed: rawFail is bool ? rawFail : null,
      otaFailedFirmwareVersion:
          (json['ota_failed_firmware_version'] as String?)?.trim(),
      otaFailedSlot: (json['ota_failed_slot'] as String?)?.trim(),
    );
  }
}

class BoardOtaStatus {
  const BoardOtaStatus({
    required this.state,
    required this.percent,
    required this.message,
  });

  final String state;
  final int percent;
  final String message;

  factory BoardOtaStatus.fromJson(Map<String, dynamic> json) {
    return BoardOtaStatus(
      state: (json['state'] as String?)?.trim() ?? 'idle',
      percent: (json['percent'] as num?)?.toInt() ?? 0,
      message: (json['message'] as String?) ?? '',
    );
  }
}

class FirmwareManifest {
  const FirmwareManifest({
    required this.version,
    required this.url,
    this.changelog,
  });

  final String version;
  final String url;
  final String? changelog;

  factory FirmwareManifest.fromJson(Map<String, dynamic> json) {
    return FirmwareManifest(
      version: (json['version'] as String?)?.trim() ?? '',
      url: (json['url'] as String?)?.trim() ?? '',
      changelog: json['changelog'] as String?,
    );
  }
}

/// Volné porovnání `1.0.10` vs `v1.2` (doplňuje nuly).
int compareSemverLoose(String a, String b) {
  String norm(String s) {
    var t = s.trim();
    if (t.startsWith('v') || t.startsWith('V')) {
      t = t.substring(1);
    }
    return t;
  }

  List<int> parts(String s) {
    final t = norm(s);
    final raw = t
        .split(RegExp(r'[.\-+]'))
        .where((e) => e.isNotEmpty)
        .map((e) => int.tryParse(e) ?? 0)
        .toList();
    final out = List<int>.from(raw);
    while (out.length < 3) {
      out.add(0);
    }
    return out;
  }

  final pa = parts(a);
  final pb = parts(b);
  for (var i = 0; i < 3; i++) {
    final c = pa[i].compareTo(pb[i]);
    if (c != 0) {
      return c;
    }
  }
  return 0;
}
