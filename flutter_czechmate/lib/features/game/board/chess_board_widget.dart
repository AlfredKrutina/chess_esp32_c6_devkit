import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/constants/app_colors.dart';
import '../../../core/utils/board_algebraic.dart';
import '../../connection/board_session_notifier.dart';
import '../state/game_ui_notifier.dart';

void _openPromotionSheet(
  BuildContext context,
  WidgetRef ref,
  void Function(String) snack,
) {
  showModalBottomSheet<void>(
    context: context,
    builder: (ctx) => SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const ListTile(title: Text('Promoce — vyber figuru')),
          ListTile(
            title: const Text('Dáma'),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('q', snack);
              Navigator.pop(ctx);
            },
          ),
          ListTile(
            title: const Text('Věž'),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('r', snack);
              Navigator.pop(ctx);
            },
          ),
          ListTile(
            title: const Text('Střelec'),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('b', snack);
              Navigator.pop(ctx);
            },
          ),
          ListTile(
            title: const Text('Jezdec'),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('n', snack);
              Navigator.pop(ctx);
            },
          ),
        ],
      ),
    ),
  );
}

class ChessBoardWidget extends ConsumerWidget {
  const ChessBoardWidget({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final notifier = ref.read(gameUiNotifierProvider.notifier);
    final snap = session.snapshot;
    final serverBoard = snap?.board ??
        List.generate(8, (_) => List.generate(8, (_) => ''));
    final cells = notifier.effectiveBoard(serverBoard);
    final flipped = ui.boardFlipped;
    final last = snap?.history.moves.isNotEmpty == true
        ? snap!.history.moves.last
        : null;
    final err = snap?.status.errorState;
    void snack(String m) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(m)));
    }

    return AspectRatio(
      aspectRatio: 1,
      child: InteractiveViewer(
        minScale: 0.6,
        maxScale: 3.5,
        child: LayoutBuilder(
          builder: (context, c) {
            final side = c.maxWidth;
            return Stack(
              children: [
                Builder(
                  builder: (ctx) {
                    return GestureDetector(
                      onTapUp: (details) async {
                        final box = ctx.findRenderObject() as RenderBox?;
                        if (box == null) return;
                        final local = box.globalToLocal(details.globalPosition);
                        final sq = side / 8;
                        final col = (local.dx / sq).floor().clamp(0, 7);
                        final row = (local.dy / sq).floor().clamp(0, 7);
                        final alg = algebraicAt(
                          flipped: flipped,
                          screenRow: row,
                          screenCol: col,
                        );
                        if (alg.isEmpty) return;
                        await notifier.onSquareTapped(alg, session, snack);
                        if (!ctx.mounted) return;
                        final st = ref.read(gameUiNotifierProvider);
                        if (st.pendingPromotionFrom != null &&
                            st.pendingPromotionTo != null) {
                          _openPromotionSheet(ctx, ref, snack);
                        }
                      },
                      child: CustomPaint(
                        size: Size(side, side),
                        painter: _BoardPainter(
                          board: cells,
                          flipped: flipped,
                          lastFrom: last?.from,
                          lastTo: last?.to,
                          invalidSquare:
                              err?.active == true ? err?.invalidPos : null,
                          originalSquare:
                              err?.active == true ? err?.originalPos : null,
                          selected: ui.selectedSquare,
                        ),
                      ),
                    );
                  },
                ),
                if (ui.showCoordinates)
                  _CoordinateOverlay(
                    size: side,
                    square: side / 8,
                    flipped: flipped,
                  ),
              ],
            );
          },
        ),
      ),
    );
  }
}

class _BoardPainter extends CustomPainter {
  _BoardPainter({
    required this.board,
    required this.flipped,
    this.lastFrom,
    this.lastTo,
    this.invalidSquare,
    this.originalSquare,
    this.selected,
  });

  final List<List<String>> board;
  final bool flipped;
  final String? lastFrom;
  final String? lastTo;
  final String? invalidSquare;
  final String? originalSquare;
  final String? selected;

