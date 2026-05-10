import 'package:flutter/material.dart';

import '../theme/app_motion.dart';

/// Jemný scale při stisku — obaluje tlačítko; pointer události neblokují tap na dítěti.
class PressableScale extends StatefulWidget {
  const PressableScale({
    super.key,
    required this.child,
    this.pressedScale = 0.97,
  });

  final Widget child;
  final double pressedScale;

  @override
  State<PressableScale> createState() => _PressableScaleState();
}

class _PressableScaleState extends State<PressableScale> {
  bool _pressed = false;

  @override
  Widget build(BuildContext context) {
    return Listener(
      onPointerDown: (_) => setState(() => _pressed = true),
      onPointerUp: (_) => setState(() => _pressed = false),
      onPointerCancel: (_) => setState(() => _pressed = false),
      child: AnimatedScale(
        scale: _pressed ? widget.pressedScale : 1.0,
        duration: AppMotion.microInteraction,
        curve: AppMotion.standardCurve,
        child: widget.child,
      ),
    );
  }
}
