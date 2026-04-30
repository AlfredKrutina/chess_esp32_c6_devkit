import 'dart:convert';

class BoardTimerHTTPState {
  final int whiteTimeMs;
  final int blackTimeMs;
  final bool timerRunning;
  final bool isWhiteTurn;
  final bool gamePaused;
  final bool timeExpired;
  final TimerConfigPart config;
  final int totalMoves;
  final int avgMoveTimeMs;

  BoardTimerHTTPState({
    required this.whiteTimeMs,
    required this.blackTimeMs,
    required this.timerRunning,
    required this.isWhiteTurn,
    required this.gamePaused,
    required this.timeExpired,
    required this.config,
    required this.totalMoves,
    required this.avgMoveTimeMs,
  });

  factory BoardTimerHTTPState.fromJson(Map<String, dynamic> json) {
    return BoardTimerHTTPState(
      whiteTimeMs: json['white_time_ms'] as int? ?? 0,
      blackTimeMs: json['black_time_ms'] as int? ?? 0,
      timerRunning: json['timer_running'] as bool? ?? false,
      isWhiteTurn: json['is_white_turn'] as bool? ?? false,
      gamePaused: json['game_paused'] as bool? ?? false,
      timeExpired: json['time_expired'] as bool? ?? false,
      config: TimerConfigPart.fromJson(json['config'] as Map<String, dynamic>),
      totalMoves: json['total_moves'] as int? ?? 0,
      avgMoveTimeMs: json['avg_move_time_ms'] as int? ?? 0,
    );
  }

  bool get isTimeControlEnabled => config.type != 0;
}

class TimerConfigPart {
  final int type;
  final String name;
  final String description;
  final int initialTimeMs;
  final int incrementMs;
  final bool isFast;

  TimerConfigPart({
    required this.type,
    required this.name,
    required this.description,
    required this.initialTimeMs,
    required this.incrementMs,
    required this.isFast,
  });

  factory TimerConfigPart.fromJson(Map<String, dynamic> json) {
    return TimerConfigPart(
      type: json['type'] as int? ?? 0,
      name: json['name'] as String? ?? '',
      description: json['description'] as String? ?? '',
      initialTimeMs: json['initial_time_ms'] as int? ?? 0,
      incrementMs: json['increment_ms'] as int? ?? 0,
      isFast: json['is_fast'] as bool? ?? false,
    );
  }
}

class ESPWiFiStatusJSON {
  final String apSsid;
  final String apIp;
  final int apClients;
  final String staSsid;
  final String staIp;
  final bool staConnected;
  final bool online;
  final bool locked;

  ESPWiFiStatusJSON({
    required this.apSsid,
    required this.apIp,
    required this.apClients,
    required this.staSsid,
    required this.staIp,
    required this.staConnected,
    required this.online,
    required this.locked,
  });

  factory ESPWiFiStatusJSON.fromJson(Map<String, dynamic> json) {
    return ESPWiFiStatusJSON(
      apSsid: json['apSsid'] as String? ?? '',
      apIp: json['apIp'] as String? ?? '',
      apClients: json['apClients'] as int? ?? 0,
      staSsid: json['staSsid'] as String? ?? '',
      staIp: json['staIp'] as String? ?? '',
      staConnected: json['staConnected'] as bool? ?? false,
      online: json['online'] as bool? ?? false,
      locked: json['locked'] as bool? ?? false,
    );
  }
}

class ESPJSONSuccessMessage {
  final bool? success;
  final String? message;
  final String? error;

  ESPJSONSuccessMessage({this.success, this.message, this.error});

  factory ESPJSONSuccessMessage.fromJson(Map<String, dynamic> json) {
    return ESPJSONSuccessMessage(
      success: json['success'] as bool?,
      message: json['message'] as String?,
      error: json['error'] as String?,
    );
  }
}

class ESPMQTTStatusJSON {
  final String host;
  final int port;
  final String username;
  final String password;
  final bool wifiConnected;
  final bool mqttConnected;
  final String mode;

  ESPMQTTStatusJSON({
    required this.host,
    required this.port,
    required this.username,
    required this.password,
    required this.wifiConnected,
    required this.mqttConnected,
    required this.mode,
  });

  factory ESPMQTTStatusJSON.fromJson(Map<String, dynamic> json) {
    return ESPMQTTStatusJSON(
      host: json['host'] as String? ?? '',
      port: json['port'] as int? ?? 0,
      username: json['username'] as String? ?? '',
      password: json['password'] as String? ?? '',
      wifiConnected: json['wifiConnected'] as bool? ?? false,
      mqttConnected: json['mqttConnected'] as bool? ?? false,
      mode: json['mode'] as String? ?? '',
    );
  }
}

class ESPDemoStatusJSON {
  final bool enabled;

  ESPDemoStatusJSON({required this.enabled});

  factory ESPDemoStatusJSON.fromJson(Map<String, dynamic> json) {
    return ESPDemoStatusJSON(enabled: json['enabled'] as bool? ?? false);
  }
}

