import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  late final TextEditingController _url;
  late final TextEditingController _coachKey;

  @override
  void initState() {
    super.initState();
    final p = ref.read(prefsRepositoryProvider);
    _url = TextEditingController(text: p.lastBoardBaseUrl ?? '');
    _coachKey = TextEditingController(text: p.coachApiKey ?? '');
  }

  @override
  void dispose() {
    _url.dispose();
    _coachKey.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Nastavení')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          TextField(
            controller: _url,
            decoration: const InputDecoration(
              labelText: 'Výchozí URL desky (pro obnovení)',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
          ),
          const SizedBox(height: 12),
          FilledButton(
            onPressed: () async {
              final v = _url.text.trim();
              await ref.read(prefsRepositoryProvider).setLastBoardBaseUrl(v);
              if (context.mounted) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Uloženo')),
                );
              }
            },
            child: const Text('Uložit URL'),
          ),
          const Divider(height: 40),
          const Text('AI coach (volitelné OpenAI)'),
          const SizedBox(height: 8),
          TextField(
            controller: _coachKey,
            obscureText: true,
            decoration: const InputDecoration(
              labelText: 'API klíč (skladuje se lokálně)',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 8),
          FilledButton.tonal(
            onPressed: () async {
              await ref
                  .read(prefsRepositoryProvider)
                  .setCoachApiKey(_coachKey.text.trim());
              if (context.mounted) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Klíč uložen')),
                );
              }
            },
            child: const Text('Uložit klíč'),
          ),
        ],
      ),
    );
  }
}
