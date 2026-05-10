/// Parita `app_update.json` na GitHub Pages ([kAppUpdateJsonUrl]).
class AppUpdateManifest {
  const AppUpdateManifest({
    required this.latestVersion,
    this.minSupportedVersion,
    this.releasePageUrl,
  });

  final String latestVersion;
  final String? minSupportedVersion;

  /// HTTPS odkaz — typicky produktovka nebo GitHub Releases.
  final String? releasePageUrl;

  bool get isValid => latestVersion.trim().isNotEmpty;

  factory AppUpdateManifest.fromJson(Map<String, dynamic> json) {
    final minRaw = (json['min_supported_version'] as String?)?.trim();
    final relRaw = (json['release_page_url'] as String?)?.trim();
    return AppUpdateManifest(
      latestVersion: (json['latest_version'] as String?)?.trim() ?? '',
      minSupportedVersion:
          minRaw == null || minRaw.isEmpty ? null : minRaw,
      releasePageUrl: relRaw == null || relRaw.isEmpty ? null : relRaw,
    );
  }
}
