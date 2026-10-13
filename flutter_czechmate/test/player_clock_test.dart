import 'dart:math' as math;

import 'package:czechmate/core/models/board_timer_state.dart';
import 'package:czechmate/core/models/game_status_models.dart';
import 'package:czechmate/core/theme/app_theme.dart';
import 'package:czechmate/features/game/controls/player_clock.dart';
import 'package:czechmate/features/game/state/running_game.dart';
import 'package:czechmate/l10n/app_localizations.dart';
import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';

BoardTimerState timer({
  int white = 300000,
  int black = 300000,
  bool running = true,
  bool whiteTurn = true,
  bool paused = false,
  bool expired = false,
  int type = 1,
}) {
  return BoardTimerState(
    whiteTimeMs: white,
    blackTimeMs: black,
    timerRunning: running,
    isWhiteTurn: whiteTurn,
    gamePaused: paused,
    timeExpired: expired,
    config: TimerConfigPart(
      type: type,
      name: '5+0',
      description: '',
      initialTimeMs: 300000,
      incrementMs: 0,
      isFast: true,
    ),
    totalMoves: 4,
    avgMoveTimeMs: 0,
  );
}

ClockFace face(bool white, BoardTimerState? t,
        {bool connected = true, bool finished = false}) =>
    resolveClockFace(
      white: white,
      timer: t,
      connected: connected,
      gameFinished: finished,
      whiteToMoveFallback: true,
    );

Widget host(Widget child, {ThemeData? theme, Locale locale = const Locale('cs')}) {
  return MaterialApp(
    theme: theme ?? AppTheme.dark(),
    locale: locale,
    localizationsDelegates: const [
      AppLocalizations.delegate,
      GlobalMaterialLocalizations.delegate,
      GlobalWidgetsLocalizations.delegate,
      GlobalCupertinoLocalizations.delegate,
    ],
    supportedLocales: AppLocalizations.supportedLocales,
    home: Scaffold(body: Center(child: child)),
  );
}

