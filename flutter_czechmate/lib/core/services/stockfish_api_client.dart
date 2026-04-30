import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

import '../constants/app_environment.dart';

/// Parity with `StockfishAPIClient.swift` — primary backend: POST JSON `{"fen","depth"}` (chess-api style).
/// Fallbacks (automatic): GET stockfish.online, then GET Lichess cloud-eval (cached positions only).
class StockfishBestMove {
  StockfishBestMove({
    required this.from,
    required this.to,
    this.evalPawns,
    this.text,
  });

  final String from;
  final String to;
  final double? evalPawns;
  final String? text;
}

class StockfishApiException implements Exception {
  StockfishApiException(this.message, {this.statusCode});
  final String message;
  final int? statusCode;
  @override
  String toString() => 'StockfishApiException($statusCode): $message';
}

/// Pure JSON adapters — one shape per backend; throws [StockfishApiException] if unusable.
abstract final class StockfishJsonParsers {
  static double? _doubleFrom(dynamic v) {
    if (v == null) return null;
    if (v is num) return v.toDouble();
    if (v is String) {
      final t = v.trim();
      if (t.isEmpty) return null;
      return double.tryParse(t.replaceAll(',', '.'));
    }
    return null;
  }

  static double? _parseEval(Map<String, dynamic> obj) {
    return _doubleFrom(obj['eval']) ??
        (_doubleFrom(obj['centipawns']) != null ? _doubleFrom(obj['centipawns'])! / 100.0 : null) ??
        (_doubleFrom(obj['cp']) != null ? _doubleFrom(obj['cp'])! / 100.0 : null) ??
        _doubleFrom(obj['score']) ??
        (obj['evaluation'] is Map<String, dynamic>
            ? _parseEval(Map<String, dynamic>.from(obj['evaluation'] as Map))
            : _doubleFrom(obj['evaluation']));
  }

  static double? _nestedResultEval(Map<String, dynamic> obj) {
    final r = obj['result'];
    if (r is Map<String, dynamic>) return _parseEval(Map<String, dynamic>.from(r));
    return null;
  }

  /// chess-api.com and compatible mirrors: flat or nested `data` / `result` / `bestMove`.
  static StockfishBestMove fromChessApiCompatible(Map<String, dynamic> obj) {
    final dataObj = (obj['data'] as Map<String, dynamic>?) ??
        (obj['result'] as Map<String, dynamic>?) ??
        (obj['bestMove'] as Map<String, dynamic>?) ??
        obj;

    String? from = dataObj['from'] as String?;
    String? to = dataObj['to'] as String?;
    final move = dataObj['move'] as String?;
    if ((from == null || to == null) && move != null) {
      final m = move.toLowerCase().replaceAll(RegExp(r'[^a-z0-9]'), '');
      if (m.length >= 4) {
        from = from ?? m.substring(0, 2);
        to = to ?? m.substring(2, 4);
      }
    }
    final f = from?.toLowerCase();
    final t = to?.toLowerCase();
    if (f == null || t == null || f.length != 2 || t.length != 2) {
      throw StockfishApiException('Response has no move (from/to or move)');
    }
    final ev =
        _parseEval(dataObj) ?? _parseEval(obj) ?? _nestedResultEval(dataObj) ?? _nestedResultEval(obj);
    return StockfishBestMove(
      from: f,
      to: t,
      evalPawns: ev,
      text: dataObj['text'] as String?,
    );
  }

  /// stockfish.online `api/s/v2.php` — `bestmove` is engine line, e.g. `bestmove e2e4 ponder e7e5`.
  static StockfishBestMove fromStockfishOnline(Map<String, dynamic> obj) {
    if (obj['success'] != true) {
      final err = obj['error'];
      throw StockfishApiException(err is String ? err : 'stockfish.online: success=false');
    }
    final bmRaw = obj['bestmove'];
    if (bmRaw is! String || bmRaw.trim().isEmpty) {
      throw StockfishApiException('stockfish.online: missing bestmove');
    }
    final uci = _uciFromStockfishOnlineBestmoveLine(bmRaw);
    if (uci == null) {
      throw StockfishApiException('stockfish.online: could not parse UCI from "$bmRaw"');
    }
    final split = _splitUciFromTo(uci);
    if (split == null) {
      throw StockfishApiException('stockfish.online: invalid UCI "$uci"');
    }
    final evalPawns = _doubleFrom(obj['evaluation']);
    final cont = obj['continuation'];
    return StockfishBestMove(
      from: split.$1,
      to: split.$2,
      evalPawns: evalPawns,
      text: cont is String ? cont : null,
    );
  }

