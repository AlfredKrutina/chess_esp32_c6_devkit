import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'core/services/board_api_client.dart';
import 'core/services/board_home_widget_sync.dart';
import 'core/services/live_activity_service.dart';
import 'core/services/prefs_repository.dart';
import 'core/services/stockfish_api_client.dart';
import 'core/services/watch_connectivity_service.dart';
import 'features/opening/opening_progress_repository.dart';

/// Bottom navigation (`_MainShell`) — see [AppMainTab] in `app_navigation.dart` for indexes.
final mainNavTabIndexProvider = StateProvider<int>((ref) => 0);

/// After changing the connection mode (`setConnectionMode` / `setNextConnectionTransportOnce`) —
/// invalidates dependent widgets (IndexedStack without own Prefs listen).
final connectionModeUiRefreshProvider = StateProvider<int>((ref) => 0);

/// Progress Tab: 0 = Tutorial, 1 = Statistics (like `ProgressTabView` on iOS).
final progressSegmentProvider = StateProvider<int>((ref) => 0);

/// Override in `main()` via `ProviderScope(overrides: [...])`.
final sharedPreferencesProvider = Provider<SharedPreferences>((ref) {
  throw StateError('SharedPreferences — nastav override v main()');
});

/// Must not `watch(prefs)` — any prefs invalidation would close `http.Client` and the old
/// reference in [BoardSessionNotifier] would then report "Client is already closed".
final boardApiClientProvider = Provider<BoardApiClient>((ref) {
  final c = BoardApiClient(
    resolveBoardApiBearerToken: () =>
        ref.read(prefsRepositoryProvider).boardApiToken,
  );
  ref.onDispose(c.close);
  return c;
});

final prefsRepositoryProvider = Provider<PrefsRepository>((ref) {
  return PrefsRepository(ref.watch(sharedPreferencesProvider));
});

final openingProgressRepositoryProvider =
    Provider<OpeningProgressRepository>((ref) {
  return OpeningProgressRepository(ref.watch(sharedPreferencesProvider));
});

final liveActivityServiceProvider = Provider<LiveActivityService>((ref) {
  return LiveActivityService();
});

final boardHomeWidgetSyncProvider = Provider<BoardHomeWidgetSync>((ref) {
  return BoardHomeWidgetSync();
});

final watchConnectivityServiceProvider = Provider<WatchConnectivityService>((ref) {
  return WatchConnectivityService();
});

final stockfishApiClientProvider = Provider<StockfishApiClient>((ref) {
  final base = ref.read(prefsRepositoryProvider).stockfishApiBaseUrl;
  final c = StockfishApiClient(baseUrl: base);
  ref.onDispose(c.close);
  return c;
});

/// Parity `NWPathMonitor` — connection status for Stockfish / Wi‑Fi alerts vs. data.
final networkConnectivityProvider = StreamProvider<List<ConnectivityResult>>((ref) async* {
  final c = Connectivity();
  try {
    yield await c.checkConnectivity();
  } catch (_) {
    yield <ConnectivityResult>[ConnectivityResult.none];
  }
  await for (final list in c.onConnectivityChanged) {
    yield list;
  }
});
