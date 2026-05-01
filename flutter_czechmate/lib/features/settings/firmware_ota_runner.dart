import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/utils/board_http_base_url.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Společná logika OTA + polling `/api/system/ota/status`.
class FirmwareOtaRunner {
  /// `null` = OK (nebo spojení přerušeno po rebootu). Jinak chybová zpráva.
  static Future<String?> execute({
    required WidgetRef ref,
    required String binUrl,
    required void Function(int percent) onProgress,
  }) async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      return 'Chybí HTTP adresa desky (AP nebo STA IP).';
    }

    final api = ref.read(boardApiClientProvider);
    try {
      final w = await api.fetchWiFiStatus(baseUrl);
      if (!w.staConnected) {
        return 'Deska potřebuje Wi‑Fi k routeru (STA), aby si stáhla firmware z internetu.';
      }
    } catch (e) {
      return 'Nelze ověřit Wi‑Fi: $e';
    }

    try {
      await ref.read(boardSessionNotifierProvider.notifier).requestFirmwareOta(binUrl);
    } catch (e) {
      return '$e';
    }

    return _pollOta(ref, baseUrl, onProgress);
  }

  static Future<String?> _pollOta(
    WidgetRef ref,
    String baseUrl,
    void Function(int percent) onProgress,
  ) async {
    final api = ref.read(boardApiClientProvider);
    for (var i = 0; i < 360; i++) {
      await Future<void>.delayed(const Duration(seconds: 1));
      try {
        final s = await api.fetchBoardOtaStatus(baseUrl);
        onProgress(s.percent.clamp(0, 100));
        if (s.state == 'error') {
          return 'OTA: ${s.message}';
        }
        if (s.state == 'done') {
          return null;
        }
      } catch (_) {
        if (i > 15) {
          return null;
        }
      }
    }
    return 'Vypršel čas sledování OTA.';
  }
}
