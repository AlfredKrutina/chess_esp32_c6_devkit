import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import 'coach_chat_notifier.dart';
import 'coach_chat_panel.dart';

class CoachScreen extends ConsumerWidget {
  const CoachScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    ref.watch(coachChatProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final level = prefs.coachUserLevel;
    final notifier = ref.read(coachChatProvider.notifier);
    final l10n = context.l10n;

    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.coachScreenTitle),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: Chip(
              label: Text(
                notifier.providerChipLabel(prefs, l10n),
                overflow: TextOverflow.ellipsis,
              ),
              backgroundColor: Theme.of(context).colorScheme.secondaryContainer,
            ),
          ),
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: Chip(
              label: Text(l10n.coachScreenLevelLabel('$level')),
              backgroundColor: Theme.of(context).colorScheme.primaryContainer,
            ),
          ),
        ],
      ),
      body: const CoachChatPanel(embedded: false),
    );
  }
}