  @override
  void paint(Canvas canvas, Size size) {
    final sq = size.width / 8;
    for (var sr = 0; sr < 8; sr++) {
      for (var sc = 0; sc < 8; sc++) {
        final rankNum = flipped ? sr + 1 : 8 - sr;
        final fileIndex = flipped ? 7 - sc : sc;
        final isDark = (fileIndex + rankNum).isOdd;
        final rect = Rect.fromLTWH(sc * sq, sr * sq, sq, sq);
        canvas.drawRect(
          rect,
          Paint()..color = isDark ? AppColors.darkSquare : AppColors.lightSquare,
        );
      }
    }
    _markSquare(canvas, sq, invalidSquare, AppColors.badMove);
    _markSquare(canvas, sq, originalSquare, AppColors.validMove);
    _markSquare(canvas, sq, lastFrom, AppColors.lastMove);
    _markSquare(canvas, sq, lastTo, AppColors.lastMove);
    _markSquare(canvas, sq, selected, AppColors.validMove);
    _drawPieces(canvas, sq);
  }

  void _markSquare(Canvas canvas, double sq, String? algebraic, Color color) {
    if (algebraic == null || algebraic.length < 2) return;
    final p = screenCoordsFromAlgebraic(flipped: flipped, algebraic: algebraic);
    if (p.$1 < 0) return;
    canvas.drawRect(
      Rect.fromLTWH(p.$2 * sq, p.$1 * sq, sq, sq),
      Paint()..color = color,
    );
  }

  void _drawPieces(Canvas canvas, double sq) {
    final tp = TextPainter(textDirection: TextDirection.ltr);
    for (var sr = 0; sr < 8; sr++) {
      for (var sc = 0; sc < 8; sc++) {
        final br = flipped ? 7 - sr : sr;
        final bf = flipped ? 7 - sc : sc;
        if (br >= board.length) continue;
        final row = board[br];
        if (bf >= row.length) continue;
        final cell = row[bf].trim();
        if (cell.isEmpty) continue;
        final ch = cell.substring(0, 1);
        tp.text = TextSpan(
          text: _glyph(ch),
          style: TextStyle(
            fontSize: sq * 0.52,
            color: ch.toUpperCase() == ch ? Colors.black87 : Colors.blueGrey,
          ),
        );
        tp.layout();
        tp.paint(canvas, Offset(sc * sq + sq * 0.18, sr * sq + sq * 0.12));
      }
    }
  }

  String _glyph(String p) {
    switch (p) {
      case 'K':
        return '♔';
      case 'Q':
        return '♕';
      case 'R':
        return '♖';
      case 'B':
        return '♗';
      case 'N':
        return '♘';
      case 'P':
        return '♙';
      case 'k':
        return '♚';
      case 'q':
        return '♛';
      case 'r':
        return '♜';
      case 'b':
        return '♝';
      case 'n':
        return '♞';
      case 'p':
        return '♟';
      default:
        return p;
    }
  }

  @override
  bool shouldRepaint(covariant _BoardPainter oldDelegate) {
    return oldDelegate.board != board ||
        oldDelegate.flipped != flipped ||
        oldDelegate.lastFrom != lastFrom ||
        oldDelegate.lastTo != lastTo ||
        oldDelegate.invalidSquare != invalidSquare ||
        oldDelegate.originalSquare != originalSquare ||
        oldDelegate.selected != selected;
  }
}

class _CoordinateOverlay extends StatelessWidget {
  const _CoordinateOverlay({
    required this.size,
    required this.square,
    required this.flipped,
  });

  final double size;
  final double square;
  final bool flipped;

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: SizedBox(
        width: size,
        height: size,
        child: Stack(
          children: [
            for (var i = 0; i < 8; i++)
              Positioned(
                left: i * square + 2,
                top: size - 14,
                child: Text(
                  flipped ? kFiles[7 - i] : kFiles[i],
                  style: const TextStyle(fontSize: 10, color: Colors.black54),
                ),
              ),
            for (var i = 0; i < 8; i++)
              Positioned(
                left: 2,
                top: i * square + 2,
                child: Text(
                  '${flipped ? i + 1 : 8 - i}',
                  style: const TextStyle(fontSize: 10, color: Colors.black54),
                ),
              ),
          ],
        ),
      ),
    );
  }
}
