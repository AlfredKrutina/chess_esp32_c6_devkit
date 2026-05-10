import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/debug/connection_debug_log.dart';
import '../../core/platform/flutter_blue_plus_host_supported.dart';
import '../../core/platform/host_runtime_permissions.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/services/ble_czechmate_client.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../core/theme/app_motion.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../l10n/app_localizations.dart';
import 'board_session_notifier.dart';
import 'board_session_state.dart';
import 'widgets/board_wifi_provision_sheet.dart';
import 'widgets/discovery_connection_recovery_card.dart';
import 'board_discovery_advanced_screen.dart';

class BoardDiscoveryScreen extends ConsumerStatefulWidget {
  const BoardDiscoveryScreen({super.key, this.autoStartBleScan = true});

  /// When true (default), BLE scan starts once after open unless the next connection is one-shot Wi‑Fi only.
  final bool autoStartBleScan;

  @override
  ConsumerState<BoardDiscoveryScreen> createState() =>
      _BoardDiscoveryScreenState();
}

/// Deska inzeruje UUID služby v ADV nebo jméno v ADV / po Scan Response (iOS).
bool _looksLikeCzechmateBoard(ScanResult r) {
  final pn = r.device.platformName.toUpperCase();
  final an = r.advertisementData.advName.toUpperCase();
  if (pn.contains('CZECHMATE') || an.contains('CZECHMATE')) {
    return true;
  }
  final want =
      czechmateServiceGuid.toString().toLowerCase().replaceAll('-', '');
  for (final u in r.advertisementData.serviceUuids) {
    final g = u.toString().toLowerCase().replaceAll('-', '');
    if (g == want) {
      return true;
    }
  }
  return false;
}

bool _sessionConnectedForDiscovery(BoardSessionState s) {
  if (s.busy) return false;
  switch (s.transport) {
    case BoardTransport.wifi:
    case BoardTransport.mock:
      return true;
    case BoardTransport.ble:
      return s.bleGattConnected;
    case BoardTransport.none:
      return false;
  }
}

class _BoardDiscoveryScreenState extends ConsumerState<BoardDiscoveryScreen> {
  StreamSubscription<List<ScanResult>>? _scanSub;
  final _found = <String, ScanResult>{};
  bool _scanning = false;
  bool _showConnectionRecovery = false;

