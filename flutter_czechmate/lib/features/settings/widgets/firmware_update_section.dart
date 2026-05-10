import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/constants/app_environment.dart';
import '../../../core/constants/firmware_defaults.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../l10n/app_localizations.dart';
import '../../../core/models/board_firmware_models.dart';
import '../../../core/models/board_timer_state.dart';
import '../../../core/services/firmware_phone_host_ota.dart';
import '../../../core/utils/board_http_base_url.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../../core/utils/phone_wifi_info.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../../connection/widgets/board_wifi_provision_fields.dart';
import '../firmware_ota_runner.dart';
import '../firmware_update_availability.dart';
import 'ota_https_sta_gate_dialog.dart';

/// OTA: manifest z Gitu v aplikaci; stažení .bin na telefon + přenos na desku přes hotspot HTTP, LAN nebo čistě BLE.
class FirmwareUpdateSection extends ConsumerStatefulWidget {
  const FirmwareUpdateSection({super.key});

  @override
  ConsumerState<FirmwareUpdateSection> createState() =>
      _FirmwareUpdateSectionState();
}

class _FirmwareUpdateSectionState extends ConsumerState<FirmwareUpdateSection>
    with WidgetsBindingObserver {
  late final TextEditingController _manifestCtrl;
  late final TextEditingController _wifiSsidCtrl;
  late final TextEditingController _wifiPwdCtrl;
  bool _ctrlReady = false;

  EspWifiStatus? _wifiStatus;
  String? _detail;
  bool _otaBusy = false;
  int _otaPercent = 0;

  /// BLE stream OTA (`uploadFirmwareOtaBle`) — při přechodu do pozadí upozorníme po návratu.
  bool _bleStreamOtaWatchBackground = false;
  bool _pendingBleBackgroundNotice = false;
  File? _cachedOtaFile;
  String? _cachedOtaVersion;
  bool _downloadBusy = false;
  bool _wifiSurveyBusy = false;
  bool? _wifiSurveyVisible;
  String? _wifiPhoneGuess;

  String _firmwareTransportLabel(AppLocalizations l10n, BoardTransport t) {
    switch (t) {
      case BoardTransport.none:
        return l10n.firmwareTransportLabelNone;
      case BoardTransport.wifi:
        return l10n.firmwareTransportLabelWifi;
      case BoardTransport.ble:
        return l10n.firmwareTransportLabelBle;
      case BoardTransport.mock:
        return l10n.firmwareTransportLabelMock;
    }
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _manifestCtrl = TextEditingController();
    _wifiSsidCtrl = TextEditingController();
    _wifiPwdCtrl = TextEditingController();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      final prefs = ref.read(prefsRepositoryProvider);
      final saved = prefs.firmwareManifestUrl?.trim();
      if (saved != null && saved.isNotEmpty) {
        final fixed = normalizeFirmwareManifestUrl(saved);
        if (fixed != saved) {
          await prefs.setFirmwareManifestUrl(fixed);
        }
      }
      final prefsAfter = ref.read(prefsRepositoryProvider);
      _manifestCtrl.text = prefsAfter.firmwareManifestUrlEffective;
      setState(() => _ctrlReady = true);
      await _syncFirmwareCacheWithPrefs();
      final guess = await PhoneWifiInfo.tryCurrentWifiSsid();
      if (mounted && guess != null && _wifiSsidCtrl.text.isEmpty) {
        _wifiSsidCtrl.text = guess;
        _wifiPhoneGuess = guess;
        setState(() {});
      }
      unawaited(_reloadWifi());
      unawaited(
        ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(),
      );
    });
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _manifestCtrl.dispose();
    _wifiSsidCtrl.dispose();
    _wifiPwdCtrl.dispose();
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);
    if (!_bleStreamOtaWatchBackground || !_otaBusy) {
      return;
    }
    switch (state) {
      case AppLifecycleState.inactive:
      case AppLifecycleState.hidden:
      case AppLifecycleState.paused:
        _pendingBleBackgroundNotice = true;
        break;
      case AppLifecycleState.resumed:
        unawaited(_syncFirmwareCacheWithPrefs());
        if (_pendingBleBackgroundNotice && mounted) {
          _pendingBleBackgroundNotice = false;
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content:
                  Text(context.l10n.firmwareBleOtaReturnedFromBackgroundSnack),
            ),
          );
        }
        break;
      case AppLifecycleState.detached:
        break;
    }
  }

  String? _resolvedBoardHttpUrl(BoardSessionState session) {
    return resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: ref.read(prefsRepositoryProvider).lastBoardBaseUrl,
      bleStaIp: session.bleStaIp,
    );
  }

  Future<void> _sendWifiCredentialsToBoard() async {
    final ssid = _wifiSsidCtrl.text.trim();
    if (ssid.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.errWifiSsidEmpty)),
      );
      return;
    }
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .provisionStaWifiOverBle(
            ssid: ssid,
            password: _wifiPwdCtrl.text,
          );
      if (!mounted) {
        return;
      }
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.firmwareWifiBleProvStartedSnack)),
      );
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(userFacingErrorSummary(l10n, e))),
        );
      }
    }
  }

  Future<void> _onFirmwareUsePhoneSsid() async {
    final s = await PhoneWifiInfo.tryCurrentWifiSsid();
    if (!mounted) return;
    setState(() {
      _wifiPhoneGuess = s;
      if (s != null) {
        _wifiSsidCtrl.text = s;
      }
      _wifiSurveyVisible = null;
    });
  }

  Future<void> _scanWifiSurveyFirmware() async {
    final snap = ref.read(firmwareUpdateAvailabilityProvider);
    if (snap.loading || _otaBusy || _downloadBusy) return;
    setState(() {
      _wifiSurveyBusy = true;
      _wifiSurveyVisible = null;
    });
    try {
      final r = await ref
          .read(boardSessionNotifierProvider.notifier)
          .fetchWifiSurveyOverBle();
      if (!mounted) return;
      final phone = _wifiSsidCtrl.text.trim().isNotEmpty
          ? _wifiSsidCtrl.text.trim()
          : (_wifiPhoneGuess ?? '');
      if (!r.ok) {
        setState(() => _wifiSurveyVisible = null);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.boardWifiProvisionSurveyFailed)),
        );
        return;
      }
      setState(() {
        _wifiSurveyVisible = phone.isNotEmpty ? r.hasSsid(phone) : false;
      });
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(userFacingErrorSummary(l10n, e))),
        );
      }
    } finally {
      if (mounted) setState(() => _wifiSurveyBusy = false);
    }
  }

  Future<void> _reloadWifi() async {
    final session = ref.read(boardSessionNotifierProvider);
    final baseUrl = _resolvedBoardHttpUrl(session);
    if (baseUrl == null || baseUrl.isEmpty) {
      return;
    }
    try {
      final w = await ref.read(boardApiClientProvider).fetchWiFiStatus(baseUrl);
      if (mounted) {
        setState(() => _wifiStatus = w);
      }
    } catch (_) {}
  }

  Future<void> _syncFirmwareCacheWithPrefs() async {
    final prefs = ref.read(prefsRepositoryProvider);
    final path = prefs.firmwareCachedBinPath;
    final ver = prefs.firmwareCachedVersion;
    if (path == null || ver == null || path.isEmpty || ver.isEmpty) {
      if (mounted && (_cachedOtaFile != null || _cachedOtaVersion != null)) {
        setState(() {
          _cachedOtaFile = null;
          _cachedOtaVersion = null;
        });
      }
      return;
    }
    final f = File(path);
    if (!await f.exists()) {
      await prefs.clearFirmwareCachedBin();
      if (mounted) {
        setState(() {
          _cachedOtaFile = null;
          _cachedOtaVersion = null;
        });
      }
      return;
    }
    if (!mounted) {
      return;
    }
    if (_cachedOtaFile?.path != path || _cachedOtaVersion != ver) {
      setState(() {
        _cachedOtaFile = f;
        _cachedOtaVersion = ver;
      });
    }
  }

  Future<void> _clearPersistedFirmwareCache() async {
    final prefs = ref.read(prefsRepositoryProvider);
    final path = prefs.firmwareCachedBinPath;
    if (path != null && path.isNotEmpty) {
      try {
        final f = File(path);
        if (await f.exists()) {
          await f.delete();
        }
      } catch (_) {}
    }
    await prefs.clearFirmwareCachedBin();
    if (mounted) {
      setState(() {
        _cachedOtaFile = null;
        _cachedOtaVersion = null;
      });
    }
  }

  Future<void> _saveManifestUrl() async {
    await ref
        .read(prefsRepositoryProvider)
        .setFirmwareManifestUrl(_manifestCtrl.text.trim());
    final effective =
        ref.read(prefsRepositoryProvider).firmwareManifestUrlEffective;
    if (mounted) {
      setState(() => _manifestCtrl.text = effective);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.firmwareManifestSavedSnack)),
      );
    }
    unawaited(ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh());
  }

  Future<void> _checkUpdate() async {
    final session = ref.read(boardSessionNotifierProvider);
    final baseUrl = _resolvedBoardHttpUrl(session);
    final typed = _manifestCtrl.text.trim();
    final skipBoardHttp = baseUrl == null || baseUrl.isEmpty;

    if (mounted) {
      setState(() => _detail = null);
    }

    await ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(
          manifestUrlOverride: typed.isEmpty ? null : typed,
          skipBoardHttpFetch: skipBoardHttp,
        );
    final snap = ref.read(firmwareUpdateAvailabilityProvider);

    if (mounted &&
        snap.manifest != null &&
        _cachedOtaVersion != null &&
        snap.manifest!.version != _cachedOtaVersion) {
      await _clearPersistedFirmwareCache();
    }

    if (mounted) {
      setState(() => _detail = snap.error);
    }
    if (AppEnvironment.staging) {
      debugPrint(
        '[staging] firmware check manifest=${snap.manifest?.version} board=${snap.boardVersion}',
      );
    }
    await _reloadWifi();
  }

  Future<bool> _confirmOtaFromSettings({
    required String remoteVersion,
    required String boardVersion,
    String? changelog,
  }) async {
    final first = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final l10n = AppLocalizations.of(ctx)!;
        return AlertDialog(
          title: Text(l10n.firmwareUpdateBoardTitle),
          content: SingleChildScrollView(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(l10n.firmwareNewVersionLine(remoteVersion, boardVersion)),
                if (changelog != null && changelog.trim().isNotEmpty) ...[
                  const SizedBox(height: 12),
                  Text(changelog.trim(),
                      style: Theme.of(ctx).textTheme.bodySmall),
                ],
                const SizedBox(height: 12),
                Text(l10n.firmwareHttpsLinkExplainBody),
                const SizedBox(height: 12),
                Text(l10n.firmwareOtaHttpMayLeaveAppHint),
              ],
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(l10n.commonCancel),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(l10n.commonContinue),
            ),
          ],
        );
      },
    );
    if (first != true || !mounted) {
      return false;
    }
    final second = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final l10n = AppLocalizations.of(ctx)!;
        return AlertDialog(
          title: Text(l10n.firmwareFinalConfirmTitle),
          content: Text(l10n.firmwareSettingsSecondConfirmBody),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(l10n.commonNo),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(l10n.firmwareYesUpdate),
            ),
          ],
        );
      },
    );
    return second == true;
  }

  Future<void> _downloadFirmwareToApp(FirmwareManifest meta) async {
    setState(() {
      _downloadBusy = true;
      _detail = null;
    });
    try {
      final f = await FirmwarePhoneHostOta.downloadBinForOta(
        httpsBinUrl: meta.url,
        version: meta.version,
      );
      await ref.read(prefsRepositoryProvider).setFirmwareCachedBin(
            absolutePath: f.path,
            version: meta.version,
          );
      if (!mounted) {
        return;
      }
      setState(() {
        _cachedOtaFile = f;
        _cachedOtaVersion = meta.version;
        _downloadBusy = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.firmwareDownloadSavedSnack)),
      );
    } catch (e) {
      if (!mounted) {
        return;
      }
      final l10n = context.l10n;
      setState(() {
        _downloadBusy = false;
        _detail = l10n.firmwareDownloadFailedLine(
          userFacingErrorSummary(l10n, e),
        );
      });
    }
  }

  Future<bool> _confirmSendFromPhone() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(context.l10n.firmwareSendToBoardTitle),
        content: Text(context.l10n.firmwareSendToBoardBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(MaterialLocalizations.of(ctx).cancelButtonLabel),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(context.l10n.firmwareYesUpdate),
          ),
        ],
      ),
    );
    return ok == true;
  }

  Future<void> _sendFirmwareToBoard(FirmwareManifest meta) async {
    final file = _cachedOtaFile;
    if (file == null || !await file.exists()) {
      setState(() => _detail = context.l10n.firmwareNoCachedFirmware);
      return;
    }
    final session = ref.read(boardSessionNotifierProvider);
    final onApSubnet = await FirmwarePhoneHostOta.ipv4OnBoardApSubnet() != null;
    final boardStaReady = session.bleStaConnected &&
        session.bleStaIp != null &&
        session.bleStaIp!.trim().isNotEmpty;
    final phoneOnStaSubnet = boardStaReady &&
        await FirmwarePhoneHostOta.ipv4OnSameSubnet24As(
              session.bleStaIp!.trim(),
            ) !=
            null;

    var baseUrl = _resolvedBoardHttpUrl(session);
    if (baseUrl == null || baseUrl.isEmpty) {
      if (boardStaReady) {
        baseUrl =
            normalizeBoardHttpBaseUrl('http://${session.bleStaIp!.trim()}');
      }
    }
    /* Na subnetu AP je HTTP API desky vždy na 192.168.4.1:80 — přepsat špatnou URL
     * ze session/prefs (např. omylem stejný port jako HttpServer telefonu). */
    if (onApSubnet) {
      baseUrl = normalizeBoardHttpBaseUrl('http://192.168.4.1');
    }

    final canHttpPhoneHost = onApSubnet || phoneOnStaSubnet;

    if (!mounted) {
      return;
    }
    if (!canHttpPhoneHost) {
      if (session.transport == BoardTransport.ble && session.bleGattConnected) {
        final msg = context.l10n.firmwareOtaNoLanRouteUseBle;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(msg)),
        );
        await _sendFirmwareViaBle(meta);
        return;
      }
      setState(() => _detail = context.l10n.firmwareOtaNoLanRouteNeedBle);
      return;
    }

    if (baseUrl == null || baseUrl.isEmpty) {
      setState(() => _detail = context.l10n.firmwareBoardHttpMissingDetail);
      return;
    }

    if (!await _confirmSendFromPhone() || !mounted) {
      return;
    }

    setState(() {
      _otaBusy = true;
      _otaPercent = 0;
      _detail = null;
    });
    HttpServer? server;
    var phoneHostOtaSucceeded = false;
    try {
      final binding = await FirmwarePhoneHostOta.startServingBin(
        file,
        boardStaIpForSubnet:
            phoneOnStaSubnet && !onApSubnet ? session.bleStaIp : null,
      );
      server = binding.server;
      final err = await FirmwareOtaRunner.execute(
        ref: ref,
        binUrl: binding.otaUrl,
        boardHttpBaseUrlOverride: baseUrl,
        preferHttpOtaStartCommand: true,
        onProgress: (p) {
          if (mounted) {
            setState(() => _otaPercent = p);
          }
        },
      );
      if (!mounted) {
        return;
      }
      if (err != null) {
        setState(() => _detail = err);
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(err)));
      } else {
        phoneHostOtaSucceeded = true;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(context.l10n.firmwareOtaFinishedMaybeRebootSnack),
          ),
        );
      }
    } catch (e) {
      if (!mounted) {
        return;
      }
      final l10n = context.l10n;
      final raw = e.toString();
      final lanFail = raw.contains('NOT_ON_OTA_LAN');
      final friendly = userFacingErrorSummary(l10n, e);
      setState(
          () => _detail = lanFail ? l10n.firmwareOtaPhoneNotOnLan : friendly);
      if (lanFail) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(l10n.firmwareOtaPhoneNotOnLan)),
        );
      }
    } finally {
      await server?.close(force: true);
      if (mounted) {
        setState(() => _otaBusy = false);
      }
    }
    if (phoneHostOtaSucceeded) {
      unawaited(
        ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(
              skipBoardHttpFetch: true,
              afterBleOtaAssumeBoardMatchesManifest: true,
            ),
      );
    }
  }

  Future<bool> _confirmSendViaBle() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(context.l10n.firmwareSendViaBle),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(context.l10n.firmwareSendViaBleBody),
              const SizedBox(height: 12),
              Text(
                context.l10n.firmwareBleOtaKeepForegroundWarning,
                style: Theme.of(ctx).textTheme.bodyMedium?.copyWith(
                      fontWeight: FontWeight.w600,
                    ),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(MaterialLocalizations.of(ctx).cancelButtonLabel),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(context.l10n.firmwareYesUpdate),
          ),
        ],
      ),
    );
    return ok == true;
  }

  Future<void> _sendFirmwareViaBle(FirmwareManifest meta) async {
    final file = _cachedOtaFile;
    if (file == null || !await file.exists()) {
      setState(() => _detail = context.l10n.firmwareNoCachedFirmware);
      return;
    }
    final session = ref.read(boardSessionNotifierProvider);
    if (session.transport != BoardTransport.ble || !session.bleGattConnected) {
      setState(() => _detail = context.l10n.errOtaBleTransport);
      return;
    }
    if (!await _confirmSendViaBle() || !mounted) {
      return;
    }

    setState(() {
      _otaBusy = true;
      _otaPercent = 0;
      _detail = null;
      _bleStreamOtaWatchBackground = true;
      _pendingBleBackgroundNotice = false;
    });
    var bleStreamOtaSucceeded = false;
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .uploadFirmwareOtaBle(
        file,
        onProgress: (p) {
          if (mounted) {
            setState(() => _otaPercent = p);
          }
        },
        onBleOtaPhase: (phase) {
          if (!mounted) return;
          if (phase == 'paused_waiting_reconnect') {
            setState(() {
              _detail = context.l10n.firmwareBleOtaPausedReconnectDetail;
            });
          } else if (phase == 'resumed') {
            setState(() {
              _detail = context.l10n.firmwareBleOtaResumedTransferDetail;
            });
          }
        },
      );
      if (!mounted) {
        return;
      }
      bleStreamOtaSucceeded = true;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.firmwareBleUploadDoneSnack)),
      );
    } catch (e) {
      if (!mounted) {
        return;
      }
      final l10n = context.l10n;
      final friendly = userFacingErrorSummary(l10n, e);
      setState(() => _detail = friendly);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(friendly)),
      );
    } finally {
      if (mounted) {
        setState(() {
          _otaBusy = false;
          _bleStreamOtaWatchBackground = false;
          _pendingBleBackgroundNotice = false;
        });
      }
    }
    if (bleStreamOtaSucceeded) {
      unawaited(
        ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(
              skipBoardHttpFetch: true,
              afterBleOtaAssumeBoardMatchesManifest: true,
            ),
      );
    }
  }

  Future<void> _startOta(String binUrl, FirmwareManifest meta) async {
    final snap = ref.read(firmwareUpdateAvailabilityProvider);
    final boardV = snap.boardVersion ?? '';
    if (!await _confirmOtaFromSettings(
      remoteVersion: meta.version,
      boardVersion: boardV,
      changelog: meta.changelog,
    )) {
      return;
    }

    final remoteHttps = binUrl.trim().toLowerCase().startsWith('https://');
    if (remoteHttps && mounted) {
      final sessionForGate = ref.read(boardSessionNotifierProvider);
      final prefsForGate = ref.read(prefsRepositoryProvider);
      final baseGate = resolveBoardHttpBaseUrl(
        wifiTransportActive: sessionForGate.transport == BoardTransport.wifi,
        sessionWifiBaseUrl: sessionForGate.wifiBaseUrl,
        prefsLastBoardBaseUrl: prefsForGate.lastBoardBaseUrl,
        bleStaIp: sessionForGate.bleStaIp,
      );
      if (baseGate != null && baseGate.isNotEmpty) {
        try {
          final w =
              await ref.read(boardApiClientProvider).fetchWiFiStatus(baseGate);
          if (!w.staConnected && mounted) {
            final bleFlash = sessionForGate.transport == BoardTransport.ble &&
                sessionForGate.bleGattConnected;
            final choice = await showOtaHttpsStaGateDialog(
              context,
              bleUploadAvailable: bleFlash,
            );
            if (!mounted) return;
            if (choice == null || choice == OtaHttpsStaGateChoice.abort) {
              return;
            }
            if (choice == OtaHttpsStaGateChoice.bleUpload) {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text(context.l10n.otaHttpsBleUploadHintSnack),
                ),
              );
              return;
            }
            if (choice == OtaHttpsStaGateChoice.wifiTips) {
              await showOtaHttpsWifiTipsDialog(context);
              return;
            }
            if (choice == OtaHttpsStaGateChoice.hotspotBle) {
              try {
                await ref
                    .read(boardSessionNotifierProvider.notifier)
                    .setBoardHotspotEnabled(true);
                if (mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(
                      content: Text(context.l10n.otaHttpsHotspotEnabledSnack),
                    ),
                  );
                }
              } catch (e) {
                if (mounted) {
                  final l10n = context.l10n;
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(
                      content: Text(userFacingErrorSummary(l10n, e)),
                    ),
                  );
                }
              }
              return;
            }
          }
        } catch (_) {}
      }
    }

    setState(() {
      _otaBusy = true;
      _otaPercent = 0;
      _detail = null;
    });

    final err = await FirmwareOtaRunner.execute(
      ref: ref,
      binUrl: binUrl,
      onProgress: (p) {
        if (mounted) {
          setState(() => _otaPercent = p);
        }
      },
    );

    if (mounted) {
      setState(() => _otaBusy = false);
      if (err != null) {
        setState(() => _detail = err);
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(err)));
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(context.l10n.firmwareOtaFinishedMaybeRebootSnack),
          ),
        );
      }
    }
    if (err == null) {
      unawaited(
        ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(
              skipBoardHttpFetch: true,
              afterBleOtaAssumeBoardMatchesManifest: true,
            ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    if (!_ctrlReady) {
      return const SizedBox.shrink();
    }

    final session = ref.watch(boardSessionNotifierProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final snap = ref.watch(firmwareUpdateAvailabilityProvider);
    final devMode = prefs.developerModeUnlocked;

    final baseOk = _resolvedBoardHttpUrl(session) != null;
    final boardV = snap.boardVersion ?? '—';
    final remoteV = snap.manifest?.version ?? '—';
    final updateAvailable = snap.updateAvailable;
    final showOtaGit = snap.showOtaFromGitWithDeveloper(devMode);
    final devSameReflash = devMode &&
        snap.sameSemverAsManifest &&
        snap.hasBoardVersion &&
        !updateAvailable;
    final remindersOn = prefs.firmwareUpdateRemindersEnabled;
    final otaCapable = snap.boardOtaSupported != false;
    final bleFlashPrimary =
        session.transport == BoardTransport.ble && session.bleGattConnected;

    final theme = Theme.of(context);

    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (updateAvailable)
            Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Material(
                color:
                    theme.colorScheme.primaryContainer.withValues(alpha: 0.35),
                borderRadius: BorderRadius.circular(12),
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Icon(
                        Icons.system_update_alt_outlined,
                        color: theme.colorScheme.primary,
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          '${context.l10n.firmwareTileTitleUpdateAvailable(remoteV)}\n${context.l10n.firmwareTwoStepOtaHint} (${remindersOn ? context.l10n.firmwareRemindersOnShort : context.l10n.firmwareRemindersOffShort}).',
                          style: theme.textTheme.bodySmall,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          if (devSameReflash && snap.manifest != null)
            Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Material(
                color: theme.colorScheme.tertiaryContainer
                    .withValues(alpha: 0.45),
                borderRadius: BorderRadius.circular(12),
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Icon(
                        Icons.build_circle_outlined,
                        color: theme.colorScheme.onTertiaryContainer,
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          context.l10n.firmwareDeveloperSameVersionBanner,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: theme.colorScheme.onTertiaryContainer,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(context.l10n.firmwareDailyRemindersTitle),
            subtitle: Text(context.l10n.firmwareDailyRemindersSubtitleLong),
            value: remindersOn,
            onChanged: (v) async {
              await prefs.setFirmwareUpdateRemindersEnabled(v);
              setState(() {});
            },
          ),
          const SizedBox(height: 8),
          FilledButton(
            onPressed:
                snap.loading || _otaBusy || _downloadBusy ? null : _checkUpdate,
            style: FilledButton.styleFrom(
              minimumSize: const Size(double.infinity, 48),
            ),
            child: snap.loading
                ? const SizedBox(
                    width: 22,
                    height: 22,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : Text(
                    context.l10n.firmwareCheckForUpdate,
                    textAlign: TextAlign.center,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
          ),
          if (devMode) ...[
            const SizedBox(height: 16),
            Text(
              context.l10n.firmwareDeveloperManifestSectionTitle,
              style: theme.textTheme.titleSmall
                  ?.copyWith(fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 6),
            Text(
              context.l10n.firmwareDeveloperManifestSectionBody,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: _manifestCtrl,
              decoration: InputDecoration(
                labelText: context.l10n.firmwareManifestUrlLabel,
                hintText: context.l10n.firmwareManifestUrlHint,
                helperText: context.l10n.firmwareManifestUrlHelper,
                border: const OutlineInputBorder(),
              ),
              keyboardType: TextInputType.url,
            ),
            const SizedBox(height: 8),
            FilledButton.tonal(
              onPressed: snap.loading || _otaBusy || _downloadBusy
                  ? null
                  : _saveManifestUrl,
              style: FilledButton.styleFrom(
                minimumSize: const Size(double.infinity, 44),
              ),
              child: Text(
                context.l10n.firmwareSaveManifestUrl,
                textAlign: TextAlign.center,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
              ),
            ),
          ],
          const SizedBox(height: 12),
          Text(
            context.l10n.firmwareBoardHttpVersionLine(boardV),
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          Text(
            context.l10n.firmwareManifestVersionLine(remoteV),
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          if (_wifiStatus != null)
            Text(
              context.l10n.firmwareWifiStaLine(
                _wifiStatus!.staConnected
                    ? context.l10n.firmwareWifiStaConnected(
                        _wifiStatus!.staIp,
                      )
                    : context.l10n.firmwareWifiStaNotConnected,
              ),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          if (session.transport == BoardTransport.ble &&
              session.bleGattConnected) ...[
            const SizedBox(height: 16),
            BoardWifiProvisionFields(
              ssidController: _wifiSsidCtrl,
              pwdController: _wifiPwdCtrl,
              onSendCredentials: snap.loading || _otaBusy || _downloadBusy
                  ? () {}
                  : _sendWifiCredentialsToBoard,
              onUsePhoneSsid: snap.loading || _otaBusy || _downloadBusy
                  ? () {}
                  : _onFirmwareUsePhoneSsid,
              onScanBoardNetworks: snap.loading || _otaBusy || _downloadBusy
                  ? null
                  : _scanWifiSurveyFirmware,
              scanBusy: _wifiSurveyBusy,
              sendBusy: snap.loading || _otaBusy || _downloadBusy,
              surveyPhoneVisible: _wifiSurveyVisible,
              surveyDisplaySsidForChip: _wifiSsidCtrl.text.trim().isNotEmpty
                  ? _wifiSsidCtrl.text.trim()
                  : _wifiPhoneGuess,
              actionsEnabled: !snap.loading && !_otaBusy && !_downloadBusy,
              denseSubtitle: false,
            ),
            Padding(
              padding: const EdgeInsets.only(top: 6),
              child: Text(
                context.l10n.boardWifiProvisionFirmwareContextHint,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
              ),
            ),
            const SizedBox(height: 8),
            Text(
              session.bleStaConnected
                  ? context.l10n.firmwareBleStaOk(
                      session.bleStaSsid ?? '—',
                      session.bleStaIp ?? '—',
                    )
                  : context.l10n.firmwareBleStaWaiting,
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
          if (!baseOk && !showOtaGit)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                context.l10n.firmwareNeedDefaultBoardUrlHint,
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            ),
          if (!baseOk && showOtaGit)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                context.l10n.firmwareBleHttpOptionalHint,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
              ),
            ),
          if (snap.boardOtaSupported == false) ...[
            const SizedBox(height: 12),
            Material(
              color: Theme.of(context)
                  .colorScheme
                  .errorContainer
                  .withValues(alpha: 0.5),
              borderRadius: BorderRadius.circular(8),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Text(
                  context.l10n.firmwareOtaSlotsDisabledHint,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
            ),
          ],
          if (showOtaGit && snap.manifest != null) ...[
            const SizedBox(height: 12),
            Material(
              color: Theme.of(context)
                  .colorScheme
                  .primaryContainer
                  .withValues(alpha: 0.35),
              borderRadius: BorderRadius.circular(12),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    if (!snap.hasBoardVersion)
                      Padding(
                        padding: const EdgeInsets.only(bottom: 8),
                        child: Text(
                          context.l10n.firmwareBleGitUnknownBoardVersion,
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Icon(Icons.system_update_alt,
                            color: Theme.of(context).colorScheme.primary),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                updateAvailable
                                    ? context.l10n.firmwareNewVersionChip(
                                        snap.manifest!.version,
                                      )
                                    : (devSameReflash
                                        ? context.l10n
                                            .firmwareDeveloperReflashChipTitle(
                                              snap.manifest!.version,
                                            )
                                        : context.l10n.firmwareNewVersionChip(
                                            snap.manifest!.version,
                                          )),
                                style: Theme.of(context).textTheme.titleSmall,
                              ),
                              const SizedBox(height: 6),
                              Text(
                                context.l10n.firmwareTwoStepOtaHint,
                                style: Theme.of(context).textTheme.bodySmall,
                              ),
                              if (snap.manifest!.changelog != null &&
                                  snap.manifest!.changelog!.trim().isNotEmpty)
                                Padding(
                                  padding: const EdgeInsets.only(top: 8),
                                  child: Text(
                                    snap.manifest!.changelog!.trim(),
                                    style:
                                        Theme.of(context).textTheme.bodySmall,
                                  ),
                                ),
                            ],
                          ),
                        ),
                      ],
                    ),
                    if (_cachedOtaFile != null &&
                        _cachedOtaVersion != null &&
                        _cachedOtaFile!.existsSync()) ...[
                      const SizedBox(height: 8),
                      Text(
                        context.l10n.firmwareCachedInAppLine(
                          _cachedOtaVersion!,
                          (_cachedOtaFile!.lengthSync() / (1024 * 1024))
                              .toStringAsFixed(1),
                        ),
                        style: Theme.of(context).textTheme.labelLarge,
                      ),
                    ],
                    const SizedBox(height: 12),
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        FilledButton.tonal(
                          onPressed: !otaCapable ||
                                  snap.loading ||
                                  _otaBusy ||
                                  _downloadBusy
                              ? null
                              : () => _downloadFirmwareToApp(snap.manifest!),
                          child: _downloadBusy
                              ? const SizedBox(
                                  width: 22,
                                  height: 22,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                  ),
                                )
                              : Text(
                                  context.l10n.firmwareDownloadToApp,
                                  textAlign: TextAlign.center,
                                  maxLines: 2,
                                  overflow: TextOverflow.ellipsis,
                                ),
                        ),
                        const SizedBox(height: 8),
                        if (bleFlashPrimary) ...[
                          FilledButton(
                            onPressed: !otaCapable ||
                                    snap.loading ||
                                    _otaBusy ||
                                    _downloadBusy ||
                                    _cachedOtaFile == null
                                ? null
                                : () => _sendFirmwareViaBle(snap.manifest!),
                            child: Text(
                              context.l10n.firmwareSendViaBlePrimary,
                              textAlign: TextAlign.center,
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          const SizedBox(height: 8),
                          FilledButton.tonal(
                            onPressed: !otaCapable ||
                                    snap.loading ||
                                    _otaBusy ||
                                    _downloadBusy ||
                                    _cachedOtaFile == null
                                ? null
                                : () => _sendFirmwareToBoard(snap.manifest!),
                            child: Text(
                              context.l10n.firmwareSendToBoardHttpAlt,
                              textAlign: TextAlign.center,
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                        ] else ...[
                          FilledButton(
                            onPressed: !otaCapable ||
                                    snap.loading ||
                                    _otaBusy ||
                                    _downloadBusy ||
                                    _cachedOtaFile == null
                                ? null
                                : () => _sendFirmwareToBoard(snap.manifest!),
                            child: Text(
                              context.l10n.firmwareSendToBoard,
                              textAlign: TextAlign.center,
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          const SizedBox(height: 8),
                          OutlinedButton.icon(
                            onPressed: !otaCapable ||
                                    snap.loading ||
                                    _otaBusy ||
                                    _downloadBusy ||
                                    _cachedOtaFile == null ||
                                    session.transport != BoardTransport.ble ||
                                    !session.bleGattConnected
                                ? null
                                : () => _sendFirmwareViaBle(snap.manifest!),
                            icon: const Icon(Icons.bluetooth),
                            label: Text(
                              context.l10n.firmwareSendViaBle,
                              textAlign: TextAlign.center,
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                        ],
                        TextButton(
                          onPressed: !otaCapable ||
                                  snap.loading ||
                                  _otaBusy ||
                                  _downloadBusy
                              ? null
                              : () =>
                                  _startOta(snap.manifest!.url, snap.manifest!),
                          child: Text(
                            context.l10n.firmwareOneStepHttpsOta,
                            textAlign: TextAlign.center,
                            maxLines: 3,
                            overflow: TextOverflow.ellipsis,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
          ],
          if (_otaBusy) ...[
            const SizedBox(height: 8),
            LinearProgressIndicator(
              value: _otaPercent > 0 ? _otaPercent / 100 : null,
            ),
            Text(
              context.l10n.firmwareOtaPercentLine(_otaPercent),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
          if (_detail != null) ...[
            const SizedBox(height: 8),
            Text(
              _detail!,
              style: TextStyle(color: Theme.of(context).colorScheme.error),
            ),
          ],
          const SizedBox(height: 8),
          Text(
            context.l10n.firmwareFooterTransportHint(
              _firmwareTransportLabel(context.l10n, session.transport),
            ),
            style: Theme.of(context).textTheme.labelSmall,
          ),
        ],
      ),
    );
  }
}
