import 'package:flutter/material.dart';

class OnboardingScreen extends StatefulWidget {
  const OnboardingScreen({super.key, this.onDone});

  /// Called when user dismisses onboarding. Null = use Navigator.pop.
  final VoidCallback? onDone;

  @override
  State<OnboardingScreen> createState() => _OnboardingScreenState();
}

class _OnboardingScreenState extends State<OnboardingScreen> {
  int _step = 0;

  static const _steps = [
    _OnboardingStep(
      icon: Icons.check_box_outline_blank,
      iconColor: Colors.blueAccent,
      title: 'Smart chessboard',
      body:
          'Connect your CZECHMATE board over Wi‑Fi or Bluetooth and play with live sync.',
    ),
    _OnboardingStep(
      icon: Icons.cable,
      iconColor: Colors.orange,
      title: 'Set up the pieces',
      body:
          'Before powering on, place pieces in the starting position. The app will align with the board automatically.',
    ),
    _OnboardingStep(
      icon: Icons.lightbulb_outline,
      iconColor: Colors.amber,
      title: 'LED hints',
      body:
          'The board can highlight moves with LEDs. Hint depth is configured under Settings → Board NVS.',
    ),
    _OnboardingStep(
      icon: Icons.sports_esports,
      iconColor: Colors.deepPurple,
      title: 'Start a game',
      body:
          'On Play, tap New game and pick a time control. Flip the board (White or Black on bottom) from Game controls or Settings.',
    ),
    _OnboardingStep(
      icon: Icons.school,
      iconColor: Colors.green,
      title: 'Coach & analysis',
      body:
          'Turn on learning mode and Stockfish evaluation on Analysis for deeper insight.',
    ),
  ];

  void _next() {
    if (_step < _steps.length - 1) {
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
    final step = _steps[_step];
    return Scaffold(
      appBar: AppBar(
        title: const Text('Welcome to CZECHMATE'),
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
            child: const Text('Skip'),
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
              Text(step.title, style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
              const SizedBox(height: 16),
              Text(step.body, textAlign: TextAlign.center, style: const TextStyle(fontSize: 16)),
              const Spacer(),
              // Progress dots
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: List.generate(_steps.length, (i) => Container(
                  margin: const EdgeInsets.symmetric(horizontal: 4),
                  width: 10, height: 10,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: _step == i ? Theme.of(context).colorScheme.primary : Colors.grey.withOpacity(0.4),
                  ),
                )),
              ),
              const SizedBox(height: 32),
              SizedBox(
                width: double.infinity,
                child: FilledButton(
                  onPressed: _next,
                  style: FilledButton.styleFrom(padding: const EdgeInsets.all(16)),
                  child: Text(_step == _steps.length - 1 ? 'Start playing' : 'Next'),
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
