import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/utils/app_debug_log.dart';
import '../../../core/models/board_timer_state.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../connection/board_session_notifier.dart';
import '../connection/connection_diagnostics_screen.dart';
import 'board_device_features_view.dart';
import 'widgets/board_lamp_widgets.dart';

class DeveloperSettingsView extends ConsumerStatefulWidget {
  const DeveloperSettingsView({super.key});

  @override
  ConsumerState<DeveloperSettingsView> createState() =>
      _DeveloperSettingsViewState();
}

class _DeveloperSettingsViewState extends ConsumerState<DeveloperSettingsView> {
  EspWifiStatus? _wifiStatus;
  bool _isLoading = false;
  String _pingResult = '';

  final _ssidCtrl = TextEditingController();
  final _passCtrl = TextEditingController();

  /// An active Wi‑Fi session takes precedence over just a saved URL in prefs.
  String? _effectiveWifiBase() {
    final session = ref.read(boardSessionNotifierProvider);
    final w = session.wifiBaseUrl?.trim().replaceAll(RegExp(r'/$'), '') ?? '';
    if (w.isNotEmpty) return w;
    final p = ref
            .read(prefsRepositoryProvider)
            .lastBoardBaseUrl
            ?.trim()
            .replaceAll(RegExp(r'/$'), '') ??
        '';
    return p.isEmpty ? null : p;
  }

  @override
  void initState() {
    super.initState();
    _refreshWiFi();
  }