  @override
  void initState() {
    super.initState();
    final prefs = ref.read(prefsRepositoryProvider);
    if (widget.autoStartBleScan &&
        isFlutterBluePlusHostSupported &&
        prefs.effectiveConnectionMode != 'wifi_only') {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) unawaited(_findBoardScan());
      });
    }
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    unawaited(_stopBleScan());
    super.dispose();
  }

  Future<bool> _ensureBleScanPermissions() async {
    final ok = await ensureBlePermissionsForScan();
    if (!ok && mounted) {
      final l10n = context.l10n;
      final msg = defaultTargetPlatform == TargetPlatform.android
          ? l10n.discoveryBlePermissionAndroid
          : l10n.discoveryBlePermissionDenied;
      showAppSnackBar(context, msg, errorStyle: true);
    }
    return ok;
  }

  /// Po startu aplikace často chvíli trvá, než systém hlásí `BluetoothAdapterState.on`.
  Future<bool> _ensureBluetoothPoweredOn() async {
    if (!isFlutterBluePlusHostSupported) {
      if (mounted) {
        showAppSnackBar(
          context,
          context.l10n.errBleHostUnsupported,
          errorStyle: true,
        );
      }
      return false;
    }
    try {
      final supported = await FlutterBluePlus.isSupported;
      if (supported == false) {
        if (mounted) {
          showAppSnackBar(
            context,
            context.l10n.discoveryBluetoothNotReady,
            errorStyle: true,
          );
        }
        return false;
      }
    } catch (_) {}

    if (!mounted) return false;
    if (FlutterBluePlus.adapterStateNow == BluetoothAdapterState.on) {
      return true;
    }

    try {
      await FlutterBluePlus.adapterState
          .where((s) => s == BluetoothAdapterState.on)
          .timeout(const Duration(seconds: 10))
          .first;
    } catch (_) {
      // Timeout nebo stream — uživatel má BT vypnutý / nepřipravené.
    }

    if (!mounted) return false;
    if (FlutterBluePlus.adapterStateNow == BluetoothAdapterState.on) {
      return true;
    }
    showAppSnackBar(
      context,
      context.l10n.discoveryBluetoothNotReady,
      errorStyle: true,
    );
    return false;
  }

  Future<void> _findBoardScan() async {
    if (!isFlutterBluePlusHostSupported) {
      if (mounted) {
        showAppSnackBar(
          context,
          context.l10n.errBleHostUnsupported,
          errorStyle: true,
        );
      }
      return;
    }
    if (!await _ensureBleScanPermissions()) return;
    if (!await _ensureBluetoothPoweredOn()) return;
    await _scanSub?.cancel();
    _scanSub = null;
    _found.clear();
    setState(() {
      _scanning = true;
      _showConnectionRecovery = false;
    });
    connDebugLog(
        'BLE scan start', 'timeout=12s client-side filtr CZECHMATE/service');
    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE scan start (Najít desku)');
    }
    try {
      _scanSub = FlutterBluePlus.scanResults.listen((list) {
        for (final r in list) {
          if (!_looksLikeCzechmateBoard(r)) {
            continue;
          }
          final id = r.device.remoteId.str;
          _found[id] = r;
        }
        if (mounted) setState(() {});
      });
      // Bez `withServices`: na iOS filtr podle UUID v inzerátu často nevrátí žádné
      // výsledky; filtrujeme client-side (jméno + service UUID z ADV).
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 12),
      );
    } catch (e) {
      connDebugLog('BLE scan error', '$e');
      if (AppEnvironment.staging) {
        debugPrint('[staging] BLE scan error: $e');
      }
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        showAppSnackBar(
          context,
          userFacingErrorSummary(l10n, e),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _scanning = false);
    }
  }

  Future<void> _stopBleScan() async {
    connDebugLog('BLE scan stop');
    if (!isFlutterBluePlusHostSupported) {
      await _scanSub?.cancel();
      _scanSub = null;
      if (mounted) {
        setState(() => _scanning = false);
      }
      return;
    }
    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {
      // Žádný probíhající scan nebo stack už ukonšuje — ignorovat.
    }
    await _scanSub?.cancel();
    _scanSub = null;
    if (mounted) {
      setState(() => _scanning = false);
    }
  }

  String _sessionStateLabel(BoardSessionState session, AppLocalizations l10n) {
    final transport = switch (session.transport) {
      BoardTransport.none => l10n.transportDisconnected,
      BoardTransport.mock => l10n.transportDemo,
      BoardTransport.wifi => l10n.transportWifi,
      BoardTransport.ble => l10n.transportBluetooth,
    };
    if (session.busy) return l10n.transportConnecting(transport);
    return transport;
  }

  Future<void> _toggleBoardHotspot(WidgetRef ref,
      {required bool enable}) async {
    final sessionN = ref.read(boardSessionNotifierProvider.notifier);
    final l10n = AppLocalizations.of(context)!;
    try {
      await sessionN.setBoardHotspotEnabled(enable);
      if (!mounted) return;
      showAppSnackBar(context, l10n.discoveryBoardApCommandSent);
    } catch (e, st) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] hotspot toggle failed: $e\n$st');
      }
      if (!mounted) return;
      final msg = e is StateError
          ? e.message
          : context.l10n.discoveryBoardApError(
              userFacingErrorSummary(context.l10n, e),
            );
      showAppSnackBar(context, msg, errorStyle: true);
    }
  }

  Future<void> _connectToScanResult(ScanResult r) async {
    connDebugLog(
      'Discovery tap → connectBle',
      'remoteId=${r.device.remoteId.str} advName="${r.advertisementData.advName}" '
          'platformName="${r.device.platformName}"',
    );
    final notifier = ref.read(boardSessionNotifierProvider.notifier);
    final skipWifiProvision =
        ref.read(prefsRepositoryProvider).effectiveConnectionMode == 'ble_only';
    if (ref.read(boardSessionNotifierProvider).busy) {
      await notifier.disconnectAwaitBle();
    }
    if (!mounted) return;
    final d = r.device;
    final name = d.platformName.isEmpty
        ? AppLocalizations.of(context)!.defaultBoardName
        : d.platformName;
    await _stopBleScan();
    await notifier.connectBle(d, label: name);
    if (!mounted) return;
    if (_sessionConnectedForDiscovery(ref.read(boardSessionNotifierProvider))) {
      if (!skipWifiProvision) {
        await showBoardWifiProvisionSheet(context);
        if (!mounted) return;
      }
      closeBoardDiscoveryAndFocusPlay(ref, context);
    } else {
      final err = ref.read(boardSessionNotifierProvider).lastError;
      final l10n = AppLocalizations.of(context)!;
      var msg = userFacingErrorSummary(l10n, err);
      if (msg.isEmpty) msg = l10n.userFacingErrGeneric;
      setState(() => _showConnectionRecovery = true);
      showAppSnackBar(context, msg, errorStyle: true);
    }
  }

  Widget _numberedDiscoveryStep(
    BuildContext context,
    int index,
    String text,
    ThemeData theme,
  ) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 28,
            child: Text(
              '$index.',
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.w600,
              ),
            ),
          ),
          Expanded(child: Text(text, style: theme.textTheme.bodyMedium)),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    ref.watch(connectionModeUiRefreshProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final l10n = context.l10n;

    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.discoveryTitle),
        leading: IconButton(
          tooltip: l10n.discoveryCloseTooltip,
          icon: const Icon(Icons.close),
          onPressed: () {
            if (Navigator.of(context).canPop()) {
              Navigator.of(context).pop();
            } else {
              ref.read(mainNavTabIndexProvider.notifier).state =
                  AppMainTab.game;
            }
          },
        ),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            elevation: 0,
            color: cs.surfaceContainerHighest,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
              side: BorderSide(color: cs.outlineVariant),
            ),
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    children: [
                      Icon(Icons.link, color: cs.primary),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          l10n.discoveryConnectionStatusTitle,
                          style: theme.textTheme.titleSmall?.copyWith(
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    _sessionStateLabel(session, l10n),
                    style: theme.textTheme.titleMedium,
                  ),
                  AnimatedSwitcher(
                    duration: AppMotion.crossFade,
                    switchInCurve: AppMotion.standardCurve,
                    switchOutCurve: AppMotion.reverseCurve,
                    child: session.busy
                        ? const Column(
                            key: ValueKey<String>('discovery_sess_busy'),
                            mainAxisSize: MainAxisSize.min,
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              SizedBox(height: 12),
                              LinearProgressIndicator(),
                            ],
                          )
                        : const SizedBox.shrink(
                            key: ValueKey<String>('discovery_sess_idle'),
                          ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),
          if (!isFlutterBluePlusHostSupported) ...[
            Card(
              elevation: 0,
              color: cs.secondaryContainer.withValues(alpha: 0.35),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
                side: BorderSide(color: cs.outlineVariant),
              ),
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Icon(Icons.info_outline, color: cs.primary),
                        const SizedBox(width: 10),
                        Expanded(
                          child: Text(
                            l10n.discoveryDesktopBleUnavailableTitle,
                            style: theme.textTheme.titleSmall?.copyWith(
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    Text(
                      l10n.discoveryDesktopBleUnavailableBody,
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: cs.onSurfaceVariant,
                      ),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 12),
          ],
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  PressableScale(
                    child: FilledButton.icon(
                      onPressed: (!isFlutterBluePlusHostSupported || _scanning)
                          ? null
                          : _findBoardScan,
                      icon: _scanning
                          ? SizedBox(
                              width: 20,
                              height: 20,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: cs.onPrimary,
                              ),
                            )
                          : const Icon(Icons.bluetooth_searching),
                      label: Text(_scanning
                          ? l10n.discoveryScanningBoards
                          : l10n.discoveryFindBle),
                    ),
                  ),
                  AnimatedSwitcher(
                    duration: AppMotion.crossFade,
                    switchInCurve: AppMotion.standardCurve,
                    switchOutCurve: AppMotion.reverseCurve,
                    child: _scanning
                        ? Column(
                            key: const ValueKey<String>('discovery_scan_on'),
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              const SizedBox(height: 12),
                              const LinearProgressIndicator(),
                              const SizedBox(height: 12),
                              OutlinedButton.icon(
                                onPressed: () => unawaited(_stopBleScan()),
                                icon: const Icon(Icons.stop_circle_outlined),
                                label: Text(l10n.discoveryStopScan),
                              ),
                            ],
                          )
                        : const SizedBox.shrink(
                            key: ValueKey<String>('discovery_scan_off'),
                          ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),
          Text(
            devMode
                ? l10n.discoveryBleDevicesDev('$czechmateServiceGuid')
                : l10n.discoveryFoundBoards,
            style: theme.textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 8),
          if (!_scanning && _found.isEmpty)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 12),
              child: Text(
                l10n.discoveryEmptyBleList,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
              ),
            ),
          ..._found.values.map((r) {
            final d = r.device;
            final name =
                d.platformName.isEmpty ? l10n.defaultBoardName : d.platformName;
            final busy = session.busy;
            return Card(
              margin: const EdgeInsets.only(bottom: 8),
              child: ListTile(
                leading: const Icon(Icons.grid_on),
                title: Text(name),
                subtitle: devMode ? Text(d.remoteId.str) : null,
                trailing: busy
                    ? const SizedBox(
                        width: 28,
                        height: 28,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Icon(Icons.chevron_right),
                onTap: () => unawaited(_connectToScanResult(r)),
              ),
            );
          }),
          if (_showConnectionRecovery) ...[
            const SizedBox(height: 12),
            DiscoveryConnectionRecoveryCard(
              l10n: l10n,
              onDismiss: () => setState(() => _showConnectionRecovery = false),
            ),
          ],
          if (session.transport == BoardTransport.ble &&
              session.bleGattConnected) ...[
            const SizedBox(height: 16),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Row(
                      children: [
                        Icon(Icons.wifi_tethering, color: cs.primary),
                        const SizedBox(width: 10),
                        Expanded(
                          child: Text(
                            l10n.discoveryBoardApSubtitle,
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: cs.onSurfaceVariant,
                            ),
                          ),
                        ),
                      ],
                    ),
                    if (session.bleApSsid != null &&
                        session.bleApSsid!.isNotEmpty) ...[
                      const SizedBox(height: 8),
                      Text(
                        session.bleApSsid!,
                        style: theme.textTheme.titleSmall?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ],
                    const SizedBox(height: 12),
                    FilledButton.tonalIcon(
                      onPressed: session.busy
                          ? null
                          : () => unawaited(_toggleBoardHotspot(
                                ref,
                                enable: !session.bleApBroadcasting,
                              )),
                      icon: Icon(
                        session.bleApBroadcasting
                            ? Icons.portable_wifi_off_outlined
                            : Icons.wifi_tethering,
                      ),
                      label: Text(
                        session.bleApBroadcasting
                            ? l10n.discoveryBoardApDisable
                            : l10n.discoveryBoardApEnable,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
          Theme(
            data: theme.copyWith(dividerColor: Colors.transparent),
            child: ExpansionTile(
              tilePadding: EdgeInsets.zero,
              title: Text(
                l10n.discoveryHelpConnectingTitle,
                style: theme.textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.w600,
                ),
              ),
              subtitle: Text(
                l10n.discoveryHelpConnectingSubtitle,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
              ),
              children: [
                Align(
                  alignment: Alignment.centerLeft,
                  child: Padding(
                    padding: const EdgeInsets.only(bottom: 12),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Text(
                          l10n.discoveryIntro,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: cs.onSurfaceVariant,
                          ),
                        ),
                        const SizedBox(height: 12),
                        Text(
                          l10n.discoveryFlowStepsTitle,
                          style: theme.textTheme.titleSmall?.copyWith(
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                        const SizedBox(height: 8),
                        _numberedDiscoveryStep(
                            context, 1, l10n.discoveryFlowStep1, theme),
                        _numberedDiscoveryStep(
                            context, 2, l10n.discoveryFlowStep2, theme),
                        _numberedDiscoveryStep(
                            context, 3, l10n.discoveryFlowStep3, theme),
                        if (widget.autoStartBleScan &&
                            isFlutterBluePlusHostSupported) ...[
                          const SizedBox(height: 10),
                          Text(
                            l10n.discoveryAutoScanHint,
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: cs.onSurfaceVariant,
                            ),
                          ),
                        ],
                        const SizedBox(height: 10),
                        Text(
                          l10n.discoveryScanCardLead,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: cs.onSurfaceVariant,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),
          Card(
            clipBehavior: Clip.antiAlias,
            child: ListTile(
              leading: Icon(Icons.tune_outlined, color: cs.primary),
              title: Text(l10n.settingsAdvanced),
              subtitle: Text(
                l10n.discoveryAdvancedSubtitle,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
              ),
              trailing: const Icon(Icons.chevron_right),
              onTap: () {
                Navigator.of(context).push<void>(
                  MaterialPageRoute<void>(
                    builder: (_) => const BoardDiscoveryAdvancedScreen(),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}
