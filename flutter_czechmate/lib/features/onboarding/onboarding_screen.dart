import 'package:flutter/material.dart';

import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';

class OnboardingScreen extends StatefulWidget {
  const OnboardingScreen({super.key, this.onDone});

  /// Called when user dismisses onboarding. Null = use Navigator.pop.
  final VoidCallback? onDone;

  @override
  State<OnboardingScreen> createState() => _OnboardingScreenState();
}

class _OnboardingScreenState extends State<OnboardingScreen> {
  int _step = 0;

  List<_OnboardingStep> _steps(AppLocalizations l10n) => [
        _OnboardingStep(
          icon: Icons.check_box_outline_blank,
          iconColor: Colors.blueAccent,
          title: l10n.onboard1Title,
          body: l10n.onboard1Body,
        ),
        _OnboardingStep(
          icon: Icons.cable,
          iconColor: Colors.orange,
          title: l10n.onboard2Title,
          body: l10n.onboard2Body,
        ),
        _OnboardingStep(
          icon: Icons.lightbulb_outline,
          iconColor: Colors.amber,
          title: l10n.onboard3Title,
          body: l10n.onboard3Body,
        ),
        _OnboardingStep(
          icon: Icons.sports_esports,
          iconColor: Colors.deepPurple,
          title: l10n.onboard4Title,
          body: l10n.onboard4Body,
        ),
        _OnboardingStep(
          icon: Icons.school,
          iconColor: Colors.green,
          title: l10n.onboard5Title,
          body: l10n.onboard5Body,
        ),
      ];

  void _next() {
    final steps = _steps(context.l10n);
    if (_step < steps.length - 1) {
      setState(() => _step++);
    } else {
      if (widget.onDone != null) {
        widget.onDone!();
      } else {
        Navigator.of(context).pop();
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final steps = _steps(l10n);
    final step = steps[_step];
    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.onboardingWelcomeTitle),
        automaticallyImplyLeading: false,
        actions: [
          TextButton(
            onPressed: () {
              if (widget.onDone != null) {
                widget.onDone!();
              } else {
                Navigator.of(context).pop();
              }
            },
            child: Text(l10n.onboardingSkip),
          ),
        ],
      ),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Spacer(),
              Icon(step.icon, size: 100, color: step.iconColor),
              const SizedBox(height: 32),
              Text(step.title,
                  style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
              const SizedBox(height: 16),
              Text(step.body,
                  textAlign: TextAlign.center,
                  style: const TextStyle(fontSize: 16)),
              const Spacer(),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: List.generate(
                    steps.length,
                    (i) => Container(
                          margin: const EdgeInsets.symmetric(horizontal: 4),
                          width: 10,
                          height: 10,
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            color: _step == i
                                ? Theme.of(context).colorScheme.primary
                                : Colors.grey.withValues(alpha: 0.4),
                          ),
                        )),
              ),
              const SizedBox(height: 32),
              SizedBox(
                width: double.infinity,
                child: FilledButton(
                  onPressed: _next,
                  style: FilledButton.styleFrom(padding: const EdgeInsets.all(16)),
                  child: Text(
                      _step == steps.length - 1 ? l10n.onboardingStart : l10n.onboardingNext),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _OnboardingStep {
  const _OnboardingStep({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.body,
  });
  final IconData icon;
  final Color iconColor;
  final String title;
  final String body;
}
