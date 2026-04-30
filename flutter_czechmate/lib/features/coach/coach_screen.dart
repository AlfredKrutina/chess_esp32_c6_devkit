import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
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

    return Scaffold(
      appBar: AppBar(
        title: const Text('AI Coach'),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: Chip(
              label: Text(
                notifier.providerChipLabel(prefs),
                overflow: TextOverflow.ellipsis,
              ),
              backgroundColor: Theme.of(context).colorScheme.secondaryContainer,
            ),
          ),
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: Chip(
              label: Text('Level $level'),
              backgroundColor: Theme.of(context).colorScheme.primaryContainer,
            ),
          ),
        ],
      ),
      body: const CoachChatPanel(embedded: false),
    );
  }
}
