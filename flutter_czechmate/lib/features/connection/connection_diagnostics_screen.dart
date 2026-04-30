import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'board_session_notifier.dart';

/// Parita `AdvancedConnectionDiagnosticsView` — REST poll / WS souhrn.
class ConnectionDiagnosticsScreen extends ConsumerWidget {
  const ConnectionDiagnosticsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final s = ref.watch(boardSessionNotifierProvider);
    return Scaffold(
      appBar: AppBar(title: const Text('Diagnostika připojení')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          ListTile(
            title: const Text('Transport'),
            subtitle: Text(s.transport.name),
          ),
          ListTile(
            title: const Text('Wi‑Fi base URL'),
            subtitle: Text(s.wifiBaseUrl ?? '—'),
          ),
          ListTile(
            title: const Text('Polling'),
            subtitle: Text(s.pollingActive ? 'aktivní' : 'vypnuto'),
          ),
          ListTile(
            title: const Text('WebSocket'),
            subtitle: Text(s.webSocketActive ? 'aktivní' : 'vypnuto'),
          ),
          ListTile(
            title: const Text('Úspěšné snapshot GET (včetně 304)'),
            subtitle: Text('${s.pollSuccessCount}'),
          ),
          ListTile(
            title: const Text('Chybné GET / výjimky při poll'),
            subtitle: Text('${s.pollFailureCount}'),
          ),
          ListTile(
            title: const Text('WS zprávy (rámce)'),
            subtitle: Text('${s.wsMessageCount}'),
          ),
          ListTile(
            title: const Text('Poslední úspěšný poll'),
            subtitle: Text(
              s.lastSuccessfulPoll?.toIso8601String() ?? '—',
            ),
          ),
          if (s.lastError != null)
            ListTile(
              title: const Text('Poslední chyba'),
              subtitle: Text('${s.lastError}'),
              textColor: Theme.of(context).colorScheme.error,
            ),
        ],
      ),
    );
  }
}
