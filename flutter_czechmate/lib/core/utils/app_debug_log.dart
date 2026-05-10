import 'package:flutter/foundation.dart';

import '../constants/app_environment.dart';

/// Detailní logy jen ve vývoji / staging — v release profilu bez výstupu.
bool get appVerboseLoggingEnabled => kDebugMode || AppEnvironment.staging;

void appDebugLog(String message, [Object? detail]) {
  if (!appVerboseLoggingEnabled) return;
  if (detail == null) {
    debugPrint(message);
  } else {
    debugPrint('$message | $detail');
  }
}
