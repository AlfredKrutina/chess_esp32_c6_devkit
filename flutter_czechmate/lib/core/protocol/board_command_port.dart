import '../services/ble_czechmate_client.dart';
import '../services/board_api_client.dart';

/// One command set for the live board. HTTP maps onto REST paths.
/// BLE wraps the same fields in `{"cmd":...}`. OTA *start* is routed here;
/// the chunked BLE upload stays on [BleCzechmateClient].
abstract class BoardCommandPort {
  Future<void> postMove({
    required String from,
    required String to,
    String? promotion,
  });

  Future<void> postNewGame({String? fen});

  Future<void> postTimerConfig({
    required int type,
    int? customMinutes,
    int? customIncrement,
  });

  Future<void> postTimerPause();

  Future<void> postTimerResume();

  Future<void> postHintDestination(String square);

  Future<void> postHintClear();

  Future<void> postGuardClear();

  Future<void> postSetupTutorial(String action);

  Future<void> postOpening(Map<String, dynamic> body);

  Future<void> postBrightness(int percent);

  Future<void> postLightCommand({
    required bool lampState,
    required int r,
    required int g,
    required int b,
  });

  Future<void> postLightGameMode();

  Future<void> postAutoLampTimeout(int seconds);

  Future<void> postOtaStart(String firmwareUrl);
}

/// `POST /api/...` — paths stay here, JSON bodies come from [BoardApiClient]
/// which builds them with [BoardCommandPayload].
class HttpBoardCommandPort implements BoardCommandPort {
  HttpBoardCommandPort(this._api, this.baseUrl);

  final BoardApiClient _api;
  final String baseUrl;

  @override
  Future<void> postMove({
    required String from,
    required String to,
    String? promotion,
  }) {
    return _api.postGameMove(
      baseUrl,
      from: from,
      to: to,
      promotion: promotion,
    );
  }

  @override
  Future<void> postNewGame({String? fen}) =>
      _api.postNewGame(baseUrl, fen: fen);

  @override
  Future<void> postTimerConfig({
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) {
    return _api.postTimerConfig(
      baseUrl,
      type: type,
      customMinutes: customMinutes,
      customIncrement: customIncrement,
    );
  }

  @override
  Future<void> postTimerPause() => _api.postTimerPause(baseUrl);

  @override
  Future<void> postTimerResume() => _api.postTimerResume(baseUrl);

  @override
  Future<void> postHintDestination(String square) =>
      _api.postHintHighlightDestinationOnly(baseUrl, square);

  @override
  Future<void> postHintClear() => _api.postHintClear(baseUrl);

  @override
  Future<void> postGuardClear() => _api.postGuardClear(baseUrl);

  @override
  Future<void> postSetupTutorial(String action) =>
      _api.postSetupTutorial(baseUrl, action: action);

  @override
  Future<void> postOpening(Map<String, dynamic> body) =>
      _api.postOpening(baseUrl, body);

  @override
  Future<void> postBrightness(int percent) =>
      _api.postBrightness(baseUrl, percent);

  @override
  Future<void> postLightCommand({
    required bool lampState,
    required int r,
    required int g,
    required int b,
  }) {
    return _api.postLightCommand(
      baseUrl,
      state: lampState,
      r: r,
      g: g,
      b: b,
    );
  }

  @override
  Future<void> postLightGameMode() => _api.postLightGameMode(baseUrl);

  @override
  Future<void> postAutoLampTimeout(int seconds) =>
      _api.postAutoLampTimeout(baseUrl, seconds);

  @override
  Future<void> postOtaStart(String firmwareUrl) =>
      _api.postBoardOtaStart(baseUrl, url: firmwareUrl);
}

/// GATT command characteristic. Does not open a second radio.
class BleBoardCommandPort implements BoardCommandPort {
  BleBoardCommandPort(this._ble);

  final BleCzechmateClient _ble;

  @override
  Future<void> postMove({
    required String from,
    required String to,
    String? promotion,
  }) {
    return _ble.postMove(from, to, promotion: promotion);
  }

  @override
  Future<void> postNewGame({String? fen}) => _ble.postNewGame(fen: fen);

  @override
  Future<void> postTimerConfig({
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) {
    return _ble.postTimerConfig(
      type,
      customMinutes: customMinutes,
      customIncrement: customIncrement,
    );
  }

  @override
  Future<void> postTimerPause() => _ble.postTimerPause();

  @override
  Future<void> postTimerResume() => _ble.postTimerResume();

  @override
  Future<void> postHintDestination(String square) =>
      _ble.postHintHighlightDestinationOnly(square);

  @override
  Future<void> postHintClear() => _ble.postHintClear();

  @override
  Future<void> postGuardClear() => _ble.postGuardClear();

  @override
  Future<void> postSetupTutorial(String action) =>
      _ble.postSetupTutorial(action);

  @override
  Future<void> postOpening(Map<String, dynamic> body) =>
      _ble.postOpening(body);

  @override
  Future<void> postBrightness(int percent) => _ble.postBrightness(percent);

  @override
  Future<void> postLightCommand({
    required bool lampState,
    required int r,
    required int g,
    required int b,
  }) {
    return _ble.postLightCommand(state: lampState, r: r, g: g, b: b);
  }

  @override
  Future<void> postLightGameMode() => _ble.postLightGameMode();

  @override
  Future<void> postAutoLampTimeout(int seconds) =>
      _ble.postAutoLampTimeout(seconds);

  @override
  Future<void> postOtaStart(String firmwareUrl) =>
      _ble.postOtaStart(firmwareUrl);
}
