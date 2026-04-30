import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/analysis/move_evaluation.dart';
import '../../core/analysis/move_quality_aggregator.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/utils/fen_from_board.dart';
import '../../core/utils/opening_eco.dart';
import '../../core/widgets/network_status_banners.dart';
import '../connection/board_session_notifier.dart';
import '../game/state/game_ui_notifier.dart';
import 'eval_line_chart.dart';
import 'move_eval_notifier.dart';

String _moveText(GameHistoryMove m) {
  final s = m.san?.trim();
  if (s != null && s.isNotEmpty) return s;
  if (m.from != null && m.to != null) return '${m.from}${m.to}';
  return '—';
}

Color _gradeColor(MoveGrade? g, ColorScheme cs) {
  switch (g) {
    case MoveGrade.best:
    case MoveGrade.good:
      return cs.primaryContainer == cs.surface
          ? Colors.green.shade700
          : cs.primary;
    case MoveGrade.inaccuracy:
      return Colors.orange.shade800;
    case MoveGrade.mistake:
      return cs.errorContainer;
    case MoveGrade.blunder:
      return cs.error;
    case MoveGrade.error:
    default:
      return cs.onSurface;
  }
}

class _PlyRow {
  _PlyRow(
      {required this.moveNum,
      required this.white,
      this.black,
      required this.whiteIdx,
      this.blackIdx});
  final int moveNum;
  final GameHistoryMove white;
  final GameHistoryMove? black;
  final int whiteIdx;
  final int? blackIdx;
}

List<_PlyRow> _plyRows(List<GameHistoryMove> moves) {
  final out = <_PlyRow>[];
  for (var i = 0; i < moves.length; i += 2) {
    final w = moves[i];
    final b = i + 1 < moves.length ? moves[i + 1] : null;
    out.add(_PlyRow(
        moveNum: i ~/ 2 + 1,
        white: w,
        black: b,
        whiteIdx: i,
        blackIdx: b != null ? i + 1 : null));
  }
  return out;
}

MoveGrade? _gradeFor(List<MoveEvalHistoryEntry> entries, int globalIndex) {
  final ply = globalIndex + 1;
  for (final e in entries) {
    if (e.moveIndex1Based == ply) return e.grade;
  }
  return null;
}

Widget _buildQualityBlock(
  BuildContext context,
  String title,
  MoveQualityWindowStats w,
  MoveQualityWindowStats b,
) {
  String fmt(MoveQualityWindowStats s) {
    if (s.count == 0) return '—';
    final q = s.averageQualityPercent;
    final cp = s.averageCpLoss;
    final qStr = q != null ? '${q.round()} %' : '—';
    final cpStr = cp != null ? ' · Ø ztráta ${cp.round()} cp' : '';
    return '$qStr$cpStr · ${s.count} tahů';
  }

  Widget col(String label, MoveQualityWindowStats s) => Expanded(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(label, style: Theme.of(context).textTheme.labelSmall),
            Text(fmt(s), style: Theme.of(context).textTheme.bodyMedium),
          ],
        ),
      );

  return Padding(
    padding: const EdgeInsets.only(bottom: 8),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title, style: Theme.of(context).textTheme.labelLarge),
        const SizedBox(height: 4),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            col('Bílý', w),
            col('Černý', b),
          ],
        ),
      ],
    ),
  );
}

/// Parita iOS `AnalysisView` — graf evaluace, kvalita tahů, historie, druhá varianta, vlastní FEN.
class AnalysisScreen extends ConsumerStatefulWidget {
  const AnalysisScreen({super.key});

  @override
  ConsumerState<AnalysisScreen> createState() => _AnalysisScreenState();
}

class _AnalysisScreenState extends ConsumerState<AnalysisScreen> {
  final _fenCtrl = TextEditingController();
  int _depth = 10;
  String _freeResult = '';
  String _secondaryLine = '';
  bool _busyFree = false;
  bool _busySecondary = false;

  @override
  void dispose() {
    _fenCtrl.dispose();
    super.dispose();
  }

  void _syncFenFromSession(GameSnapshot? snap) {
    if (snap == null) return;
    if (_fenCtrl.text.isEmpty) {
      _fenCtrl.text = fenFromSnapshot(snap);
    }
  }

