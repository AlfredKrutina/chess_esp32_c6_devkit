/// Canonical CzechMate command fields shared by HTTP and BLE.
///
/// Firmware (`web_handlers_game.c`, `web_server_task.c` BLE dispatch) accepts
/// promotion letters in either case (`q`/`Q`, `r`/`R`, `b`/`B`, `n`/`N`). The
/// documented wire form is a single lowercase UCI letter. Both transports must
/// emit that form so the payloads cannot drift.
class BoardCommandPayload {
  const BoardCommandPayload._();

  static String square(String raw) => raw.trim().toLowerCase();

  /// Single lowercase promotion letter, or null when the move carries none.
  /// Unknown letters become `q`, matching the firmware default.
  static String? promotionLetter(String? raw) {
    if (raw == null) return null;
    final trimmed = raw.trim().toLowerCase();
    if (trimmed.isEmpty) return null;
    final letter = trimmed[0];
    switch (letter) {
      case 'q':
      case 'r':
      case 'b':
      case 'n':
        return letter;
      default:
        return 'q';
    }
  }

  /// `POST /api/move` body. Squares are lowercase. Promotion is `q`/`r`/`b`/`n`.
  static Map<String, dynamic> httpMove({
    required String from,
    required String to,
    String? promotion,
  }) {
    final map = <String, dynamic>{
      'from': square(from),
      'to': square(to),
    };
    final letter = promotionLetter(promotion);
    if (letter != null) map['promotion'] = letter;
    return map;
  }

  /// GATT write `{"cmd":"move",...}` — same squares and promotion letter as HTTP.
  static Map<String, dynamic> bleMove({
    required String from,
    required String to,
    String? promotion,
  }) {
    return bleWrap('move', httpMove(from: from, to: to, promotion: promotion));
  }

  /// `POST /api/game/virtual_action` with `action=promote`. Choice is one lowercase letter.
  static Map<String, dynamic> httpPromote({
    required String choice,
    String? square,
  }) {
    final map = <String, dynamic>{
      'action': 'promote',
      'choice': promotionLetter(choice) ?? 'q',
    };
    if (square != null && square.trim().isNotEmpty) {
      map['square'] = BoardCommandPayload.square(square);
    }
    return map;
  }

  /// GATT `{"cmd":"promotion","choice":"q"}`.
  static Map<String, dynamic> blePromotion(String choice) {
    return <String, dynamic>{
      'cmd': 'promotion',
      'choice': promotionLetter(choice) ?? 'q',
    };
  }

  static Map<String, dynamic> httpNewGame({String? fen}) {
    final trimmed = fen?.trim();
    if (trimmed == null || trimmed.isEmpty) return const <String, dynamic>{};
    return <String, dynamic>{'fen': trimmed};
  }

  static Map<String, dynamic> bleNewGame({String? fen}) {
    return bleWrap('new_game', httpNewGame(fen: fen));
  }

  /// `POST /api/game/resign` body. [side] is `white` or `black` — the colour
  /// the app shows on move. The board refuses when the other side is on move.
  static Map<String, dynamic> httpResign({required String side}) {
    final s = side.trim().toLowerCase();
    if (s != 'white' && s != 'black') {
      throw ArgumentError.value(side, 'side', 'must be white or black');
    }
    return <String, dynamic>{'side': s};
  }

  static Map<String, dynamic> bleResign({required String side}) {
    return bleWrap('resign', httpResign(side: side));
  }

