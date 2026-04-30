import 'dart:convert';

import 'package:http/http.dart' as http;

/// Parity with `PuzzleService.swift` — Lichess daily puzzle.
class DailyPuzzle {
  const DailyPuzzle({
    required this.id,
    required this.fen,
    this.rating,
    this.solutionMoves = const [],
    this.themes = const [],
  });

  final String id;
  final String fen;
  final int? rating;
  final List<String> solutionMoves;
  final List<String> themes;
}

class PuzzleService {
  static const _dailyUrl = 'https://lichess.org/api/puzzle/daily';

  static Future<DailyPuzzle> fetchDaily({http.Client? client}) async {
    final c = client ?? http.Client();
    try {
      final req = Uri.parse(_dailyUrl);
      final res = await c.get(req, headers: {'Accept': 'application/json'}).timeout(const Duration(seconds: 25));
      if (res.statusCode < 200 || res.statusCode >= 300) {
        throw Exception('HTTP ${res.statusCode}');
      }
      final map = jsonDecode(utf8.decode(res.bodyBytes));
      if (map is! Map<String, dynamic>) throw Exception('Invalid JSON response');
      final puzzle = map['puzzle'];
      if (puzzle is! Map<String, dynamic>) {
        throw Exception('Daily puzzle payload is missing the puzzle object.');
      }
      final fen = (puzzle['fen'] as String?)?.trim();
      if (fen == null || fen.isEmpty) {
        throw Exception('Daily puzzle is missing a valid FEN position.');
      }
      final sol = (puzzle['solution'] as List<dynamic>?)?.map((e) => '$e').toList() ?? const <String>[];
      final themes = (puzzle['themes'] as List<dynamic>?)?.map((e) => '$e').toList() ?? const <String>[];
      return DailyPuzzle(
        id: puzzle['id'] as String? ?? 'daily',
        fen: fen,
        rating: (puzzle['rating'] as num?)?.toInt(),
        solutionMoves: sol,
        themes: themes,
      );
    } finally {
      if (client == null) {
        c.close();
      }
    }
  }
}
