import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/locale_bridge.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Keeps Live Activity in sync with [boardSessionNotifierProvider].
class LiveActivitySessionListener extends ConsumerWidget {
  const LiveActivitySessionListener({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    ref.listen(boardSessionNotifierProvider, (previous, next) {
      unawaited(_sync(ref, next));
    });
    return const SizedBox.shrink();
  }

  Future<void> _sync(WidgetRef ref, BoardSessionState next) async {
    final prefs = ref.read(prefsRepositoryProvider);
    final live = ref.read(liveActivityServiceProvider);
    final watch = ref.read(watchConnectivityServiceProvider);
    final lang = resolvedLocaleForPrefs(
      prefs,
      WidgetsBinding.instance.platformDispatcher.locale,
    ).languageCode;

    await live.syncFromBoardSession(
      next,
      enabled: prefs.liveActivityEnabled,
      languageCode: lang,
    );

    final homeWidget = ref.read(boardHomeWidgetSyncProvider);
    await homeWidget.syncPayload(live.extensionPayload(next));

    final mirrorCompanion = (!kIsWeb &&
            defaultTargetPlatform == TargetPlatform.iOS &&
            prefs.watchCompanionMirrorEnabled) ||
        (!kIsWeb &&
            defaultTargetPlatform == TargetPlatform.android &&
            prefs.wearDataLayerMirrorEnabled);

    if (mirrorCompanion) {
      await watch.sendGameMirror(
        live.extensionPayload(next, languageCode: lang),
      );
    }
  }
}