  /// `type` 0–14. Custom (14) includes both `custom_minutes` and `custom_increment`
  /// only when both are present — BLE rejects a partial custom pair.
  static Map<String, dynamic> timerConfig({
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) {
    final map = <String, dynamic>{'type': type};
    if (type == 14 && customMinutes != null && customIncrement != null) {
      map['custom_minutes'] = customMinutes;
      map['custom_increment'] = customIncrement;
    }
    return map;
  }

  static Map<String, dynamic> bleTimerConfig({
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) {
    return bleWrap(
      'timer_config',
      timerConfig(
        type: type,
        customMinutes: customMinutes,
        customIncrement: customIncrement,
      ),
    );
  }

  static Map<String, dynamic> hintDestination(String square) =>
      <String, dynamic>{'to': BoardCommandPayload.square(square)};

  static Map<String, dynamic> bleHintDestination(String square) =>
      bleWrap('hint_highlight', hintDestination(square));

  static Map<String, dynamic> hintSquares({
    required String from,
    required String to,
  }) =>
      <String, dynamic>{
        'from': square(from),
        'to': BoardCommandPayload.square(to),
      };

  static Map<String, dynamic> bleHintSquares({
    required String from,
    required String to,
  }) =>
      bleWrap('hint_highlight', hintSquares(from: from, to: to));

  static Map<String, dynamic> setupTutorial(String action) =>
      <String, dynamic>{'action': action.trim().toLowerCase()};

  static Map<String, dynamic> bleSetupTutorial(String action) =>
      bleWrap('setup_tutorial', setupTutorial(action));

  static Map<String, dynamic> bleOpening(Map<String, dynamic> body) =>
      bleWrap('opening', body);

  /// `POST /api/game/puzzle` and BLE `{"cmd":"puzzle",...}`.
  /// [solution] is solver-first: solver move, opponent reply, solver move…
  static Map<String, dynamic> puzzle({
    required String action,
    String? fen,
    String? title,
    List<String>? solution,
    int? id,
  }) {
    final map = <String, dynamic>{'action': action.trim().toLowerCase()};
    final trimmed = fen?.trim();
    if (trimmed != null && trimmed.isNotEmpty) map['fen'] = trimmed;
    final name = title?.trim();
    if (name != null && name.isNotEmpty) map['title'] = name;
    if (solution != null) {
      map['solution'] = solution
          .map((e) => e.trim().toLowerCase())
          .where((e) => e.isNotEmpty)
          .toList();
    }
    if (id != null) map['id'] = id;
    return map;
  }

  static Map<String, dynamic> blePuzzle(Map<String, dynamic> body) =>
      bleWrap('puzzle', body);

  /// HTTP `POST /api/settings/brightness` reads `"brightness"`.
  static Map<String, dynamic> httpBrightness(int percent) =>
      <String, dynamic>{'brightness': percent.clamp(0, 100)};

  /// BLE `brightness` reads `"percent"` first (also accepts `"brightness"`).
  static Map<String, dynamic> bleBrightness(int percent) => bleWrap(
        'brightness',
        <String, dynamic>{'percent': percent.clamp(0, 100)},
      );

  static Map<String, dynamic> lightCommand({
    required bool state,
    required int r,
    required int g,
    required int b,
  }) =>
      <String, dynamic>{
        'state': state,
        'r': r.clamp(0, 255),
        'g': g.clamp(0, 255),
        'b': b.clamp(0, 255),
      };

  static Map<String, dynamic> bleLightCommand({
    required bool state,
    required int r,
    required int g,
    required int b,
  }) =>
      bleWrap(
        'light_command',
        lightCommand(state: state, r: r, g: g, b: b),
      );

  /// HTTP `POST /api/settings/auto_lamp_timeout` and BLE `settings_auto_lamp_timeout`.
  static Map<String, dynamic> autoLampTimeout(int seconds) =>
      <String, dynamic>{'seconds': seconds.clamp(5, 7200)};

  static Map<String, dynamic> bleAutoLampTimeout(int seconds) =>
      bleWrap('settings_auto_lamp_timeout', autoLampTimeout(seconds));

  /// HTTP `POST /api/settings/led_guidance` and BLE `settings_led_guidance`.
  /// Firmware reads `"level"` (1..5), not `led_guidance_level`.
  static Map<String, dynamic> ledGuidanceLevel(int level) =>
      <String, dynamic>{'level': level.clamp(1, 5)};

  static Map<String, dynamic> bleLedGuidanceLevel(int level) =>
      bleWrap('settings_led_guidance', ledGuidanceLevel(level));

  /// HTTP `POST /api/settings/guided_hints` matches the literal `"enabled":true`.
  static Map<String, dynamic> guidedCaptureHints(bool enabled) =>
      <String, dynamic>{'enabled': enabled};

  static Map<String, dynamic> bleGuidedCaptureHints(bool enabled) =>
      bleWrap('settings_guided_hints', guidedCaptureHints(enabled));

  static Map<String, dynamic> otaStartUrl(String url) =>
      <String, dynamic>{'url': url.trim()};

  static Map<String, dynamic> bleOtaStart(String url) =>
      bleWrap('ota_start', otaStartUrl(url));

  static Map<String, dynamic> bleWrap(
    String cmd, [
    Map<String, dynamic>? fields,
  ]) {
    return <String, dynamic>{'cmd': cmd, ...?fields};
  }
}