  void _openSequential(BuildContext context, List<GameHistoryMove> moves,
      List<MoveEvalHistoryEntry> evals) {
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (ctx) {
        final cs = Theme.of(ctx).colorScheme;
        return DraggableScrollableSheet(
          expand: false,
          initialChildSize: 0.55,
          minChildSize: 0.35,
          maxChildSize: 0.92,
          builder: (_, scroll) => ListView(
            controller: scroll,
            padding: const EdgeInsets.all(16),
            children: [
              Text('Průběh partie', style: Theme.of(ctx).textTheme.titleLarge),
              const SizedBox(height: 8),
              Text(
                'Každý řádek je jeden tah v pořadí — nejdřív bílý, pak černý.',
                style: Theme.of(ctx).textTheme.bodySmall,
              ),
              const Divider(height: 24),
              for (var i = 0; i < moves.length; i++)
                ListTile(
                  dense: true,
                  leading: Text('${i + 1}.',
                      style: const TextStyle(
                          fontFeatures: [FontFeature.tabularFigures()])),
                  title: Text(
                    _moveText(moves[i]),
                    style: TextStyle(
                        color: _gradeColor(_gradeFor(evals, i), cs),
                        fontWeight: FontWeight.w500),
                  ),
                  subtitle: Text(i % 2 == 0 ? 'Bílý' : 'Černý'),
                ),
            ],
          ),
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    ref.watch(moveEvalNotifierProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final snap = session.snapshot;
    final ui = ref.watch(gameUiNotifierProvider);
    final moveEval = ref.watch(moveEvalNotifierProvider);
    final cs = Theme.of(context).colorScheme;

    _syncFenFromSession(snap);

    final chartPoints = moveEval.entries
        .where((e) => e.evalWhitePawns != null)
        .map((e) => (
              moveIndex: e.moveIndex1Based,
              eval: e.evalWhitePawns!,
              grade: e.grade
            ))
        .toList();

    final summary = moveEval.entries.isEmpty
        ? null
        : MoveQualityAggregator.boardSummary(moveEval.entries);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Analýza'),
        actions: [
          IconButton(
            tooltip: 'Záložka Hra',
            icon: const Icon(Icons.grid_on_outlined),
            onPressed: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.game,
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(12, 12, 12, 32),
        children: [
          const NetworkStatusBanners(),
          const SizedBox(height: 12),
          Card(
            child: ListTile(
              leading: Icon(Icons.psychology, color: cs.primary),
              title: const Text('Stockfish'),
              subtitle: const Text(
                  'Eval tahů a graf — chess-api nebo vlastní URL v Nastavení (vývojář).'),
            ),
          ),
          const SizedBox(height: 12),
          if (snap == null) ...[
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text('Žádná pozice z desky',
                        style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 8),
                    const Text(
                        'Na záložce Hra připoj šachovnici. Níže můžeš analyzovat vlastní FEN.'),
                    const SizedBox(height: 12),
                    FilledButton.tonal(
                      onPressed: () => ref
                          .read(mainNavTabIndexProvider.notifier)
                          .state = AppMainTab.game,
                      child: const Text('Otevřít Hru'),
                    ),
                  ],
                ),
              ),
            ),
          ] else ...[
            if (!ui.moveEvaluationEnabled)
              Card(
                color: cs.secondaryContainer,
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      ListTile(
                        contentPadding: EdgeInsets.zero,
                        leading: Icon(Icons.info_outline,
                            color: cs.onSecondaryContainer),
                        title: Text('Graf a hodnocení tahů jsou vypnuté',
                            style: TextStyle(color: cs.onSecondaryContainer)),
                        subtitle: Text(
                          'Zapni přepínač níže nebo v Nastavení → Vzhled šachovnice.',
                          style: TextStyle(color: cs.onSecondaryContainer),
                        ),
                      ),
                      SwitchListTile(
                        contentPadding: EdgeInsets.zero,
                        title: const Text('Povolit hodnocení tahů (Stockfish)'),
                        value: ui.moveEvaluationEnabled,
                        onChanged: (v) => ref
                            .read(gameUiNotifierProvider.notifier)
                            .setMoveEvaluationEnabled(v),
                      ),
                    ],
                  ),
                ),
              ),
            if (!ui.moveEvaluationEnabled) const SizedBox(height: 12),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text('Přehled partie',
                        style: Theme.of(context).textTheme.titleMedium),
                    const Text(
                        'Eval pozice po tahu (perspektiva bílého: + výhoda bílého)',
                        style: TextStyle(fontSize: 13)),
                    const SizedBox(height: 8),
                    if (chartPoints.isEmpty)
                      Text(
                        ui.moveEvaluationEnabled
                            ? 'Graf se naplní po tazích, až Stockfish vyhodnotí pozici (hraj na kartě Hra s internetem).'
                            : 'Po zapnutí automatického vyhodnocení uvidíš křivku zde.',
                        style: Theme.of(context).textTheme.bodySmall,
                      )
                    else ...[
                      EvalLineChart(points: chartPoints),
                      TextButton(
                        onPressed: () => ref
                            .read(moveEvalNotifierProvider.notifier)
                            .clearHistory(),
                        child: const Text(
                            'Vymazat graf a uložené evaluace tahů',
                            style: TextStyle(color: Colors.red)),
                      ),
                    ],
                    const SizedBox(height: 8),
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Icon(Icons.grid_on, size: 20, color: cs.primary),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                  'Stav: ${snap.status.gameState} · Na tahu: ${snap.status.currentPlayer} · ${snap.history.moves.length} tahů'),
                              const SizedBox(height: 4),
                              Builder(builder: (ctx) {
                                final fenLine = fenFromSnapshot(snap);
                                final eco = OpeningEco.titleForFen(fenLine);
                                return Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    if (eco != null)
                                      Padding(
                                        padding:
                                            const EdgeInsets.only(bottom: 4),
                                        child: Text(eco,
                                            style: TextStyle(
                                                color: cs.primary,
                                                fontWeight: FontWeight.w600)),
                                      ),
                                    SelectableText(fenLine,
                                        style: const TextStyle(
                                            fontFamily: 'monospace',
                                            fontSize: 11)),
                                  ],
                                );
                              }),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            if (summary != null) ...[
              const Divider(height: 32),
              Text('Kvalita tahů',
                  style: Theme.of(context).textTheme.titleMedium),
              const Text('Průměr 0–100 podle stupně Stockfish a Ø ztráta v cp',
                  style: TextStyle(fontSize: 12)),
              const SizedBox(height: 8),
              _buildQualityBlock(context, 'Poslední 3 tahy strany',
                  summary.white.last3, summary.black.last3),
              _buildQualityBlock(context, 'Posledních 10 tahů strany',
                  summary.white.last10, summary.black.last10),
              _buildQualityBlock(context, 'Celá partie', summary.white.fullGame,
                  summary.black.fullGame),
            ],
            if (moveEval.lastMessage != null) ...[
              const Divider(height: 24),
              ListTile(
                title: const Text('Poslední zhodnocení tahu'),
                subtitle: Text(
                  moveEval.lastMessage!,
                  style: TextStyle(color: _gradeColor(moveEval.lastGrade, cs)),
                ),
              ),
            ],
            if (moveEval.lastBusy) const LinearProgressIndicator(),
            const Divider(height: 32),
            Text('Historie tahů',
                style: Theme.of(context).textTheme.titleMedium),
            const Text('Klepnutím otevřeš chronologický průběh.',
                style: TextStyle(fontSize: 12)),
            const SizedBox(height: 8),
            InkWell(
              onTap: () => _openSequential(
                  context, snap.history.moves, moveEval.entries),
              child: Column(
                children: [
                  for (final row in _plyRows(snap.history.moves))
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 4),
                      child: Row(
                        children: [
                          SizedBox(
                              width: 28,
                              child: Text('${row.moveNum}.',
                                  textAlign: TextAlign.right)),
                          Expanded(
                            child: Text(
                              _moveText(row.white),
                              style: TextStyle(
                                  color: _gradeColor(
                                      _gradeFor(moveEval.entries, row.whiteIdx),
                                      cs)),
                            ),
                          ),
                          if (row.black != null && row.blackIdx != null)
                            Expanded(
                              child: Text(
                                _moveText(row.black!),
                                textAlign: TextAlign.right,
                                style: TextStyle(
                                  color: _gradeColor(
                                      _gradeFor(
                                          moveEval.entries, row.blackIdx!),
                                      cs),
                                ),
                              ),
                            ),
                        ],
                      ),
                    ),
                ],
              ),
            ),
          ],
          const Divider(height: 32),
          Text('Druhá varianta',
              style: Theme.of(context).textTheme.titleMedium),
          const Text(
              'Porovnání hlavní hloubky s nižší (heuristika jako na iOS).',
              style: TextStyle(fontSize: 12)),
          const SizedBox(height: 8),
          if (_secondaryLine.isNotEmpty) Text(_secondaryLine),
          FilledButton.tonal(
            onPressed: _busySecondary || snap == null
                ? null
                : () async {
                    final fen = fenFromSnapshot(snap);
                    setState(() {
                      _busySecondary = true;
                      _secondaryLine = '';
                    });
                    try {
                      final d = ref.read(prefsRepositoryProvider).hintDepth;
                      final pair = await ref
                          .read(stockfishApiClientProvider)
                          .fetchPrimaryAndSecondary(fen, depth: d);
                      final p = pair.primary;
                      final s = pair.secondary;
                      setState(() {
                        if (s != null) {
                          final ev = p.evalPawns != null
                              ? ' · eval ≈ ${p.evalPawns!.toStringAsFixed(2)} pěš.'
                              : '';
                          _secondaryLine =
                              '1) ${p.from}→${p.to} · 2) ${s.from}→${s.to}$ev';
                        } else {
                          _secondaryLine =
                              'Obě hloubiny dávají stejný tah ${p.from}→${p.to}.';
                        }
                      });
                    } catch (e) {
                      setState(() => _secondaryLine = '$e');
                    } finally {
                      if (mounted) setState(() => _busySecondary = false);
                    }
                  },
            child: Text(_busySecondary
                ? 'Počítám…'
                : 'Načíst druhou variantu (ze session FEN)'),
          ),
          const Divider(height: 32),
          Text('Vlastní pozice',
              style: Theme.of(context).textTheme.titleMedium),
          const Text('Analýza bez fyzické desky',
              style: TextStyle(fontSize: 12)),
          const SizedBox(height: 8),
          TextField(
            controller: _fenCtrl,
            maxLines: 3,
            decoration: const InputDecoration(
              labelText: 'FEN',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 8),
          Text('Hloubka: $_depth'),
          Slider(
            min: 1,
            max: 18,
            divisions: 17,
            value: _depth.toDouble(),
            label: '$_depth',
            onChanged: (v) => setState(() => _depth = v.round()),
          ),
          Wrap(
            spacing: 12,
            runSpacing: 8,
            children: [
              FilledButton(
                onPressed: _busyFree
                    ? null
                    : () async {
                        final fen = _fenCtrl.text.trim();
                        if (fen.isEmpty) return;
                        setState(() {
                          _busyFree = true;
                          _freeResult = 'Počítám…';
                        });
                        try {
                          final mv = await ref
                              .read(stockfishApiClientProvider)
                              .fetchBestMove(fen, depth: _depth);
                          final ev = mv.evalPawns != null
                              ? ' eval ≈ ${mv.evalPawns!.toStringAsFixed(2)} pěš.'
                              : '';
                          setState(() {
                            _freeResult =
                                'Nejlepší tah: ${mv.from}–${mv.to}$ev${mv.text != null ? '\n${mv.text}' : ''}';
                          });
                        } catch (e) {
                          setState(() => _freeResult = 'Chyba: $e');
                        } finally {
                          if (mounted) setState(() => _busyFree = false);
                        }
                      },
                child: const Text('Analyzovat pozici'),
              ),
              OutlinedButton(
                onPressed: () {
                  final t = _fenCtrl.text.trim();
                  if (t.isEmpty) return;
                  ref
                      .read(gameUiNotifierProvider.notifier)
                      .enterSandboxFromCustomFen(t);
                  ref.read(mainNavTabIndexProvider.notifier).state =
                      AppMainTab.game;
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                        content: Text('Sandbox z FEN — záložka Hra')),
                  );
                },
                child: const Text('Náhled v Hře'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          SelectableText(_freeResult, style: const TextStyle(fontSize: 14)),
        ],
      ),
    );
  }
}
