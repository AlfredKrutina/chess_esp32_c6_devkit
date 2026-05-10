import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../constants/app_environment.dart';

/// Jednotný prefix do terminálu při `flutter run` / `--dart-define=STAGING=true`.
/// Grep: `[czechmate][conn]`
void connDebugLog(String message, [Object? detail]) {
  if (!kDebugMode && !AppEnvironment.staging) return;
  if (detail == null) {
    debugPrint('[czechmate][conn] $message');
  } else {
    debugPrint('[czechmate][conn] $message | $detail');
  }
}

/// Rozšířený výpis chyb BLE (FBP / obecné).
String connBleErrorDetail(Object error) {
  if (error is FlutterBluePlusException) {
    final d = error.description;
    return 'FlutterBluePlusException(code=${error.code}'
        '${d != null && d.isNotEmpty ? ', description=$d' : ''}) $error';
  }
  return error.toString();
}
