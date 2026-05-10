import 'package:flutter/material.dart';

import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';

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
