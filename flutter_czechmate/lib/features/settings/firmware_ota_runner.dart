import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import '../../app_providers.dart';
import '../../core/utils/board_http_base_url.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// OTA over HTTPS: phone sends only the `.bin` URL; ESP downloads (needs STA).
/// Start command can be sent over Wi‑Fi HTTP or BLE; progress is read over HTTP.
class FirmwareOtaRunner {
  /// `null` = success (or connection lost after reboot). Otherwise an error message.
  ///
  /// [boardHttpBaseUrlOverride] — např. `http://192.168.4.1` na hotspotu; když chybí,
  /// použije se session/prefs. Musí sedět s `POST /api/system/ota` (transport Wi‑Fi).
  static Future<String?> execute({
    required WidgetRef ref,
    required String binUrl,
    required void Function(int percent) onProgress,
    String? boardHttpBaseUrlOverride,
  }) async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final baseUrl = normalizeBoardHttpBaseUrl(boardHttpBaseUrlOverride) ??
        resolveBoardHttpBaseUrl(
          wifiTransportActive: session.transport == BoardTransport.wifi,
          sessionWifiBaseUrl: session.wifiBaseUrl,
          prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
          bleStaIp: session.bleStaIp,
        );
    if (baseUrl == null || baseUrl.isEmpty) {
      return 'Board HTTP URL is missing (AP or STA IP). '
          'Save it under Default board URL — BLE alone is not enough for status.';
    }

    final api = ref.read(boardApiClientProvider);
    try {
      final fw = await api.fetchBoardFirmwareInfo(baseUrl);
      if (fw.otaSupported == false) {
        return 'This board build has no OTA slots (ota_0 + ota_1). '
            'Reflash with a dual-OTA partition CSV or update via USB.';
      }
    } catch (_) {}

    final remoteHttps = binUrl.trim().toLowerCase().startsWith('https://');
    if (remoteHttps) {
      try {
        final w = await api.fetchWiFiStatus(baseUrl);
        if (!w.staConnected) {
          return 'Connect the board to Wi‑Fi as a station (STA) so it can download '
              'the firmware from the internet over HTTPS.';
        }
      } catch (e) {
        return 'Could not verify Wi‑Fi: $e';
      }
    }

    await WakelockPlus.enable();
    try {
      try {
        await ref
            .read(boardSessionNotifierProvider.notifier)
            .requestFirmwareOta(binUrl, httpBoardBaseUrl: baseUrl);
      } catch (e) {
        return '$e';
      }

      return await _pollOta(ref, baseUrl, onProgress);
    } finally {
      await WakelockPlus.disable();
    }
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
    return 'Timed out waiting for OTA to finish.';
  }
}
