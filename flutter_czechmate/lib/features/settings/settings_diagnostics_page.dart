import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import 'board_device_features_view.dart';
import 'developer_settings_view.dart';

class SettingsDiagnosticsPage extends ConsumerWidget {
  const SettingsDiagnosticsPage({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsTileNvsDiagTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 32),
        children: [
          Text(
            l10n.settingsTileNvsDiagSubtitle,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 16),
          Card(
            margin: EdgeInsets.zero,
            clipBehavior: Clip.antiAlias,
            child: Column(
              children: [
                ListTile(
                  leading: const Icon(Icons.memory_outlined),
                  title: Text(l10n.settingsNavBoardNvs),
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () => Navigator.push<void>(
                    context,
                    MaterialPageRoute<void>(
                      builder: (_) => const BoardDeviceFeaturesView(),
                    ),
                  ),
                ),
                if (devMode) ...[
                  const Divider(height: 1),
                  ListTile(
                    leading: Icon(Icons.bug_report_outlined,
                        color: Theme.of(context).colorScheme.tertiary),
                    title: Text(l10n.settingsNavDeveloperDiag),
                    trailing: const Icon(Icons.chevron_right),
                    onTap: () => Navigator.push<void>(
                      context,
                      MaterialPageRoute<void>(
                        builder: (_) => const DeveloperSettingsView(),
                      ),
                    ),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}
