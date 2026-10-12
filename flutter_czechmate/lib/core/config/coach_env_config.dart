import 'dart:io';

import 'package:flutter/foundation.dart';

/// Optional Azure AI Foundry credentials from `.env` / `.env.local` or `--dart-define`.
///
/// Settings (SharedPreferences) always win when non-empty. Never logs secret values.
abstract final class CoachEnvConfig {
  static const baseUrlKey = 'AZURE_FOUNDRY_BASE_URL';
  static const apiKeyKey = 'AZURE_FOUNDRY_API_KEY';
  static const deploymentKey = 'AZURE_FOUNDRY_DEPLOYMENT';
  static const apiVersionKey = 'AZURE_FOUNDRY_API_VERSION';

  static final Map<String, String> _fileEnv = {};
  static bool _loaded = false;

  /// Loads `.env.local` then `.env` from the process working directory when present.
  /// Safe no-op on platforms / cwd where files are missing (mobile release).
  static Future<void> load() async {
    if (_loaded) return;
    _loaded = true;
    // Prefer .env.local over .env (same keys: later file does not wipe earlier;
    // we load .env first, then .env.local overrides).
    for (final name in ['.env', '.env.local']) {
      try {
        final f = File(name);
        if (await f.exists()) {
          _mergeParse(await f.readAsString());
          if (kDebugMode) {
            debugPrint('[staging][coach_env] loaded $name (values not logged)');
          }
        }
      } catch (e) {
        if (kDebugMode) {
          debugPrint('[staging][coach_env] skip $name: $e');
        }
      }
    }
  }

  static String? get azureFoundryBaseUrl =>
      _resolve(baseUrlKey, const String.fromEnvironment(baseUrlKey));

  static String? get azureFoundryApiKey =>
      _resolve(apiKeyKey, const String.fromEnvironment(apiKeyKey));

  static String? get azureFoundryDeployment =>
      _resolve(deploymentKey, const String.fromEnvironment(deploymentKey));

  static String? get azureFoundryApiVersion =>
      _resolve(apiVersionKey, const String.fromEnvironment(apiVersionKey));

  static String? _resolve(String key, String fromDefine) {
    final fromFile = _fileEnv[key]?.trim();
    if (fromFile != null && fromFile.isNotEmpty) return fromFile;
    final d = fromDefine.trim();
    return d.isEmpty ? null : d;
  }

  static void _mergeParse(String contents) {
    for (final rawLine in contents.split(RegExp(r'\r?\n'))) {
      var line = rawLine.trim();
      if (line.isEmpty || line.startsWith('#')) continue;
      final eq = line.indexOf('=');
      if (eq <= 0) continue;
      final key = line.substring(0, eq).trim();
      if (key.isEmpty) continue;
      var value = line.substring(eq + 1).trim();
      if ((value.startsWith('"') && value.endsWith('"')) ||
          (value.startsWith("'") && value.endsWith("'"))) {
        if (value.length >= 2) {
          value = value.substring(1, value.length - 1);
        }
      }
      _fileEnv[key] = value;
    }
  }
}
