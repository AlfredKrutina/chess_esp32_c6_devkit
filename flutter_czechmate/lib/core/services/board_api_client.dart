import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../utils/http_json_decode.dart';
import '../models/board_firmware_models.dart';
import '../models/board_timer_state.dart';
import '../models/game_snapshot.dart';
import '../models/status_models.dart';
import '../utils/game_snapshot_codec.dart';
import 'board_api_exception.dart';

/// REST klient ESP — `ChessboardAPIClient.swift` + `ChessboardAPIClient+Extras.swift`.
class BoardApiClient {
  BoardApiClient({
    http.Client? httpClient,
    Duration? timeout,
    this.resolveBoardApiBearerToken,
  })  : _client = httpClient ?? http.Client(),
        _timeout = timeout ?? const Duration(seconds: 35);

  final http.Client _client;
  final Duration _timeout;

  /// 64 hex znaků z UART `API_TOKEN` — hlavička `Authorization: Bearer …`.
  final String? Function()? resolveBoardApiBearerToken;

  Map<String, String> _headersJson() {
    final h = <String, String>{'Content-Type': 'application/json'};
    final t = resolveBoardApiBearerToken?.call()?.trim();
    if (t != null && t.isNotEmpty) {
      h['Authorization'] = 'Bearer $t';
    }
    return h;
  }

  Map<String, String> _headersOptional() {
    final t = resolveBoardApiBearerToken?.call()?.trim();
    if (t == null || t.isEmpty) {
      return <String, String>{};
    }
    return <String, String>{'Authorization': 'Bearer $t'};
  }

  /// IP bez schématu (`192.168.4.1`) dává v Dartu URI bez hostitele → `resolve('api/…')` je jen cesta a HTTP spadne.
  Uri _base(String baseUrl) {
    var u = baseUrl.trim();
    if (u.endsWith('/')) u = u.substring(0, u.length - 1);
    if (u.isEmpty) {
      throw BoardApiException('Missing board URL', statusCode: 0);
    }
    if (!u.contains('://')) {
      u = 'http://$u';
    }
    final parsed = Uri.parse(u);
    if (parsed.host.isEmpty) {
      throw BoardApiException(
        'Invalid board URL (no host). Example: http://192.168.4.1',
        statusCode: 0,
      );
    }
    return parsed;
  }

  Uri _api(String baseUrl, String path) => _base(baseUrl).resolve(path);

  Future<void> close() async => _client.close();

