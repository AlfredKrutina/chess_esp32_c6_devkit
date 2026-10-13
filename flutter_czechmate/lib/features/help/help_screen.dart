import 'package:flutter/material.dart';

import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../onboarding/onboarding_screen.dart';

class HelpScreen extends StatelessWidget {
  const HelpScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.helpAppBarTitle)),
      body: desktopFormDetailBody(
        ListView(
          padding: const EdgeInsets.all(16),
          children: [
            const _BoardGuideCard(),
            const SizedBox(height: 8),
            _HelpSection(
                title: l10n.helpConnectTitle, body: l10n.helpConnectBody),
            _HelpSection(
                title: l10n.helpPlayingTitle, body: l10n.helpPlayingBody),
            _HelpSection(title: l10n.helpCoachTitle, body: l10n.helpCoachBody),
            _HelpSection(
                title: l10n.helpSettingsTitle, body: l10n.helpSettingsBody),
          ],
        ),
      ),
    );
  }
}

class _BoardGuideCard extends StatelessWidget {
  const _BoardGuideCard();

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    return Card(
      elevation: 0,
      margin: EdgeInsets.zero,
      color: cs.primaryContainer.withValues(alpha: 0.45),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.explore_outlined, color: cs.primary),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    l10n.helpBoardGuideTitle,
                    style: theme.textTheme.titleMedium
                        ?.copyWith(fontWeight: FontWeight.w600),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(l10n.helpBoardGuideBody, style: theme.textTheme.bodyMedium),
            const SizedBox(height: 12),
            FilledButton.tonalIcon(
              onPressed: () => Navigator.push<void>(
                context,
                MaterialPageRoute<void>(
                  builder: (_) => const OnboardingScreen(guideOnly: true),
                ),
              ),
              icon: const Icon(Icons.play_arrow_rounded),
              label: Text(l10n.helpBoardGuideOpen),
            ),
          ],
        ),
      ),
    );
  }
}

class _HelpSection extends StatelessWidget {
  const _HelpSection({required this.title, required this.body});
  final String title;
  final String body;

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 12),
          child: Text(title,
              style: TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 16,
                  color: cs.primary)),
        ),
        Text(body, style: Theme.of(context).textTheme.bodyMedium),
        const Divider(),
      ],
    );
  }
}
