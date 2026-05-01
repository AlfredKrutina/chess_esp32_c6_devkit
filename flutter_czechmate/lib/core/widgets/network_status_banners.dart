import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../localization/context_l10n.dart';

bool networkResultsSatisfied(List<ConnectivityResult> results) {
  if (results.isEmpty) return false;
  return results.any((r) => r != ConnectivityResult.none);
}

bool networkResultsWifi(List<ConnectivityResult> results) {
  return results.contains(ConnectivityResult.wifi);
}

/// Hints for Stockfish / chess-api when connectivity is limited.
class NetworkStatusBanners extends ConsumerWidget {
  const NetworkStatusBanners({super.key, this.showWifiLanHint = true});

  final bool showWifiLanHint;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final async = ref.watch(networkConnectivityProvider);
    return async.when(
      loading: () => const SizedBox.shrink(),
      error: (_, __) => const SizedBox.shrink(),
      data: (results) {
        final l10n = context.l10n;
        final online = networkResultsSatisfied(results);
        final wifi = networkResultsWifi(results);
        final cs = Theme.of(context).colorScheme;
        final children = <Widget>[];

        if (!online) {
          children.add(
            Material(
              color: cs.errorContainer,
              borderRadius: BorderRadius.circular(12),
              child: ListTile(
                leading: Icon(Icons.cloud_off, color: cs.onErrorContainer),
                title: Text(
                  l10n.netNoInternetTitle,
                  style: TextStyle(
                    color: cs.onErrorContainer,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                subtitle: Text(
                  l10n.netNoInternetBody,
                  style: TextStyle(color: cs.onErrorContainer),
                ),
              ),
            ),
          );
        } else if (showWifiLanHint && !wifi) {
          children.add(
            Material(
              color: cs.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(12),
              child: ListTile(
                leading: Icon(Icons.network_cell, color: cs.primary),
                title: Text(l10n.netNotOnWifiTitle),
                subtitle: Text(l10n.netNotOnWifiBody),
              ),
            ),
          );
        }

        if (children.isEmpty) return const SizedBox.shrink();
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            for (var i = 0; i < children.length; i++) ...[
              if (i > 0) const SizedBox(height: 8),
              children[i],
            ],
          ],
        );
      },
    );
  }
}
