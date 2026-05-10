import 'package:flutter/material.dart';

import '../theme/app_motion.dart';

/// [LinearProgressIndicator] s jemným náběhem/výběhem při přepínání [busy].
class AnimatedLinearBusyStrip extends StatelessWidget {
  const AnimatedLinearBusyStrip({
    super.key,
    required this.busy,
    this.minHeight,
  });

  final bool busy;
  final double? minHeight;

  @override
  Widget build(BuildContext context) {
    return AnimatedSwitcher(
      duration: AppMotion.crossFade,
      switchInCurve: AppMotion.standardCurve,
      switchOutCurve: AppMotion.reverseCurve,
      child: busy
          ? LinearProgressIndicator(
              key: const ValueKey<String>('animated_linear_busy_on'),
              minHeight: minHeight,
            )
          : const SizedBox.shrink(
              key: ValueKey<String>('animated_linear_busy_off'),
            ),
    );
  }
}
