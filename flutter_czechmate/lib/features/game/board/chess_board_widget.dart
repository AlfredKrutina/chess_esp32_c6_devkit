import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/constants/board_styles.dart';
import '../../../core/models/game_snapshot.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/theme/app_motion.dart';
import '../../../core/widgets/app_modal_sheet.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/utils/board_algebraic.dart';
import '../../../core/utils/fen_board_parser.dart';
import '../../connection/board_session_notifier.dart';
import '../state/game_ui_notifier.dart';
import 'board_coordinates.dart';
import 'board_pieces_animator.dart';

/// Fixed figure animator key: changes on move / reposition / sandbox — not on every `stateVersion` from poll (which would break animations).
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

    final themeColors = BoardStyleColors.fromRaw(ui.boardStyleRaw);
    final lastFrom = lastMoveHighlight?.from;
    final lastTo = lastMoveHighlight?.to;
    List<BoardSquareMark> marksFor({
      required double serverAlpha,
      required double clientAlpha,
    }) {
      return [
        if (serverInvalid && err.originalPos != null)
          (square: err.originalPos!, color: themeColors.selected),
        if (lastFrom != null) (square: lastFrom, color: themeColors.lastMove),
        if (lastTo != null) (square: lastTo, color: themeColors.lastMove),
        if (serverInvalid)
          (
            square: err.invalidPos!,
            color: themeColors.error.withValues(alpha: serverAlpha),
          ),
        if (ui.invalidDestinationPulseSquare != null)
          (
            square: ui.invalidDestinationPulseSquare!,
            color: const Color(0xFFE5675A).withValues(alpha: clientAlpha),
          ),
        if (ui.hintFrom != null) (square: ui.hintFrom!, color: _kHintFromColor),
        if (ui.hintTo != null) (square: ui.hintTo!, color: _kHintToColor),
        if (ui.selectedSquare != null)
          (square: ui.selectedSquare!, color: themeColors.selected),
      ];
    }

    // Pulsing marks use their peak alpha for label ink so the label color does
    // not flip back and forth during the animation.
    final labelMarks = marksFor(serverAlpha: 0.9, clientAlpha: 0.86);

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
              final g = BoardGeometry.forSide(
                c.maxWidth,
                clipRadius: _kGameBoardClipRadius,
              );
              return Stack(
                children: [
                  Positioned.fromRect(
                    rect: g.boardRect,
                    child: Stack(
                      children: [
                        Builder(
                          builder: (ctx) {
                            return GestureDetector(
                              onTapUp: (details) async {
                                final box =
                                    ctx.findRenderObject() as RenderBox?;
                                if (box == null) return;
                                final local =
                                    box.globalToLocal(details.globalPosition);
                                final col =
                                    (local.dx / g.square).floor().clamp(0, 7);
                                final row =
                                    (local.dy / g.square).floor().clamp(0, 7);
                                final alg = algebraicAt(
                                  flipped: flipped,
                                  screenRow: row,
                                  screenCol: col,
                                );
                                if (alg.isEmpty) return;
                                await notifier.onSquareTapped(
                                    alg, session, snack);
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
                                    size: Size.square(g.boardSide),
                                    painter: _BoardPainter(
                                      flipped: flipped,
                                      themeColors: themeColors,
                                      marks: marksFor(
                                        serverAlpha: 0.28 +
                                            0.62 * pulseT.clamp(0.0, 1.0),
                                        clientAlpha:
                                            ui.invalidDestinationPulseLit
                                                ? 0.86
                                                : 0.2,
                                      ),
                                    ),
                                  );
                                },
                              ),
                            );
                          },
                        ),
                        // Below labels and pieces so the puzzle flash never
                        // washes out coordinates or piece outlines.
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
                    ),
                  ),
                  BoardCoordinatesLayer(
                    geometry: g,
                    flipped: flipped,
                    themeColors: themeColors,
                    marks: labelMarks,
                  ),
                  Positioned.fromRect(
                    rect: g.boardRect,
                    child: IgnorePointer(
                      child: BoardPiecesAnimator(
                        key: ValueKey<String>(piecesKey),
                        board: cells,
                        flipped: flipped,
                        animationsEnabled:
                            ui.moveAnimationsEnabled && !AppMotion.reduced(context),
                        squareW: g.square,
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

/// Matches the `ClipRRect` around the board in `GameScreen._framedBoard`.
const double _kGameBoardClipRadius = 14;
const double _kPreviewClipRadius = 10;
const Color _kHintFromColor = Color(0x598FB3E8);
const Color _kHintToColor = Color(0xA68FB3E8);
const Color _kAccentColor = Color(0x998FB3E8);

class _BoardPainter extends CustomPainter {
  _BoardPainter({
    required this.flipped,
    required this.themeColors,
    this.marks = const [],
  });

  final bool flipped;
  final BoardStyleColors themeColors;

  /// Painted in list order over the base squares.
  final List<BoardSquareMark> marks;

  @override
  void paint(Canvas canvas, Size size) {
    final sq = size.width / 8;
    // One light fill + dark squares on top: adjacent anti-aliased rects would
    // otherwise leave hairline seams of the background between squares.
    canvas.drawRect(Offset.zero & size, Paint()..color = themeColors.light);
    final darkPaint = Paint()..color = themeColors.dark;
    for (var sr = 0; sr < 8; sr++) {
      for (var sc = 0; sc < 8; sc++) {
        final rankNum = flipped ? sr + 1 : 8 - sr;
        final fileIndex = flipped ? 7 - sc : sc;
        if ((fileIndex + rankNum).isOdd) {
          canvas.drawRect(Rect.fromLTWH(sc * sq, sr * sq, sq, sq), darkPaint);
        }
      }
    }
    for (final m in marks) {
      if (m.square.length < 2) continue;
      final p =
          screenCoordsFromAlgebraic(flipped: flipped, algebraic: m.square);
      if (p.$1 < 0) continue;
      canvas.drawRect(
        Rect.fromLTWH(p.$2 * sq, p.$1 * sq, sq, sq),
        Paint()..color = m.color,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _BoardPainter oldDelegate) {
    return oldDelegate.flipped != flipped ||
        oldDelegate.themeColors != themeColors ||
        !listEquals(oldDelegate.marks, marks);
  }
}

/// Static view of position from FEN (puzzle, library) — same colors/style as on Play.
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

  /// Ignored: coordinates are always drawn. Kept so existing callers compile.
  final bool showCoordinates;
  final String? highlightFrom;
  final String? highlightTo;
  final List<String> accentSquares;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ui = ref.watch(gameUiNotifierProvider);
    final parsed = cells ?? boardFromPlacementFen((fen ?? '').trim());
    final themeColors = BoardStyleColors.fromRaw(ui.boardStyleRaw);
    final flipped = ui.boardFlipped;
    final marks = <BoardSquareMark>[
      if (highlightFrom != null) ...[
        (square: highlightFrom!, color: themeColors.lastMove),
        (square: highlightFrom!, color: _kHintFromColor),
      ],
      if (highlightTo != null) ...[
        (square: highlightTo!, color: themeColors.lastMove),
        (square: highlightTo!, color: _kHintToColor),
      ],
      for (final s in accentSquares) (square: s, color: _kAccentColor),
    ];

    return RepaintBoundary(
      child: AspectRatio(
        aspectRatio: 1,
        child: LayoutBuilder(
          builder: (context, c) {
            final g = BoardGeometry.forSide(
              c.maxWidth,
              clipRadius: _kPreviewClipRadius,
            );
            return Stack(
              children: [
                Positioned.fromRect(
                  rect: g.boardRect,
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(_kPreviewClipRadius),
                    child: CustomPaint(
                      size: Size.square(g.boardSide),
                      painter: _BoardPainter(
                        flipped: flipped,
                        themeColors: themeColors,
                        marks: marks,
                      ),
                    ),
                  ),
                ),
                BoardCoordinatesLayer(
                  geometry: g,
                  flipped: flipped,
                  themeColors: themeColors,
                  marks: marks,
                ),
                Positioned.fromRect(
                  rect: g.boardRect,
                  child: BoardPiecesAnimator(
                    board: parsed,
                    flipped: flipped,
                    animationsEnabled: false,
                    squareW: g.square,
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
