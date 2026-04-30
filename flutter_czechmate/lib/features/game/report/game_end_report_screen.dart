import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../../../core/models/game_end_report_model.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../../core/utils/game_end_report_timing.dart';
import '../../../core/utils/opening_eco.dart';
import '../../../core/utils/pgn_export.dart';
import '../../analysis/eval_line_chart.dart';
import '../../analysis/move_eval_notifier.dart';
import '../../connection/board_session_notifier.dart';
import 'timing_charts.dart';

class GameEndReportScreen extends ConsumerStatefulWidget {
  const GameEndReportScreen({super.key});

  @override
  ConsumerState<GameEndReportScreen> createState() => _GameEndReportScreenState();
}

class _GameEndReportScreenState extends ConsumerState<GameEndReportScreen> {
  final GlobalKey _shareBoundaryKey = GlobalKey();
  bool _isExporting = false;

  Future<void> _sharePgn() async {
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    if (snap == null) return;
    final pgn = buildPgnFromSnapshot(snap);
    final bytes = Uint8List.fromList(utf8.encode(pgn));
    if (!mounted) return;
    final box = context.findRenderObject() as RenderBox?;
    final rect = box != null ? box.localToGlobal(Offset.zero) & box.size : null;
    final tmp = await getTemporaryDirectory();
    final dir = Directory('${tmp.path}/czechmate_share');
    await dir.create(recursive: true);
    final file = File('${dir.path}/czechmate_game.pgn');
    await file.writeAsBytes(bytes);
    await Share.shareXFiles(
      [XFile(file.path, mimeType: 'application/x-chess-pgn')],
      text: 'PGN partie CZECHMATE',
      sharePositionOrigin: rect,
    );
  }

