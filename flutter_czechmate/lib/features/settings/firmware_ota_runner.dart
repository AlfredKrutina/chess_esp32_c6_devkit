import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import '../../app_providers.dart';
import '../../core/localization/locale_bridge.dart';
import '../../core/services/board_api_exception.dart';
import '../../core/services/firmware_phone_host_ota.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// OTA over HTTPS: CzechMate sends only the `.bin` URL; ESP downloads (needs STA).
/// Start command can be sent over Wi‑Fi HTTP or BLE; progress is read over HTTP.
class FirmwareOtaRunner {
  /// `null` = success (or connection lost after reboot). Otherwise an error message.
  ///
  /// [boardHttpBaseUrlOverride] — např. `http://192.168.4.1` na hotspotu; když chybí,
  /// použije se session/prefs. Musí sedět s `POST /api/system/ota` (transport Wi‑Fi).
  ///
  /// [preferHttpOtaStartCommand] — když toto zařízení hostuje .bin, vždy `true`: příkaz startu OTA
  /// pošle přímo HTTP na desku (funguje na AP i když je aktivní BLE transport).
  static Future<String?> execute({
    required WidgetRef ref,
    required String binUrl,
    required void Function(int percent) onProgress,
    String? boardHttpBaseUrlOverride,
    bool preferHttpOtaStartCommand = false,
  }) async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final strings = appStringsForPrefs(prefs);
    var baseUrl = normalizeBoardHttpBaseUrl(boardHttpBaseUrlOverride) ??
        resolveBoardHttpBaseUrl(
          wifiTransportActive: session.transport == BoardTransport.wifi,
          sessionWifiBaseUrl: session.wifiBaseUrl,
          prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
          bleStaIp: session.bleStaIp,
        );
    if (baseUrl == null || baseUrl.isEmpty) {
      return strings.errOtaBoardHttpMissingDetail;
    }
    /* Zařízení na Wi‑Fi hotspotu desky (192.168.4.x) — API je na gateway 192.168.4.1:80,
     * ne na zastaralé STA URL z prefs (stejná logika jako hostované OTA v UI). */
    if (await FirmwarePhoneHostOta.ipv4OnBoardApSubnet() != null) {
      final ap = normalizeBoardHttpBaseUrl('http://192.168.4.1');
      if (ap != null) {
        baseUrl = ap;
      }
    }

    final api = ref.read(boardApiClientProvider);
    try {
      final fw = await api.fetchBoardFirmwareInfo(baseUrl);
      if (fw.otaSupported == false) {
        return strings.errOtaNoOtaPartitions;
      }
    } catch (_) {}

    final binLower = binUrl.trim().toLowerCase();
    final remoteHttps = binLower.startsWith('https://');
    final remoteLanHttp = binLower.startsWith('http://');
    final usePreferHttpStart =
        preferHttpOtaStartCommand || remoteLanHttp;
    if (remoteHttps) {
      try {
        final w = await api.fetchWiFiStatus(baseUrl);
        if (!w.staConnected) {
          return strings.errOtaStaRequiredForHttps;
        }
      } catch (e) {
        return strings.errOtaWifiStatusCheckFailed(
          userFacingErrorSummary(strings, e),
        );
      }
    }

    await WakelockPlus.enable();
    try {
      try {
        await ref
            .read(boardSessionNotifierProvider.notifier)
            .requestFirmwareOta(
              binUrl,
              httpBoardBaseUrl: baseUrl,
              preferHttpOtaStart: usePreferHttpStart,
            );
      } on BoardApiException catch (e) {
        if (e.statusCode == 428) {
          return strings.errOtaStaRequiredForHttps;
        }
        final d = e.detail?.trim();
        if (d != null && d.isNotEmpty) {
          return '${e.message} ($d)';
        }
        return e.message;
      } catch (e) {
        return userFacingErrorSummary(strings, e);
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
    final strings = appStringsForPrefs(ref.read(prefsRepositoryProvider));
    var sawStatusOk = false;
    var lastWasDownloading = false;

    const tickMs = 500;
    const maxTicks = 1200;

    for (var i = 0; i < maxTicks; i++) {
      await Future<void>.delayed(const Duration(milliseconds: tickMs));
      try {
        final s = await api.fetchBoardOtaStatus(baseUrl);
        sawStatusOk = true;
        lastWasDownloading = s.state == 'downloading';
        onProgress(s.percent.clamp(0, 100));
        if (s.state == 'error') {
          final m = s.message.trim();
          return m.isEmpty
              ? strings.errOtaBoardReported('error')
              : strings.errOtaBoardReported(m);
        }
        if (s.state == 'done') {
          return null;
        }
      } catch (_) {
        if (sawStatusOk && lastWasDownloading && i >= 8) {
          return null;
        }
        if (!sawStatusOk && i >= 47) {
          return strings.errOtaPollUnreachable;
        }
      }
    }
    return strings.errOtaPollTimeout;
  }
}