  Future<GameSnapshot> fetchSnapshot(String baseUrl) async {
    final s = await fetchSnapshotIfChanged(baseUrl, ifNoneMatch: null);
    if (s == null) {
      throw BoardApiException('Empty snapshot response', statusCode: 304);
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
    final map = decodeHttpJsonMap(res.bodyBytes);
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
        .post(uri, headers: _headersJson(), body: body)
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
              headers: _headersJson(),
              body: jsonEncode({'fen': fen}),
            )
            .timeout(_timeout)
        : await _client
            .post(uri, headers: _headersOptional())
            .timeout(_timeout);
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
        .post(uri, headers: _headersJson(), body: jsonEncode(map))
        .timeout(_timeout);
    if (res.statusCode == 403) {
      final err = _extractJsonErrorCode(res.bodyBytes);
      if (err == 'web_locked') throw BoardApiException.webLocked();
      if (err == 'api_token_required') {
        throw BoardApiException.apiTokenRequired();
      }
    }
    if (res.statusCode == 400) {
      final msg = _extractMessage(res.bodyBytes);
      throw BoardApiException('Illegal move', statusCode: 400, detail: msg);
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
        .post(uri, headers: _headersJson(), body: jsonEncode(map))
        .timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<EspWifiStatus> fetchWiFiStatus(String baseUrl) async {
    final uri = _api(baseUrl, 'api/wifi/status');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
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
          headers: _headersJson(),
          body: jsonEncode({'ssid': ssid, 'password': password}),
        )
        .timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postHintHighlight(String baseUrl, {required String from, required String to}) async {
    final uri = _api(baseUrl, 'api/game/hint_highlight');
    final body = jsonEncode({'from': from.toLowerCase(), 'to': to.toLowerCase()});
    final res = await _client.post(uri, headers: _headersJson(), body: body).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postHintClear(String baseUrl) =>
      _postEmpty(baseUrl, 'api/game/hint_clear', webLock: false);

  /// Jako `ChessboardAPIClient+SetupWizard` — jen cílové pole na LED.
  Future<void> postHintHighlightDestinationOnly(String baseUrl, String toSquare) async {
    final uri = _api(baseUrl, 'api/game/hint_highlight');
    final body = jsonEncode({'to': toSquare.toLowerCase()});
    final res = await _client.post(uri, headers: _headersJson(), body: body).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  /// `POST /api/game/setup_tutorial` — `start` | `cancel` | `finish`.
  Future<void> postSetupTutorial(String baseUrl, {required String action}) async {
    final uri = _api(baseUrl, 'api/game/setup_tutorial');
    final body = jsonEncode({'action': action});
    final res = await _client.post(uri, headers: _headersJson(), body: body).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  /// `GET /api/status` — pro `matrix_occupied` během tutoriálu (Wi‑Fi).
  Future<List<int>?> fetchMatrixOccupied(String baseUrl) async {
    final uri = _api(baseUrl, 'api/status');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
    final raw = map['matrix_occupied'];
    if (raw is! List) return null;
    return raw.map((e) => (e as num).toInt()).toList();
  }

  Future<void> postBrightness(String baseUrl, int percent) async {
    final uri = _api(baseUrl, 'api/settings/brightness');
    final body = jsonEncode({'brightness': percent.clamp(0, 100)});
    final res = await _client.post(uri, headers: _headersJson(), body: body).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  Future<ESPMQTTStatusJSON> fetchMQTTStatus(String baseUrl) async {
    final uri = _api(baseUrl, 'api/mqtt/status');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
    return ESPMQTTStatusJSON.fromJson(map);
  }

  Future<void> postMQTTConfig(String baseUrl, String host, int port, {String? username, String? password}) async {
    final uri = _api(baseUrl, 'api/mqtt/config');
    final map = <String, dynamic>{'host': host, 'port': port};
    if (username != null && username.isNotEmpty) map['username'] = username;
    if (password != null && password.isNotEmpty) map['password'] = password;
    final res = await _client.post(uri, headers: _headersJson(), body: jsonEncode(map)).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postFactoryReset(String baseUrl) async {
    final uri = _api(baseUrl, 'api/system/factory_reset');
    final res = await _client.post(
       uri, 
       headers: _headersJson(), 
       body: jsonEncode({'confirm': 'erase_all_nvs'})
    ).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<BoardUISettingsEnvelope> fetchBoardUISettings(String baseUrl) async {
    final uri = _api(baseUrl, 'api/settings/ui');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
    return BoardUISettingsEnvelope.fromJson(map);
  }

  Future<void> postBoardUISettings(String baseUrl, BoardUISettingsEnvelope payload) async {
    final uri = _api(baseUrl, 'api/settings/ui');
    final res = await _client.post(
      uri,
      headers: _headersJson(),
      body: jsonEncode(payload.toJson()),
    ).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  /// Fáze 4C.4 — lampa: auto timeout v sekundách (5..7200).
  Future<void> postAutoLampTimeout(String baseUrl, int seconds) async {
    final uri = _api(baseUrl, 'api/settings/lamp');
    final res = await _client.post(
      uri,
      headers: _headersJson(),
      body: jsonEncode({'auto_lamp_timeout_sec': seconds.clamp(5, 7200)}),
    ).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  /// F\u00e1ze 4C \u2014 LED n\u00e1pov\u011bda \u00farove\u0148 1-5 \u2014 `POST /api/settings/led_guidance`.
  Future<void> postLedGuidanceLevel(String baseUrl, int level) async {
    final uri = _api(baseUrl, 'api/settings/led_guidance');
    final res = await _client.post(
      uri,
      headers: _headersJson(),
      body: jsonEncode({'led_guidance_level': level.clamp(1, 5)}),
    ).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  /// F\u00e1ze 4C \u2014 Guided capture hints ON/OFF \u2014 `POST /api/settings/guided_hints`.
  Future<void> postGuidedCaptureHints(String baseUrl, bool enabled) async {
    final uri = _api(baseUrl, 'api/settings/guided_hints');
    final res = await _client.post(
      uri,
      headers: _headersJson(),
      body: jsonEncode({'guided_capture_hints_enabled': enabled}),
    ).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  /// `POST /api/light/command` — RGB + zap/vyp (firmware `ha_light_request_web_lamp`).
  Future<void> postLightCommand(
    String baseUrl, {
    required bool state,
    required int r,
    required int g,
    required int b,
  }) async {
    final uri = _api(baseUrl, 'api/light/command');
    final body = jsonEncode({
      'state': state,
      'r': r.clamp(0, 255),
      'g': g.clamp(0, 255),
      'b': b.clamp(0, 255),
    });
    final res =
        await _client.post(uri, headers: _headersJson(), body: body).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  /// `POST /api/light/game_mode` — LED podle herního režimu.
  Future<void> postLightGameMode(String baseUrl) async {
    final uri = _api(baseUrl, 'api/light/game_mode');
    final res =
        await _client.post(uri, headers: _headersOptional()).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  Future<ESPDemoStatusJSON> fetchDemoStatus(String baseUrl) async {
    final uri = _api(baseUrl, 'api/demo/status');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
    return ESPDemoStatusJSON.fromJson(map);
  }

  Future<void> postDemoConfig(
    String baseUrl, {
    required bool enabled,
    int? speedMs,
  }) async {
    final uri = _api(baseUrl, 'api/demo/config');
    final map = <String, dynamic>{'enabled': enabled};
    if (speedMs != null) map['speed_ms'] = speedMs;
    final res = await _client
        .post(uri, headers: _headersJson(), body: jsonEncode(map))
        .timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postDemoStart(String baseUrl) async {
    final uri = _api(baseUrl, 'api/demo/start');
    final res =
        await _client.post(uri, headers: _headersOptional()).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postWiFiDisconnect(String baseUrl) async {
    final uri = _api(baseUrl, 'api/wifi/disconnect');
    final res =
        await _client.post(uri, headers: _headersOptional()).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  Future<void> postWiFiClear(String baseUrl) async {
    final uri = _api(baseUrl, 'api/wifi/clear');
    final res =
        await _client.post(uri, headers: _headersOptional()).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: true);
  }

  /// Veřejný manifest (`version.json`) — libovolné HTTPS; nejde přes IP desky.
  Future<FirmwareManifest> fetchFirmwareManifest(String manifestUrl) async {
    final uri = Uri.parse(manifestUrl.trim());
    // Identity encoding avoids gzip/binary mismatch with some HTTP stacks; GitHub likes a User-Agent.
    final res = await _client
        .get(
          uri,
          headers: const {
            'Accept': 'application/json',
            'Accept-Encoding': 'identity',
            'User-Agent':
                'CzechMate/1.0 (Flutter; +https://github.com/alfredkrutina/chess_esp32_c6_devkit)',
          },
        )
        .timeout(_timeout);
    if (res.statusCode != 200) {
      final detail = _manifestFetchErrorDetail(res.statusCode, res.bodyBytes);
      throw BoardApiException(
        'Manifest HTTP ${res.statusCode}',
        statusCode: res.statusCode,
        detail: detail,
      );
    }
    final map = decodeHttpJsonMap(res.bodyBytes);
    final m = FirmwareManifest.fromJson(map);
    if (m.version.isEmpty || m.url.isEmpty || !m.url.startsWith('https://')) {
      throw BoardApiException(
        'Invalid manifest (expected version and HTTPS url)',
        statusCode: 0,
      );
    }
    return m;
  }

  Future<BoardFirmwareInfo> fetchBoardFirmwareInfo(String baseUrl) async {
    final uri = _api(baseUrl, 'api/system/firmware');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
    return BoardFirmwareInfo.fromJson(map);
  }

  Future<BoardOtaStatus> fetchBoardOtaStatus(String baseUrl) async {
    final uri = _api(baseUrl, 'api/system/ota/status');
    final res = await _client.get(uri).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
    final map = decodeHttpJsonMap(res.bodyBytes);
    return BoardOtaStatus.fromJson(map);
  }

  /// Deska si firmware stáhne sama přes HTTPS — vyžaduje připojené Wi‑Fi STA.
  Future<void> postBoardOtaStart(String baseUrl, {required String url}) async {
    final uri = _api(baseUrl, 'api/system/ota');
    final res = await _client
        .post(
          uri,
          headers: _headersJson(),
          body: jsonEncode({'url': url}),
        )
        .timeout(_timeout);
    if (res.statusCode == 409) {
      throw BoardApiException(
        'Cannot start OTA (another update is already in progress).',
        statusCode: 409,
        detail: _extractMessage(res.bodyBytes),
      );
    }
    if (res.statusCode == 428) {
      throw BoardApiException(
        'HTTPS firmware download requires the board connected to Wi‑Fi as a station.',
        statusCode: 428,
        detail: _extractMessage(res.bodyBytes),
      );
    }
    if (res.statusCode == 503) {
      throw BoardApiException(
        'Over-the-air update is disabled: flash partition table must include '
        'both ota_0 and ota_1 (factory-only layouts cannot use HTTPS OTA). '
        'Update the board via USB UART using esptool / idf.py flash.',
        statusCode: 503,
        detail: _extractMessage(res.bodyBytes),
      );
    }
    if (res.statusCode == 403) {
      final err = _extractJsonErrorCode(res.bodyBytes);
      if (err == 'web_locked') throw BoardApiException.webLocked();
      if (err == 'api_token_required') {
        throw BoardApiException.apiTokenRequired();
      }
    }
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: false);
  }

  Future<void> _postEmpty(String baseUrl, String path, {bool webLock = false}) async {
    final uri = _api(baseUrl, path);
    final res =
        await _client.post(uri, headers: _headersOptional()).timeout(_timeout);
    _validate(res.statusCode, res.bodyBytes, treat403WebLock: webLock);
  }

  String? _extractJsonErrorCode(List<int> body) {
    if (body.isEmpty) return null;
    try {
      final obj = decodeHttpJsonMap(body);
      final e = obj['error'];
      if (e is String && e.trim().isNotEmpty) return e.trim();
    } catch (_) {}
    return null;
  }

  void _validate(int code, List<int> body, {required bool treat403WebLock}) {
    if (code == 403) {
      final err = _extractJsonErrorCode(body);
      if (err == 'web_locked') {
        throw BoardApiException.webLocked();
      }
      if (err == 'api_token_required') {
        throw BoardApiException.apiTokenRequired();
      }
      if (treat403WebLock && err == null) {
        throw BoardApiException.webLocked();
      }
    }
    if (code >= 200 && code < 300) return;
    final msg = _extractMessage(body);
    throw BoardApiException(
      'HTTP $code',
      statusCode: code,
      detail: msg ?? _previewBody(body),
    );
  }

  String _previewBody(List<int> body) =>
      previewHttpBodyUtf8(body, maxChars: 200);

  String _manifestFetchErrorDetail(int code, List<int> body) {
    if (code == 404) {
      return 'URL not found (404). Use the full path ending in '
          '/firmware/version.json (see project docs). Clear the field to use '
          'the default manifest.';
    }
    final preview = _previewBody(body);
    final head = preview.trimLeft().toLowerCase();
    if (head.startsWith('<!doctype') || head.startsWith('<html')) {
      return 'Server returned an HTML page instead of JSON (wrong URL?).';
    }
    return preview;
  }

  String? _extractMessage(List<int> body) {
    if (body.isEmpty) return null;
    try {
      final obj = decodeHttpJsonMap(body);
      final m = obj['message'] as String?;
      if (m != null && m.trim().isNotEmpty) return m.trim();
      final e = obj['error'] as String?;
      if (e != null && e.trim().isNotEmpty) return e.trim();
    } catch (_) {}
    return null;
  }
}
