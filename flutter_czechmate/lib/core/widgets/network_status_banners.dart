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

/// How connectivity hints occupy space in the widget tree.
enum NetworkBannerPresentation {
  /// Full-width cards that participate in layout (legacy).
  flow,

  /// Compact strip meant for a top overlay — avoids shrinking the board area.
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
        final children = <Widget>[];

        if (!online) {
          children.add(
            presentation == NetworkBannerPresentation.overlay
                ? _OverlayBanner(
                    color: cs.errorContainer,
                    foreground: cs.onErrorContainer,
                    icon: Icons.cloud_off_rounded,
                    text:
                        '${l10n.netNoInternetTitle} · ${l10n.netNoInternetBody}',
                  )
                : Material(
                    color: cs.errorContainer,
                    borderRadius: BorderRadius.circular(12),
                    child: ListTile(
                      leading:
                          Icon(Icons.cloud_off, color: cs.onErrorContainer),
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
            presentation == NetworkBannerPresentation.overlay
                ? _OverlayBanner(
                    color: cs.surfaceContainerHighest,
                    foreground: cs.onSurface,
                    icon: Icons.network_cell_rounded,
                    text:
                        '${l10n.netNotOnWifiTitle} · ${l10n.netNotOnWifiBody}',
                  )
                : Material(
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
          mainAxisSize: MainAxisSize.min,
          children: [
            for (var i = 0; i < children.length; i++) ...[
              if (i > 0)
                SizedBox(
                    height: presentation == NetworkBannerPresentation.overlay
                        ? 6
                        : 8),
              children[i],
            ],
          ],
        );
      },
    );
  }
}

class _OverlayBanner extends StatelessWidget {
  const _OverlayBanner({
    required this.color,
    required this.foreground,
    required this.icon,
    required this.text,
  });

  final Color color;
  final Color foreground;
  final IconData icon;
  final String text;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: color,
      elevation: 3,
      shadowColor: Colors.black38,
      borderRadius: BorderRadius.circular(10),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(icon, size: 18, color: foreground),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                text,
                maxLines: 3,
                overflow: TextOverflow.ellipsis,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: foreground,
                      height: 1.25,
                      fontSize: 12.5,
                    ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