class BoardUISettingsEnvelope {
  final int version;
  final BoardUIPrefsPayload prefs;

  BoardUISettingsEnvelope({required this.version, required this.prefs});

  factory BoardUISettingsEnvelope.fromJson(Map<String, dynamic> json) {
    return BoardUISettingsEnvelope(
      version: json['version'] as int? ?? 1,
      prefs: json['prefs'] != null 
          ? BoardUIPrefsPayload.fromJson(json['prefs'] as Map<String, dynamic>) 
          : BoardUIPrefsPayload.empty(),
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'version': version,
      'prefs': prefs.toJson(),
    };
  }
}

class BoardUIPrefsPayload {
  final int chessHintDepth;
  final bool chessEvaluateMove;
  final int chessHintLimit;
  final bool chessHintAwardBest;
  final bool chessHintAwardGood;
  final bool chessHintAwardCapture;
  final bool chessShowHintStats;
  final bool chessBotLedTargetOnlyAfterLift;
  final bool chessTutorialsEnabled;
  final bool chessConfirmNewGame;
  final BotSettingsPayload botSettings;
  final String moveHintTier; // 'H1' | 'H2' | 'H3'

  BoardUIPrefsPayload({
    required this.chessHintDepth,
    required this.chessEvaluateMove,
    required this.chessHintLimit,
    required this.chessHintAwardBest,
    required this.chessHintAwardGood,
    required this.chessHintAwardCapture,
    required this.chessShowHintStats,
    required this.chessBotLedTargetOnlyAfterLift,
    required this.chessTutorialsEnabled,
    required this.chessConfirmNewGame,
    required this.botSettings,
    this.moveHintTier = 'H1',
  });

  factory BoardUIPrefsPayload.empty() {
    return BoardUIPrefsPayload(
      chessHintDepth: 10,
      chessEvaluateMove: false,
      chessHintLimit: 0,
      chessHintAwardBest: true,
      chessHintAwardGood: false,
      chessHintAwardCapture: false,
      chessShowHintStats: false,
      chessBotLedTargetOnlyAfterLift: false,
      chessTutorialsEnabled: false,
      chessConfirmNewGame: false,
      botSettings: BotSettingsPayload.empty(),
      moveHintTier: 'H1',
    );
  }

  factory BoardUIPrefsPayload.fromJson(Map<String, dynamic> json) {
    return BoardUIPrefsPayload(
      chessHintDepth: json['chessHintDepth'] as int? ?? 10,
      chessEvaluateMove: json['chessEvaluateMove'] as bool? ?? false,
      chessHintLimit: json['chessHintLimit'] as int? ?? 0,
      chessHintAwardBest: json['chessHintAwardBest'] as bool? ?? true,
      chessHintAwardGood: json['chessHintAwardGood'] as bool? ?? false,
      chessHintAwardCapture: json['chessHintAwardCapture'] as bool? ?? false,
      chessShowHintStats: json['chessShowHintStats'] as bool? ?? false,
      chessBotLedTargetOnlyAfterLift: json['chessBotLedTargetOnlyAfterLift'] as bool? ?? false,
      chessTutorialsEnabled: json['chessTutorialsEnabled'] as bool? ?? false,
      chessConfirmNewGame: json['chess_confirm_new_game'] as bool? ?? false,
      botSettings: json['botSettings'] != null ? BotSettingsPayload.fromJson(json['botSettings'] as Map<String, dynamic>) : BotSettingsPayload.empty(),
      moveHintTier: json['moveHintTier'] as String? ?? 'H1',
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'chessHintDepth': chessHintDepth,
      'chessEvaluateMove': chessEvaluateMove,
      'chessHintLimit': chessHintLimit,
      'chessHintAwardBest': chessHintAwardBest,
      'chessHintAwardGood': chessHintAwardGood,
      'chessHintAwardCapture': chessHintAwardCapture,
      'chessShowHintStats': chessShowHintStats,
      'chessBotLedTargetOnlyAfterLift': chessBotLedTargetOnlyAfterLift,
      'chessTutorialsEnabled': chessTutorialsEnabled,
      'chess_confirm_new_game': chessConfirmNewGame,
      'botSettings': botSettings.toJson(),
      'moveHintTier': moveHintTier,
    };
  }
}

class BotSettingsPayload {
  final String mode;
  final String strength;
  final String side;

  BotSettingsPayload({
    required this.mode,
    required this.strength,
    required this.side,
  });

  factory BotSettingsPayload.empty() {
    return BotSettingsPayload(mode: 'pvp', strength: '10', side: 'white');
  }

  factory BotSettingsPayload.fromJson(Map<String, dynamic> json) {
    return BotSettingsPayload(
      mode: json['mode'] as String? ?? 'pvp',
      strength: json['strength'] as String? ?? '10',
      side: json['side'] as String? ?? 'white',
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'mode': mode,
      'strength': strength,
      'side': side,
    };
  }
}
