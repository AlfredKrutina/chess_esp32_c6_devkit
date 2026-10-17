import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/models/board_timer_state.dart';
import '../../core/models/puzzle_challenge_state.dart';
import '../../core/theme/app_motion.dart';
import '../../core/theme/app_theme.dart';
import '../../core/utils/board_action_feedback.dart';
import '../../core/utils/fen_from_board.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/widgets/matrix_guard_banner.dart';
import '../../core/widgets/network_status_banners.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/widgets/session_error_panel.dart';
import '../../l10n/app_localizations.dart';
import '../analysis/move_eval_notifier.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import 'board/chess_board_widget.dart';
import 'controls/game_more_sheet.dart';
import 'controls/player_clock.dart';
import 'live_activity_session_listener.dart';
import 'report/game_end_report_screen.dart';
import 'state/game_ui_notifier.dart';

enum ConnectionTier { live, connecting, offline }

DateTime? _lastVerifiedBoardInteraction(BoardSessionState s) {
  if (s.transport == BoardTransport.wifi) {
    final snap = s.snapshotReceivedAt;
    final poll = s.lastSuccessfulPoll;
    if (snap != null && poll != null) {
      return snap.isAfter(poll) ? snap : poll;
    }
    return snap ?? poll;
  }
  return s.snapshotReceivedAt;
}

ConnectionTier connectionTierOf(BoardSessionState s) {
  if (s.transport == BoardTransport.none) return ConnectionTier.offline;
  if (s.busy) return ConnectionTier.connecting;
  if (s.transport == BoardTransport.mock) return ConnectionTier.live;

  final transportSessionAlive = s.pollingActive ||
      (s.transport == BoardTransport.ble && s.bleGattConnected);
  if (!transportSessionAlive) return ConnectionTier.offline;

  final staleLimit = s.transport == BoardTransport.ble
      ? const Duration(seconds: 300)
      : const Duration(seconds: 55);
  final now = DateTime.now();
  final last = _lastVerifiedBoardInteraction(s);

  if (last != null) {
    if (now.difference(last) <= staleLimit) return ConnectionTier.live;
    if (s.transport == BoardTransport.ble && s.bleGattConnected) {
      return ConnectionTier.live;
    }
    return ConnectionTier.offline;
  }

  final started = s.transportStartedAt;
  if (started != null &&
      now.difference(started) <= const Duration(seconds: 55)) {
    return ConnectionTier.connecting;
  }
  if (s.transport == BoardTransport.ble && s.bleGattConnected) {
    return ConnectionTier.live;
  }
  return ConnectionTier.offline;
}

class GameScreen extends ConsumerStatefulWidget {
  const GameScreen({super.key});

  @override
  ConsumerState<GameScreen> createState() => _GameScreenState();
}

class _GameScreenState extends ConsumerState<GameScreen> {
  bool _hintBusy = false;

  Future<void> _requestHint(
      BoardSessionState session, GameUiNotifier uiN) async {
    final snap = session.snapshot;
    if (snap == null || _hintBusy) return;
    final l10n = AppLocalizations.of(context)!;
    setState(() => _hintBusy = true);
    try {
      final prefs = ref.read(prefsRepositoryProvider);
      final depth = prefs.hintDepth.clamp(1, 18);
      final fen = fenFromSnapshot(snap);
      final best = await ref
          .read(stockfishApiClientProvider)
          .fetchBestMove(fen, depth: depth);
      uiN.showHintOverlay(best.from, best.to);
      if (session.transport == BoardTransport.wifi ||
          session.transport == BoardTransport.ble) {
        await ref
            .read(boardSessionNotifierProvider.notifier)
            .postHintDestination(best.to);
      }
    } catch (e) {
      if (!mounted) return;
      showAppSnackBar(
        context,
        l10n.gameHintFailed(userFacingErrorSummary(l10n, e)),
        errorStyle: true,
      );
    } finally {
      if (mounted) setState(() => _hintBusy = false);
    }
  }