  Future<void> _exportAndShare() async {
    setState(() => _isExporting = true);
    try {
      final boundary = _shareBoundaryKey.currentContext?.findRenderObject() as RenderRepaintBoundary?;
      if (boundary == null) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Export není připravený — zkus znovu za chvíli.')),
          );
        }
        return;
      }

      // Jedna překreslená frame — někdy je boundary hned po otevření obrazovky bez rozvržení.
      await WidgetsBinding.instance.endOfFrame;

      final image = await boundary.toImage(pixelRatio: 3.0);
      final byteData = await image.toByteData(format: ui.ImageByteFormat.png);
      if (byteData == null) {
        throw StateError('Nepodařilo se zakódovat obrázek do PNG.');
      }
      final pngBytes = byteData.buffer.asUint8List();

      final snap = ref.read(boardSessionNotifierProvider).snapshot;
      final reportModel = snap != null ? GameEndReportModel.fromSnapshot(snap) : null;
      if (!mounted) return;
      final box = context.findRenderObject() as RenderBox?;
      final rect = box != null ? box.localToGlobal(Offset.zero) & box.size : null;

      final meta = reportModel == null
          ? 'CZECHMATE'
          : 'Výsledek: ${_resultText(reportModel.result)} • Tahů: ${reportModel.totalMoves} • Délka: ${reportModel.durationSec}s';
      final tmp = await getTemporaryDirectory();
      final dir = Directory('${tmp.path}/czechmate_share');
      await dir.create(recursive: true);
      final pngFile = File('${dir.path}/czechmate_game_report.png');
      await pngFile.writeAsBytes(pngBytes);
      await Share.shareXFiles(
        [XFile(pngFile.path, mimeType: 'image/png')],
        text: 'CZECHMATE — $meta',
        sharePositionOrigin: rect,
      );
    } catch (e) {
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Chyba při exportu: $e')));
    } finally {
      if (mounted) setState(() => _isExporting = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final snapshot = session.snapshot;
    if (snapshot == null) return const Scaffold(body: Center(child: Text('Žádná herní data.')));

    final model = GameEndReportModel.fromSnapshot(snapshot);
    final fen = fenFromSnapshot(snapshot);
    final eco = OpeningEco.titleForFen(fen);
    final moveEval = ref.watch(moveEvalNotifierProvider);
    final nMoves = snapshot.history.moves.length;
    final evalPoints = moveEval.entries
        .where((e) => e.evalWhitePawns != null && e.moveIndex1Based <= nMoves)
        .map((e) => (moveIndex: e.moveIndex1Based, eval: e.evalWhitePawns!, grade: e.grade))
        .toList();

    final think = GameEndReportTiming.thinkPlyPoints(snapshot.history.moves);
    final cumulative = GameEndReportTiming.cumulativePoints(think);
    final barPoints = think.where((p) => p.secondsFromPrevious != null).toList();
    final hasTiming = barPoints.isNotEmpty || cumulative.isNotEmpty;
    final clock = snapshot.clock;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Souhrn partie'),
        actions: [
          IconButton(
            icon: const Icon(Icons.close),
            onPressed: () => Navigator.pop(context),
          )
        ],
      ),
      body: LayoutBuilder(
        builder: (context, constraints) {
          const sidePad = 16.0;
          final avail = (constraints.maxWidth - sidePad * 2).clamp(0.0, double.infinity);
          final evalHasChart = evalPoints.isNotEmpty;
          final useTwoCol = avail >= 920 && evalHasChart && hasTiming;
          final maxW = useTwoCol ? math.min(avail, 1080.0) : math.min(avail, 640.0);

          Widget evalSection() {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text('Evaluace tahů', style: Theme.of(context).textTheme.titleMedium),
                if (evalPoints.isEmpty)
                  Text(
                    'Žádná data Stockfish — zapni eval tahů a odehraj partii.',
                    style: Theme.of(context).textTheme.bodySmall,
                  )
                else ...[
                  Text(
                    'Graf z perspektivy bílého (+ = výhoda bílého)',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 8),
                  EvalLineChart(points: evalPoints, height: useTwoCol ? 228 : 200),
                ],
              ],
            );
          }

          Widget timingSection() {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text('Časová osa (z razítek tahů)', style: Theme.of(context).textTheme.titleMedium),
                if (!hasTiming)
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Text(
                        'Deska neposlala časové značky u tahů — grafy času nejsou k dispozici.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  )
                else ...[
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Odehraný čas', style: TextStyle(fontWeight: FontWeight.w600)),
                          Text(
                            'Součet sekund od začátku partie po půltazích',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                          const SizedBox(height: 8),
                          CumulativePlayedTimeChart(points: cumulative, accent: cs.primary),
                          const Text('půltah → min', style: TextStyle(fontSize: 11, color: Colors.grey)),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Čas na tah', style: TextStyle(fontWeight: FontWeight.w600)),
                          Text(
                            'Sekundy od předchozího tahu (posun vpravo)',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                          const SizedBox(height: 8),
                          TimePerMoveBarChart(
                            points: think,
                            whiteColor: cs.primary,
                            blackColor: Colors.purple,
                          ),
                          const SizedBox(height: 8),
                          Wrap(
                            spacing: 20,
                            children: [
                              _LegendDot(color: cs.primary.withValues(alpha: 0.75), label: 'Bílý'),
                              _LegendDot(color: Colors.purple.withValues(alpha: 0.55), label: 'Černý'),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ],
            );
          }

          return Align(
            alignment: Alignment.topCenter,
            child: ConstrainedBox(
              constraints: BoxConstraints(maxWidth: maxW),
              child: ListView(
                padding: EdgeInsets.fromLTRB(sidePad, 12, sidePad, 32),
                children: [
                  Text('Konec hry: ${model.reason}', style: Theme.of(context).textTheme.headlineSmall),
                  const SizedBox(height: 16),
                  RepaintBoundary(
                    key: _shareBoundaryKey,
                    child: Container(
                      padding: const EdgeInsets.all(24),
                      decoration: BoxDecoration(
                        color: Theme.of(context).colorScheme.surfaceContainerHigh,
                        borderRadius: BorderRadius.circular(20),
                        boxShadow: const [
                          BoxShadow(color: Colors.black12, blurRadius: 10, offset: Offset(0, 4)),
                        ],
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            mainAxisAlignment: MainAxisAlignment.spaceBetween,
                            children: [
                              Text(
                                'CZECHMATE šachovnice',
                                style: TextStyle(
                                  fontWeight: FontWeight.bold,
                                  color: Theme.of(context).colorScheme.primary,
                                ),
                              ),
                              const Icon(Icons.check_circle_outline, color: Colors.green),
                            ],
                          ),
                          const Divider(height: 32),
                          Text(
                            _resultText(model.result),
                            style: const TextStyle(fontSize: 32, fontWeight: FontWeight.bold),
                          ),
                          Text(model.reason, style: const TextStyle(color: Colors.grey)),
                          const SizedBox(height: 24),
                          Wrap(
                            spacing: 28,
                            runSpacing: 16,
                            alignment: WrapAlignment.center,
                            children: [
                              _StatColumn(label: 'ČAS', value: '${model.durationSec} s'),
                              _StatColumn(label: 'TAHŮ', value: '${model.totalMoves}'),
                              _StatColumn(label: 'Ø TAH', value: '${model.whiteAvgMoveSec.toStringAsFixed(1)} s'),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  FilledButton.icon(
                    onPressed: _isExporting ? null : _exportAndShare,
                    icon: _isExporting
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2),
                          )
                        : const Icon(Icons.ios_share),
                    label: const Text('Exportovat nálepku (Strava-like)'),
                    style: FilledButton.styleFrom(padding: const EdgeInsets.all(16)),
                  ),
                  const SizedBox(height: 8),
                  OutlinedButton.icon(
                    onPressed: _sharePgn,
                    icon: const Icon(Icons.description_outlined),
                    label: const Text('Sdílet PGN jako soubor'),
                  ),
                  const Divider(height: 28),
                  if (useTwoCol)
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(child: evalSection()),
                        const SizedBox(width: 20),
                        Expanded(child: timingSection()),
                      ],
                    )
                  else ...[
                    evalSection(),
                    const Divider(height: 28),
                    timingSection(),
                  ],
                  if (clock != null && clock.isTimeControlEnabled) ...[
                    const Divider(height: 28),
                    Text('Čas na hodinách', style: Theme.of(context).textTheme.titleMedium),
                    Card(
                      child: Padding(
                        padding: const EdgeInsets.all(12),
                        child: Row(
                          children: [
                            Expanded(
                              child: Column(
                                children: [
                                  const Text('Bílý'),
                                  Text(
                                    '${(clock.whiteTimeMs / 60000).toStringAsFixed(1)} min',
                                    style: Theme.of(context).textTheme.titleLarge,
                                  ),
                                ],
                              ),
                            ),
                            Expanded(
                              child: Column(
                                children: [
                                  const Text('Černý'),
                                  Text(
                                    '${(clock.blackTimeMs / 60000).toStringAsFixed(1)} min',
                                    style: Theme.of(context).textTheme.titleLarge,
                                  ),
                                ],
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ],
                  const Divider(height: 28),
                  Text('Konečný FEN', style: Theme.of(context).textTheme.titleMedium),
                  if (eco != null)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 6),
                      child: Text(eco, style: TextStyle(color: cs.primary, fontWeight: FontWeight.w600)),
                    ),
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: SelectableText(fen, style: const TextStyle(fontFamily: 'monospace', fontSize: 12)),
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }

  String _resultText(GameResultState res) {
    if (res == GameResultState.whiteWon) return 'Bílý vyhrál';
    if (res == GameResultState.blackWon) return 'Černý vyhrál';
    if (res == GameResultState.draw) return 'Remíza';
    return 'Ukončeno';
  }
}

class _LegendDot extends StatelessWidget {
  const _LegendDot({required this.color, required this.label});
  final Color color;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(width: 8, height: 8, decoration: BoxDecoration(color: color, shape: BoxShape.circle)),
        const SizedBox(width: 6),
        Text(label, style: Theme.of(context).textTheme.bodySmall),
      ],
    );
  }
}

class _StatColumn extends StatelessWidget {
  const _StatColumn({required this.label, required this.value});
  final String label;
  final String value;
  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(value, style: const TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
        Text(label, style: const TextStyle(fontSize: 10, color: Colors.grey, letterSpacing: 1.5)),
      ],
    );
  }
}
