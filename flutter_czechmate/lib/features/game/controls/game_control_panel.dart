import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_navigation.dart';
import '../../../app_providers.dart';
import '../../../core/utils/board_action_feedback.dart';
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
        'Nápověda: ${best.from}→${best.to}${best.evalPawns != null ? ' · eval ${best.evalPawns!.toStringAsFixed(2)}' : ''}',
      );
      if (session.transport == BoardTransport.wifi ||
          session.transport == BoardTransport.ble) {
        await ref
            .read(boardSessionNotifierProvider.notifier)
            .postHintDestination(best.to);
      }
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Nejlepší tah: ${best.from}→${best.to}'),
        ),
      );
    } catch (e) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Nápověda selhala: $e')),
      );
    } finally {
      if (mounted) setState(() => _hintBusy = false);
    }
  }

  void _demoOnlySnack(BuildContext context, String feature) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          'Demo deska: $feature funguje jen přes Bluetooth nebo Wi‑Fi na reálný hardware.',
        ),
      ),
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

    final inner = Wrap(
      spacing: 8,
      runSpacing: 8,
      children: [
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: SegmentedButton<String>(
            segments: const [
              ButtonSegment(
                  value: 'boardOnly',
                  label: Text('Jen deska'),
                  icon: Icon(Icons.grid_on, size: 18)),
              ButtonSegment(
                  value: 'standard',
                  label: Text('Standard'),
                  icon: Icon(Icons.view_column, size: 18)),
            ],
            selected: {ui.layoutMode},
            onSelectionChanged: (s) {
              if (s.isEmpty) return;
              uiN.setLayoutMode(s.first);
            },
          ),
        ),
        Text('Zoom desky ${(ui.boardZoomStorage * 100).round()} %'),
        SizedBox(
          width: double.infinity,
          child: Slider(
            min: 0.7,
            max: 1.5,
            divisions: 16,
            label: '${(ui.boardZoomStorage * 100).round()} %',
            value: ui.boardZoomStorage.clamp(0.7, 1.5),
            onChanged: uiN.setBoardZoom,
          ),
        ),
        FilledButton.tonal(
          onPressed: uiN.toggleFlip,
          child: Text(
              ui.boardFlipped ? 'Perspektiva: černá' : 'Perspektiva: bílá'),
        ),
        FilledButton.tonal(
          onPressed: uiN.toggleCoordinates,
          child: Text(ui.showCoordinates
              ? 'Souřadnice: zapnuto'
              : 'Souřadnice: vypnuto'),
        ),
        FilterChip(
          label: const Text('Sandbox'),
          selected: ui.sandboxMode,
          onSelected: (_) => unawaited(uiN.toggleSandbox(snap)),
        ),
        if (ui.sandboxMode && ui.sandboxUndoStack.isNotEmpty)
          OutlinedButton.icon(
            onPressed: uiN.sandboxUndo,
            icon: const Icon(Icons.undo),
            label: Text('Zpět tah (${ui.sandboxUndoStack.length})'),
          ),
        if (ui.sandboxMode)
          OutlinedButton.icon(
            onPressed: () async => uiN.exitSandboxAndRefresh(),
            icon: const Icon(Icons.exit_to_app),
            label: const Text('Ukončit sandbox a obnovit desku'),
          ),
        FilterChip(
          label: const Text('Tahy z aplikace'),
          selected: ui.remoteMovesEnabled,
          onSelected: (_) => uiN.toggleRemoteMoves(),
        ),
        if (ui.remoteMovesEnabled)
          Padding(
            padding: const EdgeInsets.only(left: 4, right: 4),
            child: Text(
              session.transport == BoardTransport.wifi
                  ? 'Dvojitý výběr pole odešle tah na desku přes Wi‑Fi.'
                  : session.transport == BoardTransport.ble
                      ? 'Dvojitý výběr pole odešle tah na desku přes Bluetooth.'
                      : session.transport == BoardTransport.mock
                          ? 'Demo: dvojitý výběr odešle tah lokálně (simulace desky + časomíry).'
                          : prefs.preferBluetoothOnly
                              ? 'Zapnuto, ale deska není připojená. Připoj ji přes Bluetooth.'
                              : 'Zapnuto — pro odeslání tahů připoj desku přes Wi‑Fi nebo Bluetooth.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('Učící mód (výklad)'),
          value: ui.learningMode,
          onChanged: (v) => uiN.setLearningMode(v),
        ),
        if (ui.learningMode)
          TextButton.icon(
            onPressed: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.settings,
            icon: const Icon(Icons.tune, size: 18),
            label: const Text('Úroveň výkladu (1–4) v Nastavení'),
          ),
        if (ui.learningMode)
          const Padding(
            padding: EdgeInsets.only(left: 4, right: 4, bottom: 4),
            child: Text(
              'Gemma / cloud trenér v aplikaci; Stockfish na desce řeší LED nápovědu podle NVS.',
              style: TextStyle(fontSize: 12),
            ),
          ),
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
          label: Text(_hintBusy ? 'Počítám nápovědu…' : 'Nápověda tahu'),
        ),
        if (session.transport == BoardTransport.wifi ||
            session.transport == BoardTransport.ble ||
            session.transport == BoardTransport.mock)
          FilledButton.tonal(
            onPressed: () => showNewGameWithTimeSheet(context),
            child: const Text('Nová hra…'),
          ),
        if (session.transport == BoardTransport.wifi ||
            session.transport == BoardTransport.ble) ...[
          OutlinedButton.icon(
            onPressed: () => runBoardCommandWithSnackBar(
              context,
              sessionN.postHintClearBoard,
              successMessage: 'Vymazání LED nápovědy odesláno',
            ),
            icon: const Icon(Icons.visibility_off_outlined),
            label: const Text('Skrýt LED nápovědu'),
          ),
          FilledButton.tonal(
            onPressed: () => runBoardCommandWithSnackBar(
              context,
              sessionN.postTimerPause,
              successMessage: 'Pauza časomíry odeslána',
            ),
            child: const Text('Pause časomíry'),
          ),
          FilledButton.tonal(
            onPressed: () => runBoardCommandWithSnackBar(
              context,
              sessionN.postTimerResume,
              successMessage: 'Časomíra obnovena',
            ),
            child: const Text('Resume časomíry'),
          ),
          FilledButton.tonal(
            onPressed: () => runBoardCommandWithSnackBar(
              context,
              sessionN.refreshNow,
              successMessage: 'Stav desky aktualizován',
            ),
            child: const Text('Obnovit'),
          ),
        ],
        if (session.transport == BoardTransport.mock) ...[
          OutlinedButton.icon(
            onPressed: () => _demoOnlySnack(context, 'LED nápověda'),
            icon: const Icon(Icons.visibility_off_outlined),
            label: const Text('Skrýt LED nápovědu'),
          ),
          FilledButton.tonal(
            onPressed: () => _demoOnlySnack(context, 'Časomíra'),
            child: const Text('Pause časomíry'),
          ),
          FilledButton.tonal(
            onPressed: () => _demoOnlySnack(context, 'Časomíra'),
            child: const Text('Resume časomíry'),
          ),
          FilledButton.tonal(
            onPressed: () => runBoardCommandWithSnackBar(
              context,
              sessionN.refreshNow,
              successMessage: 'Demo snapshot znovu načten',
            ),
            child: const Text('Obnovit'),
          ),
        ],
      ],
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        ExpansionTile(
          initiallyExpanded: ui.isControlPanelExpanded,
          onExpansionChanged: uiN.setControlPanelExpanded,
          title: const Text('Ovládání hry'),
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
