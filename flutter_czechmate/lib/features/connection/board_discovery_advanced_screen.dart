import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/platform/flutter_blue_plus_host_supported.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../l10n/app_localizations.dart';
import '../settings/manual_connection_screen.dart';
import 'board_session_notifier.dart';
import 'board_session_state.dart';
import 'widgets/discovery_connection_recovery_card.dart';

bool _sessionConnectedForAdvanced(BoardSessionState s) {
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

/// Wi‑Fi URL, uložené BLE a jednorázové přepnutí transportu — odděleně od hlavního skenu.
class BoardDiscoveryAdvancedScreen extends ConsumerStatefulWidget {
  const BoardDiscoveryAdvancedScreen({super.key});

  @override
  ConsumerState<BoardDiscoveryAdvancedScreen> createState() =>
      _BoardDiscoveryAdvancedScreenState();
}

class _BoardDiscoveryAdvancedScreenState
    extends ConsumerState<BoardDiscoveryAdvancedScreen> {
  late final TextEditingController _url;
  late final TextEditingController _blockedOctets;
  bool _showConnectionRecovery = false;

  void _onConnectionAttemptFailed(AppLocalizations l10n) {
    final err = ref.read(boardSessionNotifierProvider).lastError;
    var msg = userFacingErrorSummary(l10n, err);
    if (msg.isEmpty) msg = l10n.userFacingErrGeneric;
    setState(() => _showConnectionRecovery = true);
    showAppSnackBar(context, msg, errorStyle: true);
  }

  @override
  void initState() {
    super.initState();
    final prefs = ref.read(prefsRepositoryProvider);
    _url = TextEditingController(text: prefs.lastBoardBaseUrl ?? '');
    _blockedOctets = TextEditingController(
      text: prefs.wifiBlockedThirdOctetsEditingText(),
    );
  }

  @override
  void dispose() {
    _url.dispose();
    _blockedOctets.dispose();
    super.dispose();
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
        title: Text(l10n.settingsAdvanced),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Text(
            l10n.discoveryTransportAdvancedExplanation,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: cs.onSurfaceVariant,
            ),
          ),
          if (_showConnectionRecovery) ...[
            const SizedBox(height: 16),
            DiscoveryConnectionRecoveryCard(
              l10n: l10n,
              onDismiss: () => setState(() => _showConnectionRecovery = false),
            ),
          ],
          const SizedBox(height: 16),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text(
                    l10n.discoveryAdvancedSubtitle,
                    style: theme.textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 12),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: TextButton.icon(
                      onPressed: () {
                        Navigator.of(context).pop();
                        ref.read(mainNavTabIndexProvider.notifier).state =
                            AppMainTab.settings;
                      },
                      icon: const Icon(Icons.settings_outlined, size: 20),
                      label: Text(l10n.discoveryOpenTransportSettingsButton),
                    ),
                  ),
                  const SizedBox(height: 8),
                  if (isFlutterBluePlusHostSupported &&
                      prefs.lastBleRemoteId != null &&
                      prefs.lastBleRemoteId!.isNotEmpty)
                    OutlinedButton.icon(
                      onPressed: () async {
                        final n =
                            ref.read(boardSessionNotifierProvider.notifier);
                        if (ref.read(boardSessionNotifierProvider).busy) {
                          await n.disconnectAwaitBle();
                        }
                        await n.reconnectSavedBle();
                        if (!context.mounted) return;
                        if (_sessionConnectedForAdvanced(
                            ref.read(boardSessionNotifierProvider))) {
                          setState(() => _showConnectionRecovery = false);
                          closeBoardDiscoveryAndFocusPlay(ref, context);
                        } else {
                          _onConnectionAttemptFailed(l10n);
                        }
                      },
                      icon: const Icon(Icons.bluetooth_connected),
                      label: Text(l10n.discoveryReconnectSavedBoard),
                    ),
                  if (isFlutterBluePlusHostSupported &&
                      prefs.lastBleRemoteId != null &&
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
                  Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      l10n.discoveryAdvancedBlockedOctetsTitle,
                      style: theme.textTheme.titleSmall?.copyWith(
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    l10n.discoveryAdvancedBlockedOctetsBody,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: cs.onSurfaceVariant,
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextField(
                    controller: _blockedOctets,
                    decoration: InputDecoration(
                      labelText: l10n.discoveryAdvancedBlockedOctetsLabel,
                      hintText: l10n.discoveryAdvancedBlockedOctetsHint,
                      border: const OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 10),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      FilledButton.tonal(
                        onPressed: () async {
                          await ref
                              .read(prefsRepositoryProvider)
                              .setWifiBlockedThirdOctetsFromUi(
                                _blockedOctets.text,
                              );
                          await ref
                              .read(boardSessionNotifierProvider.notifier)
                              .syncWifiStaIpBlockToBoardIfBle();
                          if (!context.mounted) return;
                          showAppSnackBar(
                            context,
                            l10n.discoveryAdvancedBlockedOctetsSavedSnack,
                          );
                        },
                        child: Text(l10n.discoveryAdvancedBlockedOctetsSave),
                      ),
                      OutlinedButton(
                        onPressed: () async {
                          await ref
                              .read(prefsRepositoryProvider)
                              .clearWifiBlockedThirdOctetsToDefault();
                          setState(() => _blockedOctets.text = '88');
                          await ref
                              .read(boardSessionNotifierProvider.notifier)
                              .syncWifiStaIpBlockToBoardIfBle();
                          if (!context.mounted) return;
                          showAppSnackBar(
                            context,
                            l10n.discoveryAdvancedBlockedOctetsSavedSnack,
                          );
                        },
                        child: Text(l10n.discoveryAdvancedBlockedOctetsReset),
                      ),
                    ],
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
                  PressableScale(
                    child: FilledButton(
                      onPressed: () async {
                        final n =
                            ref.read(boardSessionNotifierProvider.notifier);
                        if (ref.read(boardSessionNotifierProvider).busy) {
                          await n.disconnectAwaitBle();
                        }
                        final u =
                            _url.text.trim().replaceAll(RegExp(r'/$'), '');
                        if (u.isEmpty) {
                          if (!context.mounted) return;
                          showAppSnackBar(
                            context,
                            l10n.discoveryEnterBoardUrlSnack,
                            errorStyle: true,
                          );
                          return;
                        }
                        await n.connectWifi(u);
                        if (!context.mounted) return;
                        if (_sessionConnectedForAdvanced(
                            ref.read(boardSessionNotifierProvider))) {
                          setState(() => _showConnectionRecovery = false);
                          closeBoardDiscoveryAndFocusPlay(ref, context);
                        } else {
                          _onConnectionAttemptFailed(l10n);
                        }
                      },
                      child: Text(l10n.discoveryConnectWifi),
                    ),
                  ),
                  const SizedBox(height: 8),
                  if (devMode)
                    OutlinedButton(
                      onPressed: () async {
                        final n =
                            ref.read(boardSessionNotifierProvider.notifier);
                        if (ref.read(boardSessionNotifierProvider).busy) {
                          await n.disconnectAwaitBle();
                        }
                        await n.connectMock();
                        if (!context.mounted) return;
                        if (_sessionConnectedForAdvanced(
                            ref.read(boardSessionNotifierProvider))) {
                          closeBoardDiscoveryAndFocusPlay(ref, context);
                        }
                      },
                      child: Text(l10n.mockBoardTileTitle),
                    ),
                  if (devMode) const SizedBox(height: 8),
                  if (devMode || session.bleApBroadcasting)
                    OutlinedButton.icon(
                      onPressed: () async {
                        final n =
                            ref.read(boardSessionNotifierProvider.notifier);
                        if (ref.read(boardSessionNotifierProvider).busy) {
                          await n.disconnectAwaitBle();
                        }
                        await n.connectWifi('http://192.168.4.1');
                        if (!context.mounted) return;
                        if (_sessionConnectedForAdvanced(
                            ref.read(boardSessionNotifierProvider))) {
                          setState(() => _showConnectionRecovery = false);
                          closeBoardDiscoveryAndFocusPlay(ref, context);
                        } else {
                          _onConnectionAttemptFailed(l10n);
                        }
                      },
                      icon: const Icon(Icons.wifi_tethering),
                      label: Text(l10n.discoveryBoardHotspotButton),
                    ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
