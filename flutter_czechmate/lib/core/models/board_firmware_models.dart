/// Parita `GET /api/system/firmware`, `GET /api/system/ota/status` a hostovaného `version.json`.
class BoardFirmwareInfo {
  const BoardFirmwareInfo({
    required this.version,
    this.projectName,
    this.idfVersion,
  });

  final String version;
  final String? projectName;
  final String? idfVersion;

  factory BoardFirmwareInfo.fromJson(Map<String, dynamic> json) {
    return BoardFirmwareInfo(
      version: (json['version'] as String?)?.trim() ?? '',
      projectName: json['project_name'] as String?,
      idfVersion: json['idf'] as String?,
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
