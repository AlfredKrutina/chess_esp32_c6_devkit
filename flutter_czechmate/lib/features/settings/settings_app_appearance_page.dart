import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';

class SettingsAppAppearancePage extends ConsumerStatefulWidget {
  const SettingsAppAppearancePage({super.key});

  @override
  ConsumerState<SettingsAppAppearancePage> createState() =>
      _SettingsAppAppearancePageState();
}

class _SettingsAppAppearancePageState
    extends ConsumerState<SettingsAppAppearancePage> {
  @override
  Widget build(BuildContext context) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final l10n = context.l10n;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsAppAppearanceTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
        children: [
          DropdownButtonFormField<String>(
            value: prefs.appearance,
            decoration: InputDecoration(
              labelText: l10n.settingsColorSchemeLabel,
              border: const OutlineInputBorder(),
              helperText: l10n.settingsColorSchemeHelper,
              helperMaxLines: 3,
            ),
            items: [
              DropdownMenuItem(
                  value: 'system', child: Text(l10n.settingsThemeSystem)),
              DropdownMenuItem(
                  value: 'light', child: Text(l10n.settingsThemeLight)),
              DropdownMenuItem(
                  value: 'dark', child: Text(l10n.settingsThemeDark)),
              DropdownMenuItem(
                  value: 'oled', child: Text(l10n.settingsThemeOledBlack)),
            ],
            onChanged: (v) async {
              if (v != null) {
                await prefs.setAppearance(v);
                ref.invalidate(prefsRepositoryProvider);
                setState(() {});
              }
            },
          ),
          const SizedBox(height: 8),
          SwitchListTile(
            title: Text(l10n.settingsHapticsTitle),
            value: prefs.hapticsEnabled,
            onChanged: (v) async {
              await prefs.setHapticsEnabled(v);
              setState(() {});
            },
          ),
          SwitchListTile(
            title: Text(l10n.settingsSoundTitle),
            value: prefs.soundEffectsEnabled,
            onChanged: (v) async {
              await prefs.setSoundEffectsEnabled(v);
              setState(() {});
            },
          ),
          SwitchListTile(
            title: Text(l10n.settingsAutoGameSummaryTitle),
            value: prefs.endgameReportAutoOpen,
            onChanged: (v) async {
              await prefs.setEndgameReportAutoOpen(v);
              setState(() {});
            },
          ),
        ],
      ),
    );
  }
}
