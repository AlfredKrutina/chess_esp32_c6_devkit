import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';

import 'flutter_blue_plus_host_supported.dart';

/// BLE scan / connect: runtime oprávnění podle platformy (Android 12+ / Apple).
///
/// Windows [isFlutterBluePlusHostSupported] je false — volání před scanem je no-op.
Future<bool> ensureBlePermissionsForScan() async {
  if (kIsWeb || !isFlutterBluePlusHostSupported) return true;

  switch (defaultTargetPlatform) {
    case TargetPlatform.android:
      final connect = await Permission.bluetoothConnect.request();
      final scan = await Permission.bluetoothScan.request();
      return connect.isGranted && scan.isGranted;
    case TargetPlatform.iOS:
    case TargetPlatform.macOS:
      try {
        final r = await Permission.bluetooth.request();
        return r.isGranted;
      } catch (_) {
        return false;
      }
    case TargetPlatform.linux:
      try {
        final r = await Permission.bluetooth.request();
        return r.isGranted;
      } catch (_) {
        return true;
      }
    default:
      return true;
  }
}

/// Stav BLE oprávnění bez promptu (onboarding indikátory).
Future<bool?> blePermissionGrantedSnapshot() async {
  if (kIsWeb || !isFlutterBluePlusHostSupported) return null;
  try {
    switch (defaultTargetPlatform) {
      case TargetPlatform.android:
        final c = await Permission.bluetoothConnect.status;
        final s = await Permission.bluetoothScan.status;
        return c.isGranted && s.isGranted;
      case TargetPlatform.iOS:
      case TargetPlatform.macOS:
      case TargetPlatform.linux:
        final b = await Permission.bluetooth.status;
        return b.isGranted;
      default:
        return null;
    }
  } catch (_) {
    return null;
  }
}

/// Čtení aktuálního SSID ([network_info_plus]): Android 13+ „Nearby Wi‑Fi“, starší Android
/// často Location; iOS vyžaduje Location When In Use (+ plist).
Future<bool> ensureWifiSsidReadPermissions() async {
  if (kIsWeb) return false;

  switch (defaultTargetPlatform) {
    case TargetPlatform.android:
      final nearby = await Permission.nearbyWifiDevices.request();
      if (nearby.isGranted) return true;
      final loc = await Permission.locationWhenInUse.request();
      return loc.isGranted;
    case TargetPlatform.iOS:
      final loc = await Permission.locationWhenInUse.request();
      return loc.isGranted;
    default:
      return true;
  }
}
