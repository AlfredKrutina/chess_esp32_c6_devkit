import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/core/services/stockfish_api_client.dart';

void main() {
  group('StockfishJsonParsers.fromChessApiCompatible', () {
    test('parses flat move + eval', () {
      final m = StockfishJsonParsers.fromChessApiCompatible(jsonDecode(r'''
{"move":"e2e4","eval":0.4,"from":"e2","to":"e4"}
''') as Map<String, dynamic>);
      expect(m.from, 'e2');
      expect(m.to, 'e4');
      expect(m.evalPawns, closeTo(0.4, 0.001));
    });

    test('parses nested data + centipawns string', () {
      final m = StockfishJsonParsers.fromChessApiCompatible(jsonDecode(r'''
{"data":{"from":"g1","to":"f3","centipawns":"40"}}
''') as Map<String, dynamic>);
      expect(m.from, 'g1');
      expect(m.to, 'f3');
      expect(m.evalPawns, closeTo(0.4, 0.001));
    });

    test('throws without move', () {
      expect(
        () => StockfishJsonParsers.fromChessApiCompatible(<String, dynamic>{}),
        throwsA(isA<StockfishApiException>()),
      );
    });
  });

  group('StockfishJsonParsers.fromStockfishOnline', () {
    test('parses bestmove line', () {
      final m = StockfishJsonParsers.fromStockfishOnline(jsonDecode(r'''
{"success":true,"evaluation":0.47,"mate":null,"bestmove":"bestmove e2e4 ponder c7c5","continuation":"e2e4 c7c5"}
''') as Map<String, dynamic>);
      expect(m.from, 'e2');
      expect(m.to, 'e4');
      expect(m.evalPawns, closeTo(0.47, 0.001));
    });

    test('throws when success false', () {
      expect(
        () => StockfishJsonParsers.fromStockfishOnline(jsonDecode(r'''
{"success":false,"error":"Invalid FEN"}
''') as Map<String, dynamic>),
        throwsA(predicate((Object e) =>
            e is StockfishApiException && e.message.contains('Invalid FEN'))),
      );
    });
  });

  group('StockfishJsonParsers.fromLichessCloudEval', () {
    test('parses pvs moves + cp', () {
      final m = StockfishJsonParsers.fromLichessCloudEval(jsonDecode(r'''
{"fen":"...","depth":75,"pvs":[{"moves":"e2e4 e7e5","cp":19}]}
''') as Map<String, dynamic>);
      expect(m.from, 'e2');
      expect(m.to, 'e4');
      expect(m.evalPawns, closeTo(0.19, 0.001));
    });

    test('throws on error field', () {
      expect(
        () => StockfishJsonParsers.fromLichessCloudEval(jsonDecode(r'''
{"error":"No cloud evaluation available for that position"}
''') as Map<String, dynamic>),
        throwsA(isA<StockfishApiException>()),
      );
    });
  });
}
