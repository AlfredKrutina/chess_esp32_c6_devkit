import 'package:flutter/foundation.dart';

/// The host has a native implementation of `flutter_blue_plus` (Android, iOS, macOS, Linux).
/// On **Windows** (and Fuchsia), the plugin is missing — calling [FlutterBluePlus] throws [UnsupportedError].
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