  @override
  void dispose() {
    _ssidCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  Future<void> _refreshWiFi() async {
    final baseUrl = _effectiveWifiBase();
    if (baseUrl == null || baseUrl.isEmpty) return;
    setState(() => _isLoading = true);
    try {
      final s = await ref.read(boardApiClientProvider).fetchWiFiStatus(baseUrl);
      if (mounted) setState(() => _wifiStatus = s);
    } catch (e) {
      if (mounted) {
        showAppSnackBar(context, 'Error: $e', errorStyle: true);
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  Future<void> _ping() async {
    final baseUrl = _effectiveWifiBase();
    if (baseUrl == null || baseUrl.isEmpty) return;
    setState(() {
      _isLoading = true;
      _pingResult = 'Measuring...';
    });
    final sw = Stopwatch()..start();
    try {
      await ref.read(boardApiClientProvider).fetchSnapshotIfChanged(baseUrl);
      sw.stop();
      if (mounted) setState(() => _pingResult = '${sw.elapsedMilliseconds} ms');
    } catch (e) {
      sw.stop();
      if (mounted) setState(() => _pingResult = 'Error: $e');
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  Future<void> _saveWiFi() async {
    final baseUrl = _effectiveWifiBase();
    if (baseUrl == null || baseUrl.isEmpty) return;
    setState(() => _isLoading = true);
    try {
      await ref.read(boardApiClientProvider).postWiFiConfig(
            baseUrl,
            ssid: _ssidCtrl.text.trim(),
            password: _passCtrl.text,
          );
      if (mounted) {
        showAppSnackBar(context, 'Sent to ESP');
      }
    } catch (e) {
      if (mounted) {
        showAppSnackBar(context, 'Error: $e', errorStyle: true);
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
      _refreshWiFi();
    }
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final baseUrl = _effectiveWifiBase() ?? '';
    final fen =
        session.snapshot != null ? fenFromSnapshot(session.snapshot!) : 'N/A';

    return Scaffold(
      appBar: AppBar(
        title: const Text('Diagnostics & Developer'),
        actions: [
          if (_isLoading)
            const Center(
                child: Padding(
                    padding: EdgeInsets.all(8.0),
                    child: CircularProgressIndicator())),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          const Text('Stockfish and FEN',
              style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          SwitchListTile(
            title: const Text('Move eval (moveEvaluationEnabled)'),
            value: prefs.moveEvaluationEnabled,
            onChanged: (v) async {
              await prefs.setMoveEvaluationEnabled(v);
              setState(() {});
            },
          ),
          ListTile(
            title: Text('Hint depth (hintDepth): ${prefs.hintDepth}'),
            subtitle: Slider(
              min: 1,
              max: 18,
              divisions: 17,
              value: prefs.hintDepth.toDouble(),
              onChanged: (v) async {
                await prefs.setHintDepth(v.round());
                setState(() {});
              },
            ),
          ),
          SelectableText('Current FEN from board:\n$fen',
              style: const TextStyle(fontFamily: 'monospace')),
          const Divider(height: 32),
          const Text('Network and transport',
              style: TextStyle(fontWeight: FontWeight.bold)),
          ListTile(
            title: const Text('Board base URL (ESP)'),
            subtitle: Text(baseUrl.isEmpty ? 'None' : baseUrl),
          ),
          ListTile(
            title: const Text('Connection state (Active Link)'),
            subtitle: Text(session.transport.name.toUpperCase()),
          ),
          SwitchListTile(
            title: const Text('Coach detailed logs (coach trace)'),
            value: prefs.coachTraceLogsEnabled,
            onChanged: (v) async {
              await prefs.setCoachTraceLogsEnabled(v);
              appDebugLog('[staging] coachTraceLogs=$v');
              setState(() {});
            },
          ),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              FilledButton.tonal(
                onPressed: _isLoading
                    ? null
                    : () async {
                        await ref
                            .read(boardSessionNotifierProvider.notifier)
                            .tryResumeFromPrefs();
                        if (context.mounted) {
                          showAppSnackBar(context, 'Resumed from prefs');
                        }
                      },
                child: const Text('Start connection'),
              ),
              OutlinedButton(
                onPressed: () {
                  ref.read(boardSessionNotifierProvider.notifier).disconnect();
                  if (context.mounted) {
                    showAppSnackBar(context, 'Transport stopped');
                  }
                },
                child: const Text('Stop'),
              ),
            ],
          ),
          ElevatedButton.icon(
            onPressed: _isLoading ? null : _ping,
            icon: const Icon(Icons.timer),
            label: Text('Ping Snapshot (RTT): $_pingResult'),
          ),
          ListTile(
            title: const Text('Connection diagnostics (REST / WS)'),
            trailing: const Icon(Icons.chevron_right),
            onTap: () => Navigator.push(
              context,
              MaterialPageRoute<void>(
                  builder: (_) => const ConnectionDiagnosticsScreen()),
            ),
          ),
          if (baseUrl.isNotEmpty) ...[
            const SizedBox(height: 8),
            OutlinedButton(
              onPressed: _isLoading
                  ? null
                  : () async {
                      setState(() => _isLoading = true);
                      try {
                        await ref
                            .read(boardApiClientProvider)
                            .postWiFiDisconnect(baseUrl);
                        if (context.mounted) {
                          showAppSnackBar(context, 'STA disconnected');
                        }
                      } catch (e) {
                        if (context.mounted) {
                          final l10n = context.l10n;
                          showAppSnackBar(
                            context,
                            userFacingErrorSummary(l10n, e),
                            errorStyle: true,
                          );
                        }
                      } finally {
                        if (mounted) setState(() => _isLoading = false);
                        _refreshWiFi();
                      }
                    },
              child: const Text('Disconnect ESP from STA'),
            ),
            OutlinedButton(
              onPressed: _isLoading
                  ? null
                  : () async {
                      final ok = await showDialog<bool>(
                        context: context,
                        builder: (ctx) => AlertDialog(
                          title: const Text('Clear Wi‑Fi from NVS?'),
                          content: const Text('ESP will lose the saved network.'),
                          actions: [
                            TextButton(
                                onPressed: () => Navigator.pop(ctx, false),
                                child: const Text('Cancel')),
                            TextButton(
                                onPressed: () => Navigator.pop(ctx, true),
                                child: const Text('Clear')),
                          ],
                        ),
                      );
                      if (ok != true) return;
                      setState(() => _isLoading = true);
                      try {
                        await ref
                            .read(boardApiClientProvider)
                            .postWiFiClear(baseUrl);
                        if (context.mounted) {
                          showAppSnackBar(context, 'Wi‑Fi NVS cleared');
                        }
                      } catch (e) {
                        if (context.mounted) {
                          final l10n = context.l10n;
                          showAppSnackBar(
                            context,
                            userFacingErrorSummary(l10n, e),
                            errorStyle: true,
                          );
                        }
                      } finally {
                        if (mounted) setState(() => _isLoading = false);
                        _refreshWiFi();
                      }
                    },
              child: const Text('Clear saved Wi‑Fi from NVS'),
            ),
          ],
          const Divider(height: 32),
          const BoardLampBlock(showTitle: false),
          const Divider(height: 32),
          const Text('Board Wi-Fi configuration',
              style: TextStyle(fontWeight: FontWeight.bold)),
          if (_wifiStatus != null) ...[
            Text(
                'STA: ${_wifiStatus!.staSsid} (${_wifiStatus!.staIp}) - ${_wifiStatus!.staConnected ? "ONLINE" : "Offline"}'),
            Text(
                'AP: ${_wifiStatus!.apSsid} (${_wifiStatus!.apIp}) - Clients: ${_wifiStatus!.apClients}'),
          ] else ...[
            const Text(
                'Wi-Fi status unavailable. Fetch it with the button below.'),
          ],
          const SizedBox(height: 8),
          ElevatedButton(
              onPressed: _isLoading ? null : _refreshWiFi,
              child: const Text('Refresh Wi-Fi status')),
          const SizedBox(height: 16),
          TextField(
              controller: _ssidCtrl,
              decoration: const InputDecoration(
                  labelText: 'Wi-Fi SSID', border: OutlineInputBorder())),
          const SizedBox(height: 8),
          TextField(
              controller: _passCtrl,
              decoration: const InputDecoration(
                  labelText: 'Password', border: OutlineInputBorder()),
              obscureText: true),
          const SizedBox(height: 8),
          FilledButton.tonal(
              onPressed: _isLoading ? null : _saveWiFi,
              child: const Text('Save to board and Connect (STA)')),
          const Divider(height: 32),
          const Text('Firmware and memory',
              style: TextStyle(fontWeight: FontWeight.bold)),
          OutlinedButton(
            onPressed: () => Navigator.push(
                context,
                MaterialPageRoute(
                    builder: (ctx) => const BoardDeviceFeaturesView())),
            child: const Text('Detailed tools (NVS namespaces)'),
          ),
        ],
      ),
    );
  }
}
