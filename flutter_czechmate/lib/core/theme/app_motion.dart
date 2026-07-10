import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

/// Sdílené časy a křivky pro levné animace napříč aplikací.
abstract final class AppMotion {
  static const Duration sheetOpen = Duration(milliseconds: 280);
  static const Duration sheetClose = Duration(milliseconds: 220);

  static const Duration microInteraction = Duration(milliseconds: 110);
  static const Duration crossFade = Duration(milliseconds: 240);

  static const Curve standardCurve = Curves.easeOutCubic;
  static const Curve reverseCurve = Curves.easeInCubic;
  static const Curve emphasisCurve = Curves.easeOutBack;

  /// Přechody mezi obrazovkami (MaterialApp theme).
  static const PageTransitionsTheme pageTransitionsTheme = PageTransitionsTheme(
    builders: {
      TargetPlatform.android: FadeUpwardsPageTransitionsBuilder(),
      TargetPlatform.iOS: CupertinoPageTransitionsBuilder(),
      TargetPlatform.macOS: FadeUpwardsPageTransitionsBuilder(),
      TargetPlatform.linux: FadeUpwardsPageTransitionsBuilder(),
      TargetPlatform.windows: FadeUpwardsPageTransitionsBuilder(),
      TargetPlatform.fuchsia: FadeUpwardsPageTransitionsBuilder(),
    },
  );
}
