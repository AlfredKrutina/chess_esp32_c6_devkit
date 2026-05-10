import 'dart:math' as math;
import 'dart:ui' show ImageFilter;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/theme/app_motion.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';
import '../../core/models/board_timer_state.dart';
import '../../core/models/puzzle_challenge_state.dart';
import '../../core/utils/board_action_feedback.dart';
import '../../core/widgets/app_modal_sheet.dart';
import '../../core/widgets/brief_celebration_dialog.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/utils/fen_from_board.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/widgets/network_status_banners.dart';
import '../../core/widgets/session_error_panel.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../analysis/move_eval_notifier.dart';
import '../coach/coach_chat_panel.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import 'board/chess_board_widget.dart';
import 'controls/game_control_panel.dart';
import 'controls/new_game_time_sheet.dart';
import 'controls/timer_display.dart';
import 'history/move_history_view.dart';
import 'live_activity_session_listener.dart';
import 'report/game_end_report_screen.dart';
import 'state/game_ui_notifier.dart';

enum _ConnectionTier { live, connecting, offline }

enum _GameOverflowAction {
  newGame,
  gameControls,
  analysis,
  toggleCoachPanel,
  disconnect,
}

/// Minimum width for board + game panel + AI side chat (desktop).
const double _kTriplePaneMinWidth = 1060;

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

_ConnectionTier _connectionTier(BoardSessionState s) {
  if (s.transport == BoardTransport.none) return _ConnectionTier.offline;
  if (s.busy) return _ConnectionTier.connecting;
  if (s.transport == BoardTransport.mock) return _ConnectionTier.live;

  final transportSessionAlive = s.pollingActive ||
      (s.transport == BoardTransport.ble && s.bleGattConnected);
  if (!transportSessionAlive) return _ConnectionTier.offline;

  final staleLimit = s.transport == BoardTransport.ble
      ? const Duration(seconds: 300)
      : const Duration(seconds: 55);
  final now = DateTime.now();
  final last = _lastVerifiedBoardInteraction(s);

  if (last != null) {
    if (now.difference(last) <= staleLimit) return _ConnectionTier.live;
    if (s.transport == BoardTransport.ble && s.bleGattConnected) {
      return _ConnectionTier.live;
    }
    return _ConnectionTier.offline;
  }

  final started = s.transportStartedAt;
  if (started != null &&
      now.difference(started) <= const Duration(seconds: 55)) {
    return _ConnectionTier.connecting;
  }
  if (s.transport == BoardTransport.ble && s.bleGattConnected) {
    return _ConnectionTier.live;
  }
  return _ConnectionTier.offline;
}

Color _connectionDotColor(BoardSessionState s, ColorScheme cs) {
  return switch (_connectionTier(s)) {
    _ConnectionTier.live => cs.primary,
    _ConnectionTier.connecting => cs.tertiary,
    _ConnectionTier.offline => cs.outline,
  };
}

class GameScreen extends ConsumerStatefulWidget {
  const GameScreen({super.key});

  @override
  ConsumerState<GameScreen> createState() => _GameScreenState();
}

class _GameScreenState extends ConsumerState<GameScreen> {
  bool _hintBusy = false;
  bool _demoClockPaused = false;

