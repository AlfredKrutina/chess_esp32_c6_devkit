import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/services/ble_czechmate_client.dart';
import 'board_session_notifier.dart';

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

  @override
  void initState() {
    super.initState();
    final prefs = ref.read(prefsRepositoryProvider);
    _url.text = prefs.lastBoardBaseUrl ?? 'http://192.168.4.1';
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _url.dispose();
    super.dispose();
  }

  Future<void> _startBleScan() async {
    await _scanSub?.cancel();
    _found.clear();
    setState(() {});
    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE scan start');
    }
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
  }

  Future<void> _stopBleScan() async {
    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    _scanSub = null;
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    return Scaffold(
      appBar: AppBar(title: const Text('Deska')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Text('Stav: ${session.transport} ${session.busy ? "(busy)" : ""}'),
          const SizedBox(height: 16),
          TextField(
            controller: _url,
            decoration: const InputDecoration(
              labelText: 'HTTP base URL desky',
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
                    await ref
                        .read(boardSessionNotifierProvider.notifier)
                        .connectWifi(u);
                    if (context.mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(content: Text('Wi‑Fi session spuštěna')),
                      );
                    }
                  },
            child: const Text('Připojit přes Wi‑Fi'),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: session.busy
                ? null
                : () => ref
                    .read(boardSessionNotifierProvider.notifier)
                    .connectMock(),
            child: const Text('Mock deska (asset)'),
          ),
          const Divider(height: 32),
          Text(
            'Bluetooth $czechmateServiceGuid',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          Row(
            children: [
              FilledButton.tonal(
                onPressed: _startBleScan,
                child: const Text('Skenovat'),
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
            final name = d.platformName.isEmpty ? d.remoteId.str : d.platformName;
            return ListTile(
              title: Text(name),
              subtitle: Text(d.remoteId.str),
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
                      child: const Text('Připojit'),
                    ),
            );
          }),
        ],
      ),
    );
  }
}
