import 'package:flutter/widgets.dart';

import '../../l10n/app_localizations.dart';
import '../services/prefs_repository.dart';

/// Resolves content locale: explicit [PrefsRepository.uiLanguage] or device language → cs/en only.
Locale resolvedLocaleForPrefs(PrefsRepository prefs, Locale platformLocale) {
  switch (prefs.uiLanguage) {
    case 'cs':
      return const Locale('cs');
    case 'en':
      return const Locale('en');
    default:
      final lc = platformLocale.languageCode.toLowerCase();
      if (lc.startsWith('cs')) return const Locale('cs');
      return const Locale('en');
  }
}

AppLocalizations appStringsForPrefs(PrefsRepository prefs) {
  final platform =
      WidgetsBinding.instance.platformDispatcher.locale;
  final loc = resolvedLocaleForPrefs(prefs, platform);
  switch (loc.languageCode) {
    case 'cs':
      return lookupAppLocalizations(loc);
    default:
      return lookupAppLocalizations(const Locale('en'));
  }
}
