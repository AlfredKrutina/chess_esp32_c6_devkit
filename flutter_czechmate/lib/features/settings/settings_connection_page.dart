import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/services/prefs_repository.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/state/game_ui_notifier.dart';
import '../help/help_screen.dart';
import 'firmware_update_availability.dart';
import 'manual_connection_screen.dart';

class SettingsConnectionPage extends ConsumerStatefulWidget {
  const SettingsConnectionPage({super.key});

  @override
  ConsumerState<SettingsConnectionPage> createState() =>
      _SettingsConnectionPageState();
}

class _SettingsConnectionPageState
    extends ConsumerState<SettingsConnectionPage> {
  late final TextEditingController _url;
  late final TextEditingController _boardApiToken;

  @override
  void initState() {
    super.initState();
    final p = ref.read(prefsRepositoryProvider);
    _url = TextEditingController(text: p.lastBoardBaseUrl ?? '');
    _boardApiToken = TextEditingController(text: p.boardApiToken ?? '');
  }

  void _syncControllersFromPrefs(PrefsRepository p) {
    _url.text = p.lastBoardBaseUrl ?? '';
    _boardApiToken.text = p.boardApiToken ?? '';
  }

  Future<void> _offerApplicationFactoryReset() async {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final first = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(l10n.settingsAppFactoryResetTitle),
        content: SingleChildScrollView(
          child: Text(l10n.settingsAppFactoryResetSubtitle),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(MaterialLocalizations.of(ctx).cancelButtonLabel),
          ),
          TextButton(
            style:
                TextButton.styleFrom(foregroundColor: theme.colorScheme.error),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(l10n.settingsAppFactoryResetButton),
          ),
        ],
      ),
    );
    if (first != true || !mounted) return;
    final second = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(l10n.settingsAppFactoryResetConfirmTitle),
        content: Text(l10n.settingsAppFactoryResetConfirmBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(l10n.commonNo),
          ),
          FilledButton(
            style: FilledButton.styleFrom(
              backgroundColor: theme.colorScheme.error,
              foregroundColor: theme.colorScheme.onError,
            ),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(l10n.settingsAppFactoryResetConfirmAction),
          ),
        ],
      ),
    );
    if (second != true || !mounted) return;

    ref.read(boardSessionNotifierProvider.notifier).disconnect();
    await ref.read(prefsRepositoryProvider).factoryResetApplicationLocalState();
    ref.invalidate(boardApiClientProvider);
    ref.invalidate(stockfishApiClientProvider);
    ref.invalidate(gameUiNotifierProvider);
    ref.read(connectionModeUiRefreshProvider.notifier).state++;
    await ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh();

    final prefsAfter = ref.read(prefsRepositoryProvider);
    _syncControllersFromPrefs(prefsAfter);
    if (mounted) {
      setState(() {});
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            '${l10n.settingsAppFactoryResetDoneSnack}\n${l10n.settingsAppFactoryResetRestartHint}',
          ),
        ),
      );
    }
  }

  @override
  void dispose() {
    _url.dispose();
    _boardApiToken.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    ref.watch(connectionModeUiRefreshProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final l10n = context.l10n;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsConnectionTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
        children: [
          Align(
            alignment: Alignment.centerLeft,
            child: TextButton.icon(
              icon: const Icon(Icons.help_outline, size: 20),
              label: Text(l10n.settingsConnectionLearnMore),
              onPressed: () => Navigator.push<void>(
                context,
                MaterialPageRoute<void>(
                  builder: (_) => const HelpScreen(),
                ),
              ),
            ),
          ),
          const SizedBox(height: 8),
          PressableScale(
            child: FilledButton.icon(
              onPressed: () => pushBoardDiscoveryRoute(context),
              icon: const Icon(Icons.bluetooth_searching),
              label: Text(l10n.discoveryFindBle),
            ),
          ),
          if (session.transport != BoardTransport.none) ...[
            const SizedBox(height: 10),
            OutlinedButton.icon(
              onPressed: session.busy
                  ? null
                  : () => ref
                      .read(boardSessionNotifierProvider.notifier)
                      .disconnect(),
              icon: const Icon(Icons.link_off),
              label: Text(l10n.settingsDisconnect),
            ),
          ],
          Theme(
            data: Theme.of(context).copyWith(
              dividerColor: Colors.transparent,
            ),
            child: ExpansionTile(
              tilePadding: EdgeInsets.zero,
              title: Text(l10n.settingsAdvanced),
              subtitle: Text(
                l10n.settingsAdvancedConnectionSubtitleV2,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
              ),
              children: [
                Padding(
                  padding: const EdgeInsets.only(bottom: 12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      if (prefs.lastBleRemoteId != null &&
                          prefs.lastBleRemoteId!.isNotEmpty)
                        OutlinedButton.icon(
                          onPressed: session.busy
                              ? null
                              : () async {
                                  try {
                                    await ref
                                        .read(boardSessionNotifierProvider
                                            .notifier)
                                        .reconnectSavedBle();
                                    if (context.mounted) {
                                      ScaffoldMessenger.of(context)
                                          .showSnackBar(
                                        SnackBar(
                                          content: Text(
                                            l10n.settingsReconnectingBle,
                                          ),
                                        ),
                                      );
                                    }
                                  } catch (e) {
                                    if (context.mounted) {
                                      ScaffoldMessenger.of(context)
                                          .showSnackBar(
                                        SnackBar(
                                          content: Text(
                                            userFacingErrorSummary(
                                              l10n,
                                              e,
                                            ),
                                          ),
                                        ),
                                      );
                                    }
                                  }
                                },
                          icon: const Icon(Icons.bluetooth),
                          label: Text(l10n.settingsReconnectSavedBleShort),
                        ),
                      if (prefs.lastBleRemoteId != null &&
                          prefs.lastBleRemoteId!.isNotEmpty)
                        const SizedBox(height: 10),
                      OutlinedButton.icon(
                        onPressed: () => Navigator.push(
                          context,
                          MaterialPageRoute<void>(
                            builder: (_) => const ManualConnectionScreen(),
                          ),
                        ),
                        icon: const Icon(Icons.edit),
                        label: Text(l10n.settingsManualEntry),
                      ),
                      const Divider(height: 28),
                      Text(
                        l10n.settingsSavedDefaultsTitle,
                        style: Theme.of(context)
                            .textTheme
                            .titleSmall
                            ?.copyWith(fontWeight: FontWeight.w600),
                      ),
                      const SizedBox(height: 4),
                      Text(
                        l10n.settingsSavedDefaultsBody,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context)
                                  .colorScheme
                                  .onSurfaceVariant,
                            ),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        l10n.settingsNextConnectionTitle,
                        style: Theme.of(context)
                            .textTheme
                            .titleSmall
                            ?.copyWith(fontWeight: FontWeight.w600),
                      ),
                      const SizedBox(height: 6),
                      Text(
                        l10n.settingsNextConnectionIntro,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context)
                                  .colorScheme
                                  .onSurfaceVariant,
                            ),
                      ),
                      const SizedBox(height: 12),
                      if (prefs.nextConnectionTransportOnce != null)
                        Material(
                          color:
                              Theme.of(context).colorScheme.secondaryContainer,
                          borderRadius: BorderRadius.circular(12),
                          child: Padding(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 10,
                            ),
                            child: Row(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Icon(
                                  Icons.info_outline,
                                  size: 22,
                                  color: Theme.of(context)
                                      .colorScheme
                                      .onSecondaryContainer,
                                ),
                                const SizedBox(width: 10),
                                Expanded(
                                  child: Text(
                                    prefs.nextConnectionTransportOnce ==
                                            'wifi_only'
                                        ? l10n
                                            .settingsNextConnectionActiveWifiOnce
                                        : l10n
                                            .settingsNextConnectionActiveBleOnce,
                                    style: Theme.of(context)
                                        .textTheme
                                        .bodySmall
                                        ?.copyWith(
                                          color: Theme.of(context)
                                              .colorScheme
                                              .onSecondaryContainer,
                                        ),
                                  ),
                                ),
                                TextButton(
                                  onPressed: () async {
                                    await prefs
                                        .setNextConnectionTransportOnce(null);
                                    ref
                                        .read(
                                          connectionModeUiRefreshProvider
                                              .notifier,
                                        )
                                        .state++;
                                  },
                                  child: Text(
                                    l10n.settingsNextConnectionUseAuto,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                      if (prefs.nextConnectionTransportOnce != null)
                        const SizedBox(height: 12),
                      Wrap(
                        spacing: 10,
                        runSpacing: 10,
                        children: [
                          OutlinedButton(
                            onPressed: () async {
                              await prefs
                                  .setNextConnectionTransportOnce('wifi_only');
                              ref
                                  .read(
                                    connectionModeUiRefreshProvider.notifier,
                                  )
                                  .state++;
                            },
                            child: Text(
                              l10n.settingsNextConnectionWifiOnceButton,
                            ),
                          ),
                          OutlinedButton(
                            onPressed: () async {
                              await prefs
                                  .setNextConnectionTransportOnce('ble_only');
                              ref
                                  .read(
                                    connectionModeUiRefreshProvider.notifier,
                                  )
                                  .state++;
                            },
                            child: Text(
                              l10n.settingsNextConnectionBleOnceButton,
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 12),
                      TextField(
                        controller: _url,
                        decoration: InputDecoration(
                          labelText: l10n.settingsDefaultBoardUrl,
                          border: const OutlineInputBorder(),
                        ),
                        keyboardType: TextInputType.url,
                      ),
                      const SizedBox(height: 10),
                      FilledButton(
                        onPressed: () async {
                          final n = normalizeBoardHttpBaseUrl(_url.text);
                          if (n == null) {
                            if (context.mounted) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                SnackBar(
                                  content: Text(
                                    l10n.settingsInvalidUrlSnack,
                                  ),
                                ),
                              );
                            }
                            return;
                          }
                          await prefs.setLastBoardBaseUrl(n);
                          _url.text = n;
                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(content: Text(l10n.settingsSavedSnack)),
                            );
                          }
                        },
                        child: Text(l10n.settingsSaveUrl),
                      ),
                      const SizedBox(height: 16),
                      TextField(
                        controller: _boardApiToken,
                        decoration: InputDecoration(
                          labelText: l10n.settingsBoardApiTokenLabel,
                          helperText: l10n.settingsBoardApiTokenSubtitle,
                          border: const OutlineInputBorder(),
                        ),
                        keyboardType: TextInputType.visiblePassword,
                        autocorrect: false,
                      ),
                      const SizedBox(height: 10),
                      FilledButton.tonal(
                        onPressed: () async {
                          await prefs.setBoardApiToken(
                            _boardApiToken.text,
                          );
                          final saved = prefs.boardApiToken;
                          _boardApiToken.text = saved ?? '';
                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                content: Text(
                                  saved == null || saved.isEmpty
                                      ? l10n.settingsBoardApiTokenClearedSnack
                                      : l10n.settingsBoardApiTokenSavedSnack,
                                ),
                              ),
                            );
                          }
                        },
                        child: Text(l10n.settingsSaveBoardApiToken),
                      ),
                      SwitchListTile(
                        contentPadding: EdgeInsets.zero,
                        title: Text(l10n.settingsWebSocketTitle),
                        subtitle: Text(
                          l10n.settingsWebSocketSubtitle,
                        ),
                        value: prefs.useWebSocket,
                        onChanged: (v) async {
                          await prefs.setUseWebSocket(v);
                          setState(() {});
                        },
                      ),
                      const Divider(height: 32),
                      Text(
                        l10n.settingsAppFactoryResetTitle,
                        style: Theme.of(context)
                            .textTheme
                            .titleSmall
                            ?.copyWith(fontWeight: FontWeight.w600),
                      ),
                      const SizedBox(height: 8),
                      Text(
                        l10n.settingsAppFactoryResetSubtitle,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context)
                                  .colorScheme
                                  .onSurfaceVariant,
                            ),
                      ),
                      const SizedBox(height: 12),
                      OutlinedButton.icon(
                        onPressed:
                            session.busy ? null : _offerApplicationFactoryReset,
                        icon: Icon(
                          Icons.delete_forever_outlined,
                          color: Theme.of(context).colorScheme.error,
                        ),
                        label: Text(l10n.settingsAppFactoryResetButton),
                        style: OutlinedButton.styleFrom(
                          foregroundColor: Theme.of(context).colorScheme.error,
                        ),
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
