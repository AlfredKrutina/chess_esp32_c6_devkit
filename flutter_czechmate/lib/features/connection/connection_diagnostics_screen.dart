import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/localization/context_l10n.dart';
import '../../core/utils/user_facing_error_message.dart';
import 'board_session_notifier.dart';

/// Parita `AdvancedConnectionDiagnosticsView` — REST poll / WS souhrn.
class ConnectionDiagnosticsScreen extends ConsumerWidget {
  const ConnectionDiagnosticsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final s = ref.watch(boardSessionNotifierProvider);
    return Scaffold(
      appBar: AppBar(title: Text(l10n.connDiagTitle)),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          ListTile(
            title: Text(l10n.connDiagTransport),
            subtitle: Text(s.transport.name),
          ),
          ListTile(
            title: Text(l10n.connDiagWifiBaseUrl),
            subtitle: Text(s.wifiBaseUrl ?? l10n.transportShortDash),
          ),
          ListTile(
            title: Text(l10n.connDiagPolling),
            subtitle:
                Text(s.pollingActive ? l10n.connDiagActive : l10n.connDiagOff),
          ),
          ListTile(
            title: Text(l10n.connDiagWebSocket),
            subtitle: Text(
                s.webSocketActive ? l10n.connDiagActive : l10n.connDiagOff),
          ),
          ListTile(
            title: Text(l10n.connDiagPollSuccessTitle),
            subtitle: Text('${s.pollSuccessCount}'),
          ),
          ListTile(
            title: Text(l10n.connDiagPollFailureTitle),
            subtitle: Text('${s.pollFailureCount}'),
          ),
          ListTile(
            title: Text(l10n.connDiagWsFramesTitle),
            subtitle: Text('${s.wsMessageCount}'),
          ),
          ListTile(
            title: Text(l10n.connDiagLastPollOk),
            subtitle: Text(
              s.lastSuccessfulPoll?.toIso8601String() ??
                  l10n.transportShortDash,
            ),
          ),
          if (s.lastError != null)
            ListTile(
              title: Text(l10n.connDiagLastErrorTitle),
              subtitle: Text(userFacingErrorSummary(l10n, s.lastError)),
              textColor: Theme.of(context).colorScheme.error,
            ),
        ],
      ),
    );
  }
}
