import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/app_providers.dart';
import 'package:flutter_czechmate/main.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('CzechMate smoke — shell a navigace', (WidgetTester tester) async {
    TestWidgetsFlutterBinding.ensureInitialized();
    SharedPreferences.setMockInitialValues({});
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

    expect(find.text('Hra'), findsWidgets); // label v NavigationBar + dest.
    expect(find.text('Deska'), findsWidgets);
  });
}
