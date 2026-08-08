/// Manifest always from the `main` branch — works even if GitHub Pages returns a 404.
/// The binary in the manifest goes to `raw.githubusercontent.com/.../gh-pages/firmware/…` after successful deployment.
/// Viz [.github/workflows/gh-pages.yml].
const kDefaultFirmwareManifestUrl =
    'https://raw.githubusercontent.com/alfredkrutina/chess_esp32_c6_devkit/main/firmware/version.json';

/// Fixes truncated GitHub Pages URLs (e.g. `…/chess_`), repo paths missing `version.json`,
/// and migrates `*.github.io/.../version.json` for this project to [kDefaultFirmwareManifestUrl].
String normalizeFirmwareManifestUrl(String raw) {
  var s = raw.trim();
  if (s.isEmpty) return kDefaultFirmwareManifestUrl;
  while (s.endsWith('/')) {
    s = s.substring(0, s.length - 1);
  }

  /// The manifest must be JSON (`version.json`). Common error: embedding a reference to `.bin`
  /// from gh-pages — that file is firmware, not manifest (the Unicode preview then looks like "commas").
  final rawGh = Uri.tryParse(s);
  if (rawGh != null &&
      rawGh.scheme == 'https' &&
      rawGh.host.toLowerCase() == 'raw.githubusercontent.com') {
    final segs = rawGh.pathSegments.where((e) => e.isNotEmpty).toList();
    if (segs.length >= 3) {
      final last = segs.last.toLowerCase();
      if (last.endsWith('.bin')) {
        final user = segs[0];
        final repo = segs[1];
        return 'https://raw.githubusercontent.com/$user/$repo/main/firmware/version.json';
      }
    }
  }

  final uri = Uri.tryParse(s);
  if (uri == null || uri.host.isEmpty || uri.scheme != 'https') return s;

  final host = uri.host.toLowerCase();
  if (!host.endsWith('.github.io')) return s;

  final segments = uri.pathSegments.where((e) => e.isNotEmpty).toList();
  final seg0 = segments.isEmpty ? '' : segments[0].replaceAll('-', '_').toLowerCase();

  if (segments.any((e) => e.toLowerCase() == 'version.json')) {
    if (uri.path.toLowerCase().contains('chess_esp32_c6_devkit')) {
      return kDefaultFirmwareManifestUrl;
    }
    return s;
  }

  // Typical error from keyboard / link: just `…github.io/chess_`
  if (segments.length == 1 && seg0 == 'chess_') {
    return kDefaultFirmwareManifestUrl;
  }

  // Project root without `/firmware/version.json`
  if (segments.length == 1 && seg0 == 'chess_esp32_c6_devkit') {
    return kDefaultFirmwareManifestUrl;
  }

  if (segments.length == 2 &&
      seg0 == 'chess_esp32_c6_devkit' &&
      segments[1].toLowerCase() == 'firmware') {
    return kDefaultFirmwareManifestUrl;
  }

  return s;
}
