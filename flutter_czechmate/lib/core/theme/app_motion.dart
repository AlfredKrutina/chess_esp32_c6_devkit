import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

/// Shared timings and curves. Everything is short and ease-out; nothing waits
/// for effect. Use [resolve] so the system "reduce motion" setting turns
/// animations off.
abstract final class AppMotion {
  static const Duration sheetOpen = Duration(milliseconds: 220);
  static const Duration sheetClose = Duration(milliseconds: 180);

  static const Duration microInteraction = Duration(milliseconds: 150);
  static const Duration stateChange = Duration(milliseconds: 200);
  static const Duration crossFade = Duration(milliseconds: 200);

  static const Curve standardCurve = Curves.easeOutCubic;
  static const Curve reverseCurve = Curves.easeInCubic;
  static const Curve emphasisCurve = Curves.easeOutCubic;

  static bool reduced(BuildContext context) =>
      MediaQuery.maybeDisableAnimationsOf(context) ?? false;

  static Duration resolve(BuildContext context, Duration d) =>
      reduced(context) ? Duration.zero : d;

  static AnimationStyle sheetStyle(BuildContext context) => reduced(context)
      ? AnimationStyle.noAnimation
      : const AnimationStyle(
          duration: sheetOpen,
          reverseDuration: sheetClose,
          curve: standardCurve,
          reverseCurve: reverseCurve,
        );

  /// Edge swipe back on every platform. The fade transition has no gesture,
  /// and a fullscreen dialog disables the iOS back swipe.
  static const PageTransitionsTheme pageTransitionsTheme = PageTransitionsTheme(
    builders: {
      TargetPlatform.android: CupertinoPageTransitionsBuilder(),
      TargetPlatform.iOS: CupertinoPageTransitionsBuilder(),
      TargetPlatform.macOS: CupertinoPageTransitionsBuilder(),
      TargetPlatform.linux: CupertinoPageTransitionsBuilder(),
      TargetPlatform.windows: CupertinoPageTransitionsBuilder(),
      TargetPlatform.fuchsia: CupertinoPageTransitionsBuilder(),
    },
  );
}
