/// Manifest vždy z větve `main` — funguje i když GitHub Pages vrací 404.
/// Binárka v manifestu míří na `raw.githubusercontent.com/.../gh-pages/firmware/…` po úspěšném deployi.
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

  // Typická chyba z klávesnice / odkazu: jen `…github.io/chess_`
  if (segments.length == 1 && seg0 == 'chess_') {
    return kDefaultFirmwareManifestUrl;
  }

  // Kořen projektu bez `/firmware/version.json`
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
