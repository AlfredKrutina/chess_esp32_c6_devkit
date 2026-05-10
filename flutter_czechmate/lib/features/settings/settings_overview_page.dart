import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';

class SettingsOverviewPage extends ConsumerWidget {
  const SettingsOverviewPage({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsOverviewTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
        children: [
          Text(
            l10n.settingsOverviewBody,
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          const SizedBox(height: 16),
          OutlinedButton.icon(
            onPressed: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.game,
            icon: const Icon(Icons.grid_on),
            label: Text(l10n.settingsGoToPlayTab),
          ),
        ],
      ),
    );
  }
}
