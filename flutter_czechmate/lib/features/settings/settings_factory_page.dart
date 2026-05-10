import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../core/widgets/glass_snackbar.dart';

class SettingsFactoryPage extends ConsumerWidget {
  const SettingsFactoryPage({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsFactoryTileTitle)),
      body: desktopSettingsDetailBody(
        ListView(
          padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
          children: [
            Text(
              l10n.settingsFactoryTileSubtitle,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: cs.onSurfaceVariant,
                  ),
            ),
            const SizedBox(height: 24),
            OutlinedButton(
              style: OutlinedButton.styleFrom(foregroundColor: cs.error),
              onPressed: () async {
                final confirm = await showDialog<bool>(
                  context: context,
                  builder: (ctx) => AlertDialog(
                    title: Text(l10n.settingsFactoryDialogTitle),
                    content: Text(l10n.settingsFactoryDialogBody),
                    actions: [
                      TextButton(
                        onPressed: () => Navigator.pop(ctx, false),
                        child: Text(l10n.commonCancel),
                      ),
                      TextButton(
                        onPressed: () => Navigator.pop(ctx, true),
                        child: Text(
                          l10n.settingsFactoryErase,
                          style: TextStyle(color: cs.error),
                        ),
                      ),
                    ],
                  ),
                );
                if (confirm == true && context.mounted) {
                  final baseUrl =
                      normalizeBoardHttpBaseUrl(prefs.lastBoardBaseUrl);
                  if (baseUrl != null) {
                    try {
                      await ref
                          .read(boardApiClientProvider)
                          .postFactoryReset(baseUrl);
                      if (context.mounted) {
                        showAppSnackBar(
                          context,
                          l10n.settingsFactoryCommandSent,
                        );
                      }
                    } catch (e) {
                      if (context.mounted) {
                        showAppSnackBar(
                          context,
                          l10n.settingsFactoryError(
                            userFacingErrorSummary(l10n, e),
                          ),
                          errorStyle: true,
                        );
                      }
                    }
                  }
                }
              },
              child: Text(l10n.settingsFactoryRunButton),
            ),
          ],
        ),
      ),
    );
  }
}