  static String? _uciFromStockfishOnlineBestmoveLine(String line) {
    final parts = line.trim().split(RegExp(r'\s+'));
    for (var i = 0; i < parts.length; i++) {
      if (parts[i] == 'bestmove' && i + 1 < parts.length) {
        final cand = parts[i + 1].toLowerCase();
        if (cand != '(none)' && cand != 'none' && _looksLikeUci(cand)) return cand;
        return null;
      }
    }
    final direct = RegExp(r'\b([a-h][1-8][a-h][1-8][qrbn]?)\b').firstMatch(line.toLowerCase());
    return direct?.group(1);
  }

  static bool _looksLikeUci(String s) => RegExp(r'^[a-h][1-8][a-h][1-8][qrbn]?$').hasMatch(s);

  /// First square = from, rest = to (handles promotion e7e8q).
  static (String, String)? _splitUciFromTo(String uci) {
    if (uci.length < 4) return null;
    final from = uci.substring(0, 2);
    final to = uci.substring(2, 4);
    if (!_isSq(from) || !_isSq(to)) return null;
    return (from, to);
  }

  static bool _isSq(String s) =>
      s.length == 2 && RegExp(r'^[a-h][1-8]$').hasMatch(s);

  /// Lichess GET `/api/cloud-eval` — `pvs[].moves` space-separated UCI, optional `cp` / `mate`.
  static StockfishBestMove fromLichessCloudEval(Map<String, dynamic> obj) {
    if (obj['error'] != null) {
      final e = obj['error'];
      throw StockfishApiException(e is String ? e : 'lichess cloud-eval error');
    }
    final pvs = obj['pvs'];
    if (pvs is! List || pvs.isEmpty) {
      throw StockfishApiException('lichess cloud-eval: no pvs');
    }
    final first = pvs.first;
    if (first is! Map<String, dynamic>) {
      throw StockfishApiException('lichess cloud-eval: invalid pv');
    }
    final movesRaw = first['moves'];
    if (movesRaw is! String || movesRaw.trim().isEmpty) {
      throw StockfishApiException('lichess cloud-eval: empty moves');
    }
    final firstMove = movesRaw.trim().split(RegExp(r'\s+')).first.toLowerCase();
    final split = _splitUciFromTo(firstMove);
    if (split == null) {
      throw StockfishApiException('lichess cloud-eval: bad first move "$firstMove"');
    }
    final evalPawns =
        _doubleFrom(first['cp']) != null ? _doubleFrom(first['cp'])! / 100.0 : null;
    return StockfishBestMove(
      from: split.$1,
      to: split.$2,
      evalPawns: evalPawns,
      text: null,
    );
  }
}

/// Built-in fallback GET endpoints (chess-api style POST uses [baseUrl] from prefs).
abstract final class StockfishFallbackEndpoints {
  static final Uri stockfishOnlineV2 =
      Uri.parse('https://stockfish.online/api/s/v2.php');
  static final Uri lichessCloudEval =
      Uri.parse('https://lichess.org/api/cloud-eval');
}

class StockfishApiClient {
  StockfishApiClient({http.Client? httpClient, this.baseUrl = 'https://chess-api.com/v1'})
      : _client = httpClient ?? http.Client();

  final http.Client _client;
  final String baseUrl;

  Future<StockfishBestMove> fetchBestMove(String fen, {int depth = 10}) async {
    final d = depth.clamp(1, 18);
    final failures = <String>[];

    try {
      return await _postChessApiCompatible(fen, d);
    } catch (e, st) {
      failures.add(_formatFailure('primary ($baseUrl)', e));
      if (AppEnvironment.staging) {
        debugPrint('[staging][stockfish] primary failed: $e\n$st');
      }
    }

    try {
      return await _getStockfishOnline(fen, d);
    } catch (e, st) {
      failures.add(_formatFailure('stockfish.online', e));
      if (AppEnvironment.staging) {
        debugPrint('[staging][stockfish] fallback stockfish.online failed: $e\n$st');
      }
    }

    try {
      return await _getLichessCloudEval(fen);
    } catch (e, st) {
      failures.add(_formatFailure('lichess cloud-eval', e));
      if (AppEnvironment.staging) {
        debugPrint('[staging][stockfish] fallback lichess failed: $e\n$st');
      }
    }

    throw StockfishApiException(
      'All Stockfish backends failed:\n${failures.join('\n')}',
    );
  }

