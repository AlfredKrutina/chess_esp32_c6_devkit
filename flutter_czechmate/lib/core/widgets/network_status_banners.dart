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

/// Where the connectivity icon sits. The hint itself is only an icon until tapped.
enum NetworkBannerPresentation {
  /// Icon aligned to the start of a scrolling page.
  flow,

  /// Icon aligned to the end, meant for a corner overlay.
  overlay,
}

/// Hints for Stockfish / chess-api when connectivity is limited.
class NetworkStatusBanners extends ConsumerWidget {
  const NetworkStatusBanners({
    super.key,
    this.showWifiLanHint = true,
    this.presentation = NetworkBannerPresentation.flow,
  });

  final bool showWifiLanHint;
  final NetworkBannerPresentation presentation;

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

        final IconData icon;
        final Color color;
        final String title;
        final String body;
        if (!online) {
          icon = Icons.cloud_off_rounded;
          color = cs.error;
          title = l10n.netNoInternetTitle;
          body = l10n.netNoInternetBody;
        } else if (showWifiLanHint && !wifi) {
          icon = Icons.network_cell_rounded;
          color = cs.primary;
          title = l10n.netNotOnWifiTitle;
          body = l10n.netNotOnWifiBody;
        } else {
          return const SizedBox.shrink();
        }

        return Align(
          alignment: presentation == NetworkBannerPresentation.overlay
              ? Alignment.centerRight
              : Alignment.centerLeft,
          child: IconButton(
            tooltip: title,
            color: color,
            onPressed: () {
              showDialog<void>(
                context: context,
                builder: (ctx) => AlertDialog(
                  title: Text(title),
                  content: Text(body),
                  actions: [
                    TextButton(
                      onPressed: () => Navigator.of(ctx).pop(),
                      child: Text(
                        MaterialLocalizations.of(ctx).okButtonLabel,
                      ),
                    ),
                  ],
                ),
              );
            },
            icon: Icon(icon),
          ),
        );
      },
    );
  }
}
