import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:permission_handler/permission_handler.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/services/ble_czechmate_client.dart';
import 'board_session_notifier.dart';
import 'board_session_state.dart';

class BoardDiscoveryScreen extends ConsumerStatefulWidget {
  const BoardDiscoveryScreen({super.key});

  @override
  ConsumerState<BoardDiscoveryScreen> createState() =>
      _BoardDiscoveryScreenState();
}

class _BoardDiscoveryScreenState extends ConsumerState<BoardDiscoveryScreen> {
  final _url = TextEditingController();
  StreamSubscription<List<ScanResult>>? _scanSub;
  final _found = <String, ScanResult>{};
  String _connectionMode = 'auto';
  bool _preferBluetoothOnly = false;

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
        const SnackBar(
            content:
                Text(
                    'Allow Bluetooth permissions in system settings to scan for BLE boards.')),
      );
    }
    return ok;
  }

  Future<void> _startBleScan() async {
    if (!await _ensureAndroidBleScanPermissions()) return;
    await _scanSub?.cancel();
    _found.clear();
    setState(() {});
    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE scan start');
    }
    try {
      await FlutterBluePlus.startScan(
        withServices: [czechmateServiceGuid],
        timeout: const Duration(seconds: 12),
      );
      _scanSub = FlutterBluePlus.scanResults.listen((list) {
        for (final r in list) {
          final id = r.device.remoteId.str;
          _found[id] = r;
        }
        setState(() {});
      });
    } catch (e) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] BLE scan error: $e');
      }
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Bluetooth scan failed: $e')),
        );
      }
    }
  }

  Future<void> _stopBleScan() async {
    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    _scanSub = null;
  }

  String _sessionStateLabel(BoardSessionState session) {
    final transport = switch (session.transport) {
      BoardTransport.none => 'Disconnected',
      BoardTransport.mock => 'Demo mode',
      BoardTransport.wifi => 'Connected via Wi‑Fi',
      BoardTransport.ble => 'Connected via Bluetooth',
    };
    if (session.busy) return '$transport · connecting…';
    return transport;
  }

  Future<void> _persistConnectionPrefs() async {
    final prefs = ref.read(prefsRepositoryProvider);
    await prefs.setConnectionMode(_connectionMode);
    await prefs.setPreferBluetoothOnly(_preferBluetoothOnly);
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Board'),
        leading: IconButton(
          tooltip: 'Close',
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
                  Text(
                    'Connection mode',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  const SizedBox(height: 8),
                  SingleChildScrollView(
                    scrollDirection: Axis.horizontal,
                    child: SegmentedButton<String>(
                      segments: const [
                        ButtonSegment(value: 'auto', label: Text('Auto')),
                        ButtonSegment(
                            value: 'wifi_only', label: Text('Wi‑Fi only')),
                        ButtonSegment(
                            value: 'ble_only', label: Text('BLE only')),
                      ],
                      selected: {_connectionMode},
                      onSelectionChanged: (s) async {
                        if (s.isEmpty) return;
                        setState(() => _connectionMode = s.first);
                        await _persistConnectionPrefs();
                      },
                    ),
                  ),
                  const SizedBox(height: 8),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Bluetooth only (don’t switch to Wi‑Fi)'),
                    subtitle: const Text(
                      'After BLE connect, stay on Bluetooth even if the board knows a Wi‑Fi URL.',
                    ),
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
                          : () => ref
                              .read(boardSessionNotifierProvider.notifier)
                              .reconnectSavedBle(),
                      icon: const Icon(Icons.bluetooth_connected),
                      label: const Text('Reconnect last BLE board'),
                    ),
                ],
              ),
            ),
          ),
          ListTile(
            contentPadding: EdgeInsets.zero,
            leading: const Icon(Icons.link),
            title: const Text('Connection status'),
            subtitle: Text(_sessionStateLabel(session)),
          ),
          const SizedBox(height: 16),
          TextField(
            controller: _url,
            decoration: const InputDecoration(
              labelText: 'Board URL (Wi‑Fi)',
              hintText: 'Board address on your LAN',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
          ),
          const SizedBox(height: 8),
          FilledButton(
            onPressed: session.busy
                ? null
                : () async {
                    final u = _url.text.trim().replaceAll(RegExp(r'/$'), '');
                    if (u.isEmpty) {
                      if (!context.mounted) return;
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(
                            content: Text(
                                'Enter the board URL or IP (e.g. http://192.168.4.1).')),
                      );
                      return;
                    }
                    await ref
                        .read(boardSessionNotifierProvider.notifier)
                        .connectWifi(u);
                    if (context.mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(content: Text('Wi‑Fi session started')),
                      );
                    }
                  },
            child: const Text('Connect via Wi‑Fi'),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: session.busy
                ? null
                : () => ref
                    .read(boardSessionNotifierProvider.notifier)
                    .connectMock(),
            child: const Text('Mock board (demo)'),
          ),
          const SizedBox(height: 8),
          OutlinedButton.icon(
            onPressed: session.busy
                ? null
                : () => ref
                    .read(boardSessionNotifierProvider.notifier)
                    .connectWifi('http://192.168.4.1'),
            icon: const Icon(Icons.wifi_tethering),
            label: const Text('Connect board hotspot (192.168.4.1)'),
          ),
          const Divider(height: 32),
          Text(
            devMode ? 'Bluetooth $czechmateServiceGuid' : 'Bluetooth devices',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          Row(
            children: [
              FilledButton.tonal(
                onPressed: _startBleScan,
                child: const Text('Scan'),
              ),
              const SizedBox(width: 8),
              OutlinedButton(
                onPressed: _stopBleScan,
                child: const Text('Stop'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          ..._found.values.map((r) {
            final d = r.device;
            final name =
                d.platformName.isEmpty ? 'CZECHMATE Board' : d.platformName;
            return ListTile(
              title: Text(name),
              subtitle: devMode ? Text(d.remoteId.str) : null,
              trailing: session.busy
                  ? null
                  : FilledButton(
                      onPressed: () async {
                        await _stopBleScan();
                        await ref
                            .read(boardSessionNotifierProvider.notifier)
                            .connectBle(d, label: name);
                        if (context.mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                            SnackBar(content: Text('BLE: $name')),
                          );
                        }
                      },
                      child: const Text('Connect'),
                    ),
            );
          }),
        ],
      ),
    );
  }
}
