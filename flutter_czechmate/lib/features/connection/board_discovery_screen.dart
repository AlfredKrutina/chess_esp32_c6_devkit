import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:permission_handler/permission_handler.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/services/ble_czechmate_client.dart';
import '../../l10n/app_localizations.dart';
import '../settings/manual_connection_screen.dart';
import 'board_session_notifier.dart';
import 'board_session_state.dart';

class BoardDiscoveryScreen extends ConsumerStatefulWidget {
  const BoardDiscoveryScreen({super.key});

  @override
  ConsumerState<BoardDiscoveryScreen> createState() =>
      _BoardDiscoveryScreenState();
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
  final _url = TextEditingController();
  StreamSubscription<List<ScanResult>>? _scanSub;
  final _found = <String, ScanResult>{};
  String _connectionMode = 'auto';
  bool _preferBluetoothOnly = false;
  bool _scanning = false;

  @override
  void initState() {
    super.initState();
    final prefs = ref.read(prefsRepositoryProvider);
    _url.text = prefs.lastBoardBaseUrl ?? '';
    _connectionMode = prefs.connectionMode;
    _preferBluetoothOnly = prefs.preferBluetoothOnly;
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    unawaited(_stopBleScan());
    _url.dispose();
    super.dispose();
  }

  Future<bool> _ensureAndroidBleScanPermissions() async {
    if (defaultTargetPlatform != TargetPlatform.android) return true;
    final connect = await Permission.bluetoothConnect.request();
    final scan = await Permission.bluetoothScan.request();
    final ok = connect.isGranted && scan.isGranted;
    if (!ok && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.discoveryBlePermissionAndroid)),
      );
    }
    return ok;
  }

  Future<void> _findBoardScan() async {
    if (!await _ensureAndroidBleScanPermissions()) return;
    await _scanSub?.cancel();
    _scanSub = null;
    _found.clear();
    setState(() => _scanning = true);
    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE scan start (Najít desku)');
    }
    try {
      _scanSub = FlutterBluePlus.scanResults.listen((list) {
        for (final r in list) {
          final id = r.device.remoteId.str;
          _found[id] = r;
        }
        if (mounted) setState(() {});
      });
      await FlutterBluePlus.startScan(
        withServices: [czechmateServiceGuid],
        timeout: const Duration(seconds: 12),
      );
    } catch (e) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] BLE scan error: $e');
      }
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.discoveryBleScanFailed('$e'))),
        );
      }
    } finally {
      if (mounted) setState(() => _scanning = false);
    }
  }

  Future<void> _stopBleScan() async {
    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    _scanSub = null;
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

  Future<void> _persistConnectionPrefs() async {
    final prefs = ref.read(prefsRepositoryProvider);
    await prefs.setConnectionMode(_connectionMode);
    await prefs.setPreferBluetoothOnly(_preferBluetoothOnly);
  }

  Future<void> _connectToScanResult(ScanResult r) async {
    final session = ref.read(boardSessionNotifierProvider);
    if (session.busy) return;
    final d = r.device;
    final name = d.platformName.isEmpty
        ? AppLocalizations.of(context)!.defaultBoardName
        : d.platformName;
    await _stopBleScan();
    await ref.read(boardSessionNotifierProvider.notifier).connectBle(d, label: name);
    if (!mounted) return;
    if (_sessionConnectedForDiscovery(ref.read(boardSessionNotifierProvider))) {
      closeBoardDiscoveryAndFocusPlay(ref, context);
    }
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
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
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    children: [
                      Icon(Icons.info_outline, color: cs.primary),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          l10n.discoveryIntro,
                          style: theme.textTheme.bodyMedium,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 14),
                  ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: Icon(Icons.link, color: cs.primary),
                    title: Text(l10n.discoveryConnectionStatusTitle),
                    subtitle: Text(_sessionStateLabel(session, l10n)),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),
          FilledButton.icon(
            onPressed: session.busy || _scanning ? null : _findBoardScan,
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
            label: Text(
                _scanning ? l10n.discoveryScanningBoards : l10n.discoveryFindBle),
          ),
          if (_scanning) ...[
            const SizedBox(height: 8),
            const LinearProgressIndicator(),
          ],
          const SizedBox(height: 20),
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
            final name = d.platformName.isEmpty
                ? l10n.defaultBoardName
                : d.platformName;
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
                onTap: busy ? null : () => _connectToScanResult(r),
              ),
            );
          }),
          const SizedBox(height: 8),
          Theme(
            data: theme.copyWith(dividerColor: Colors.transparent),
            child: ExpansionTile(
              tilePadding: const EdgeInsets.symmetric(horizontal: 12),
              collapsedShape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
                side: BorderSide(color: cs.outlineVariant),
              ),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
                side: BorderSide(color: cs.outlineVariant),
              ),
              title: Text(l10n.settingsAdvanced),
              subtitle: Text(
                l10n.discoveryAdvancedSubtitle,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
              ),
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      Text(
                        l10n.discoveryConnectionModeHeading,
                        style: theme.textTheme.titleSmall?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                      const SizedBox(height: 8),
                      SingleChildScrollView(
                        scrollDirection: Axis.horizontal,
                        child: SegmentedButton<String>(
                          segments: [
                            ButtonSegment(
                              value: 'auto',
                              label: Text(l10n.connectionModeAutoShort),
                            ),
                            ButtonSegment(
                              value: 'wifi_only',
                              label: Text(l10n.connectionModeWifiOnly),
                            ),
                            ButtonSegment(
                              value: 'ble_only',
                              label: Text(l10n.discoveryBleSegmentShort),
                            ),
                          ],
                          selected: {_connectionMode},
                          onSelectionChanged: (s) async {
                            if (s.isEmpty) return;
                            setState(() => _connectionMode = s.first);
                            await _persistConnectionPrefs();
                          },
                        ),
                      ),
                      SwitchListTile(
                        contentPadding: EdgeInsets.zero,
                        title: Text(l10n.settingsBleOnlyTitle),
                        subtitle: Text(l10n.preferBleOnlySubtitle),
                        value: _preferBluetoothOnly,
                        onChanged: (v) async {
                          setState(() => _preferBluetoothOnly = v);
                          await _persistConnectionPrefs();
                        },
                      ),
                      if (prefs.lastBleRemoteId != null &&
                          prefs.lastBleRemoteId!.isNotEmpty)
                        OutlinedButton.icon(
                          onPressed: session.busy
                              ? null
                              : () async {
                                  await ref
                                      .read(boardSessionNotifierProvider
                                          .notifier)
                                      .reconnectSavedBle();
                                  if (!context.mounted) return;
                                  if (_sessionConnectedForDiscovery(ref.read(
                                      boardSessionNotifierProvider))) {
                                    closeBoardDiscoveryAndFocusPlay(
                                        ref, context);
                                  }
                                },
                          icon: const Icon(Icons.bluetooth_connected),
                          label: Text(l10n.discoveryReconnectSavedBoard),
                        ),
                      if (prefs.lastBleRemoteId != null &&
                          prefs.lastBleRemoteId!.isNotEmpty)
                        const SizedBox(height: 10),
                      OutlinedButton.icon(
                        onPressed: () => Navigator.push<void>(
                          context,
                          MaterialPageRoute<void>(
                            builder: (_) => const ManualConnectionScreen(),
                          ),
                        ),
                        icon: const Icon(Icons.edit),
                        label: Text(l10n.discoveryManualAdvanced),
                      ),
                      const Divider(height: 28),
                      TextField(
                        controller: _url,
                        decoration: InputDecoration(
                          labelText: l10n.discoveryWifiUrlLabel,
                          hintText: l10n.discoveryWifiUrlHint,
                          border: const OutlineInputBorder(),
                        ),
                        keyboardType: TextInputType.url,
                      ),
                      const SizedBox(height: 8),
                      FilledButton(
                        onPressed: session.busy
                            ? null
                            : () async {
                                final u =
                                    _url.text.trim().replaceAll(RegExp(r'/$'), '');
                                if (u.isEmpty) {
                                  if (!context.mounted) return;
                                  ScaffoldMessenger.of(context).showSnackBar(
                                    SnackBar(
                                      content: Text(
                                        l10n.discoveryEnterBoardUrlSnack,
                                      ),
                                    ),
                                  );
                                  return;
                                }
                                await ref
                                    .read(boardSessionNotifierProvider.notifier)
                                    .connectWifi(u);
                                if (!context.mounted) return;
                                if (_sessionConnectedForDiscovery(ref.read(
                                    boardSessionNotifierProvider))) {
                                  closeBoardDiscoveryAndFocusPlay(ref, context);
                                }
                              },
                        child: Text(l10n.discoveryConnectWifi),
                      ),
                      const SizedBox(height: 8),
                      OutlinedButton(
                        onPressed: session.busy
                            ? null
                            : () async {
                                await ref
                                    .read(boardSessionNotifierProvider.notifier)
                                    .connectMock();
                                if (!context.mounted) return;
                                if (_sessionConnectedForDiscovery(ref.read(
                                    boardSessionNotifierProvider))) {
                                  closeBoardDiscoveryAndFocusPlay(ref, context);
                                }
                              },
                        child: Text(l10n.mockBoardTileTitle),
                      ),
                      const SizedBox(height: 8),
                      OutlinedButton.icon(
                        onPressed: session.busy
                            ? null
                            : () async {
                                await ref
                                    .read(boardSessionNotifierProvider.notifier)
                                    .connectWifi('http://192.168.4.1');
                                if (!context.mounted) return;
                                if (_sessionConnectedForDiscovery(ref.read(
                                    boardSessionNotifierProvider))) {
                                  closeBoardDiscoveryAndFocusPlay(ref, context);
                                }
                              },
                        icon: const Icon(Icons.wifi_tethering),
                        label: Text(l10n.discoveryBoardHotspotButton),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          OutlinedButton(
                            onPressed: _stopBleScan,
                            child: Text(l10n.discoveryStopScan),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
