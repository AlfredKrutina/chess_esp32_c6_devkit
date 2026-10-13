import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import '../../app_providers.dart';
import '../../core/localization/locale_bridge.dart';
import '../../core/models/board_firmware_models.dart';
import '../../core/services/board_api_client.dart';
import '../../core/services/board_api_exception.dart';
import '../../core/services/firmware_phone_host_ota.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../l10n/app_localizations.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// OTA over HTTPS: CzechMate sends only the `.bin` URL; ESP downloads (needs STA).
/// Start command can be sent over Wi‑Fi HTTP or BLE; progress is read over HTTP.
class FirmwareOtaRunner {
  /// `null` = the new firmware booted and confirmed a stable start. Otherwise a
  /// user-facing message (rolled back, aborted, error, or outcome unknown).
  ///
  /// [boardHttpBaseUrlOverride] — eg `http://192.168.4.1` on hotspot; when missing
  /// session/prefs will be used. Must sit with `POST /api/system/ota` (Wi‑Fi transport).
  ///
  /// [preferHttpOtaStartCommand] — always `true` when this device hosts .bin: OTA start command
  /// sends direct HTTP to the board (works on AP even if BLE transport is active).
  static Future<String?> execute({
    required WidgetRef ref,
    required String binUrl,
    required void Function(int percent) onProgress,
    String? boardHttpBaseUrlOverride,
    bool preferHttpOtaStartCommand = false,
    void Function(String detail)? onDetail,
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
    /* Device on the board Wi‑Fi hotspot (192.168.4.x) — API is at gateway 192.168.4.1:80,
     * not a stale STA URL from prefs (same logic as hosted OTA in the UI). */
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

    int? baselineSeq;
    try {
      baselineSeq = (await api.fetchBoardOtaStatus(baseUrl)).seq;
    } catch (_) {}

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

      return await _pollOta(
        ref,
        baseUrl,
        onProgress,
        baselineSeq: baselineSeq,
        onDetail: onDetail,
      );
    } finally {
      await WakelockPlus.disable();
    }
  }

  /// User-facing result of a final board OTA state: `null` only for a confirmed `success`.
  static String? outcomeMessage(AppLocalizations strings, BoardOtaStatus? s) {
    if (s == null) {
      return strings.errOtaOutcomeUnknown;
    }
    final why = s.reason.isNotEmpty
        ? s.reason
        : (s.message.trim().isNotEmpty ? s.message.trim() : s.state);
    switch (s.state) {
      case BoardOtaState.success:
        return null;
      case BoardOtaState.rolledBack:
        return strings.errOtaRolledBack(why);
      case BoardOtaState.aborted:
        return strings.errOtaAborted(why);
      case BoardOtaState.error:
        return strings.errOtaBoardReported(why);
      default:
        return strings.errOtaOutcomeUnknown;
    }
  }

  /// Polls `GET /api/system/ota/status` until the board reports a final state of
  /// *this* attempt (`seq` differs from [baselineSeq]). Success is returned only after
  /// the new image confirmed its start; while the board reboots (HTTP gone, e.g. the
  /// hotspot is off after restart) the state is read over BLE when a board is saved.
  static Future<String?> _pollOta(
    WidgetRef ref,
    String baseUrl,
    void Function(int percent) onProgress, {
    int? baselineSeq,
    void Function(String detail)? onDetail,
  }) async {
    final api = ref.read(boardApiClientProvider);
    final strings = appStringsForPrefs(ref.read(prefsRepositoryProvider));
    final session = ref.read(boardSessionNotifierProvider.notifier);
    var sawStatusOk = false;
    var sawTransfer = false;
    var lostAfterTransfer = false;
    var waitingNoticeShown = false;

    const tickMs = 500;
    const maxTicks = 1200;
    const bleProbeEveryTicks = 8;

    /// [_OtaVerdict] or a final error message.
    Object evaluate(BoardOtaStatus s) {
      if (!s.isGuardFirmware) {
        switch (s.state) {
          case 'error':
            final m = s.message.trim();
            return strings.errOtaBoardReported(m.isEmpty ? 'error' : m);
          case 'downloading':
          case 'done':
            sawTransfer = true;
            return _OtaVerdict.keepPolling;
          default:
            return lostAfterTransfer
                ? _OtaVerdict.legacyAfterReboot
                : _OtaVerdict.keepPolling;
        }
      }
      if (s.seq == baselineSeq && baselineSeq != null) {
        return _OtaVerdict.keepPolling;
      }
      switch (s.state) {
        case BoardOtaState.receiving:
        case BoardOtaState.verifying:
        case BoardOtaState.rebooting:
          sawTransfer = true;
          if (s.awaitingConfirm && !waitingNoticeShown) {
            waitingNoticeShown = true;
            onDetail?.call(strings.firmwareOtaWaitingConfirmDetail);
          }
          return _OtaVerdict.keepPolling;
        case BoardOtaState.idle:
          return _OtaVerdict.keepPolling;
        default:
          final msg = outcomeMessage(strings, s);
          return msg ?? _OtaVerdict.success;
      }
    }

    for (var i = 0; i < maxTicks; i++) {
      await Future<void>.delayed(const Duration(milliseconds: tickMs));
      Object verdict = _OtaVerdict.keepPolling;
      try {
        final s = await api.fetchBoardOtaStatus(baseUrl);
        sawStatusOk = true;
        onProgress(s.percent.clamp(0, 100));
        verdict = evaluate(s);
      } catch (_) {
        if (sawTransfer) {
          lostAfterTransfer = true;
          if (i % bleProbeEveryTicks == 0) {
            try {
              final b = await session.probeOtaStatusViaBle();
              if (b != null) {
                final ota = b.ota;
                verdict = ota == null
                    ? _OtaVerdict.legacyAfterReboot
                    : evaluate(ota);
              }
            } catch (_) {}
          }
        } else if (!sawStatusOk && i >= 47) {
          return strings.errOtaPollUnreachable;
        }
      }
      if (verdict == _OtaVerdict.success) {
        return null;
      }
      if (verdict == _OtaVerdict.legacyAfterReboot) {
        return _legacyOutcome(api, baseUrl, strings);
      }
      if (verdict is String) {
        return verdict;
      }
    }
    return sawTransfer ? strings.errOtaOutcomeUnknown : strings.errOtaPollTimeout;
  }


  /// Board came back with firmware that has no OTA guard: it cannot report a confirmed
  /// start, so only a detected bootloader rollback is a definite answer.
  static Future<String?> _legacyOutcome(
    BoardApiClient api,
    String baseUrl,
    AppLocalizations strings,
  ) async {
    try {
      final fw = await api.fetchBoardFirmwareInfo(baseUrl);
      if (fw.otaLastBootFailed == true) {
        return strings.errOtaRolledBack(
          fw.otaFailedFirmwareVersion ?? 'bootloader_rollback',
        );
      }
    } catch (_) {}
    return strings.errOtaOutcomeUnknown;
  }
}

enum _OtaVerdict { keepPolling, success, legacyAfterReboot }