  String _formatFailure(String label, Object e) {
    if (e is StockfishApiException) {
      return '$label: ${e.message}${e.statusCode != null ? ' (HTTP ${e.statusCode})' : ''}';
    }
    return '$label: $e';
  }

  Future<StockfishBestMove> _postChessApiCompatible(String fen, int depth) async {
    final trimmed = baseUrl.trim();
    if (trimmed.isEmpty) {
      throw StockfishApiException('Primary Stockfish URL is empty');
    }
    late final Uri uri;
    try {
      uri = Uri.parse(trimmed);
    } catch (e) {
      throw StockfishApiException('Invalid primary URL: $e');
    }
    if (!uri.hasScheme || uri.host.isEmpty) {
      throw StockfishApiException('Primary URL must include scheme and host');
    }

    final res = await _client
        .post(
          uri,
          headers: const {'Content-Type': 'application/json', 'Accept': 'application/json'},
          body: jsonEncode({'fen': fen, 'depth': depth}),
        )
        .timeout(const Duration(seconds: 20));

    if (res.statusCode < 200 || res.statusCode >= 300) {
      throw StockfishApiException('HTTP ${res.statusCode}', statusCode: res.statusCode);
    }
    final decoded = jsonDecode(utf8.decode(res.bodyBytes));
    if (decoded is! Map<String, dynamic>) {
      throw StockfishApiException('Invalid JSON (expected object)');
    }
    return StockfishJsonParsers.fromChessApiCompatible(decoded);
  }

  /// Documented limit depth &lt; 16.
  Future<StockfishBestMove> _getStockfishOnline(String fen, int depth) async {
    final d = depth.clamp(1, 15);
    final uri = StockfishFallbackEndpoints.stockfishOnlineV2.replace(
      queryParameters: <String, String>{
        'fen': fen,
        'depth': '$d',
      },
    );
    final res = await _client
        .get(uri, headers: const {'Accept': 'application/json'})
        .timeout(const Duration(seconds: 25));
    if (res.statusCode < 200 || res.statusCode >= 300) {
      throw StockfishApiException('HTTP ${res.statusCode}', statusCode: res.statusCode);
    }
    final decoded = jsonDecode(utf8.decode(res.bodyBytes));
    if (decoded is! Map<String, dynamic>) {
      throw StockfishApiException('Invalid JSON (expected object)');
    }
    return StockfishJsonParsers.fromStockfishOnline(decoded);
  }

  Future<StockfishBestMove> _getLichessCloudEval(String fen) async {
    final uri = StockfishFallbackEndpoints.lichessCloudEval.replace(
      queryParameters: <String, String>{
        'fen': fen,
        'multiPv': '1',
      },
    );
    final res = await _client
        .get(uri, headers: const {'Accept': 'application/json'})
        .timeout(const Duration(seconds: 15));
    if (res.statusCode == 404) {
      throw StockfishApiException('No cached cloud evaluation for this position',
          statusCode: 404);
    }
    if (res.statusCode < 200 || res.statusCode >= 300) {
      throw StockfishApiException('HTTP ${res.statusCode}', statusCode: res.statusCode);
    }
    final decoded = jsonDecode(utf8.decode(res.bodyBytes));
    if (decoded is! Map<String, dynamic>) {
      throw StockfishApiException('Invalid JSON (expected object)');
    }
    return StockfishJsonParsers.fromLichessCloudEval(decoded);
  }

  /// Same as iOS `fetchPrimaryAndSecondary` — lower depth may suggest a different best move.
  Future<({StockfishBestMove primary, StockfishBestMove? secondary})> fetchPrimaryAndSecondary(
    String fen, {
    required int depth,
  }) async {
    final primary = await fetchBestMove(fen, depth: depth);
    final d2 = (depth - 3).clamp(1, 18);
    if (d2 == depth) {
      return (primary: primary, secondary: null);
    }
    try {
      final alt = await fetchBestMove(fen, depth: d2);
      if (alt.from == primary.from && alt.to == primary.to) {
        return (primary: primary, secondary: null);
      }
      return (primary: primary, secondary: alt);
    } catch (_) {
      return (primary: primary, secondary: null);
    }
  }

  Future<void> close() async {
    _client.close();
  }
}