  void _openGameControlsSheet() {
    showAppModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (ctx) => DraggableScrollableSheet(
        initialChildSize: 0.58,
        minChildSize: 0.32,
        maxChildSize: 0.94,
        expand: false,
        builder: (_, scroll) => SingleChildScrollView(
          controller: scroll,
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 28),
          child: const GameControlPanel(),
        ),
      ),
    );
  }

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
      if (!mounted) return;
      showAppSnackBar(
        context,
        l10n.gameBestMoveSnack(best.from, best.to),
      );
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

  void _demoBoardFeatureSnack(AppLocalizations l10n, String label) {
    if (!mounted) return;
    showAppSnackBar(
      context,
      l10n.gameDemoBoardSnack(label),
    );
  }

  Widget _framedBoard(double side) {
    // Bez Material: žádný outline (desktop BorderSide), žádný stín ani surfaceTint (M3),
    // které vypadaly jako „rámeček“ pod deskou i při stejné barvě výplně.
    final shell = Theme.of(context).scaffoldBackgroundColor;
    return ClipRRect(
      borderRadius: BorderRadius.circular(14),
      child: ColoredBox(
        color: shell,
        child: SizedBox(
          width: side,
          height: side,
          child: const RepaintBoundary(
            child: ChessBoardWidget(),
          ),
        ),
      ),
    );
  }

  /// Kompaktní panel pod šachovnicí — nesnižuje viditelnou plochu figurek (žádný overlay nahoře).
  Widget _puzzleGlassHud(PuzzleChallengeState pc) {
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;
    return ClipRRect(
      borderRadius: BorderRadius.circular(18),
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 22, sigmaY: 22),
        child: DecoratedBox(
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(18),
            color: cs.surface.withValues(alpha: 0.52),
            border: Border.all(color: cs.onSurface.withValues(alpha: 0.11)),
            boxShadow: [
              BoxShadow(
                color: Colors.black.withValues(alpha: 0.08),
                blurRadius: 20,
                offset: const Offset(0, 6),
              ),
            ],
          ),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 4),
            child: Row(
              children: [
                Icon(Icons.extension, size: 22, color: cs.primary),
                const SizedBox(width: 10),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        pc.title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: tt.titleSmall?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                      Text(
                        l10n.gamePuzzleMovesProgress(
                          pc.playedUci.length,
                          pc.solutionUci.length,
                        ),
                        style: tt.bodySmall?.copyWith(
                          color: cs.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
                IconButton(
                  icon: const Icon(Icons.sports_esports_outlined),
                  tooltip: l10n.gameReturnLive,
                  onPressed: () => ref
                      .read(gameUiNotifierProvider.notifier)
                      .returnToLiveGame(),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  /// [edgeToEdge] — „Jen deska“. [expandInParent] — vyplní výšku řádku (Row); v Column scroll nesmí být true.
  Widget _boardPane(
    BoxConstraints c,
    double zoom, {
    bool edgeToEdge = false,
    bool expandInParent = false,

    /// V „Jen deska“ je HUD vytažený do spodního sloupce Stacku, aby ho nepřekryla „Nová hra“.
    bool embedPuzzleHudInBoardPane = true,
  }) {
    final ui = ref.watch(gameUiNotifierProvider);
    final pc = ui.puzzleChallenge;
    final puzzleHud =
        (pc != null && pc.hasVerifier) ? _puzzleGlassHud(pc) : null;

    final outerPad = edgeToEdge ? 4.0 : 12.0;
    final maxW = math.max(0.0, c.maxWidth - outerPad * 2);
    final maxH = c.maxHeight.isFinite
        ? math.max(0.0, c.maxHeight - outerPad * 2)
        : double.infinity;
    final raw = maxH.isFinite ? math.min(maxW, maxH) : maxW;
    final side = math.max(raw, 200.0);

    final scaledBoard = Transform.scale(
      scale: zoom,
      alignment: Alignment.center,
      child: _framedBoard(side),
    );

    if (expandInParent) {
      final bg = Theme.of(context).scaffoldBackgroundColor;
      if (puzzleHud != null && embedPuzzleHudInBoardPane) {
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              child: Stack(
                fit: StackFit.expand,
                children: [
                  Positioned.fill(child: ColoredBox(color: bg)),
                  Center(
                    child: Padding(
                      padding: EdgeInsets.all(outerPad),
                      child: LayoutBuilder(
                        builder: (ctx, bc) {
                          final d = math.min(bc.maxWidth, bc.maxHeight);
                          final s = math.max(d, 200.0);
                          return Transform.scale(
                            scale: zoom,
                            alignment: Alignment.center,
                            child: _framedBoard(s),
                          );
                        },
                      ),
                    ),
                  ),
                ],
              ),
            ),
            SafeArea(
              top: false,
              minimum: EdgeInsets.zero,
              child: Padding(
                padding: const EdgeInsets.fromLTRB(12, 6, 12, 10),
                child: puzzleHud,
              ),
            ),
          ],
        );
      }
      return SizedBox.expand(
        child: Stack(
          fit: StackFit.expand,
          children: [
            Positioned.fill(child: ColoredBox(color: bg)),
            Center(
              child: Padding(
                padding: EdgeInsets.all(outerPad),
                child: scaledBoard,
              ),
            ),
          ],
        ),
      );
    }

    if (puzzleHud != null) {
      final innerW = math.max(0.0, c.maxWidth - outerPad * 2);
      return Padding(
        padding: EdgeInsets.all(outerPad),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            SizedBox(
              width: innerW,
              height: side,
              child: Center(child: scaledBoard),
            ),
            const SizedBox(height: 10),
            puzzleHud,
          ],
        ),
      );
    }

    return Padding(
      padding: EdgeInsets.all(outerPad),
      child: Stack(
        alignment: Alignment.center,
        children: [
          Center(child: scaledBoard),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    ref.listen(
        boardSessionNotifierProvider
            .select((s) => s.snapshot?.status.isGameFinished), (prev, next) {
      if (next == true && prev != true) {
        Future.microtask(() {
          if (context.mounted) {
            Navigator.of(context).push(MaterialPageRoute(
                builder: (ctx) => const GameEndReportScreen()));
          }
        });
      }
    });

    final prefs = ref.watch(prefsRepositoryProvider);
    final l10n = context.l10n;

    ref.listen(moveEvalNotifierProvider, (prev, next) {
      if (!ref.read(prefsRepositoryProvider).moveEvaluationEnabled) return;
      if (next.lastBusy) return;
      final msg = next.lastMessage;
      if (msg == null || msg.isEmpty) return;
      // Nový výsledek = skončilo počítání (busy→hotovo) nebo nová zpráva po předchozím běhu.
      final completedRun = prev?.lastBusy == true && next.lastBusy == false;
      final newText = prev?.lastMessage != next.lastMessage;
      if (!completedRun && !newText) return;
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!context.mounted) return;
        showAppSnackBar(
          context,
          msg,
          duration: const Duration(seconds: 6),
        );
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
        WidgetsBinding.instance.addPostFrameCallback((_) async {
          if (!context.mounted) return;
          final l10n = context.l10n;
          final delta = next;
          final cs = Theme.of(context).colorScheme;
          await showBriefCelebrationDialog(
            context: context,
            barrierColor: Colors.green.withValues(alpha: 0.22),
            icon: Icon(
              Icons.emoji_events_rounded,
              color: cs.tertiary,
              size: 56,
            ),
            title: l10n.puzzleCelebrationTitle,
            message: delta > 0
                ? l10n.puzzleCelebrationBodyElo(delta)
                : l10n.puzzleCelebrationBodyPlain,
            actionLabel: l10n.commonOk,
          );
          if (!context.mounted) return;
          ref.read(gameUiNotifierProvider.notifier).clearPuzzleCelebration();
        });
      },
    );
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    final sessionN = ref.read(boardSessionNotifierProvider.notifier);
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;
    final isFinished = session.snapshot?.status.isGameFinished == true;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: Theme.of(context).scaffoldBackgroundColor,
      appBar: AppBar(
        leadingWidth: 52,
        leading: IconButton(
          tooltip: l10n.gameFindBoardTooltip,
          padding: const EdgeInsets.all(10),
          onPressed: () => pushBoardDiscoveryRoute(context),
          icon: AnimatedSwitcher(
            duration:
                AppMotion.microInteraction + const Duration(milliseconds: 40),
            switchInCurve: AppMotion.standardCurve,
            switchOutCurve: AppMotion.reverseCurve,
            transitionBuilder: (child, anim) =>
                FadeTransition(opacity: anim, child: child),
            child: Container(
              key: ValueKey<Color>(_connectionDotColor(session, cs)),
              width: 14,
              height: 14,
              decoration: BoxDecoration(
                color: _connectionDotColor(session, cs),
                shape: BoxShape.circle,
                boxShadow: [
                  BoxShadow(
                      color: cs.shadow.withValues(alpha: 0.25), blurRadius: 3)
                ],
              ),
            ),
          ),
        ),
        titleSpacing: 4,
        title: Row(
          children: [
            Icon(Icons.grid_view_rounded, color: cs.primary, size: 22),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                l10n.gameAppBarTitle,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: Theme.of(context).textTheme.titleLarge,
              ),
            ),
          ],
        ),
        actions: [
          PopupMenuButton<String>(
            tooltip: l10n.gameLayoutTooltip,
            onSelected: uiN.setLayoutMode,
            itemBuilder: (ctx) => [
              PopupMenuItem(
                  value: 'standard', child: Text(l10n.gameLayoutStandard)),
              PopupMenuItem(
                  value: 'boardOnly', child: Text(l10n.gameLayoutBoardOnly)),
            ],
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 6),
              child: Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
                decoration: BoxDecoration(
                  color: cs.surfaceContainerHigh,
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(
                      ui.layoutMode == 'boardOnly'
                          ? Icons.grid_on
                          : Icons.view_column_outlined,
                      size: 18,
                    ),
                    const SizedBox(width: 4),
                    Text(
                      ui.layoutMode == 'boardOnly'
                          ? l10n.gameLayoutBoardOnly
                          : l10n.gameLayoutStandard,
                      style: Theme.of(context).textTheme.labelMedium,
                    ),
                    const Icon(Icons.arrow_drop_down, size: 18),
                  ],
                ),
              ),
            ),
          ),
          if (isFinished)
            IconButton(
              icon: const Icon(Icons.summarize_outlined, color: Colors.amber),
              tooltip: l10n.gameSummaryTooltip,
              onPressed: () => Navigator.of(context).push(MaterialPageRoute(
                  builder: (ctx) => const GameEndReportScreen())),
            ),
          PopupMenuButton<_GameOverflowAction>(
            tooltip: l10n.gameMoreOptionsTooltip,
            icon: const Icon(Icons.more_vert),
            onSelected: (action) async {
              switch (action) {
                case _GameOverflowAction.newGame:
                  await showNewGameWithTimeSheet(context);
                  break;
                case _GameOverflowAction.gameControls:
                  _openGameControlsSheet();
                  break;
                case _GameOverflowAction.analysis:
                  ref.read(mainNavTabIndexProvider.notifier).state =
                      AppMainTab.analysis;
                  break;
                case _GameOverflowAction.toggleCoachPanel:
                  final p = ref.read(prefsRepositoryProvider);
                  await p
                      .setDesktopCoachRailVisible(!p.desktopCoachRailVisible);
                  ref.invalidate(prefsRepositoryProvider);
                  break;
                case _GameOverflowAction.disconnect:
                  ref.read(boardSessionNotifierProvider.notifier).disconnect();
                  break;
              }
            },
            itemBuilder: (ctx) {
              final wideDesktopCoach = isDesktopEmbedder() &&
                  ui.layoutMode != 'boardOnly' &&
                  MediaQuery.sizeOf(context).width >= 680;
              return [
                if (session.transport == BoardTransport.wifi ||
                    session.transport == BoardTransport.ble ||
                    session.transport == BoardTransport.mock)
                  PopupMenuItem(
                    value: _GameOverflowAction.newGame,
                    child: Row(
                      children: [
                        Icon(Icons.restart_alt_rounded,
                            size: 22, color: cs.primary),
                        const SizedBox(width: 12),
                        Expanded(child: Text(l10n.gameNewGame)),
                      ],
                    ),
                  ),
                PopupMenuItem(
                  value: _GameOverflowAction.gameControls,
                  child: Row(
                    children: [
                      Icon(Icons.tune, size: 22, color: cs.onSurface),
                      const SizedBox(width: 12),
                      Expanded(child: Text(l10n.gameControlsTitle)),
                    ],
                  ),
                ),
                PopupMenuItem(
                  value: _GameOverflowAction.analysis,
                  child: Row(
                    children: [
                      Icon(Icons.show_chart, size: 22, color: cs.onSurface),
                      const SizedBox(width: 12),
                      Expanded(child: Text(l10n.gameAnalysisTooltip)),
                    ],
                  ),
                ),
                if (wideDesktopCoach)
                  PopupMenuItem(
                    value: _GameOverflowAction.toggleCoachPanel,
                    child: Row(
                      children: [
                        Icon(
                          prefs.desktopCoachRailVisible
                              ? Icons.chat_rounded
                              : Icons.chat_outlined,
                          size: 22,
                          color: cs.onSurface,
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            prefs.desktopCoachRailVisible
                                ? l10n.gameCoachRailHide
                                : l10n.gameCoachRailShow,
                          ),
                        ),
                      ],
                    ),
                  ),
                PopupMenuItem(
                  value: _GameOverflowAction.disconnect,
                  child: Row(
                    children: [
                      Icon(Icons.link_off, size: 22, color: cs.error),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Text(
                          l10n.gameDisconnectTooltip,
                          style: TextStyle(color: cs.error),
                        ),
                      ),
                    ],
                  ),
                ),
              ];
            },
          ),
        ],
      ),
      body: Stack(
        clipBehavior: Clip.none,
        alignment: Alignment.topCenter,
        children: [
          Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              const LiveActivitySessionListener(),
              if (session.snapshot != null)
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 8, 12, 0),
                  child: SingleChildScrollView(
                    scrollDirection: Axis.horizontal,
                    child: Builder(
                      builder: (context) {
                        final theme = Theme.of(context);
                        BoardTimerState? timerState =
                            session.snapshot?.clock ?? session.timer;
                        final snap = session.snapshot;
                        final gameOver = snap != null &&
                            (snap.status.gameEnd?.ended == true ||
                                snap.status.gameState.toLowerCase() ==
                                    'finished');
                        final clockPaused = timerState?.gamePaused ?? false;
                        final canClockToggle = timerState != null &&
                            timerState.isTimeControlEnabled &&
                            !gameOver &&
                            !timerState.timeExpired;
                        final clockBtnStyle = OutlinedButton.styleFrom(
                          textStyle: theme.textTheme.labelMedium
                              ?.copyWith(fontWeight: FontWeight.w500),
                          visualDensity: VisualDensity.compact,
                          padding: const EdgeInsets.symmetric(
                            horizontal: 12,
                            vertical: 8,
                          ),
                        );
                        final boardQuickActions = <Widget>[
                          if (session.transport == BoardTransport.wifi ||
                              session.transport == BoardTransport.ble) ...[
                            if (canClockToggle)
                              OutlinedButton.icon(
                                style: clockBtnStyle,
                                onPressed: () => runBoardCommandWithSnackBar(
                                  context,
                                  clockPaused
                                      ? sessionN.postTimerResume
                                      : sessionN.postTimerPause,
                                  successMessage: clockPaused
                                      ? l10n.gameClockResumedSnackbar
                                      : l10n.gameClockPauseSent,
                                ),
                                icon: Icon(
                                  clockPaused
                                      ? Icons.play_circle_outline
                                      : Icons.pause_circle_outline,
                                ),
                                label: Text(
                                  clockPaused
                                      ? l10n.gameResumeClock
                                      : l10n.gamePauseClock,
                                ),
                              ),
                            IconButton(
                              tooltip: l10n.gameHideBoardHintTooltip,
                              style: IconButton.styleFrom(
                                foregroundColor: theme.colorScheme.onSurface,
                              ),
                              onPressed: () => runBoardCommandWithSnackBar(
                                context,
                                sessionN.postHintClearBoard,
                                successMessage:
                                    l10n.gameHintLedsClearedSnackbar,
                              ),
                              icon: const Icon(Icons.visibility_off_outlined),
                            ),
                          ] else if (session.transport ==
                              BoardTransport.mock) ...[
                            OutlinedButton.icon(
                              style: clockBtnStyle,
                              onPressed: () {
                                setState(
                                    () => _demoClockPaused = !_demoClockPaused);
                                _demoBoardFeatureSnack(
                                  l10n,
                                  _demoClockPaused
                                      ? l10n.gamePauseClock
                                      : l10n.gameResumeClock,
                                );
                              },
                              icon: Icon(
                                _demoClockPaused
                                    ? Icons.play_circle_outline
                                    : Icons.pause_circle_outline,
                              ),
                              label: Text(
                                _demoClockPaused
                                    ? l10n.gameResumeClock
                                    : l10n.gamePauseClock,
                              ),
                            ),
                            IconButton(
                              tooltip: l10n.gameHideBoardHintTooltip,
                              style: IconButton.styleFrom(
                                foregroundColor: theme.colorScheme.onSurface,
                              ),
                              onPressed: () => _demoBoardFeatureSnack(
                                  l10n, l10n.gameClearHintLeds),
                              icon: const Icon(Icons.visibility_off_outlined),
                            ),
                          ],
                        ];
                        return Row(
                          crossAxisAlignment: CrossAxisAlignment.center,
                          children: [
                            IconButton.filledTonal(
                              tooltip: _hintBusy
                                  ? l10n.gameHintThinking
                                  : l10n.gameHint,
                              onPressed: _hintBusy
                                  ? null
                                  : () => _requestHint(session, uiN),
                              icon: _hintBusy
                                  ? const SizedBox(
                                      width: 22,
                                      height: 22,
                                      child: CircularProgressIndicator(
                                          strokeWidth: 2),
                                    )
                                  : const Icon(Icons.tips_and_updates_outlined),
                            ),
                            for (final w in boardQuickActions) ...[
                              const SizedBox(width: 8),
                              w,
                            ],
                          ],
                        );
                      },
                    ),
                  ),
                ),
              Expanded(
                child: session.snapshot == null
                    ? LayoutBuilder(
                        builder: (context, constraints) {
                          return SingleChildScrollView(
                            padding: const EdgeInsets.all(24),
                            child: ConstrainedBox(
                              constraints: BoxConstraints(
                                minWidth: constraints.maxWidth,
                                minHeight: constraints.maxHeight,
                              ),
                              child: Column(
                                mainAxisAlignment: MainAxisAlignment.center,
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  Text(l10n.gameNoSnapshotYet),
                                  const SizedBox(height: 12),
                                  Text(
                                    l10n.gameNoSnapshotBody,
                                    textAlign: TextAlign.center,
                                    style:
                                        Theme.of(context).textTheme.bodyMedium,
                                  ),
                                  const SizedBox(height: 20),
                                  PressableScale(
                                    child: FilledButton.icon(
                                      onPressed: () =>
                                          pushBoardDiscoveryRoute(context),
                                      icon:
                                          const Icon(Icons.bluetooth_searching),
                                      label: Text(l10n.gameFindBoard),
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          );
                        },
                      )
                    : LayoutBuilder(
                        builder: (context, c) {
                          final layout = ui.layoutMode;
                          final zoom = ui.boardZoomStorage.clamp(0.7, 1.5);
                          final coachRailDesired = isDesktopEmbedder() &&
                              prefs.desktopCoachRailVisible;
                          final sidePanel = Padding(
                            padding: const EdgeInsets.all(12),
                            child: Card(
                              child: Padding(
                                padding: const EdgeInsets.all(12),
                                child: Column(
                                  crossAxisAlignment:
                                      CrossAxisAlignment.stretch,
                                  children: [
                                    _statusLine(session, devMode, l10n),
                                    if (ui.transientBoardMessage != null)
                                      SizedBox(
                                        height: 72,
                                        width: double.infinity,
                                        child: ClipRRect(
                                          borderRadius:
                                              BorderRadius.circular(8),
                                          child: Material(
                                            color: Theme.of(context)
                                                .colorScheme
                                                .secondaryContainer,
                                            child: Align(
                                              alignment: Alignment.centerLeft,
                                              child: Padding(
                                                padding:
                                                    const EdgeInsets.symmetric(
                                                  horizontal: 12,
                                                  vertical: 8,
                                                ),
                                                child: Text(
                                                  ui.transientBoardMessage!,
                                                  maxLines: 3,
                                                  overflow:
                                                      TextOverflow.ellipsis,
                                                  style: TextStyle(
                                                    color: Theme.of(context)
                                                        .colorScheme
                                                        .onSecondaryContainer,
                                                  ),
                                                ),
                                              ),
                                            ),
                                          ),
                                        ),
                                      ),
                                    if (ui.sandboxMessage != null)
                                      Text(
                                        ui.sandboxMessage!,
                                        style: TextStyle(
                                            color: Theme.of(context)
                                                .colorScheme
                                                .error),
                                      ),
                                    const TimerDisplay(),
                                    const SizedBox(height: 8),
                                    OutlinedButton.icon(
                                      onPressed: () => _openGameControlsSheet(),
                                      icon: const Icon(Icons.tune),
                                      label: Text(l10n.gameControlsTitle),
                                    ),
                                    if (session.transport ==
                                            BoardTransport.wifi ||
                                        session.transport ==
                                            BoardTransport.ble ||
                                        session.transport ==
                                            BoardTransport.mock) ...[
                                      const SizedBox(height: 8),
                                      FilledButton.tonalIcon(
                                        onPressed: () =>
                                            showNewGameWithTimeSheet(context),
                                        icon: const Icon(
                                            Icons.restart_alt_rounded),
                                        label: Text(l10n.gameNewGame),
                                      ),
                                    ],
                                    const SizedBox(height: 8),
                                    Text(l10n.gameHistory,
                                        style: const TextStyle(
                                            fontWeight: FontWeight.w600)),
                                    const MoveHistoryView(),
                                  ],
                                ),
                              ),
                            ),
                          );
                          if (layout == 'boardOnly') {
                            final pc = ui.puzzleChallenge;
                            final showPuzzleHud = pc != null && pc.hasVerifier;
                            return Column(
                              crossAxisAlignment: CrossAxisAlignment.stretch,
                              children: [
                                Expanded(
                                  child: Stack(
                                    clipBehavior: Clip.none,
                                    children: [
                                      Positioned.fill(
                                        child: LayoutBuilder(
                                          builder: (context, inner) {
                                            return _boardPane(
                                              inner,
                                              zoom,
                                              edgeToEdge: true,
                                              expandInParent: true,
                                              embedPuzzleHudInBoardPane: false,
                                            );
                                          },
                                        ),
                                      ),
                                      SafeArea(
                                        child: Padding(
                                          padding: const EdgeInsets.all(8),
                                          child: Align(
                                            alignment: Alignment.topLeft,
                                            child: Material(
                                              elevation: 2,
                                              shape: RoundedRectangleBorder(
                                                borderRadius:
                                                    BorderRadius.circular(24),
                                              ),
                                              color: Theme.of(context)
                                                  .colorScheme
                                                  .surfaceContainerHigh,
                                              child: IconButton(
                                                tooltip:
                                                    l10n.gameBackToPanelTooltip,
                                                icon: const Icon(Icons
                                                    .view_sidebar_outlined),
                                                onPressed: () => ref
                                                    .read(gameUiNotifierProvider
                                                        .notifier)
                                                    .setLayoutMode('standard'),
                                              ),
                                            ),
                                          ),
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                                if (showPuzzleHud)
                                  SafeArea(
                                    top: false,
                                    minimum: EdgeInsets.zero,
                                    child: Padding(
                                      padding: const EdgeInsets.fromLTRB(
                                          12, 0, 12, 6),
                                      child: _puzzleGlassHud(pc),
                                    ),
                                  ),
                              ],
                            );
                          }
                          final splitSidePanel =
                              c.maxWidth >= (isDesktopEmbedder() ? 680 : 840);
                          final triplePane = coachRailDesired &&
                              layout != 'boardOnly' &&
                              splitSidePanel &&
                              c.maxWidth >= _kTriplePaneMinWidth;

                          final boardAreaRow = _boardPane(
                            c,
                            zoom,
                            expandInParent: true,
                          );

                          if (triplePane) {
                            final cs = Theme.of(context).colorScheme;
                            return Row(
                              crossAxisAlignment: CrossAxisAlignment.stretch,
                              children: [
                                Expanded(flex: 5, child: boardAreaRow),
                                Expanded(
                                  flex: 3,
                                  child:
                                      SingleChildScrollView(child: sidePanel),
                                ),
                                Expanded(
                                  flex: 4,
                                  child: Padding(
                                    padding: const EdgeInsets.fromLTRB(
                                        0, 12, 16, 12),
                                    child: Card(
                                      clipBehavior: Clip.antiAlias,
                                      elevation: 2,
                                      child: Column(
                                        crossAxisAlignment:
                                            CrossAxisAlignment.stretch,
                                        children: [
                                          Padding(
                                            padding: const EdgeInsets.fromLTRB(
                                                12, 6, 4, 6),
                                            child: Row(
                                              children: [
                                                Icon(Icons.smart_toy_outlined,
                                                    size: 20,
                                                    color: cs.primary),
                                                const SizedBox(width: 8),
                                                Expanded(
                                                  child: Text(
                                                    l10n.gameAiCoachTitle,
                                                    style: Theme.of(context)
                                                        .textTheme
                                                        .titleSmall
                                                        ?.copyWith(
                                                            fontWeight:
                                                                FontWeight
                                                                    .w600),
                                                  ),
                                                ),
                                                IconButton(
                                                  tooltip:
                                                      l10n.gameHidePanelTooltip,
                                                  icon: const Icon(Icons.close,
                                                      size: 22),
                                                  onPressed: () async {
                                                    final p = ref.read(
                                                        prefsRepositoryProvider);
                                                    await p
                                                        .setDesktopCoachRailVisible(
                                                            false);
                                                    ref.invalidate(
                                                        prefsRepositoryProvider);
                                                  },
                                                ),
                                              ],
                                            ),
                                          ),
                                          const Divider(height: 1),
                                          const Expanded(
                                            child: CoachChatPanel(
                                              embedded: true,
                                            ),
                                          ),
                                        ],
                                      ),
                                    ),
                                  ),
                                ),
                              ],
                            );
                          }

                          if (splitSidePanel) {
                            final wideDesktop = isDesktopEmbedder() &&
                                c.maxWidth >= _kTriplePaneMinWidth;
                            final boardFlex =
                                wideDesktop && !coachRailDesired ? 7 : 3;
                            final sideFlex =
                                wideDesktop && !coachRailDesired ? 4 : 2;
                            return Row(
                              crossAxisAlignment: CrossAxisAlignment.stretch,
                              children: [
                                Expanded(
                                  flex: boardFlex,
                                  child: boardAreaRow,
                                ),
                                Expanded(
                                  flex: sideFlex,
                                  child:
                                      SingleChildScrollView(child: sidePanel),
                                ),
                              ],
                            );
                          }
                          final boardAreaStacked = _boardPane(
                            c,
                            zoom,
                            expandInParent: false,
                          );
                          return SingleChildScrollView(
                            child:
                                Column(children: [boardAreaStacked, sidePanel]),
                          );
                        },
                      ),
              ),
            ],
          ),
          Positioned(
            left: 12,
            right: 12,
            top: 8,
            child: SafeArea(
              bottom: false,
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                mainAxisSize: MainAxisSize.min,
                children: [
                  const NetworkStatusBanners(
                    presentation: NetworkBannerPresentation.overlay,
                  ),
                  if (session.lastError != null) ...[
                    const SizedBox(height: 6),
                    SessionErrorPanel(
                      error: session.lastError!,
                      title: l10n.lastErrorTitle,
                      devMode: devMode,
                      compact: true,
                    ),
                  ],
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  String _transportLabel(
      BoardSessionState s, bool devMode, AppLocalizations l10n) {
    return switch (s.transport) {
      BoardTransport.none => l10n.transportDisconnected,
      BoardTransport.mock => l10n.transportDemo,
      BoardTransport.wifi => devMode
          ? '${l10n.transportWifi}: ${s.wifiBaseUrl ?? ''}'
          : l10n.transportWifi,
      BoardTransport.ble => devMode
          ? l10n.statusBleDevLine(s.bleLabel ?? l10n.defaultBoardName)
          : l10n.transportBluetooth,
    };
  }

  Widget _statusLine(BoardSessionState s, bool devMode, AppLocalizations l10n) {
    final tier = _connectionTier(s);
    final transportLabel = _transportLabel(s, devMode, l10n);
    final t = switch (tier) {
      _ConnectionTier.live => s.transport == BoardTransport.none
          ? l10n.transportDisconnected
          : l10n.statusConnectedTransport(transportLabel),
      _ConnectionTier.connecting =>
        l10n.statusConnectingTransport(transportLabel),
      _ConnectionTier.offline => s.transport == BoardTransport.none
          ? l10n.transportDisconnected
          : l10n.statusBoardNotRespondingTransport(transportLabel),
    };
    final poll = devMode && s.pollingActive ? ' • poll' : '';
    final ws = devMode && s.webSocketActive ? ' • WS' : '';
    return Text('$t$poll$ws', style: const TextStyle(fontSize: 13));
  }
}
