import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';

class SettingsLanguagePage extends ConsumerStatefulWidget {
  const SettingsLanguagePage({super.key});

  @override
  ConsumerState<SettingsLanguagePage> createState() =>
      _SettingsLanguagePageState();
}

class _SettingsLanguagePageState extends ConsumerState<SettingsLanguagePage> {
  @override
  Widget build(BuildContext context) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsLanguage)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
        children: [
          Text(
            l10n.languageDescription,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 16),
          DropdownButtonFormField<String>(
            initialValue: prefs.uiLanguage,
            decoration: InputDecoration(
              labelText: l10n.settingsLanguage,
              border: const OutlineInputBorder(),
            ),
            items: [
              DropdownMenuItem(
                  value: 'system', child: Text(l10n.languageSystem)),
              DropdownMenuItem(value: 'en', child: Text(l10n.languageEnglish)),
              DropdownMenuItem(value: 'cs', child: Text(l10n.languageCzech)),
            ],
            onChanged: (v) async {
              if (v != null) {
                await prefs.setUiLanguage(v);
                ref.invalidate(prefsRepositoryProvider);
                setState(() {});
              }
            },
          ),
        ],
      ),
    );
  }
}
