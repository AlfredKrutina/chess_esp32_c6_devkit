import 'dart:math' as math;
import 'dart:ui' show ImageFilter;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/models/puzzle_challenge_state.dart';
import '../../core/utils/board_action_feedback.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/utils/fen_from_board.dart';
import '../../core/widgets/network_status_banners.dart';
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
import 'state/game_ui_state.dart';

enum _ConnectionTier { live, connecting, offline }

enum _GameOverflowAction { gameControls, analysis, toggleCoachPanel, disconnect }

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

  void _openGameControlsSheet() {
    ref.read(gameUiNotifierProvider.notifier).setPinnedNewGameBarVisible(true);
    showModalBottomSheet<void>(
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
    setState(() => _hintBusy = true);
    try {
      final prefs = ref.read(prefsRepositoryProvider);
      final depth = prefs.hintDepth.clamp(1, 18);
      final fen = fenFromSnapshot(snap);
      final best = await ref
          .read(stockfishApiClientProvider)
          .fetchBestMove(fen, depth: depth);
      uiN.showHintOverlay(best.from, best.to);
      uiN.showTransientBoardMessage(
        'Hint: ${best.from}→${best.to}${best.evalPawns != null ? ' · eval ${best.evalPawns!.toStringAsFixed(2)}' : ''}',
      );
      if (session.transport == BoardTransport.wifi ||
          session.transport == BoardTransport.ble) {
        await ref
            .read(boardSessionNotifierProvider.notifier)
            .postHintDestination(best.to);
      }
      if (!mounted) return;
      showGlassSnackBar(
        context,
        'Best move: ${best.from}→${best.to}',
      );
    } catch (e) {
      if (!mounted) return;
      showGlassSnackBar(context, 'Hint failed: $e');
    } finally {
      if (mounted) setState(() => _hintBusy = false);
    }
  }

  void _demoBoardFeatureSnack(String label) {
    if (!mounted) return;
    showGlassSnackBar(
      context,
      'Demo board: $label only works over Bluetooth or Wi‑Fi to real hardware.',
    );
  }

  /// Wide „New game“ button — only after opening Game controls once this session.
  Widget _newGamePinnedBar(BoardSessionState session, GameUiState ui) {
    if (!ui.pinnedNewGameBarVisible) return const SizedBox.shrink();
    final can = session.transport == BoardTransport.wifi ||
        session.transport == BoardTransport.ble ||
        session.transport == BoardTransport.mock;
    if (!can) return const SizedBox.shrink();
    final cs = Theme.of(context).colorScheme;
    return Material(
      elevation: 8,
      color: cs.surfaceContainerLow,
      child: SafeArea(
        top: false,
        child: Padding(
          padding: const EdgeInsets.fromLTRB(12, 8, 12, 10),
          child: SizedBox(
            width: double.infinity,
            child: FilledButton.icon(
              onPressed: () => showNewGameWithTimeSheet(context),
              icon: const Icon(Icons.fiber_new),
              label: const Text('New game'),
            ),
          ),
        ),
      ),
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
          child: const ChessBoardWidget(),
        ),
      ),
    );
  }

  /// Kompaktní panel pod šachovnicí — nesnižuje viditelnou plochu figurek (žádný overlay nahoře).
  Widget _puzzleGlassHud(PuzzleChallengeState pc) {
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
                        'Moves ${pc.playedUci.length} / ${pc.solutionUci.length}',
                        style: tt.bodySmall?.copyWith(
                          color: cs.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
                IconButton(
                  icon: const Icon(Icons.sports_esports_outlined),
                  tooltip: 'Return to live game',
                  onPressed: () => ref
                      .read(gameUiNotifierProvider.notifier)
                      .returnToLiveGame(),
                ),
                IconButton(
                  icon: const Icon(Icons.close),
                  tooltip: 'Exit puzzle check (keep position)',
                  onPressed: () => ref
                      .read(gameUiNotifierProvider.notifier)
                      .exitPuzzleChallenge(),
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
    final puzzleHud = (pc != null && pc.hasVerifier)
        ? _puzzleGlassHud(pc)
        : null;

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
        showGlassSnackBar(
          context,
          msg,
          duration: const Duration(seconds: 6),
        );
        ref.read(gameUiNotifierProvider.notifier).showTransientBoardMessage(msg);
      });
    });

    ref.listen(
      gameUiNotifierProvider.select((s) => s.puzzleSnackText),
      (prev, next) {
        if (next == null || next.isEmpty) return;
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!context.mounted) return;
          showGlassSnackBar(context, next);
          ref.read(gameUiNotifierProvider.notifier).clearPuzzleSnack();
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
          tooltip: 'Find board',
          padding: const EdgeInsets.all(10),
          onPressed: () => pushBoardDiscoveryRoute(context),
          icon: Container(
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
        titleSpacing: 4,
        title: Row(
          children: [
            Icon(Icons.grid_view_rounded, color: cs.primary, size: 22),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                'czechmate',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: Theme.of(context).textTheme.titleLarge,
              ),
            ),
          ],
        ),
        actions: [
          PopupMenuButton<String>(
            tooltip: 'Layout mode',
            onSelected: uiN.setLayoutMode,
            itemBuilder: (ctx) => const [
              PopupMenuItem(value: 'standard', child: Text('Standard')),
              PopupMenuItem(value: 'boardOnly', child: Text('Board only')),
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
                      ui.layoutMode == 'boardOnly' ? 'Board' : 'Standard',
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
              tooltip: 'Game summary',
              onPressed: () => Navigator.of(context).push(MaterialPageRoute(
                  builder: (ctx) => const GameEndReportScreen())),
            ),
          PopupMenuButton<_GameOverflowAction>(
            tooltip: 'More options',
            icon: const Icon(Icons.more_vert),
            onSelected: (action) async {
              switch (action) {
                case _GameOverflowAction.gameControls:
                  _openGameControlsSheet();
                  break;
                case _GameOverflowAction.analysis:
                  ref.read(mainNavTabIndexProvider.notifier).state =
                      AppMainTab.analysis;
                  break;
                case _GameOverflowAction.toggleCoachPanel:
                  final p = ref.read(prefsRepositoryProvider);
                  await p.setDesktopCoachRailVisible(
                      !p.desktopCoachRailVisible);
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
                PopupMenuItem(
                  value: _GameOverflowAction.gameControls,
                  child: Row(
                    children: [
                      Icon(Icons.tune, size: 22, color: cs.onSurface),
                      const SizedBox(width: 12),
                      const Expanded(child: Text('Game controls')),
                    ],
                  ),
                ),
                PopupMenuItem(
                  value: _GameOverflowAction.analysis,
                  child: Row(
                    children: [
                      Icon(Icons.show_chart, size: 22, color: cs.onSurface),
                      const SizedBox(width: 12),
                      const Expanded(child: Text('Analysis')),
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
                                ? 'Hide AI coach panel'
                                : 'Show AI coach panel',
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
                          'Disconnect session',
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
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const LiveActivitySessionListener(),
          const Padding(
            padding: EdgeInsets.fromLTRB(12, 8, 12, 0),
            child: NetworkStatusBanners(),
          ),
          if (session.snapshot != null)
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 8, 12, 0),
              child: SingleChildScrollView(
                scrollDirection: Axis.horizontal,
                child: Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    FilledButton.icon(
                      onPressed:
                          _hintBusy ? null : () => _requestHint(session, uiN),
                      icon: _hintBusy
                          ? const SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Icon(Icons.tips_and_updates_outlined),
                      label: Text(_hintBusy ? 'Thinking…' : 'Hint'),
                    ),
                    if (session.transport == BoardTransport.wifi ||
                        session.transport == BoardTransport.ble) ...[
                      OutlinedButton.icon(
                        onPressed: () => runBoardCommandWithSnackBar(
                          context,
                          sessionN.postTimerPause,
                          successMessage: 'Clock pause sent',
                        ),
                        icon: const Icon(Icons.pause_circle_outline),
                        label: const Text('Pause clock'),
                      ),
                      OutlinedButton.icon(
                        onPressed: () => runBoardCommandWithSnackBar(
                          context,
                          sessionN.postTimerResume,
                          successMessage: 'Clock resumed',
                        ),
                        icon: const Icon(Icons.play_circle_outline),
                        label: const Text('Resume clock'),
                      ),
                      OutlinedButton.icon(
                        onPressed: () => runBoardCommandWithSnackBar(
                          context,
                          sessionN.postHintClearBoard,
                          successMessage: 'Hint LEDs cleared',
                        ),
                        icon: const Icon(Icons.visibility_off_outlined),
                        label: const Text('Clear hint LEDs'),
                      ),
                    ] else if (session.transport == BoardTransport.mock) ...[
                      OutlinedButton.icon(
                        onPressed: () =>
                            _demoBoardFeatureSnack('Clock pause'),
                        icon: const Icon(Icons.pause_circle_outline),
                        label: const Text('Pause clock'),
                      ),
                      OutlinedButton.icon(
                        onPressed: () =>
                            _demoBoardFeatureSnack('Clock'),
                        icon: const Icon(Icons.play_circle_outline),
                        label: const Text('Resume clock'),
                      ),
                      OutlinedButton.icon(
                        onPressed: () =>
                            _demoBoardFeatureSnack('LED hint'),
                        icon: const Icon(Icons.visibility_off_outlined),
                        label: const Text('Clear hint LEDs'),
                      ),
                    ],
                  ],
                ),
              ),
            ),
          Expanded(
            child: session.snapshot == null
                ? Center(
                    child: Padding(
                      padding: const EdgeInsets.all(24),
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          const Text('No board snapshot yet.'),
                          const SizedBox(height: 12),
                          Text(
                            'Tap top-left or below to find a board (Bluetooth). Wi‑Fi and more options are under advanced settings.',
                            textAlign: TextAlign.center,
                            style: Theme.of(context).textTheme.bodyMedium,
                          ),
                          const SizedBox(height: 20),
                          FilledButton.icon(
                            onPressed: () => pushBoardDiscoveryRoute(context),
                            icon: const Icon(Icons.bluetooth_searching),
                            label: const Text('Find board'),
                          ),
                          if (session.lastError != null) ...[
                            const SizedBox(height: 16),
                            Text(
                              '${session.lastError}',
                              textAlign: TextAlign.center,
                              style: TextStyle(color: cs.error),
                            ),
                          ],
                        ],
                      ),
                    ),
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
                              crossAxisAlignment: CrossAxisAlignment.stretch,
                              children: [
                                _statusLine(session, devMode),
                                if (ui.transientBoardMessage != null)
                                  Material(
                                    color: Theme.of(context)
                                        .colorScheme
                                        .secondaryContainer,
                                    shape: RoundedRectangleBorder(
                                      borderRadius: BorderRadius.circular(8),
                                    ),
                                    child: ListTile(
                                      dense: true,
                                      title: Text(
                                        ui.transientBoardMessage!,
                                        style: TextStyle(
                                            color: Theme.of(context)
                                                .colorScheme
                                                .onSecondaryContainer),
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
                                  onPressed: () =>
                                      _openGameControlsSheet(),
                                  icon: const Icon(Icons.tune),
                                  label: const Text('Game controls'),
                                ),
                                const SizedBox(height: 8),
                                const Text('History',
                                    style:
                                        TextStyle(fontWeight: FontWeight.w600)),
                                const MoveHistoryView(),
                                if (session.lastError != null) ...[
                                  const SizedBox(height: 8),
                                  Text(
                                    '${session.lastError}',
                                    style: TextStyle(
                                        color: Theme.of(context)
                                            .colorScheme
                                            .error),
                                  ),
                                ],
                              ],
                            ),
                          ),
                        ),
                      );
                      if (layout == 'boardOnly') {
                        final pc = ui.puzzleChallenge;
                        final showPuzzleHud =
                            pc != null && pc.hasVerifier;
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
                                                'Back to layout with panel',
                                            icon: const Icon(
                                                Icons.view_sidebar_outlined),
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
                              child: SingleChildScrollView(child: sidePanel),
                            ),
                            Expanded(
                              flex: 4,
                              child: Padding(
                                padding:
                                    const EdgeInsets.fromLTRB(0, 12, 16, 12),
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
                                                size: 20, color: cs.primary),
                                            const SizedBox(width: 8),
                                            Expanded(
                                              child: Text(
                                                'AI coach',
                                                style: Theme.of(context)
                                                    .textTheme
                                                    .titleSmall
                                                    ?.copyWith(
                                                        fontWeight:
                                                            FontWeight.w600),
                                              ),
                                            ),
                                            IconButton(
                                              tooltip: 'Hide panel',
                                              icon: const Icon(
                                                  Icons.close, size: 22),
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
                              child: SingleChildScrollView(
                                  child: sidePanel),
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
          _newGamePinnedBar(session, ui),
        ],
      ),
    );
  }

  Widget _statusLine(BoardSessionState s, bool devMode) {
    final tier = _connectionTier(s);
    final transportLabel = switch (s.transport) {
      BoardTransport.none => 'Disconnected',
      BoardTransport.mock => 'Mock deska',
      BoardTransport.wifi =>
        devMode ? 'Wi‑Fi: ${s.wifiBaseUrl ?? ''}' : 'Wi‑Fi',
      BoardTransport.ble =>
        devMode ? 'BLE: ${s.bleLabel ?? "deska"}' : 'Bluetooth',
    };
    final t = switch (tier) {
      _ConnectionTier.live => s.transport == BoardTransport.none
          ? 'Disconnected'
          : 'Connected ($transportLabel)',
      _ConnectionTier.connecting => 'Connecting ($transportLabel)…',
      _ConnectionTier.offline => s.transport == BoardTransport.none
          ? 'Disconnected'
          : 'Board not responding ($transportLabel)',
    };
    final poll = devMode && s.pollingActive ? ' • poll' : '';
    final ws = devMode && s.webSocketActive ? ' • WS' : '';
    return Text('$t$poll$ws', style: const TextStyle(fontSize: 13));
  }
}
