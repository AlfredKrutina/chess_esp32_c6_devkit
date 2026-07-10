import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/constants/board_styles.dart';
import '../../../core/models/game_snapshot.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/widgets/app_modal_sheet.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/utils/board_algebraic.dart';
import '../../../core/utils/fen_board_parser.dart';
import '../../connection/board_session_notifier.dart';
import '../state/game_ui_notifier.dart';
import 'board_pieces_animator.dart';

/// Stabilní klíč animatoru figurek: mění se při tahu / změně pozice / sandboxu — ne při každém `stateVersion` z poll (což by rušilo animace).
String _boardPlacementSig(List<List<String>> cells) {
  final b = StringBuffer();
  for (final row in cells) {
    for (final c in row) {
      b.write(c);
    }
  }
  return b.toString();
}

void _openPromotionSheet(
  BuildContext context,
  WidgetRef ref,
  void Function(String) snack,
) {
  final l10n = context.l10n;
  showAppModalBottomSheet<void>(
    context: context,
    showDragHandle: false,
    builder: (ctx) => SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          ListTile(title: Text(l10n.gamePromotionPickTitle)),
          ListTile(
            title: Text(l10n.gamePromotionQueen),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('q', snack);
              Navigator.pop(ctx);
            },
          ),
          ListTile(
            title: Text(l10n.gamePromotionRook),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('r', snack);
              Navigator.pop(ctx);
            },
          ),
          ListTile(
            title: Text(l10n.gamePromotionBishop),
            onTap: () {
              ref
                  .read(gameUiNotifierProvider.notifier)
                  .completePromotion('b', snack);
              Navigator.pop(ctx);
            },
          ),
          ListTile(
            title: Text(l10n.gamePromotionKnight),
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

class ChessBoardWidget extends ConsumerStatefulWidget {
  const ChessBoardWidget({super.key});

  @override
  ConsumerState<ChessBoardWidget> createState() => _ChessBoardWidgetState();
}

class _ChessBoardWidgetState extends ConsumerState<ChessBoardWidget>
    with SingleTickerProviderStateMixin {
  late final AnimationController _invalidRecoveryPulse;

  @override
  void initState() {
    super.initState();
    _invalidRecoveryPulse = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 300),
    );
  }

  @override
  void dispose() {
    _invalidRecoveryPulse.dispose();
    super.dispose();
  }

  void _syncInvalidRecoveryAnimation(bool serverInvalid) {
    if (serverInvalid) {
      if (!_invalidRecoveryPulse.isAnimating) {
        _invalidRecoveryPulse.repeat(reverse: true);
      }
    } else {
      if (_invalidRecoveryPulse.isAnimating) {
        _invalidRecoveryPulse.stop();
      }
      _invalidRecoveryPulse.reset();
    }
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final snap = session.snapshot;
    final notifier = ref.read(gameUiNotifierProvider.notifier);
    final serverBoard =
        snap?.board ?? List.generate(8, (_) => List.generate(8, (_) => ''));
    final cells = notifier.effectiveBoard(serverBoard);
    final piecesKey =
        '${ui.sandboxMode}_${snap?.history.moves.length}_${_boardPlacementSig(cells)}';
    final flipped = ui.boardFlipped;
    final moves = snap?.history.moves;
    final GameHistoryMove? lastMoveHighlight;
    if (!ui.sandboxMode &&
        ui.historyReviewMoveIndex != null &&
        moves != null &&
        moves.isNotEmpty) {
      final i = ui.historyReviewMoveIndex!;
      if (i >= 0 && i < moves.length) {
        lastMoveHighlight = moves[i];
      } else {
        lastMoveHighlight = null;
      }
    } else if (moves != null && moves.isNotEmpty) {
      lastMoveHighlight = moves.last;
    } else {
      lastMoveHighlight = null;
    }
    final err = snap?.status.errorState;
    final serverInvalid = err?.active == true &&
        err?.invalidPos != null &&
        err!.invalidPos!.length >= 2;
    _syncInvalidRecoveryAnimation(serverInvalid);
    void snack(String m) {
      showAppSnackBar(context, m);
    }

    return Semantics(
      label: context.l10n.semanticsChessBoard,
      child: AspectRatio(
        aspectRatio: 1,
        child: InteractiveViewer(
          panEnabled: false,
          minScale: 0.6,
          maxScale: 3.5,
          child: LayoutBuilder(
            builder: (context, c) {
              final side = c.maxWidth;
              final themeColors = BoardStyleColors.fromRaw(ui.boardStyleRaw);
              return Stack(
                children: [
                  Builder(
                    builder: (ctx) {
                      return GestureDetector(
                        onTapUp: (details) async {
                          final box = ctx.findRenderObject() as RenderBox?;
                          if (box == null) return;
                          final local =
                              box.globalToLocal(details.globalPosition);
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
                        child: AnimatedBuilder(
                          animation: _invalidRecoveryPulse,
                          builder: (context, _) {
                            final pulseT = serverInvalid
                                ? _invalidRecoveryPulse.value
                                : 0.0;
                            return CustomPaint(
                              size: Size(side, side),
                              painter: _BoardPainter(
                                board: cells,
                                flipped: flipped,
                                lastFrom: lastMoveHighlight?.from,
                                lastTo: lastMoveHighlight?.to,
                                serverInvalidSquare:
                                    serverInvalid ? err.invalidPos : null,
                                serverInvalidPulseT: pulseT,
                                originalSquare:
                                    serverInvalid ? err.originalPos : null,
                                clientInvalidSquare:
                                    ui.invalidDestinationPulseSquare,
                                clientInvalidLit: ui.invalidDestinationPulseLit,
                                hintFrom: ui.hintFrom,
                                hintTo: ui.hintTo,
                                selected: ui.selectedSquare,
                                themeColors: themeColors,
                              ),
                            );
                          },
                        ),
                      );
                    },
                  ),
                  if (ui.showCoordinates)
                    _CoordinateOverlay(
                      size: side,
                      square: side / 8,
                      flipped: flipped,
                      themeColors: themeColors,
                    ),
                  IgnorePointer(
                    child: BoardPiecesAnimator(
                      key: ValueKey<String>(piecesKey),
                      board: cells,
                      flipped: flipped,
                      animationsEnabled: ui.moveAnimationsEnabled,
                      squareW: side / 8,
                    ),
                  ),
                  if (ui.puzzleBoardTint > 0.001)
                    Positioned.fill(
                      child: IgnorePointer(
                        child: DecoratedBox(
                          decoration: BoxDecoration(
                            gradient: RadialGradient(
                              center: Alignment.center,
                              radius: 1.05,
                              colors: [
                                (ui.puzzleBoardTintGreen
                                        ? Colors.green
                                        : Colors.red)
                                    .withValues(
                                  alpha: 0.12 + 0.62 * ui.puzzleBoardTint,
                                ),
                                Colors.transparent,
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                ],
              );
            },
          ),
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
    this.serverInvalidSquare,
    this.serverInvalidPulseT = 0,
    this.originalSquare,
    this.clientInvalidSquare,
    this.clientInvalidLit = false,
    this.hintFrom,
    this.hintTo,
    this.selected,
    this.accentSquares = const [],
    required this.themeColors,
  });

  final List<List<String>> board;
  final bool flipped;
  final String? lastFrom;
  final String? lastTo;

  /// Pole s neplatně postavenou figurkou (snapshot `error_state`).
  final String? serverInvalidSquare;

  /// 0–1 z [AnimationController] — červené „pod“ polem.
  final double serverInvalidPulseT;
  final String? originalSquare;

  /// Lokální zpětná vazba po zamítnutém tahu z aplikace.
  final String? clientInvalidSquare;
  final bool clientInvalidLit;
  final String? hintFrom;
  final String? hintTo;
  final String? selected;
  final List<String> accentSquares;
  final BoardStyleColors themeColors;

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
          Paint()..color = isDark ? themeColors.dark : themeColors.light,
        );
      }
    }
    _markSquare(canvas, sq, originalSquare, themeColors.selected);
    _markSquare(canvas, sq, lastFrom, themeColors.lastMove);
    _markSquare(canvas, sq, lastTo, themeColors.lastMove);
    final serverPulseAlpha = 0.28 + 0.62 * serverInvalidPulseT.clamp(0.0, 1.0);
    _markSquare(
      canvas,
      sq,
      serverInvalidSquare,
      themeColors.error.withValues(alpha: serverPulseAlpha),
    );
    final clientAlpha = clientInvalidLit ? 0.86 : 0.2;
    _markSquare(
      canvas,
      sq,
      clientInvalidSquare,
      const Color(0xFFE53935).withValues(alpha: clientAlpha),
    );
    _markSquare(canvas, sq, hintFrom, const Color(0x99FFD54F));
    _markSquare(canvas, sq, hintTo, const Color(0x9976FF7A));
    for (final sqName in accentSquares) {
      _markSquare(canvas, sq, sqName, const Color(0x99673AB7));
    }
    _markSquare(canvas, sq, selected, themeColors.selected);
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

  @override
  bool shouldRepaint(covariant _BoardPainter oldDelegate) {
    return oldDelegate.board != board ||
        oldDelegate.flipped != flipped ||
        oldDelegate.lastFrom != lastFrom ||
        oldDelegate.lastTo != lastTo ||
        oldDelegate.serverInvalidSquare != serverInvalidSquare ||
        oldDelegate.serverInvalidPulseT != serverInvalidPulseT ||
        oldDelegate.originalSquare != originalSquare ||
        oldDelegate.selected != selected ||
        oldDelegate.clientInvalidSquare != clientInvalidSquare ||
        oldDelegate.clientInvalidLit != clientInvalidLit ||
        oldDelegate.hintFrom != hintFrom ||
        oldDelegate.hintTo != hintTo ||
        oldDelegate.accentSquares != accentSquares ||
        oldDelegate.themeColors != themeColors;
  }
}

bool _isDarkSquare(bool flipped, int sr, int sc) {
  final rankNum = flipped ? sr + 1 : 8 - sr;
  final fileIndex = flipped ? 7 - sc : sc;
  return (fileIndex + rankNum).isOdd;
}

Color _coordTextColor(BoardStyleColors t, bool dark) {
  return dark ? const Color(0xE6FFFFFF) : const Color(0x8A000000);
}

/// Parita iOS `ChessBoardView`: řady jen ve sloupci **a**, soubory jen na **1. řadě**, uvnitř buňky, pod figurami.
class _CoordinateOverlay extends StatelessWidget {
  const _CoordinateOverlay({
    required this.size,
    required this.square,
    required this.flipped,
    required this.themeColors,
  });

  final double size;
  final double square;
  final bool flipped;
  final BoardStyleColors themeColors;

  @override
  Widget build(BuildContext context) {
    final aCol = flipped ? 7 : 0;
    final rank1Row = flipped ? 0 : 7;
    const fs = 11.0;

    return IgnorePointer(
      child: SizedBox(
        width: size,
        height: size,
        child: Stack(
          children: [
            for (var sr = 0; sr < 8; sr++)
              Positioned(
                left: aCol * square,
                top: sr * square,
                width: square,
                height: square,
                child: sr == rank1Row
                    ? Stack(
                        children: [
                          Align(
                            alignment: Alignment.topLeft,
                            child: Padding(
                              padding: const EdgeInsets.only(left: 2, top: 1),
                              child: Text(
                                algebraicAt(
                                        flipped: flipped,
                                        screenRow: sr,
                                        screenCol: aCol)
                                    .substring(1),
                                style: TextStyle(
                                  fontSize: fs,
                                  fontWeight: FontWeight.w600,
                                  color: _coordTextColor(
                                    themeColors,
                                    _isDarkSquare(flipped, sr, aCol),
                                  ),
                                ),
                              ),
                            ),
                          ),
                          Align(
                            alignment: Alignment.bottomLeft,
                            child: Padding(
                              padding:
                                  const EdgeInsets.only(left: 2, bottom: 1),
                              child: Text(
                                algebraicAt(
                                        flipped: flipped,
                                        screenRow: sr,
                                        screenCol: aCol)
                                    .substring(0, 1),
                                style: TextStyle(
                                  fontSize: fs,
                                  fontWeight: FontWeight.w600,
                                  color: _coordTextColor(
                                    themeColors,
                                    _isDarkSquare(flipped, sr, aCol),
                                  ),
                                ),
                              ),
                            ),
                          ),
                        ],
                      )
                    : Align(
                        alignment: Alignment.topLeft,
                        child: Padding(
                          padding: const EdgeInsets.only(left: 2, top: 1),
                          child: Text(
                            algebraicAt(
                                    flipped: flipped,
                                    screenRow: sr,
                                    screenCol: aCol)
                                .substring(1),
                            style: TextStyle(
                              fontSize: fs,
                              fontWeight: FontWeight.w600,
                              color: _coordTextColor(
                                themeColors,
                                _isDarkSquare(flipped, sr, aCol),
                              ),
                            ),
                          ),
                        ),
                      ),
              ),
            for (var sc = 0; sc < 8; sc++)
              if (sc != aCol)
                Positioned(
                  left: sc * square,
                  top: rank1Row * square,
                  width: square,
                  height: square,
                  child: Align(
                    alignment: Alignment.bottomLeft,
                    child: Padding(
                      padding: const EdgeInsets.only(left: 2, bottom: 1),
                      child: Text(
                        algebraicAt(
                                flipped: flipped,
                                screenRow: rank1Row,
                                screenCol: sc)
                            .substring(0, 1),
                        style: TextStyle(
                          fontSize: fs,
                          fontWeight: FontWeight.w600,
                          color: _coordTextColor(
                            themeColors,
                            _isDarkSquare(flipped, rank1Row, sc),
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
          ],
        ),
      ),
    );
  }
}

/// Statický náhled pozice z FEN (puzzle, knihovna) — stejné barvy/styl jako na Play.
class FenBoardPreview extends ConsumerWidget {
  const FenBoardPreview({
    super.key,
    this.fen,
    this.cells,
    this.showCoordinates = true,
    this.highlightFrom,
    this.highlightTo,
    this.accentSquares = const [],
  }) : assert(fen != null || cells != null);

  final String? fen;
  final List<List<String>>? cells;
  final bool showCoordinates;
  final String? highlightFrom;
  final String? highlightTo;
  final List<String> accentSquares;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ui = ref.watch(gameUiNotifierProvider);
    final parsed = cells ??
        boardFromPlacementFen((fen ?? '').trim());
    final themeColors = BoardStyleColors.fromRaw(ui.boardStyleRaw);
    final flipped = ui.boardFlipped;

    return RepaintBoundary(
      child: AspectRatio(
        aspectRatio: 1,
        child: LayoutBuilder(
          builder: (context, c) {
            final side = c.maxWidth;
            return ClipRRect(
              borderRadius: BorderRadius.circular(10),
              child: Stack(
                clipBehavior: Clip.hardEdge,
                children: [
                  CustomPaint(
                    size: Size(side, side),
                    painter: _BoardPainter(
                      board: parsed,
                      flipped: flipped,
                      lastFrom: highlightFrom,
                      lastTo: highlightTo,
                      serverInvalidSquare: null,
                      serverInvalidPulseT: 0,
                      originalSquare: null,
                      clientInvalidSquare: null,
                      clientInvalidLit: false,
                      hintFrom: highlightFrom,
                      hintTo: highlightTo,
                      accentSquares: accentSquares,
                      selected: null,
                      themeColors: themeColors,
                    ),
                  ),
                  if (showCoordinates && ui.showCoordinates)
                    _CoordinateOverlay(
                      size: side,
                      square: side / 8,
                      flipped: flipped,
                      themeColors: themeColors,
                    ),
                  BoardPiecesAnimator(
                    board: parsed,
                    flipped: flipped,
                    animationsEnabled: false,
                    squareW: side / 8,
                  ),
                ],
              ),
            );
          },
        ),
      ),
    );
  }
}
