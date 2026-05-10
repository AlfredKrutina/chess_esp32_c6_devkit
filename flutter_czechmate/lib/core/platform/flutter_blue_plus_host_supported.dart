import 'package:flutter/foundation.dart';

/// Host má nativní implementaci `flutter_blue_plus` (Android, iOS, macOS, Linux).
/// Na **Windows** (a Fuchsia) plugin chybí — volání [FlutterBluePlus] hodí [UnsupportedError].
bool get isFlutterBluePlusHostSupported {
  if (kIsWeb) return false;
  return switch (defaultTargetPlatform) {
    TargetPlatform.android ||
    TargetPlatform.iOS ||
    TargetPlatform.macOS ||
    TargetPlatform.linux =>
      true,
    TargetPlatform.windows || TargetPlatform.fuchsia => false,
  };
}
