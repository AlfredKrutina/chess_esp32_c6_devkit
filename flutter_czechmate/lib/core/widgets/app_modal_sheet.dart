import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../layout/form_factor.dart';

/// Max width for modal bottom sheets on desktop embedders (aligned with form caps).
BoxConstraints? desktopModalSheetConstraints(
  BuildContext context, {
  double maxWidth = kDesktopSettingsDetailMaxWidth,
}) {
  if (!isDesktopEmbedder()) return null;
  final w = MediaQuery.sizeOf(context).width;
  return BoxConstraints(maxWidth: math.min(maxWidth, w));
}

BoxConstraints? _mergeDesktopSheetConstraints(
  BuildContext context,
  BoxConstraints? user,
) {
  final desktop = desktopModalSheetConstraints(context);
  if (desktop == null) return user;
  if (user == null) return desktop;
  final cap = desktop.maxWidth;
  final uw = user.maxWidth;
  final maxW = uw.isFinite ? math.min(uw, cap) : cap;
  return BoxConstraints(
    minWidth: math.min(user.minWidth, maxW),
    maxWidth: maxW,
    minHeight: user.minHeight,
    maxHeight: user.maxHeight,
  );
}

Widget _wrapEscapeDismiss(BuildContext sheetContext, Widget child, bool on) {
  if (!on || !isDesktopEmbedder()) return child;
  return Shortcuts(
    shortcuts: const <ShortcutActivator, Intent>{
      SingleActivator(LogicalKeyboardKey.escape): DismissIntent(),
    },
    child: Actions(
      actions: <Type, Action<Intent>>{
        DismissIntent: CallbackAction<DismissIntent>(
          onInvoke: (_) {
            final nav = Navigator.maybeOf(sheetContext);
            if (nav?.canPop() ?? false) nav!.pop();
            return null;
          },
        ),
      },
      child: Focus(
        canRequestFocus: false,
        skipTraversal: true,
        child: child,
      ),
    ),
  );
}

/// Jednotné otevírání bottom sheetů (drag handle, desktop max šířka, volitelně Esc).
Future<T?> showAppModalBottomSheet<T>({
  required BuildContext context,
  required WidgetBuilder builder,
  bool isScrollControlled = false,
  bool useSafeArea = false,
  bool showDragHandle = true,
  Color? backgroundColor,
  BoxConstraints? constraints,

  /// Na desktopu zavře list Escape (např. výběr času). Pro provisioning Wi‑Fi nechte `false`.
  bool escapeToDismiss = true,
}) {
  final merged = _mergeDesktopSheetConstraints(context, constraints);

  return showModalBottomSheet<T>(
    context: context,
    isScrollControlled: isScrollControlled,
    useSafeArea: useSafeArea,
    showDragHandle: showDragHandle,
    backgroundColor: backgroundColor,
    constraints: merged,
    builder: (sheetContext) => _wrapEscapeDismiss(
        sheetContext, builder(sheetContext), escapeToDismiss),
  );
}
