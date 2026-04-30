/// Mapování „e2“ → indexy do `board[][]` (řádek 0 = rank 8, řádek 7 = rank 1).
class FirmwareSquareNotation {
  static (int row, int col)? indicesFromNotation(String notation) {
    final t = notation.trim().toLowerCase();
    if (t.length < 2) return null;
    
    final file = t.codeUnitAt(0);
    final rankChar = t.substring(1);
    final rank = int.tryParse(rankChar);
    
    if (rank == null || rank < 1 || rank > 8) return null;
    
    final col = file - 'a'.codeUnitAt(0);
    if (col < 0 || col >= 8) return null;
    
    final row = 8 - rank;
    if (row < 0 || row >= 8) return null;
    
    return (row, col);
  }
}
