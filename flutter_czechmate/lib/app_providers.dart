import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'core/services/board_api_client.dart';
import 'core/services/prefs_repository.dart';

/// Přepsat v `main()` přes `ProviderScope(overrides: [...])`.
final sharedPreferencesProvider = Provider<SharedPreferences>((ref) {
  throw StateError('SharedPreferences — nastav override v main()');
});

final boardApiClientProvider = Provider<BoardApiClient>((ref) {
  final c = BoardApiClient();
  ref.onDispose(c.close);
  return c;
});

final prefsRepositoryProvider = Provider<PrefsRepository>((ref) {
  return PrefsRepository(ref.watch(sharedPreferencesProvider));
});
