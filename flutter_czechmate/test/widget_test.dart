import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/app_providers.dart';
import 'package:czechmate/main.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('shell: three tabs, Czech and dark by default', (tester) async {
    SharedPreferences.setMockInitialValues({'czechmate.onboardingSeen': true});
    final prefs = await SharedPreferences.getInstance();

    await tester.pumpWidget(
      ProviderScope(
        overrides: [sharedPreferencesProvider.overrideWithValue(prefs)],
        child: const CzechMateApp(),
      ),
    );
    await tester.pump(const Duration(milliseconds: 500));

    final bar = find.byType(NavigationBar);
    expect(bar, findsOneWidget);
    expect(tester.widget<NavigationBar>(bar).destinations, hasLength(3));
    for (final label in ['Hra', 'Trénink', 'Nastavení']) {
      expect(find.descendant(of: bar, matching: find.text(label)), findsOneWidget);
    }
    expect(find.descendant(of: bar, matching: find.text('Analýza')), findsNothing);

    final app = tester.widget<MaterialApp>(find.byType(MaterialApp));
    expect(app.themeMode, ThemeMode.dark);

    await tester.tap(
        find.descendant(of: bar, matching: find.byIcon(Icons.settings_outlined)));
    await tester.pump(const Duration(milliseconds: 300));
    expect(prefs.getInt('czechmate.shell.lastMainTab.v3'), 2);
  });

  testWidgets('shell: last tab from the four-tab layout maps onto three tabs',
      (tester) async {
    SharedPreferences.setMockInitialValues({
      'czechmate.onboardingSeen': true,
      'czechmate.shell.lastMainTab': 3,
    });
    final prefs = await SharedPreferences.getInstance();
    await tester.pumpWidget(
      ProviderScope(
        overrides: [sharedPreferencesProvider.overrideWithValue(prefs)],
        child: const CzechMateApp(),
      ),
    );
    await tester.pump(const Duration(milliseconds: 500));
    final bar = tester.widget<NavigationBar>(find.byType(NavigationBar));
    expect(bar.selectedIndex, 2);
  });
}