  void _registerListeners() {
    ref.listen(
        boardSessionNotifierProvider
            .select((s) => s.snapshot?.status.isGameFinished), (prev, next) {
      if (next == true && prev != true) {
        if (!ref.read(prefsRepositoryProvider).endgameReportAutoOpen) return;
        Future.microtask(() {
          if (!mounted) return;
          Navigator.of(context).push(MaterialPageRoute(
              builder: (ctx) => const GameEndReportScreen()));
        });
      }
    });

    ref.listen(moveEvalNotifierProvider, (prev, next) {
      if (!ref.read(prefsRepositoryProvider).moveEvaluationEnabled) return;
      if (next.lastBusy) return;
      final msg = next.lastMessage;
      if (msg == null || msg.isEmpty) return;
      final completedRun = prev?.lastBusy == true && next.lastBusy == false;
      final newText = prev?.lastMessage != next.lastMessage;
      if (!completedRun && !newText) return;
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!context.mounted) return;
        showAppSnackBar(context, msg, duration: const Duration(seconds: 6));
      });
    });

    ref.listen(
      gameUiNotifierProvider.select((s) => s.puzzleSnackText),
      (prev, next) {
        if (next == null || next.isEmpty) return;
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!context.mounted) return;
          showAppSnackBar(context, next);
          ref.read(gameUiNotifierProvider.notifier).clearPuzzleSnack();
        });
      },
    );

    ref.listen<int?>(
      gameUiNotifierProvider.select((s) => s.puzzleCelebrationEloDelta),
      (prev, next) {
        if (next == null) return;
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!context.mounted) return;
          final l10n = context.l10n;
          showAppSnackBar(
            context,
            next > 0
                ? l10n.puzzleCelebrationBodyElo(next)
                : l10n.puzzleCelebrationBodyPlain,
          );
          ref.read(gameUiNotifierProvider.notifier).clearPuzzleCelebration();
        });
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    _registerListeners();
    final session = ref.watch(boardSessionNotifierProvider);
    final tier = connectionTierOf(session);
    final hasBoard = session.snapshot != null;
    final fade = AppMotion.resolve(context, AppMotion.crossFade);

    return Scaffold(
      body: SafeArea(
        bottom: false,
        child: LayoutBuilder(
          builder: (context, box) {
            final landscape =
                box.maxWidth >= 560 && box.maxWidth > box.maxHeight * 1.2;
            final strip = _StatusStrip(
              session: session,
              tier: tier,
              hintBusy: _hintBusy,
              onHint: () => _requestHint(
                session,
                ref.read(gameUiNotifierProvider.notifier),
              ),
              onMore: () => showGameMoreSheet(context),
            );
            final stripOnTop = !landscape || !hasBoard;

            return Stack(
              children: [
                Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const LiveActivitySessionListener(),
                    if (stripOnTop) strip,
                    Expanded(
                      child: AnimatedSwitcher(
                        duration: fade,
                        switchInCurve: AppMotion.standardCurve,
                        switchOutCurve: AppMotion.reverseCurve,
                        transitionBuilder: (child, anim) => FadeTransition(
                          opacity: anim,
                          child: ScaleTransition(
                            scale: Tween(begin: 0.98, end: 1.0).animate(anim),
                            child: child,
                          ),
                        ),
                        child: hasBoard
                            ? _TableLayout(
                                key: const ValueKey('table'),
                                session: session,
                                tier: tier,
                                landscape: landscape,
                                strip: stripOnTop ? null : strip,
                              )
                            : _EmptyBoardState(
                                key: const ValueKey('empty'),
                                busy: session.busy,
                              ),
                      ),
                    ),
                  ],
                ),
                const Positioned(
                  right: 4,
                  top: 52,
                  child: NetworkStatusBanners(
                    presentation: NetworkBannerPresentation.overlay,
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }
}

/// One quiet row: connection glyph, time control, then pause / hint / more as
/// icons. Status text stays behind a tap.
class _StatusStrip extends ConsumerWidget {
  const _StatusStrip({
    required this.session,
    required this.tier,
    required this.hintBusy,
    required this.onHint,
    required this.onMore,
  });

  final BoardSessionState session;
  final ConnectionTier tier;
  final bool hintBusy;
  final VoidCallback onHint;
  final VoidCallback onMore;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final snap = session.snapshot;
    final BoardTimerState? timer = snap?.clock ?? session.timer;
    final connected = tier == ConnectionTier.live;
    final paused = timer?.gamePaused ?? false;
    final canToggleClock = connected &&
        timer != null &&
        timer.isTimeControlEnabled &&
        snap?.status.isGameFinished != true &&
        !timer.timeExpired;
    final sessionN = ref.read(boardSessionNotifierProvider.notifier);
    final fade = AppMotion.resolve(context, AppMotion.crossFade);

    final timeControl = (timer != null && timer.isTimeControlEnabled)
        ? '${(timer.config.initialTimeMs / 60000).round()}+${(timer.config.incrementMs / 1000).round()}'
        : null;

    Widget iconAction({
      required String tooltip,
      required Widget icon,
      required VoidCallback? onPressed,
    }) {
      return PressableScale(
        pressedScale: 0.88,
        enabled: onPressed != null,
        child: IconButton(
          tooltip: tooltip,
          onPressed: onPressed,
          icon: icon,
        ),
      );
    }

    return SizedBox(
      height: 52,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 4),
        child: Row(
          children: [
            Flexible(
              child: _ConnectionButton(session: session, tier: tier),
            ),
            if (snap?.status.matrixGuardActive == true)
              iconAction(
                tooltip: l10n.gameMatrixGuardTitle,
                onPressed: () {
                  final shot = snap;
                  if (shot == null) return;
                  showDialog<void>(
                    context: context,
                    builder: (ctx) => Dialog(
                      backgroundColor: Colors.transparent,
                      elevation: 0,
                      child: MatrixGuardBanner(snapshot: shot),
                    ),
                  );
                },
                icon: Icon(Icons.grid_on_outlined, color: cs.error),
              ),
            if (timeControl != null)
              Padding(
                padding: const EdgeInsets.only(left: 4),
                child: Text(
                  timeControl,
                  style: theme.textTheme.titleSmall?.copyWith(
                    color: cs.onSurfaceVariant,
                    fontFeatures: const [FontFeature.tabularFigures()],
                  ),
                ),
              ),
            const Spacer(),
            if (snap != null) ...[
              iconAction(
                tooltip: paused ? l10n.gameBarResume : l10n.gamePauseClock,
                onPressed: canToggleClock
                    ? () => runBoardCommandWithSnackBar(
                          context,
                          paused
                              ? sessionN.postTimerResume
                              : sessionN.postTimerPause,
                        )
                    : null,
                icon: AnimatedSwitcher(
                  duration: fade,
                  transitionBuilder: (c, a) => FadeTransition(
                    opacity: a,
                    child: ScaleTransition(
                      scale: Tween(begin: 0.8, end: 1.0).animate(a),
                      child: c,
                    ),
                  ),
                  child: Icon(
                    paused ? Icons.play_arrow_rounded : Icons.pause_rounded,
                    key: ValueKey(paused),
                  ),
                ),
              ),
              iconAction(
                tooltip: l10n.gameHint,
                onPressed: hintBusy ? null : onHint,
                icon: AnimatedSwitcher(
                  duration: fade,
                  child: hintBusy
                      ? SizedBox(
                          key: const ValueKey('busy'),
                          width: 20,
                          height: 20,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            semanticsLabel: l10n.gameHintThinking,
                          ),
                        )
                      : const Icon(
                          Icons.lightbulb_outline_rounded,
                          key: ValueKey('idle'),
                        ),
                ),
              ),
              iconAction(
                tooltip: l10n.gameBarMore,
                onPressed: onMore,
                icon: const Icon(Icons.more_horiz_rounded),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

/// Transport glyph only. Status text opens after a tap so the strip does not
/// reflow the board.
class _ConnectionButton extends ConsumerWidget {
  const _ConnectionButton({required this.session, required this.tier});

  final BoardSessionState session;
  final ConnectionTier tier;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;

    final none = session.transport == BoardTransport.none;
    final demo = session.transport == BoardTransport.mock;
    final error = session.lastError;
    final (IconData icon, Color color, String? word) = switch (tier) {
      ConnectionTier.connecting => (
          Icons.sync_rounded,
          cs.onSurfaceVariant,
          l10n.gameTopConnecting,
        ),
      ConnectionTier.offline when none && error == null => (
          Icons.link_off_rounded,
          cs.onSurfaceVariant,
          null,
        ),
      ConnectionTier.offline => (
          Icons.link_off_rounded,
          cs.error,
          l10n.gameTopNotResponding,
        ),
      ConnectionTier.live => (
          switch (session.transport) {
            BoardTransport.wifi => Icons.wifi_rounded,
            BoardTransport.ble => Icons.bluetooth_rounded,
            _ => Icons.smart_toy_outlined,
          },
          error == null ? cs.onSurfaceVariant : cs.error,
          null,
        ),
    };

    final tooltip = [
      if (demo) l10n.uiDemo,
      word,
      if (error != null) l10n.lastErrorTitle,
      l10n.gameTopBoardTooltip,
    ].whereType<String>().join(' · ');

    return Tooltip(
      message: tooltip,
      child: PressableScale(
        pressedScale: 0.94,
        child: TextButton(
          onPressed: () => _onPressed(context, ref, word: word, error: error),
          style: TextButton.styleFrom(
            foregroundColor: color,
            minimumSize: const Size(44, 44),
            padding: const EdgeInsets.symmetric(horizontal: 10),
          ),
          child: AnimatedSwitcher(
            duration: AppMotion.resolve(context, AppMotion.crossFade),
            switchInCurve: AppMotion.standardCurve,
            switchOutCurve: AppMotion.reverseCurve,
            child: Icon(icon, key: ValueKey(icon), size: 20, color: color),
          ),
        ),
      ),
    );
  }

  void _onPressed(
    BuildContext context,
    WidgetRef ref, {
    required String? word,
    required Object? error,
  }) {
    final quiet = tier == ConnectionTier.live && error == null;
    if (quiet || (word == null && error == null)) {
      pushBoardDiscoveryRoute(context);
      return;
    }

    final l10n = context.l10n;
    final devMode = ref.read(prefsRepositoryProvider).developerModeUnlocked;
    showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(word ?? l10n.lastErrorTitle),
        content: error == null
            ? null
            : SizedBox(
                width: math.min(420, MediaQuery.sizeOf(ctx).width - 96),
                child: SessionErrorPanel(
                  error: error,
                  title: l10n.lastErrorTitle,
                  devMode: devMode,
                  compact: true,
                ),
              ),
        actions: [
          if (error != null)
            TextButton(
              onPressed: () {
                Navigator.of(ctx).pop();
                ref.read(boardSessionNotifierProvider.notifier).refreshNow();
              },
              child: Text(l10n.uiRetry),
            ),
          TextButton(
            onPressed: () {
              Navigator.of(ctx).pop();
              pushBoardDiscoveryRoute(context);
            },
            child: Text(l10n.gameFindBoard),
          ),
        ],
      ),
    );
  }
}

class _EmptyBoardState extends StatelessWidget {
  const _EmptyBoardState({super.key, required this.busy});

  final bool busy;

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    return LayoutBuilder(
      builder: (context, constraints) => SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: ConstrainedBox(
          constraints: BoxConstraints(minHeight: constraints.maxHeight - 48),
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 360),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Container(
                    width: 96,
                    height: 96,
                    decoration: BoxDecoration(
                      color: cs.surfaceContainer,
                      borderRadius: BorderRadius.circular(AppTheme.radius + 8),
                    ),
                    child: Icon(Icons.grid_on_rounded,
                        size: 44, color: cs.onSurfaceVariant),
                  ),
                  const SizedBox(height: 24),
                  Semantics(
                    header: true,
                    child: Text(
                      l10n.uiConnectBoard,
                      textAlign: TextAlign.center,
                      style: theme.textTheme.titleLarge,
                    ),
                  ),
                  const SizedBox(height: 28),
                  PressableScale(
                    enabled: !busy,
                    child: FilledButton.icon(
                      onPressed:
                          busy ? null : () => pushBoardDiscoveryRoute(context),
                      icon: AnimatedSwitcher(
                        duration:
                            AppMotion.resolve(context, AppMotion.crossFade),
                        child: busy
                            ? const SizedBox(
                                key: ValueKey('busy'),
                                width: 18,
                                height: 18,
                                child:
                                    CircularProgressIndicator(strokeWidth: 2),
                              )
                            : const Icon(Icons.bluetooth_searching,
                                key: ValueKey('idle')),
                      ),
                      label: Text(
                          busy ? l10n.gameTopConnecting : l10n.gameFindBoard),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

/// Clock, board, clock in portrait; board beside the strip and two clocks in
/// landscape. The clocks are always visible during a game.
class _TableLayout extends ConsumerWidget {
  const _TableLayout({
    super.key,
    required this.session,
    required this.tier,
    required this.landscape,
    this.strip,
  });

  final BoardSessionState session;
  final ConnectionTier tier;
  final bool landscape;
  final Widget? strip;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ui = ref.watch(gameUiNotifierProvider);
    final snap = session.snapshot;
    final status = snap?.status;
    final timer = snap?.clock ?? session.timer;
    final connected = tier == ConnectionTier.live;
    final finished = status?.isGameFinished == true;
    final whiteToMove =
        !(status?.currentPlayer.toLowerCase().startsWith('b') ?? false);

    ClockFace faceFor(bool white) => resolveClockFace(
          white: white,
          timer: timer,
          connected: connected,
          gameFinished: finished,
          whiteToMoveFallback: whiteToMove,
        );

    final bottomIsWhite = !ui.boardFlipped;
    final pc = ui.puzzleChallenge;
    final puzzle = (pc != null && pc.hasVerifier) ? pc : null;

    Widget board(double side) => SizedBox(
          width: side,
          height: side,
          child: Stack(
            children: [
              _Board(side: side),
              Positioned(
                left: 8,
                right: 8,
                bottom: 8,
                child: _BoardNotes(session: session),
              ),
            ],
          ),
        );

    Widget clockColumn() {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (strip != null) strip!,
          if (puzzle != null) ...[
            _PuzzleStrip(puzzle: puzzle),
            const Spacer(),
          ] else ...[
            Expanded(
              child: PlayerClock(
                face: faceFor(!bottomIsWhite),
                compact: true,
              ),
            ),
            const SizedBox(height: 8),
            Expanded(
              child: PlayerClock(
                face: faceFor(bottomIsWhite),
                compact: true,
              ),
            ),
          ],
        ],
      );
    }

    return LayoutBuilder(
      builder: (context, c) {
        if (landscape) {
          final byWidth = math.min(
            c.maxWidth * 0.55,
            math.max(0.0, c.maxWidth - 16),
          );
          final fitted = math.max(
            200.0,
            math.min(c.maxHeight - 16, byWidth),
          );
          final natural = math.min(
            math.max(200.0, byWidth),
            math.max(0.0, c.maxWidth - 16),
          );
          final overflows = natural + 16 > c.maxHeight;
          final side = overflows ? natural : fitted;
          final row = Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              board(side),
              const SizedBox(width: 16),
              Expanded(child: clockColumn()),
            ],
          );
          return SafeArea(
            top: false,
            left: false,
            child: overflows
                ? SingleChildScrollView(
                    physics: const ClampingScrollPhysics(),
                    padding: const EdgeInsets.fromLTRB(8, 4, 12, 8),
                    child: SizedBox(height: side, child: row),
                  )
                : Padding(
                    padding: const EdgeInsets.fromLTRB(8, 4, 12, 8),
                    child: row,
                  ),
          );
        }

        final viewport = c.maxHeight.isFinite ? c.maxHeight : 0.0;
        final side = math.max(
          0.0,
          math.min(c.maxWidth, 720.0) - 24,
        );
        // Clocks use minHeight 112; reserve a little more so a flagged label
        // does not overflow the fitted layout.
        const clockReserve = 144.0;
        final headerReserve = puzzle != null ? 120.0 : clockReserve;
        final tailReserve = puzzle != null ? 0.0 : clockReserve;
        final needed = headerReserve + 8 + side + (tailReserve == 0 ? 0 : 8 + tailReserve) + 8;
        final overflows = needed > viewport;

        final column = Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          mainAxisAlignment: overflows || puzzle != null
              ? MainAxisAlignment.start
              : MainAxisAlignment.spaceBetween,
          children: [
            if (puzzle != null)
              _PuzzleStrip(puzzle: puzzle)
            else
              PlayerClock(face: faceFor(!bottomIsWhite)),
            const SizedBox(height: 8),
            if (!overflows && puzzle != null)
              Expanded(child: Center(child: board(side)))
            else
              Center(child: board(side)),
            if (puzzle == null) ...[
              const SizedBox(height: 8),
              PlayerClock(face: faceFor(bottomIsWhite)),
            ],
          ],
        );

        return Align(
          alignment: Alignment.topCenter,
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 720),
            child: overflows
                ? SingleChildScrollView(
                    physics: const ClampingScrollPhysics(),
                    padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
                    child: column,
                  )
                : SizedBox(
                    height: viewport,
                    child: Padding(
                      padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
                      child: column,
                    ),
                  ),
          ),
        );
      },
    );
  }
}

class _Board extends StatelessWidget {
  const _Board({required this.side});

  final double side;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(AppTheme.radius),
      child: SizedBox(
        width: side,
        height: side,
        child: const RepaintBoundary(child: ChessBoardWidget()),
      ),
    );
  }
}

