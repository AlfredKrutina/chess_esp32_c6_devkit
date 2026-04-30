import 'package:flutter/material.dart';

/// Stejné názvy a PNG jako iOS `ChessPieceGlyph.assetImageName` / ESP `piece_assets`.
String? piecePngAssetPath(String fenGlyph) {
  if (fenGlyph.isEmpty) return null;
  final c = fenGlyph.substring(0, 1);
  switch (c) {
    case 'K':
      return 'assets/pieces/PieceWhiteKing.png';
    case 'Q':
      return 'assets/pieces/PieceWhiteQueen.png';
    case 'R':
      return 'assets/pieces/PieceWhiteRook.png';
    case 'B':
      return 'assets/pieces/PieceWhiteBishop.png';
    case 'N':
      return 'assets/pieces/PieceWhiteKnight.png';
    case 'P':
      return 'assets/pieces/PieceWhitePawn.png';
    case 'k':
      return 'assets/pieces/PieceBlackKing.png';
    case 'q':
      return 'assets/pieces/PieceBlackQueen.png';
    case 'r':
      return 'assets/pieces/PieceBlackRook.png';
    case 'b':
      return 'assets/pieces/PieceBlackBishop.png';
    case 'n':
      return 'assets/pieces/PieceBlackKnight.png';
    case 'p':
      return 'assets/pieces/PieceBlackPawn.png';
    default:
      return null;
  }
}

bool pieceIsWhiteFromFen(String fenGlyph) {
  if (fenGlyph.isEmpty) return true;
  final u = fenGlyph.codeUnitAt(0);
  return u >= 65 && u <= 90;
}

/// Spodní pravý roh (h1) je světlé pole — shodně s běžnou šachovnicí a iOS stínem.
bool isLightAlgebraicSquare(String algebraic) {
  if (algebraic.length < 2) return true;
  final file = algebraic.codeUnitAt(0) - 97;
  final rank = int.tryParse(algebraic.substring(1)) ?? 1;
  if (file < 0 || file > 7) return true;
  return (file + rank) % 2 == 0;
}

String _unicodePiece(String p) {
  switch (p) {
    case 'K':
      return '\u2654';
    case 'Q':
      return '\u2655';
    case 'R':
      return '\u2656';
    case 'B':
      return '\u2657';
    case 'N':
      return '\u2658';
    case 'P':
      return '\u2659';
    case 'k':
      return '\u265A';
    case 'q':
      return '\u265B';
    case 'r':
      return '\u265C';
    case 'b':
      return '\u265D';
    case 'n':
      return '\u265E';
    case 'p':
      return '\u265F';
    default:
      return p;
  }
}

/// Parita s iOS `ChessPieceArtView`: PNG Staunton + stín podle barvy figury a pole.
class ChessPieceArt extends StatelessWidget {
  const ChessPieceArt({
    super.key,
    required this.fenGlyph,
    required this.squareSide,
    required this.algebraic,
  });

  final String fenGlyph;
  final double squareSide;
  final String algebraic;

  @override
  Widget build(BuildContext context) {
    final path = piecePngAssetPath(fenGlyph);
    if (path == null) {
      final ch = fenGlyph.isEmpty ? '' : fenGlyph.substring(0, 1);
      final isWhite = pieceIsWhiteFromFen(fenGlyph);
      return Center(
        child: Text(
          _unicodePiece(ch),
          style: TextStyle(
            fontSize: squareSide * 0.75,
            color: isWhite ? Colors.black87 : Colors.blueGrey,
            height: 1.0,
          ),
        ),
      );
    }

    final frame = squareSide * 0.92;

    return Center(
      child: SizedBox(
        width: frame,
        height: frame,
        child: Image.asset(
          path,
          fit: BoxFit.contain,
          filterQuality: FilterQuality.high,
          gaplessPlayback: true,
          isAntiAlias: true,
        ),
      ),
    );
  }
}
