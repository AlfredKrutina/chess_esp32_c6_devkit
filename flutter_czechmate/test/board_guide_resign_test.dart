import 'package:czechmate/features/game/controls/resign_action.dart';
import 'package:czechmate/features/onboarding/board_guide.dart';
import 'package:czechmate/features/onboarding/onboarding_screen.dart';
import 'package:czechmate/l10n/app_localizations.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

Widget _app(Widget home, {Locale locale = const Locale('cs')}) {
  return ProviderScope(
    child: MaterialApp(
      locale: locale,
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: home,
    ),
  );
}

void main() {
  group('board guide', () {
    test('every step has a fixed media path and resolves webp before png', () {
      final ids = kBoardGuideSteps.map((s) => s.id).toList();
      expect(ids, containsAll(<String>['hint', 'resign']));
      for (final s in kBoardGuideSteps) {
        expect(s.mediaBasePath, 'assets/guide/${s.id}/${s.id}');
      }
      expect(
        resolveGuideMedia('assets/guide/hint/hint', {
          'assets/guide/hint/hint.png',
          'assets/guide/hint/hint.webp',
        }),
        'assets/guide/hint/hint.webp',
      );
      expect(resolveGuideMedia('assets/guide/hint/hint', <String>{}), isNull);
    });

    testWidgets('guide-only: Back disabled first, Done last, keyboard works',
        (tester) async {
      await tester.pumpWidget(_app(const OnboardingScreen(guideOnly: true)));
      await tester.pumpAndSettle();

      final l = lookupAppLocalizations(const Locale('cs'));
      expect(find.text(l.boardGuideConnectTitle), findsOneWidget);
      final back = find.widgetWithText(OutlinedButton, l.boardGuideBack);
      expect(tester.widget<OutlinedButton>(back).onPressed, isNull);

      await tester.sendKeyEvent(LogicalKeyboardKey.arrowRight);
      await tester.pumpAndSettle();
      expect(find.text(l.boardGuideMoveTitle), findsOneWidget);
      expect(tester.widget<OutlinedButton>(back).onPressed, isNotNull);

      for (var i = 0; i < kBoardGuideSteps.length - 2; i++) {
        await tester.tap(find.text(l.onboardingNext));
        await tester.pumpAndSettle();
      }
      expect(find.text(l.boardGuideResignTitle), findsOneWidget);
      expect(find.text(l.boardGuideDone), findsOneWidget);
      expect(tester.takeException(), isNull);
    });

    testWidgets('missing media renders a calm slot without errors',
        (tester) async {
      await tester.pumpWidget(_app(Scaffold(
        body: BoardGuideMediaSlot(
          step: kBoardGuideSteps.firstWhere((s) => s.id == 'hint'),
          semanticLabel: 'hint',
        ),
      )));
      await tester.pumpAndSettle();
      expect(find.byType(Image), findsNothing);
      expect(find.byIcon(Icons.tips_and_updates_outlined), findsOneWidget);
      expect(tester.takeException(), isNull);
    });
  });

  group('resign confirmation', () {
    Future<void> openDialog(
      WidgetTester tester, {
      required bool white,
      required Future<void> Function() onConfirm,
      required List<bool?> results,
    }) async {
      await tester.pumpWidget(_app(Builder(
        builder: (context) => Scaffold(
          body: Center(
            child: TextButton(
              onPressed: () async => results.add(await showResignConfirmDialog(
                context,
                white: white,
                onConfirm: onConfirm,
              )),
              child: const Text('open'),
            ),
          ),
        ),
      )));
      await tester.tap(find.text('open'));
      await tester.pumpAndSettle();
    }

    testWidgets('names the colour and Cancel does not resign', (tester) async {
      final l = lookupAppLocalizations(const Locale('cs'));
      var sent = 0;
      final results = <bool?>[];
      await openDialog(tester,
          white: false, onConfirm: () async => sent++, results: results);
      expect(find.text(l.uiResignTitleBlack), findsOneWidget);
      await tester.tap(find.text(l.commonCancel));
      await tester.pumpAndSettle();
      expect(sent, 0);
      expect(results, [false]);
    });

    testWidgets('Enter on default focus cancels; confirm sends once',
        (tester) async {
      final l = lookupAppLocalizations(const Locale('cs'));
      var sent = 0;
      final results = <bool?>[];
      await openDialog(tester,
          white: true, onConfirm: () async => sent++, results: results);
      expect(find.text(l.uiResignTitleWhite), findsOneWidget);
      await tester.sendKeyEvent(LogicalKeyboardKey.enter);
      await tester.pumpAndSettle();
      expect(sent, 0);

      await tester.tap(find.text('open'));
      await tester.pumpAndSettle();
      await tester.tap(find.widgetWithText(FilledButton, l.gameResign));
      await tester.pumpAndSettle();
      expect(sent, 1);
      expect(results.last, true);
    });

    testWidgets('board refusal stays in the dialog with advice',
        (tester) async {
      final l = lookupAppLocalizations(const Locale('cs'));
      final results = <bool?>[];
      await openDialog(tester,
          white: true,
          onConfirm: () async => throw StateError('offline'),
          results: results);
      await tester.tap(find.widgetWithText(FilledButton, l.gameResign));
      await tester.pumpAndSettle();
      expect(find.text(l.uiResignTitleWhite), findsOneWidget);
      expect(find.textContaining(l.gameResignFailed('').trim()), findsOneWidget);
      expect(results, isEmpty);
    });
  });
}