/// Short-lived notes laid over the bottom edge of the board, so the layout
/// never jumps: preview, history, board messages.
class _BoardNotes extends ConsumerWidget {
  const _BoardNotes({required this.session});

  final BoardSessionState session;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ui = ref.watch(gameUiNotifierProvider);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final notes = <Widget>[];

    final puzzleActive = ui.puzzleChallenge?.hasVerifier == true;
    if ((ui.sandboxMode || ui.historyReviewMoveIndex != null) &&
        !puzzleActive) {
      notes.add(_Note(
        icon: ui.sandboxMode ? Icons.visibility_outlined : Icons.history_rounded,
        text: ui.sandboxMode ? l10n.uiPreview : l10n.gameHistory,
        actionTooltip: l10n.gameReturnLive,
        onAction: () {
          if (ui.sandboxMode) {
            uiN.exitSandboxAndRefresh();
          } else {
            uiN.setHistoryReviewIndex(null, session.snapshot);
          }
        },
      ));
    }
    final transient = ui.transientBoardMessage;
    if (transient != null) notes.add(_Note(text: transient));
    final sandboxMsg = ui.sandboxMessage;
    if (sandboxMsg != null) notes.add(_Note(text: sandboxMsg, color: cs.error));

    final duration = AppMotion.resolve(context, AppMotion.stateChange);
    return AnimatedSwitcher(
      duration: duration,
      switchInCurve: AppMotion.standardCurve,
      switchOutCurve: AppMotion.reverseCurve,
      transitionBuilder: (child, anim) => FadeTransition(
        opacity: anim,
        child: SlideTransition(
          position: Tween(begin: const Offset(0, 0.15), end: Offset.zero)
              .animate(anim),
          child: child,
        ),
      ),
      child: notes.isEmpty
          ? const SizedBox.shrink(key: ValueKey('none'))
          : Column(
              key: ValueKey(notes.length.toString() +
                  (transient ?? '') +
                  (sandboxMsg ?? '') +
                  ui.sandboxMode.toString() +
                  (ui.historyReviewMoveIndex?.toString() ?? '')),
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (var i = 0; i < notes.length; i++) ...[
                  if (i > 0) const SizedBox(height: 6),
                  notes[i],
                ],
              ],
            ),
    );
  }
}