void main() {
  group('resolveClockFace', () {
    test('side on move ticks, the other side waits', () {
      final t = timer();
      expect(face(true, t).state, ClockState.onMove);
      expect(face(true, t).ticking, isTrue);
      expect(face(false, t).state, ClockState.idle);
      expect(face(false, t).ticking, isFalse);
    });

    test('low time below 10 % of the initial time', () {
      expect(face(true, timer(white: 29000)).state, ClockState.lowTime);
      expect(face(true, timer(white: 31000)).state, ClockState.onMove);
    });

    test('flag only when the board reports time_expired', () {
      expect(face(true, timer(white: 0)).state, ClockState.lowTime);
      final expired = timer(white: 0, expired: true, running: false);
      expect(face(true, expired).state, ClockState.flagged);
      expect(face(false, expired).state, ClockState.idle);
    });

    test('pause stops both faces', () {
      final t = timer(paused: true, running: false);
      expect(face(true, t).state, ClockState.paused);
      expect(face(false, t).state, ClockState.paused);
      expect(face(true, t).ticking, isFalse);
    });

    test('lost connection freezes the face', () {
      final f = face(true, timer(), connected: false);
      expect(f.state, ClockState.offline);
      expect(f.ticking, isFalse);
      expect(f.remainingMs, 300000);
    });

    test('no time control still shows who is on move', () {
      final t = timer(type: 0);
      expect(face(true, t).state, ClockState.onMove);
      expect(face(false, t).state, ClockState.noTimeControl);
      expect(face(true, t).remainingMs, isNull);
    });
  });

  test('formatClockMs', () {
    expect(formatClockMs(300000), '5:00');
    expect(formatClockMs(65000), '1:05');
    expect(formatClockMs(9400), '9.4');
    expect(formatClockMs(3723000), '1:02:03');
    expect(formatClockMs(-5), '0.0');
  });

  group('boardHasRunningGame', () {
    GameStatus status(Map<String, dynamic> extra) => GameStatus.fromJson({
          'game_state': 'active',
          'current_player': 'White',
          'move_count': 6,
          'in_check': false,
          ...extra,
        });

    test('active game with moves', () {
      expect(boardHasRunningGame(status({})), isTrue);
    });

    test('fresh board without moves is not a running game', () {
      expect(boardHasRunningGame(status({'move_count': 0})), isFalse);
    });

    test('finished or puzzle is not a running game', () {
      expect(boardHasRunningGame(status({'game_state': 'finished'})), isFalse);
      expect(
        boardHasRunningGame(status({
          'puzzle': {'active': true}
        })),
        isFalse,
      );
    });

    test('paused game counts', () {
      expect(boardHasRunningGame(status({'game_state': 'paused'})), isTrue);
    });
  });

  group('PlayerClock widget', () {
    testWidgets('shows only the time; side and state live in semantics',
        (tester) async {
      await tester.pumpWidget(host(PlayerClock(
        face: face(false, timer(running: false, whiteTurn: false)),
      )));
      expect(find.text('5:00'), findsOneWidget);
      expect(find.text('Černý'), findsNothing);
      expect(find.text('Na tahu'), findsNothing);
      expect(
        find.bySemanticsLabel(RegExp('Černý.*5:00.*Na tahu')),
        findsOneWidget,
      );
      expect(find.byType(InkWell), findsNothing);
      expect(find.byType(GestureDetector), findsNothing);
    });

    testWidgets('local countdown never goes below zero and never flags',
        (tester) async {
      await tester.pumpWidget(host(PlayerClock(face: face(true, timer(white: 1500)))));
      await tester.pump(const Duration(seconds: 3));
      expect(find.text('0.0'), findsOneWidget);
      expect(find.text('Čas vypršel'), findsNothing);
      expect(find.text('Málo času'), findsNothing);
      await tester.pumpWidget(const SizedBox());
    });

    testWidgets('flag text and offline icon in English, light theme',
        (tester) async {
      await tester.pumpWidget(host(
        Column(children: [
          PlayerClock(face: face(true, timer(white: 0, expired: true))),
          PlayerClock(face: face(false, timer(), connected: false)),
        ]),
        theme: AppTheme.light(),
        locale: const Locale('en'),
      ));
      expect(find.text('Time is up'), findsOneWidget);
      expect(find.text('No connection'), findsNothing);
      expect(find.byIcon(Icons.link_off_rounded), findsOneWidget);
    });

    testWidgets('long labels wrap at large text scale without overflow',
        (tester) async {
      tester.view.physicalSize = const Size(320 * 3, 640 * 3);
      tester.view.devicePixelRatio = 3;
      addTearDown(tester.view.reset);
      await tester.pumpWidget(MediaQuery(
        data: const MediaQueryData(textScaler: TextScaler.linear(1.6)),
        child: host(PlayerClock(face: face(true, timer(type: 0)))),
      ));
      expect(tester.takeException(), isNull);
    });
  });

  test('clock colour pairs keep strong contrast', () {
    double lum(Color c) {
      double ch(double v) =>
          v <= 0.03928 ? v / 12.92 : math.pow((v + 0.055) / 1.055, 2.4).toDouble();
      return 0.2126 * ch(c.r) + 0.7152 * ch(c.g) + 0.0722 * ch(c.b);
    }

    double ratio(Color a, Color b) {
      final la = lum(a), lb = lum(b);
      final hi = la > lb ? la : lb, lo = la > lb ? lb : la;
      return (hi + 0.05) / (lo + 0.05);
    }

    for (final t in [ClockTokens.dark, ClockTokens.light]) {
      expect(ratio(t.idleBackground, t.idleForeground), greaterThan(7));
      expect(ratio(t.activeBackground, t.activeForeground), greaterThan(7));
      expect(ratio(t.lowBackground, t.lowForeground), greaterThan(7));
      expect(ratio(t.expiredBackground, t.expiredForeground), greaterThan(7));
      expect(ratio(t.idleBackground, t.mutedForeground), greaterThan(4.5));
    }
  });
}
