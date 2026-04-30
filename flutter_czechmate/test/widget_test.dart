import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/app_providers.dart';
import 'package:czechmate/main.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('czechmate smoke — shell and navigation', (WidgetTester tester) async {
    TestWidgetsFlutterBinding.ensureInitialized();
    SharedPreferences.setMockInitialValues({'czechmate.onboardingSeen': true});
    final prefs = await SharedPreferences.getInstance();

    await tester.pumpWidget(
      ProviderScope(
        overrides: [
          sharedPreferencesProvider.overrideWithValue(prefs),
        ],
        child: const CzechMateApp(),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('Play'), findsWidgets);
    expect(find.text('Progress'), findsWidgets);
    expect(find.text('Analysis'), findsWidgets);
  });
}
