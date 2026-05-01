import 'dart:ui';

import 'package:flutter/material.dart';

/// Floating snack bar with blur + translucent surface (liquid-glass style).
void showGlassSnackBar(
  BuildContext context,
  String message, {
  Duration duration = const Duration(seconds: 4),
}) {
  final theme = Theme.of(context);
  final cs = theme.colorScheme;

  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(
      elevation: 0,
      backgroundColor: Colors.transparent,
      behavior: SnackBarBehavior.floating,
      padding: EdgeInsets.zero,
      margin: const EdgeInsets.fromLTRB(14, 0, 14, 18),
      duration: duration,
      content: ClipRRect(
        borderRadius: BorderRadius.circular(22),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 28, sigmaY: 28),
          child: DecoratedBox(
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(22),
              gradient: LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: [
                  cs.surface.withValues(alpha: 0.78),
                  cs.surface.withValues(alpha: 0.48),
                ],
              ),
              border: Border.all(
                color: cs.onSurface.withValues(alpha: 0.12),
              ),
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withValues(alpha: 0.12),
                  blurRadius: 32,
                  offset: const Offset(0, 12),
                  spreadRadius: -4,
                ),
              ],
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
              child: DefaultTextStyle.merge(
                style: theme.textTheme.bodyMedium!.copyWith(
                  color: cs.onSurface,
                  height: 1.35,
                ),
                child: Text(message),
              ),
            ),
          ),
        ),
      ),
    ),
  );
}
