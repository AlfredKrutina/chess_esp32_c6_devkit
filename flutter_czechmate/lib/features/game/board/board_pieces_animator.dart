import 'package:flutter/material.dart';

import '../../../core/utils/board_algebraic.dart';
import 'chess_piece_art.dart';

class _PieceState {
  _PieceState({
    required this.id,
    required this.glyph,
    required this.algebraic,
  });

  final String id;
  String glyph;
  String algebraic;

  _PieceState copy() {
    return _PieceState(id: id, glyph: glyph, algebraic: algebraic);
  }
}

class BoardPiecesAnimator extends StatefulWidget {
  const BoardPiecesAnimator({
    super.key,
    required this.board,
    required this.flipped,
    required this.animationsEnabled,
    required this.squareW,
  });

  final List<List<String>> board;
  final bool flipped;
  final bool animationsEnabled;
  final double squareW;

  @override
  State<BoardPiecesAnimator> createState() => _BoardPiecesAnimatorState();
}

class _BoardPiecesAnimatorState extends State<BoardPiecesAnimator> {
  final List<_PieceState> _pieces = [];
  int _idSeq = 0;

  @override
  void initState() {
    super.initState();
    _buildInitialPieces();
  }

  @override
  void didUpdateWidget(covariant BoardPiecesAnimator oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (_hasBoardChanged(oldWidget.board, widget.board)) {
      _diffAndUpdatePieces();
    }
  }

  bool _hasBoardChanged(List<List<String>> oldB, List<List<String>> newB) {
    if (oldB.length != newB.length) return true;
    for (int r = 0; r < oldB.length; r++) {
      if (oldB[r].length != newB[r].length) return true;
      for (int c = 0; c < oldB[r].length; c++) {
        if (oldB[r][c] != newB[r][c]) return true;
      }
    }
    return false;
  }

  void _buildInitialPieces() {
    _pieces.clear();
    for (int r = 0; r < 8; r++) {
      for (int c = 0; c < 8; c++) {
        final p = widget.board[r][c].trim();
        if (p.isNotEmpty && p != '1') {
          _pieces.add(_PieceState(
            id: 'p_${_idSeq++}',
            glyph: p,
            algebraic: _algebraic(r, c),
          ));
        }
      }
    }
  }

  void _diffAndUpdatePieces() {
    if (!widget.animationsEnabled) {
      _buildInitialPieces();
      return;
    }

    // 1. Gather what is currently on the new board
    final newMap = <String, String>{}; // algebraic -> glyph
    for (int r = 0; r < 8; r++) {
      for (int c = 0; c < 8; c++) {
        final p = widget.board[r][c].trim();
        if (p.isNotEmpty && p != '1') {
          newMap[_algebraic(r, c)] = p;
        }
      }
    }

    // 2. Exact matches
    final unmatchedNew = <String, String>{};
    final unmatchedExisting = <_PieceState>[];

    for (final e in newMap.entries) {
      final alg = e.key;
      final gl = e.value;
      final existingPiece = _pieces.firstWhere(
        (p) => p.algebraic == alg,
        orElse: () => _PieceState(id: '', glyph: '', algebraic: ''),
      );
      if (existingPiece.id.isNotEmpty && existingPiece.glyph == gl) {
        // Matched exactly
      } else {
        unmatchedNew[alg] = gl;
      }
    }

    for (final p in _pieces) {
      if (!newMap.containsKey(p.algebraic) || newMap[p.algebraic] != p.glyph) {
        unmatchedExisting.add(p);
      }
    }

    // 3. Find movements
    List<_PieceState> nextPieces =
        _pieces.where((p) => !unmatchedExisting.contains(p)).toList();
    final toRemove = <String>[]; // keys from unmatchedNew that were resolved

    unmatchedNew.forEach((newAlg, newGl) {
      // Find an existing piece that is missing from the board and has the same glyph
      // OR a piece that could be promoted (pawn to queen). For simplicity, just exact glyph first.
      final srcIdx = unmatchedExisting.indexWhere((p) => p.glyph == newGl);
      if (srcIdx != -1) {
        final src = unmatchedExisting.removeAt(srcIdx);
        src.algebraic = newAlg;
        nextPieces.add(src);
        toRemove.add(newAlg);
      } else {
        // Also check for promotion: old is pawn, new is something else...
        // very naive: if there is any unmatched pawn
        final pwnIdx =
            unmatchedExisting.indexWhere((p) => p.glyph.toLowerCase() == 'p');
        if (pwnIdx != -1) {
          final src = unmatchedExisting.removeAt(pwnIdx);
          src.algebraic = newAlg;
          src.glyph = newGl; // transformed
          nextPieces.add(src);
          toRemove.add(newAlg);
        }
      }
    });

    for (final resolved in toRemove) {
      unmatchedNew.remove(resolved);
    }

    // 4. Any remaining in unmatchedNew are truly new pieces (sandbox setup etc.)
    unmatchedNew.forEach((alg, gl) {
      nextPieces.add(_PieceState(
        id: 'new_${_idSeq++}',
        glyph: gl,
        algebraic: alg,
      ));
    });

    setState(() {
      _pieces.clear();
      _pieces.addAll(nextPieces);
    });
  }

  String _algebraic(int r, int c) =>
      algebraicAt(flipped: false, screenRow: r, screenCol: c);

  @override
  Widget build(BuildContext context) {
    return Stack(
      clipBehavior: Clip.none,
      children: _pieces.map((p) {
        final pt = screenCoordsFromAlgebraic(
            flipped: widget.flipped, algebraic: p.algebraic);
        if (pt.$1 < 0) return const SizedBox.shrink();

        final top = pt.$1 * widget.squareW;
        final left = pt.$2 * widget.squareW;

        final w = widget.animationsEnabled
            ? AnimatedPositioned(
                key: ValueKey(p.id),
                duration: const Duration(milliseconds: 250),
                curve: Curves.easeInOut,
                top: top,
                left: left,
                width: widget.squareW,
                height: widget.squareW,
                child: _buildPiece(p),
              )
            : Positioned(
                key: ValueKey(p.id),
                top: top,
                left: left,
                width: widget.squareW,
                height: widget.squareW,
                child: _buildPiece(p),
              );

        return w;
      }).toList(),
    );
  }

  Widget _buildPiece(_PieceState p) {
    return ChessPieceArt(
      fenGlyph: p.glyph,
      squareSide: widget.squareW,
      algebraic: p.algebraic,
    );
  }
}
