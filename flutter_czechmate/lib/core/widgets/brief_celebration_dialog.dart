import 'package:flutter/material.dart';

import '../theme/app_motion.dart';

/// Krátký dialog úspěchu (scale + fade) — stejný jazyk animace jako puzzle oslava.
Future<void> showBriefCelebrationDialog({
  required BuildContext context,
  required Widget icon,
  required String title,
  required String message,
  Color barrierColor = const Color(0x38000000),
  String? actionLabel,
  VoidCallback? onAction,
}) {
  final l10n = Localizations.of(context, MaterialLocalizations);
  return showGeneralDialog<void>(
    context: context,
    barrierDismissible: true,
    barrierLabel: l10n.modalBarrierDismissLabel,
    barrierColor: barrierColor,
    transitionDuration: AppMotion.crossFade + const Duration(milliseconds: 60),
    transitionBuilder: (ctx, anim, _, child) {
      final curved = CurvedAnimation(
        parent: anim,
        curve: AppMotion.emphasisCurve,
      );
      return ScaleTransition(
        scale: curved,
        child: FadeTransition(opacity: anim, child: child),
      );
    },
    pageBuilder: (ctx, _, __) {
      return AlertDialog(
        icon: icon,
        title: Text(title),
        content: Text(message),
        actions: [
          FilledButton(
            onPressed: () {
              Navigator.of(ctx).pop();
              onAction?.call();
            },
            child: Text(actionLabel ?? l10n.okButtonLabel),
          ),
        ],
      );
    },
  );
}
