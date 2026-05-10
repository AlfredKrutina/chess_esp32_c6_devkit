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

  /// Aktivní Wi‑Fi session má přednost před jen uloženou URL v prefs.
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
        showAppSnackBar(context, 'Chyba: $e', errorStyle: true);
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
      _pingResult = 'Měřím...';
    });
    final sw = Stopwatch()..start();
    try {
      await ref.read(boardApiClientProvider).fetchSnapshotIfChanged(baseUrl);
      sw.stop();
      if (mounted) setState(() => _pingResult = '${sw.elapsedMilliseconds} ms');
    } catch (e) {
      sw.stop();
      if (mounted) setState(() => _pingResult = 'Chyba: $e');
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
        showAppSnackBar(context, 'Odesláno na ESP');
      }
    } catch (e) {
      if (mounted) {
        showAppSnackBar(context, 'Chyba: $e', errorStyle: true);
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
        title: const Text('Diagnostika a Vývojář'),
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
          const Text('Stockfish a FEN',
              style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          SwitchListTile(
            title: const Text('Eval tahů (moveEvaluationEnabled)'),
            value: prefs.moveEvaluationEnabled,
            onChanged: (v) async {
              await prefs.setMoveEvaluationEnabled(v);
              setState(() {});
            },
          ),
          ListTile(
            title: Text('Hloubka nápovědy (hintDepth): ${prefs.hintDepth}'),
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
          SelectableText('Aktuální FEN z desky:\n$fen',
              style: const TextStyle(fontFamily: 'monospace')),
          const Divider(height: 32),
          const Text('Síť a transport',
              style: TextStyle(fontWeight: FontWeight.bold)),
          ListTile(
            title: const Text('Základní URL desky (ESP)'),
            subtitle: Text(baseUrl.isEmpty ? 'Žádná' : baseUrl),
          ),
          ListTile(
            title: const Text('Stav spojení (Active Link)'),
            subtitle: Text(session.transport.name.toUpperCase()),
          ),
          SwitchListTile(
            title: const Text('Podrobné logy trenéra (coach trace)'),
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
                          showAppSnackBar(context, 'Obnoveno z prefs');
                        }
                      },
                child: const Text('Spustit připojení'),
              ),
              OutlinedButton(
                onPressed: () {
                  ref.read(boardSessionNotifierProvider.notifier).disconnect();
                  if (context.mounted) {
                    showAppSnackBar(context, 'Transport zastaven');
                  }
                },
                child: const Text('Zastavit'),
              ),
            ],
          ),
          ElevatedButton.icon(
            onPressed: _isLoading ? null : _ping,
            icon: const Icon(Icons.timer),
            label: Text('Ping Snapshot (RTT): $_pingResult'),
          ),
          ListTile(
            title: const Text('Diagnostika připojení (REST / WS)'),
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
                          showAppSnackBar(context, 'STA odpojeno');
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
              child: const Text('Odpojit ESP od STA'),
            ),
            OutlinedButton(
              onPressed: _isLoading
                  ? null
                  : () async {
                      final ok = await showDialog<bool>(
                        context: context,
                        builder: (ctx) => AlertDialog(
                          title: const Text('Smazat Wi‑Fi z NVS?'),
                          content: const Text('ESP ztratí uloženou síť.'),
                          actions: [
                            TextButton(
                                onPressed: () => Navigator.pop(ctx, false),
                                child: const Text('Zrušit')),
                            TextButton(
                                onPressed: () => Navigator.pop(ctx, true),
                                child: const Text('Smazat')),
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
                          showAppSnackBar(context, 'Wi‑Fi NVS vymazáno');
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
              child: const Text('Smazat uloženou Wi‑Fi z NVS'),
            ),
          ],
          const Divider(height: 32),
          const BoardLampBlock(showTitle: false),
          const Divider(height: 32),
          const Text('Konfigurace Wi-Fi na desce',
              style: TextStyle(fontWeight: FontWeight.bold)),
          if (_wifiStatus != null) ...[
            Text(
                'STA: ${_wifiStatus!.staSsid} (${_wifiStatus!.staIp}) - ${_wifiStatus!.staConnected ? "ONLINE" : "Offline"}'),
            Text(
                'AP: ${_wifiStatus!.apSsid} (${_wifiStatus!.apIp}) - Klienti: ${_wifiStatus!.apClients}'),
          ] else ...[
            const Text(
                'Stav Wi-Fi není k dispozici. Lze vyčíst po stisknutí tlačítka níže.'),
          ],
          const SizedBox(height: 8),
          ElevatedButton(
              onPressed: _isLoading ? null : _refreshWiFi,
              child: const Text('Obnovit stav Wi-Fi')),
          const SizedBox(height: 16),
          TextField(
              controller: _ssidCtrl,
              decoration: const InputDecoration(
                  labelText: 'Wi-Fi SSID', border: OutlineInputBorder())),
          const SizedBox(height: 8),
          TextField(
              controller: _passCtrl,
              decoration: const InputDecoration(
                  labelText: 'Heslo', border: OutlineInputBorder()),
              obscureText: true),
          const SizedBox(height: 8),
          FilledButton.tonal(
              onPressed: _isLoading ? null : _saveWiFi,
              child: const Text('Uložit do desky a Připojit (STA)')),
          const Divider(height: 32),
          const Text('Firmware a paměť',
              style: TextStyle(fontWeight: FontWeight.bold)),
          OutlinedButton(
            onPressed: () => Navigator.push(
                context,
                MaterialPageRoute(
                    builder: (ctx) => const BoardDeviceFeaturesView())),
            child: const Text('Detailní nástroje (Třídy NVS)'),
          ),
        ],
      ),
    );
  }
}
