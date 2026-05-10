import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/localization/context_l10n.dart';
import '../../core/widgets/pressable_scale.dart';

class SetupScreen extends ConsumerWidget {
  const SetupScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.setupModeAppBar)),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.dashboard_customize,
                  size: 80, color: Colors.blueAccent),
              const SizedBox(height: 24),
              Text(l10n.setupModePiecePlacementTitle,
                  textAlign: TextAlign.center,
                  style: const TextStyle(
                      fontSize: 24, fontWeight: FontWeight.bold)),
              const SizedBox(height: 16),
              Text(
                l10n.setupModePiecePlacementBody,
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 32),
              PressableScale(
                child: FilledButton(
                  onPressed: () => Navigator.pop(context),
                  child: Text(l10n.setupModeGoBack),
                ),
              )
            ],
          ),
        ),
      ),
    );
  }
}
