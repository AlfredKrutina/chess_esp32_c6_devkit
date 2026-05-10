import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

import '../../../l10n/app_localizations.dart';

/// Compact checklist after a failed connection attempt from discovery flows.
class DiscoveryConnectionRecoveryCard extends StatelessWidget {
  const DiscoveryConnectionRecoveryCard({
    super.key,
    required this.l10n,
    required this.onDismiss,
  });

  final AppLocalizations l10n;
  final VoidCallback onDismiss;

  static Widget _bullet(BuildContext context, String text) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('• ', style: theme.textTheme.bodyMedium),
          Expanded(child: Text(text, style: theme.textTheme.bodyMedium)),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final cs = theme.colorScheme;

    return Card(
      elevation: 0,
      color: cs.errorContainer.withValues(alpha: 0.35),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: BorderSide(color: cs.outlineVariant),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Icon(Icons.troubleshoot_outlined, color: cs.primary),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    l10n.discoveryRecoveryTitle,
                    style: theme.textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            _bullet(context, l10n.discoveryRecoveryBulletBt),
            _bullet(context, l10n.discoveryRecoveryBulletRange),
            _bullet(context, l10n.discoveryRecoveryBulletHomeWifi),
            _bullet(context, l10n.discoveryRecoveryBulletManual),
            const SizedBox(height: 8),
            Wrap(
              spacing: 8,
              runSpacing: 4,
              alignment: WrapAlignment.end,
              children: [
                TextButton(
                  onPressed: () async {
                    await openAppSettings();
                  },
                  child: Text(l10n.discoveryRecoveryOpenAppSettings),
                ),
                FilledButton.tonal(
                  onPressed: onDismiss,
                  child: Text(l10n.discoveryRecoveryDismiss),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
