import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_navigation.dart';
import '../../../app_providers.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../l10n/app_localizations.dart';
import '../../../core/models/board_timer_state.dart';
import '../../../core/utils/board_action_feedback.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/widgets/pressable_scale.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../state/game_ui_notifier.dart';
import 'new_game_time_sheet.dart';

class GameControlPanel extends ConsumerStatefulWidget {
  const GameControlPanel({super.key});

  @override
  ConsumerState<GameControlPanel> createState() => _GameControlPanelState();
}

class _GameControlPanelState extends ConsumerState<GameControlPanel> {
  bool _hintBusy = false;
  bool _demoClockPaused = false;

  Future<void> _requestHint(
    BuildContext context,
    BoardSessionState session,
    GameUiNotifier uiN,
  ) async {
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
      if (!context.mounted) return;
      showAppSnackBar(context, l10n.gameBestMoveSnack(best.from, best.to));
    } catch (e) {
      if (!context.mounted) return;
      showAppSnackBar(
        context,
        l10n.gameHintFailed(userFacingErrorSummary(l10n, e)),
        errorStyle: true,
      );
    } finally {
      if (mounted) setState(() => _hintBusy = false);
    }
  }

  void _demoOnlySnack(
      BuildContext context, AppLocalizations l10n, String feature) {
    showAppSnackBar(
      context,
      l10n.gameDemoBoardSnack(feature),
    );
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final snap = session.snapshot;
    final prefs = ref.read(prefsRepositoryProvider);
    final sessionN = ref.read(boardSessionNotifierProvider.notifier);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final l10n = context.l10n;
    BoardTimerState? timerState = snap?.clock ?? session.timer;
    final gameOver = snap != null &&
        (snap.status.gameEnd?.ended == true ||
            snap.status.gameState.toLowerCase() == 'finished');
    final clockPaused = timerState?.gamePaused ?? false;
    final canClockToggle = timerState != null &&
        timerState.isTimeControlEnabled &&
        !gameOver &&
        !timerState.timeExpired;

    TextStyle? sectionStyle() => theme.textTheme.labelLarge?.copyWith(
          fontWeight: FontWeight.w600,
          color: cs.primary,
        );

    final inner = Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(l10n.gameControlDisplaySection, style: sectionStyle()),
        const SizedBox(height: 8),
        SizedBox(
          width: double.infinity,
          child: SegmentedButton<String>(
            showSelectedIcon: false,
            segments: [
              ButtonSegment<String>(
                value: 'boardOnly',
                label: Text(l10n.gameLayoutBoardOnly),
                icon: const Icon(Icons.grid_on, size: 18),
              ),
              ButtonSegment<String>(
                value: 'standard',
                label: Text(l10n.gameLayoutStandard),
                icon: const Icon(Icons.view_column, size: 18),
              ),
            ],
            selected: {ui.layoutMode},
            onSelectionChanged: (s) {
              if (s.isEmpty) {
                return;
              }
              uiN.setLayoutMode(s.first);
            },
          ),
        ),
        const SizedBox(height: 10),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            FilledButton.tonal(
              onPressed: uiN.toggleFlip,
              child: Text(ui.boardFlipped
                  ? l10n.gameControlBlackBottom
                  : l10n.gameControlWhiteBottom),
            ),
            FilledButton.tonal(
              onPressed: uiN.toggleCoordinates,
              child: Text(
                ui.showCoordinates
                    ? l10n.gameControlCoordsOn
                    : l10n.gameControlCoordsOff,
              ),
            ),
          ],
        ),
        Padding(
          padding: const EdgeInsets.only(top: 8),
          child: Text(
            l10n.gameControlBoardSizeHint,
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ),
        const Divider(height: 28),
        Text(l10n.gameControlPlayModeSection, style: sectionStyle()),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            FilterChip(
              label: Text(l10n.gameControlSandboxLabel),
              selected: ui.sandboxMode,
              onSelected: (_) => unawaited(uiN.toggleSandbox(snap)),
            ),
            FilterChip(
              label: Text(l10n.gameControlMovesFromApp),
              selected: ui.remoteMovesEnabled,
              onSelected: (_) => uiN.toggleRemoteMoves(),
            ),
          ],
        ),
        if (ui.sandboxMode && ui.sandboxUndoStack.isNotEmpty)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                OutlinedButton.icon(
                  onPressed: uiN.sandboxUndo,
                  icon: const Icon(Icons.undo),
                  label: Text(
                      l10n.gameControlUndoMove(ui.sandboxUndoStack.length)),
                ),
              ],
            ),
          ),
        if (ui.sandboxMode)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: OutlinedButton.icon(
              onPressed: () async => uiN.exitSandboxAndRefresh(),
              icon: const Icon(Icons.exit_to_app),
              label: Text(l10n.gameControlExitSandbox),
            ),
          ),
        if (ui.remoteMovesEnabled)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: Text(
              session.transport == BoardTransport.wifi
                  ? l10n.gameRemoteMovesWifi
                  : session.transport == BoardTransport.ble
                      ? l10n.gameRemoteMovesBle
                      : session.transport == BoardTransport.mock
                          ? l10n.gameRemoteMovesMock
                          : prefs.effectiveConnectionMode == 'ble_only'
                              ? l10n.gameRemoteMovesNoBoard
                              : l10n.gameRemoteMovesConnect,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: Text(l10n.gameControlLearningModeTitle),
          subtitle: Text(l10n.gameControlLearningModeSubtitle),
          value: ui.learningMode,
          onChanged: (v) => uiN.setLearningMode(v),
        ),
        if (ui.learningMode) ...[
          TextButton.icon(
            onPressed: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.settings,
            icon: const Icon(Icons.tune, size: 18),
            label: Text(l10n.gameControlExplanationLevelSettings),
          ),
          Padding(
            padding: const EdgeInsets.only(bottom: 4),
            child: Text(
              l10n.gameControlLearningCloudHint,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
        const Divider(height: 28),
        Text(l10n.gameControlActionsSection, style: sectionStyle()),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            if (session.transport == BoardTransport.wifi ||
                session.transport == BoardTransport.ble ||
                session.transport == BoardTransport.mock)
              PressableScale(
                child: FilledButton.icon(
                  onPressed: () => showNewGameWithTimeSheet(context),
                  icon: const Icon(Icons.restart_alt_rounded),
                  label: Text(l10n.gameNewGame),
                ),
              ),
            FilledButton.tonalIcon(
              onPressed: snap == null || _hintBusy
                  ? null
                  : () => _requestHint(context, session, uiN),
              icon: _hintBusy
                  ? const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Icon(Icons.tips_and_updates_outlined),
              label: Text(
                  _hintBusy ? l10n.gameHintThinking : l10n.gameControlMoveHint),
            ),
            if (session.transport == BoardTransport.wifi ||
                session.transport == BoardTransport.ble) ...[
              OutlinedButton.icon(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.postHintClearBoard,
                  successMessage: l10n.gameHintLedsClearedSnackbar,
                ),
                icon: const Icon(Icons.visibility_off_outlined),
                label: Text(l10n.gameClearHintLeds),
              ),
              if (canClockToggle)
                FilledButton.tonal(
                  onPressed: () => runBoardCommandWithSnackBar(
                    context,
                    clockPaused
                        ? sessionN.postTimerResume
                        : sessionN.postTimerPause,
                    successMessage: clockPaused
                        ? l10n.gameClockResumedSnackbar
                        : l10n.gameClockPauseSent,
                  ),
                  child: Text(
                    clockPaused ? l10n.gameResumeClock : l10n.gamePauseClock,
                  ),
                ),
              FilledButton.tonal(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.refreshNow,
                  successMessage: l10n.gameBoardRefreshedSnack,
                ),
                child: Text(l10n.gameControlRefreshState),
              ),
            ],
            if (session.transport == BoardTransport.mock) ...[
              OutlinedButton.icon(
                onPressed: () =>
                    _demoOnlySnack(context, l10n, l10n.gameClearHintLeds),
                icon: const Icon(Icons.visibility_off_outlined),
                label: Text(l10n.gameClearHintLeds),
              ),
              FilledButton.tonal(
                onPressed: () {
                  setState(() => _demoClockPaused = !_demoClockPaused);
                  _demoOnlySnack(
                    context,
                    l10n,
                    _demoClockPaused
                        ? l10n.gamePauseClock
                        : l10n.gameResumeClock,
                  );
                },
                child: Text(
                  _demoClockPaused ? l10n.gameResumeClock : l10n.gamePauseClock,
                ),
              ),
              FilledButton.tonal(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.refreshNow,
                  successMessage: l10n.gameDemoSnapshotReloadedSnack,
                ),
                child: Text(l10n.gameControlRefreshState),
              ),
            ],
          ],
        ),
      ],
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        ExpansionTile(
          initiallyExpanded: ui.isControlPanelExpanded,
          onExpansionChanged: uiN.setControlPanelExpanded,
          title: Text(l10n.gameControlPanelTitle),
          subtitle: Text(
            l10n.gameControlPanelSubtitle,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
          ),
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
              child: inner,
            ),
          ],
        ),
      ],
    );
  }
}
