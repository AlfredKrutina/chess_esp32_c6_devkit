import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/debug/connection_debug_log.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/layout/form_factor.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/utils/user_facing_error_message.dart';
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
        final l10n = context.l10n;
        showAppSnackBar(
          context,
          l10n.manualConnWifiStatusError(userFacingErrorSummary(l10n, e)),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _wifiBusy = false);
    }
  }

  Future<void> _postStaDisconnect() async {
    final base = _normalizedBase();
    if (base == null) {
      showAppSnackBar(
        context,
        context.l10n.manualConnEnterUrlSnack,
        errorStyle: true,
      );
      return;
    }
    setState(() => _wifiBusy = true);
    try {
      await ref.read(boardApiClientProvider).postWiFiDisconnect(base);
      if (mounted) {
        showAppSnackBar(context, context.l10n.manualConnStaDisconnectedSnack);
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        showAppSnackBar(
          context,
          userFacingErrorSummary(l10n, e),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _wifiBusy = false);
      await _refreshWiFiStatus();
    }
  }

  Future<void> _postWifiClearNvs() async {
    final base = _normalizedBase();
    if (base == null) {
      showAppSnackBar(
        context,
        context.l10n.manualConnEnterUrlSnack,
        errorStyle: true,
      );
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
      if (mounted) {
        showAppSnackBar(context, context.l10n.manualConnNvsClearedSnack);
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        showAppSnackBar(
          context,
          userFacingErrorSummary(l10n, e),
          errorStyle: true,
        );
      }
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
      body: FocusTraversalGroup(
        child: desktopFormDetailBody(
          ListView(
            padding: const EdgeInsets.all(16),
            children: [
              TextField(
                controller: _url,
                decoration: InputDecoration(
                  labelText: devMode
                      ? l10n.manualConnUrlLabelDev
                      : l10n.manualConnUrlLabelUser,
                  hintText: devMode
                      ? l10n.manualConnUrlHintDev
                      : l10n.manualConnUrlHintUser,
                  border: const OutlineInputBorder(),
                ),
                keyboardType: TextInputType.url,
                onChanged: (_) => setState(() {}),
              ),
              const SizedBox(height: 12),
              PressableScale(
                child: FilledButton(
                  onPressed: () async {
                    final n = normalizeBoardHttpBaseUrl(_url.text);
                    if (n == null) {
                      if (context.mounted) {
                        showAppSnackBar(
                          context,
                          context.l10n.settingsInvalidUrlSnack,
                          errorStyle: true,
                        );
                      }
                      return;
                    }
                    await ref
                        .read(prefsRepositoryProvider)
                        .setLastBoardBaseUrl(n);
                    _url.text = n;
                    if (context.mounted) {
                      showAppSnackBar(
                          context, context.l10n.manualConnSaveUrlSnack);
                    }
                  },
                  child: Text(l10n.manualConnSaveUrl),
                ),
              ),
              const SizedBox(height: 8),
              OutlinedButton(
                onPressed: () async {
                  final u = _normalizedBase();
                  if (u == null) return;
                  final l10nCatch = context.l10n;
                  final sw = Stopwatch()..start();
                  connDebugLog('Manual URL GET snapshot test', 'GET $u');
                  try {
                    await ref
                        .read(boardApiClientProvider)
                        .fetchSnapshotIfChanged(u);
                    sw.stop();
                    connDebugLog(
                      'Manual URL test OK',
                      '${sw.elapsedMilliseconds} ms',
                    );
                    if (!mounted) return;
                    setState(() => _ping = '${sw.elapsedMilliseconds} ms');
                  } catch (e) {
                    connDebugLog('Manual URL test FAIL', '$e');
                    if (!mounted) return;
                    setState(() => _ping = devMode
                        ? l10nCatch.newGameErrorSnack(
                            userFacingErrorSummary(l10nCatch, e))
                        : l10nCatch.manualConnTestFailedUser);
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
              PressableScale(
                child: FilledButton.tonal(
                  onPressed: session.busy || _normalizedBase() == null
                      ? null
                      : () async {
                          final u = _normalizedBase()!;
                          connDebugLog(
                            'Manual screen → connectWifi(session)',
                            u,
                          );
                          await ref
                              .read(boardSessionNotifierProvider.notifier)
                              .connectWifi(u);
                          if (context.mounted) Navigator.pop(context);
                        },
                  child: Text(l10n.manualConnConnectSession),
                ),
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
                          '${_wifiStatus!.staBlkOct.isEmpty ? '' : ' · DHCP blok 3. okt.: ${_wifiStatus!.staBlkOct}'}'
                      : 'STA: ${_wifiStatus!.staConnected ? "online" : "offline"}',
                ),
                Text(
                  devMode
                      ? (_wifiStatus!.apActive
                          ? 'AP: ${_wifiStatus!.apSsid.isEmpty ? '—' : _wifiStatus!.apSsid} · ${_wifiStatus!.apIp} · klientů: ${_wifiStatus!.apClients}'
                          : 'AP: vypnutý (hotspot na desce nevysílá)')
                      : (_wifiStatus!.apActive
                          ? 'AP: ${_wifiStatus!.apSsid.isEmpty ? 'zapnutý' : _wifiStatus!.apSsid} · klientů: ${_wifiStatus!.apClients}'
                          : 'AP: vypnutý — zapni hotspot z obrazovky Najít desku přes Bluetooth'),
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
        ),
      ),
    );
  }
}
