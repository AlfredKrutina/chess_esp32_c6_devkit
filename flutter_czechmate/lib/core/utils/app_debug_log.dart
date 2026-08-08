import 'package:flutter/foundation.dart';

import '../constants/app_environment.dart';

/// Detailed logs only in development / staging — in the release profile without output.
bool get appVerboseLoggingEnabled => kDebugMode || AppEnvironment.staging;

void appDebugLog(String message, [Object? detail]) {
  if (!appVerboseLoggingEnabled) return;
  if (detail == null) {
    debugPrint(message);
  } else {
    debugPrint('$message | $detail');
  }
}
