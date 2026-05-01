import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_navigation.dart';
import '../../../app_providers.dart';
import '../../../core/utils/board_action_feedback.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/utils/fen_from_board.dart';
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

  Future<void> _requestHint(
    BuildContext context,
    BoardSessionState session,
    GameUiNotifier uiN,
  ) async {
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

      // Lokální nápověda vždy; LED nápověda jen při připojené desce.
      uiN.showTransientBoardMessage(
        'Hint: ${best.from}→${best.to}${best.evalPawns != null ? ' · eval ${best.evalPawns!.toStringAsFixed(2)}' : ''}',
      );
      if (session.transport == BoardTransport.wifi ||
          session.transport == BoardTransport.ble) {
        await ref
            .read(boardSessionNotifierProvider.notifier)
            .postHintDestination(best.to);
      }
      if (!context.mounted) return;
      showGlassSnackBar(context, 'Best move: ${best.from}→${best.to}');
    } catch (e) {
      if (!context.mounted) return;
      showGlassSnackBar(context, 'Hint failed: $e');
    } finally {
      if (mounted) setState(() => _hintBusy = false);
    }
  }

  void _demoOnlySnack(BuildContext context, String feature) {
    showGlassSnackBar(
      context,
      'Demo board: $feature only works over Bluetooth or Wi‑Fi to real hardware.',
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

    TextStyle? sectionStyle() => theme.textTheme.labelLarge?.copyWith(
          fontWeight: FontWeight.w600,
          color: cs.primary,
        );

    final inner = Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text('Display', style: sectionStyle()),
        const SizedBox(height: 8),
        SizedBox(
          width: double.infinity,
          child: SegmentedButton<String>(
            showSelectedIcon: false,
            segments: const [
              ButtonSegment<String>(
                value: 'boardOnly',
                label: Text('Board only'),
                icon: Icon(Icons.grid_on, size: 18),
              ),
              ButtonSegment<String>(
                value: 'standard',
                label: Text('Standard'),
                icon: Icon(Icons.view_column, size: 18),
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
              child: Text(ui.boardFlipped ? 'Black bottom' : 'White bottom'),
            ),
            FilledButton.tonal(
              onPressed: uiN.toggleCoordinates,
              child: Text(
                ui.showCoordinates ? 'Coords on' : 'Coords off',
              ),
            ),
          ],
        ),
        Padding(
          padding: const EdgeInsets.only(top: 8),
          child: Text(
            'Board size: Settings → Board appearance.',
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ),
        const Divider(height: 28),
        Text('Play mode', style: sectionStyle()),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            FilterChip(
              label: const Text('Sandbox'),
              selected: ui.sandboxMode,
              onSelected: (_) => unawaited(uiN.toggleSandbox(snap)),
            ),
            FilterChip(
              label: const Text('Moves from app'),
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
                  label: Text('Undo move (${ui.sandboxUndoStack.length})'),
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
              label: const Text('Exit sandbox & refresh board'),
            ),
          ),
        if (ui.remoteMovesEnabled)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: Text(
              session.transport == BoardTransport.wifi
                  ? 'Double-tap squares to send the move to the board over Wi‑Fi.'
                  : session.transport == BoardTransport.ble
                      ? 'Double-tap squares to send the move over Bluetooth.'
                      : session.transport == BoardTransport.mock
                          ? 'Demo: double-tap sends the move locally (board + clock simulation).'
                          : prefs.preferBluetoothOnly
                              ? 'Enabled, but no board connected. Connect via Bluetooth.'
                              : 'Enabled — connect a board over Wi‑Fi or Bluetooth to send moves.',
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('Learning mode'),
          subtitle: const Text('Coach explanations in the app'),
          value: ui.learningMode,
          onChanged: (v) => uiN.setLearningMode(v),
        ),
        if (ui.learningMode) ...[
          TextButton.icon(
            onPressed: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.settings,
            icon: const Icon(Icons.tune, size: 18),
            label: const Text('Explanation level (1–4) in Settings'),
          ),
          Padding(
            padding: const EdgeInsets.only(bottom: 4),
            child: Text(
              'Gemma / cloud coach in the app; on-board Stockfish drives hint LEDs per NVS.',
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
        const Divider(height: 28),
        Text('Actions', style: sectionStyle()),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            FilledButton.icon(
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
              label: Text(_hintBusy ? 'Thinking…' : 'Move hint'),
            ),
            if (session.transport == BoardTransport.wifi ||
                session.transport == BoardTransport.ble ||
                session.transport == BoardTransport.mock)
              FilledButton.tonal(
                onPressed: () => showNewGameWithTimeSheet(context),
                child: const Text('New game…'),
              ),
            if (session.transport == BoardTransport.wifi ||
                session.transport == BoardTransport.ble) ...[
              OutlinedButton.icon(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.postHintClearBoard,
                  successMessage: 'Hint LEDs cleared',
                ),
                icon: const Icon(Icons.visibility_off_outlined),
                label: const Text('Clear hint LEDs'),
              ),
              FilledButton.tonal(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.postTimerPause,
                  successMessage: 'Clock pause sent',
                ),
                child: const Text('Pause clock'),
              ),
              FilledButton.tonal(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.postTimerResume,
                  successMessage: 'Clock resumed',
                ),
                child: const Text('Resume clock'),
              ),
              FilledButton.tonal(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.refreshNow,
                  successMessage: 'Board state refreshed',
                ),
                child: const Text('Refresh state'),
              ),
            ],
            if (session.transport == BoardTransport.mock) ...[
              OutlinedButton.icon(
                onPressed: () => _demoOnlySnack(context, 'LED hint'),
                icon: const Icon(Icons.visibility_off_outlined),
                label: const Text('Clear hint LEDs'),
              ),
              FilledButton.tonal(
                onPressed: () => _demoOnlySnack(context, 'Clock'),
                child: const Text('Pause clock'),
              ),
              FilledButton.tonal(
                onPressed: () => _demoOnlySnack(context, 'Clock'),
                child: const Text('Resume clock'),
              ),
              FilledButton.tonal(
                onPressed: () => runBoardCommandWithSnackBar(
                  context,
                  sessionN.refreshNow,
                  successMessage: 'Demo snapshot reloaded',
                ),
                child: const Text('Refresh state'),
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
          title: const Text('Game controls'),
          subtitle: Text(
            'Display · Mode · Actions',
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
