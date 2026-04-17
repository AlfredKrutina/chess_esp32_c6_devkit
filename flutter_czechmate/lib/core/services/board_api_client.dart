import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/board_timer_state.dart';
import '../models/game_snapshot.dart';
import '../utils/game_snapshot_codec.dart';
import 'board_api_exception.dart';

/// REST klient ESP — `ChessboardAPIClient.swift` + `ChessboardAPIClient+Extras.swift`.
class BoardApiClient {
  BoardApiClient({http.Client? httpClient, Duration? timeout})
      : _client = httpClient ?? http.Client(),
        _timeout = timeout ?? const Duration(seconds: 35);

  final http.Client _client;
  final Duration _timeout;

  Uri _base(String baseUrl) {
    var u = baseUrl.trim();
    if (u.endsWith('/')) u = u.substring(0, u.length - 1);
    return Uri.parse(u);
  }

  Uri _api(String baseUrl, String path) => _base(baseUrl).resolve(path);

  Future<void> close() async => _client.close();

  Future<GameSnapshot> fetchSnapshot(String baseUrl) async {
    final s = await fetchSnapshotIfChanged(baseUrl, ifNoneMatch: null);
    if (s == null) {
      throw BoardApiException('Prázdná odpověď snapshot', statusCode: 304);
    }
    return s;
  }

  /// `nil` / null při 304 Not Modified.
  Future<GameSnapshot?> fetchSnapshotIfChanged(
    String baseUrl, {
    String? ifNoneMatch,
  }) async {
    final uri = _api(baseUrl, 'api/game/snapshot');
    final req = http.Request('GET', uri);
    if (ifNoneMatch != null && ifNoneMatch.isNotEmpty) {
      req.headers['If-None-Match'] = ifNoneMatch;
    }
    final streamed = await _client.send(req).timeout(_timeout);
    final body = await streamed.stream.toBytes();
    if (streamed.statusCode == 304) return null;
    _validate(streamed.statusCode, body, treat403WebLock: false);
    return GameSnapshotCodec.decodeBytes(body);
  }

  Future<BoardTimerState> fetchTimerState(String baseUrl) async {
    final uri = _api(baseUrl, 'api/timer');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = jsonDecode(utf8.decode(res.bodyBytes)) as Map<String, dynamic>;
    return BoardTimerState.fromJson(map);
  }

  Future<void> postTimerConfig(
    String baseUrl, {
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) async {
    final uri = _api(baseUrl, 'api/timer/config');
    final body = type == 14 && customMinutes != null && customIncrement != null
        ? jsonEncode({
            'type': 14,
            'custom_minutes': customMinutes,
            'custom_increment': customIncrement,
          })
        : jsonEncode({'type': type});
    final res = await _client
        .post(uri, headers: _jsonHeaders, body: body)
        .timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postTimerPause(String baseUrl) =>
      _postEmpty(baseUrl, 'api/timer/pause', webLock: true);
  Future<void> postTimerResume(String baseUrl) =>
      _postEmpty(baseUrl, 'api/timer/resume', webLock: true);
  Future<void> postTimerReset(String baseUrl) =>
      _postEmpty(baseUrl, 'api/timer/reset', webLock: true);

  Future<void> postNewGame(String baseUrl, {String? fen}) async {
    final uri = _api(baseUrl, 'api/game/new');
    final res = fen != null
        ? await _client
            .post(
              uri,
              headers: _jsonHeaders,
              body: jsonEncode({'fen': fen}),
            )
            .timeout(_timeout)
        : await _client.post(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postGameMove(
    String baseUrl, {
    required String from,
    required String to,
    String? promotion,
  }) async {
    final uri = _api(baseUrl, 'api/move');
    final map = <String, dynamic>{
      'from': from.toLowerCase(),
      'to': to.toLowerCase(),
    };
    if (promotion != null && promotion.isNotEmpty) {
      map['promotion'] = promotion.toUpperCase();
    }
    final res = await _client
        .post(uri, headers: _jsonHeaders, body: jsonEncode(map))
        .timeout(_timeout);
    if (res.statusCode == 403) throw BoardApiException.webLocked();
    if (res.statusCode == 400) {
      final msg = _extractMessage(res.bodyBytes);
      throw BoardApiException('Neplatný tah', statusCode: 400, detail: msg);
    }
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  Future<void> postVirtualAction(
    String baseUrl, {
    required String action,
    String? square,
    String? choice,
  }) async {
    final uri = _api(baseUrl, 'api/game/virtual_action');
    final map = <String, dynamic>{'action': action};
    if (square != null) map['square'] = square.toLowerCase();
    if (choice != null) map['choice'] = choice.toUpperCase();
    final res = await _client
        .post(uri, headers: _jsonHeaders, body: jsonEncode(map))
        .timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<EspWifiStatus> fetchWiFiStatus(String baseUrl) async {
    final uri = _api(baseUrl, 'api/wifi/status');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = jsonDecode(utf8.decode(res.bodyBytes)) as Map<String, dynamic>;
    return EspWifiStatus.fromJson(map);
  }

  Future<void> postWiFiConfig(
    String baseUrl, {
    required String ssid,
    required String password,
  }) async {
    final uri = _api(baseUrl, 'api/wifi/config');
    final res = await _client
        .post(
          uri,
          headers: _jsonHeaders,
          body: jsonEncode({'ssid': ssid, 'password': password}),
        )
        .timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  static const _jsonHeaders = {'Content-Type': 'application/json'};

  Future<void> _postEmpty(String baseUrl, String path, {bool webLock = false}) async {
    final uri = _api(baseUrl, path);
    final res = await _client.post(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: webLock);
  }

  void _validate(int code, List<int> body, {required bool treat403WebLock}) {
    if (code == 403 && treat403WebLock) throw BoardApiException.webLocked();
    if (code >= 200 && code < 300) return;
    final msg = _extractMessage(body);
    throw BoardApiException(
      'HTTP $code',
      statusCode: code,
      detail: msg ?? _previewBody(body),
    );
  }

  String _previewBody(List<int> body) {
    final s = utf8.decode(body);
    return s.length > 200 ? s.substring(0, 200) : s;
  }

  String? _extractMessage(List<int> body) {
    if (body.isEmpty) return null;
    try {
      final obj = jsonDecode(utf8.decode(body));
      if (obj is Map<String, dynamic>) {
        final m = obj['message'] as String?;
        if (m != null && m.trim().isNotEmpty) return m.trim();
        final e = obj['error'] as String?;
        if (e != null && e.trim().isNotEmpty) return e.trim();
      }
    } catch (_) {}
    return null;
  }
}
