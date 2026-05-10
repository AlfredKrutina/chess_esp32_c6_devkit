import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'core/services/board_api_client.dart';
import 'core/services/board_home_widget_sync.dart';
import 'core/services/live_activity_service.dart';
import 'core/services/prefs_repository.dart';
import 'core/services/stockfish_api_client.dart';
import 'core/services/watch_connectivity_service.dart';

/// Spodní navigace (`_MainShell`) — indexy viz [AppMainTab] v `app_navigation.dart`.
final mainNavTabIndexProvider = StateProvider<int>((ref) => 0);

/// Po změně režimu připojení (`setConnectionMode` / `setNextConnectionTransportOnce`) —
/// invaliduje závislé widgety (IndexedStack bez vlastního Prefs listen).
final connectionModeUiRefreshProvider = StateProvider<int>((ref) => 0);

/// Záložka Pokrok: 0 = Výuka, 1 = Statistiky (jako `ProgressTabView` na iOS).
final progressSegmentProvider = StateProvider<int>((ref) => 0);

/// Přepsat v `main()` přes `ProviderScope(overrides: [...])`.
final sharedPreferencesProvider = Provider<SharedPreferences>((ref) {
  throw StateError('SharedPreferences — nastav override v main()');
});

/// Nesmí `watch(prefs)` — každá invalidace prefs by zavřela `http.Client` a stará
/// reference v [BoardSessionNotifier] by pak hlásila „Client is already closed“.
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

/// Parita `NWPathMonitor` — stav připojení pro Stockfish / upozornění na Wi‑Fi vs. data.
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