class _Note extends StatelessWidget {
  const _Note({
    required this.text,
    this.icon,
    this.color,
    this.actionTooltip,
    this.onAction,
  });

  final String text;
  final IconData? icon;
  final Color? color;
  final String? actionTooltip;
  final VoidCallback? onAction;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final fg = color ?? cs.onSurface;
    return Semantics(
      liveRegion: true,
      child: Container(
        padding: EdgeInsets.fromLTRB(14, 4, onAction == null ? 14 : 2, 4),
        constraints: const BoxConstraints(minHeight: 44),
        decoration: BoxDecoration(
          color: cs.surface.withValues(alpha: 0.9),
          borderRadius: BorderRadius.circular(AppTheme.radius),
        ),
        child: Row(
          children: [
            if (icon != null) ...[
              Icon(icon, size: 18, color: cs.onSurfaceVariant),
              const SizedBox(width: 8),
            ],
            Expanded(
              child: Text(
                text,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                style: theme.textTheme.bodyMedium?.copyWith(color: fg),
              ),
            ),
            if (onAction != null)
              IconButton(
                tooltip: actionTooltip,
                onPressed: onAction,
                icon: const Icon(Icons.undo_rounded),
              ),
          ],
        ),
      ),
    );
  }
}

class _PuzzleStrip extends ConsumerWidget {
  const _PuzzleStrip({required this.puzzle});

  final PuzzleChallengeState puzzle;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    return Container(
      constraints: const BoxConstraints(minHeight: 64),
      padding: const EdgeInsets.fromLTRB(16, 8, 4, 8),
      decoration: BoxDecoration(
        color: cs.surfaceContainer,
        borderRadius: BorderRadius.circular(AppTheme.radius + 4),
      ),
      child: Row(
        children: [
          Icon(Icons.extension_outlined, color: cs.primary),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              puzzle.title,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: theme.textTheme.titleSmall,
            ),
          ),
          const SizedBox(width: 8),
          Text(
            '${puzzle.playedUci.length}/${puzzle.solverPlies}',
            style: theme.textTheme.titleSmall?.copyWith(
              color: cs.onSurfaceVariant,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
          IconButton(
            tooltip: l10n.gameReturnLive,
            onPressed: () =>
                ref.read(gameUiNotifierProvider.notifier).returnToLiveGame(),
            icon: const Icon(Icons.close_rounded),
          ),
        ],
      ),
    );
  }
}
