import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/models/board_timer_state.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Ruční URL, test spojení a příkazy **Wi‑Fi STA / NVS** (`/api/wifi/*`) — HTTP na desku.
class ManualConnectionScreen extends ConsumerStatefulWidget {
  const ManualConnectionScreen({super.key});

  @override
  ConsumerState<ManualConnectionScreen> createState() =>
      _ManualConnectionScreenState();
}

class _ManualConnectionScreenState
    extends ConsumerState<ManualConnectionScreen> {
  late final TextEditingController _url;
  String _ping = '';
  EspWifiStatus? _wifiStatus;
  bool _wifiBusy = false;

  @override
  void initState() {
    super.initState();
    final prefs = ref.read(prefsRepositoryProvider);
    final session = ref.read(boardSessionNotifierProvider);
    final initial =
        (session.wifiBaseUrl != null && session.wifiBaseUrl!.trim().isNotEmpty)
            ? session.wifiBaseUrl!.trim()
            : (prefs.lastBoardBaseUrl ?? '');
    _url = TextEditingController(text: initial);
  }

  @override
  void dispose() {
    _url.dispose();
    super.dispose();
  }

  String? _normalizedBase() {
    final tries = [
      _url.text.trim(),
      ref.read(boardSessionNotifierProvider).wifiBaseUrl?.trim() ?? '',
      ref.read(prefsRepositoryProvider).lastBoardBaseUrl?.trim() ?? '',
    ];
    for (final raw in tries) {
      final n = normalizeBoardHttpBaseUrl(raw);
      if (n != null) return n;
    }
    return null;
  }

  Future<void> _refreshWiFiStatus() async {
    final base = _normalizedBase();
    if (base == null) return;
    setState(() => _wifiBusy = true);
    try {
      final s = await ref.read(boardApiClientProvider).fetchWiFiStatus(base);
      if (mounted) setState(() => _wifiStatus = s);
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(context.l10n.manualConnWifiStatusError('$e'))));
      }
    } finally {
      if (mounted) setState(() => _wifiBusy = false);
    }
  }

  Future<void> _postStaDisconnect() async {
    final base = _normalizedBase();
    if (base == null) {
      ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.manualConnEnterUrlSnack)));
      return;
    }
    setState(() => _wifiBusy = true);
    try {
      await ref.read(boardApiClientProvider).postWiFiDisconnect(base);
      if (mounted)
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(context.l10n.manualConnStaDisconnectedSnack)));
    } catch (e) {
      if (mounted)
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('$e')));
    } finally {
      if (mounted) setState(() => _wifiBusy = false);
      await _refreshWiFiStatus();
    }
  }

  Future<void> _postWifiClearNvs() async {
    final base = _normalizedBase();
    if (base == null) {
      ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.manualConnEnterUrlSnack)));
      return;
    }
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final l = ctx.l10n;
        return AlertDialog(
          title: Text(l.manualConnClearWifiTitle),
          content: Text(l.manualConnClearWifiBody),
          actions: [
            TextButton(
                onPressed: () => Navigator.pop(ctx, false),
                child: Text(l.commonCancel)),
            TextButton(
                onPressed: () => Navigator.pop(ctx, true),
                child: Text(l.manualConnClearConfirmAction)),
          ],
        );
      },
    );
    if (ok != true) return;
    setState(() => _wifiBusy = true);
    try {
      await ref.read(boardApiClientProvider).postWiFiClear(base);
      if (mounted)
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(context.l10n.manualConnNvsClearedSnack)));
    } catch (e) {
      if (mounted)
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('$e')));
    } finally {
      if (mounted) setState(() => _wifiBusy = false);
      await _refreshWiFiStatus();
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final session = ref.watch(boardSessionNotifierProvider);
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;
    final canSta = _normalizedBase() != null;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.manualConnTitle)),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          TextField(
            controller: _url,
            decoration: InputDecoration(
              labelText: devMode
                  ? l10n.manualConnUrlLabelDev
                  : l10n.manualConnUrlLabelUser,
              hintText:
                  devMode ? l10n.manualConnUrlHintDev : l10n.manualConnUrlHintUser,
              border: const OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
            onChanged: (_) => setState(() {}),
          ),
          const SizedBox(height: 12),
          FilledButton(
            onPressed: () async {
              final n = normalizeBoardHttpBaseUrl(_url.text);
              if (n == null) {
                if (context.mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text(context.l10n.settingsInvalidUrlSnack)),
                  );
                }
                return;
              }
              await ref.read(prefsRepositoryProvider).setLastBoardBaseUrl(n);
              _url.text = n;
              if (context.mounted) {
                ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text(context.l10n.manualConnSaveUrlSnack)));
              }
            },
            child: Text(l10n.manualConnSaveUrl),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: () async {
              final u = _normalizedBase();
              if (u == null) return;
              final sw = Stopwatch()..start();
              try {
                await ref
                    .read(boardApiClientProvider)
                    .fetchSnapshotIfChanged(u);
                sw.stop();
                setState(() => _ping = '${sw.elapsedMilliseconds} ms');
              } catch (e) {
                setState(() => _ping = devMode
                    ? context.l10n.newGameErrorSnack('$e')
                    : context.l10n.manualConnTestFailedUser);
              }
            },
            child: Text(l10n.manualConnTestConnection),
          ),
          if (_ping.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 12),
              child: Text(_ping),
            ),
          const SizedBox(height: 16),
          FilledButton.tonal(
            onPressed: session.busy || _normalizedBase() == null
                ? null
                : () async {
                    final u = _normalizedBase()!;
                    await ref
                        .read(boardSessionNotifierProvider.notifier)
                        .connectWifi(u);
                    if (context.mounted) Navigator.pop(context);
                  },
            child: Text(l10n.manualConnConnectSession),
          ),
          const Divider(height: 40),
          Text(l10n.manualConnWifiStaNvs,
              style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          Text(
            devMode
                ? l10n.manualConnStaSectionDev
                : l10n.manualConnStaSectionUser,
            style: Theme.of(context).textTheme.bodySmall,
          ),
          const SizedBox(height: 12),
          if (session.transport == BoardTransport.ble) ...[
            Material(
              color: Theme.of(context).colorScheme.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(8),
              child: ListTile(
                dense: true,
                leading: const Icon(Icons.info_outline),
                title: Text(l10n.manualConnBleInfoTile),
              ),
            ),
            const SizedBox(height: 12),
          ],
          if (_wifiStatus != null) ...[
            Text(
              devMode
                  ? 'STA: ${_wifiStatus!.staSsid.isEmpty ? '—' : _wifiStatus!.staSsid} · ${_wifiStatus!.staIp} · ${_wifiStatus!.staConnected ? "online" : "offline"}'
                  : 'STA: ${_wifiStatus!.staConnected ? "online" : "offline"}',
            ),
            Text(
              devMode
                  ? 'AP: ${_wifiStatus!.apSsid.isEmpty ? '—' : _wifiStatus!.apSsid} · ${_wifiStatus!.apIp} · klientů: ${_wifiStatus!.apClients}'
                  : 'AP: ${_wifiStatus!.apSsid.isEmpty ? 'aktivní' : _wifiStatus!.apSsid} · klientů: ${_wifiStatus!.apClients}',
            ),
            const SizedBox(height: 8),
          ],
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              OutlinedButton(
                onPressed: !_wifiBusy && canSta ? _refreshWiFiStatus : null,
                child: Text(l10n.manualConnRefreshWifi),
              ),
              OutlinedButton(
                onPressed: !_wifiBusy && canSta ? _postStaDisconnect : null,
                child: Text(l10n.manualConnDisconnectSta),
              ),
              OutlinedButton(
                onPressed: !_wifiBusy && canSta ? _postWifiClearNvs : null,
                child: Text(l10n.manualConnClearWifiNvs),
              ),
            ],
          ),
          const SizedBox(height: 24),
        ],
      ),
    );
  }
}
