import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/services/prefs_repository.dart';

enum CustomModelStatus {
  notDownloaded,
  downloading,
  ready,
  error
}

class CoachManager extends StateNotifier<CustomModelStatus> {
  CoachManager(this._prefs) : super(CustomModelStatus.notDownloaded);

  final PrefsRepository _prefs;

  String? lastHint;

  Future<void> requestHint(String fen) async {
    if (_prefs.coachTraceLogsEnabled) {
      debugPrint('[staging][coach] requestHint fen=$fen');
    }
    state = CustomModelStatus.downloading;
    
    // Placeholder local coach — cloud Gemma/OpenAI run from [CoachScreen] via prefs.
    await Future.delayed(const Duration(seconds: 2));
    
    lastHint =
        "Try to keep your king safe and consider castling — it often improves your structure.";
    state = CustomModelStatus.ready;
  }
}

final coachManagerProvider = StateNotifierProvider<CoachManager, CustomModelStatus>((ref) {
  return CoachManager(ref.read(prefsRepositoryProvider));
});
