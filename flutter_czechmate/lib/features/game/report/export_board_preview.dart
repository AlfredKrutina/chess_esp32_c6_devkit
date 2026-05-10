import 'package:flutter/material.dart';

import '../board/chess_piece_art.dart';

/// Static board raster for export (no gestures). [cells] row 0 = rank 8.
class ExportBoardPreview extends StatelessWidget {
  const ExportBoardPreview({
    super.key,
    required this.cells,
    this.flip = false,
    this.lightSquare = const Color(0xFFE8D4B8),
    this.darkSquare = const Color(0xFFB58863),
    this.padding = 6,
  });

  final List<List<String>> cells;
  final bool flip;
  final Color lightSquare;
  final Color darkSquare;
  final double padding;

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 1,
      child: Padding(
        padding: EdgeInsets.all(padding),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(10),
          child: LayoutBuilder(
            builder: (context, c) {
              final side = c.maxWidth;
              return SizedBox(
                width: side,
                height: side,
                child: Stack(
                  children: [
                    Column(
                      children: List.generate(8, (displayRow) {
                        final rankIndex = flip ? displayRow : (7 - displayRow);
                        return Expanded(
                          child: Row(
                            children: List.generate(8, (displayCol) {
                              final fileIndex =
                                  flip ? (7 - displayCol) : displayCol;
                              final cell = cells[rankIndex][fileIndex];
                              final light = (fileIndex + rankIndex) % 2 == 0;
                              return Expanded(
                                child: DecoratedBox(
                                  decoration: BoxDecoration(
                                    color: light ? lightSquare : darkSquare,
                                  ),
                                  child: cell.trim().isEmpty
                                      ? const SizedBox.expand()
                                      : LayoutBuilder(
                                          builder: (_, cellConstraints) {
                                            final psz = cellConstraints
                                                    .biggest.shortestSide *
                                                0.92;
                                            final fileChar =
                                                String.fromCharCode(
                                                    97 + fileIndex);
                                            final rankNum = 8 - rankIndex;
                                            return ChessPieceArt(
                                              fenGlyph: cell.trim(),
                                              squareSide: psz,
                                              algebraic: '$fileChar$rankNum',
                                            );
                                          },
                                        ),
                                ),
                              );
                            }),
                          ),
                        );
                      }),
                    ),
                  ],
                ),
              );
            },
          ),
        ),
      ),
    );
  }
}
