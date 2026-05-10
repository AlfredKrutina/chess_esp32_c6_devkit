/// `GET /api/timer` / vnořený `clock` ve snapshotu — `BoardTimerHTTPState` (Swift).
class BoardTimerState {
  const BoardTimerState({
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

  final int whiteTimeMs;
  final int blackTimeMs;
  final bool timerRunning;
  final bool isWhiteTurn;
  final bool gamePaused;
  final bool timeExpired;
  final TimerConfigPart config;
  final int totalMoves;
  final int avgMoveTimeMs;

  bool get isTimeControlEnabled => config.type != 0;

  factory BoardTimerState.fromJson(Map<String, dynamic> json) {
    return BoardTimerState(
      whiteTimeMs: (json['white_time_ms'] as num?)?.toInt() ?? 0,
      blackTimeMs: (json['black_time_ms'] as num?)?.toInt() ?? 0,
      timerRunning: json['timer_running'] as bool? ?? false,
      isWhiteTurn: json['is_white_turn'] as bool? ?? true,
      gamePaused: json['game_paused'] as bool? ?? false,
      timeExpired: json['time_expired'] as bool? ?? false,
      config: TimerConfigPart.fromJson(
        Map<String, dynamic>.from(json['config'] as Map? ?? const {}),
      ),
      totalMoves: (json['total_moves'] as num?)?.toInt() ?? 0,
      avgMoveTimeMs: (json['avg_move_time_ms'] as num?)?.toInt() ?? 0,
    );
  }
}

class TimerConfigPart {
  const TimerConfigPart({
    required this.type,
    required this.name,
    required this.description,
    required this.initialTimeMs,
    required this.incrementMs,
    required this.isFast,
  });

  final int type;
  final String name;
  final String description;
  final int initialTimeMs;
  final int incrementMs;
  final bool isFast;

  factory TimerConfigPart.fromJson(Map<String, dynamic> json) {
    return TimerConfigPart(
      type: (json['type'] as num?)?.toInt() ?? 0,
      name: json['name'] as String? ?? '',
      description: json['description'] as String? ?? '',
      initialTimeMs: (json['initial_time_ms'] as num?)?.toInt() ?? 0,
      incrementMs: (json['increment_ms'] as num?)?.toInt() ?? 0,
      isFast: json['is_fast'] as bool? ?? false,
    );
  }
}

/// `GET /api/wifi/status` — zjednodušená parita s `ESPWiFiStatusJSON`.
class EspWifiStatus {
  const EspWifiStatus({
    required this.apSsid,
    required this.apIp,
    required this.apClients,
    required this.apActive,
    required this.staSsid,
    required this.staIp,
    required this.staConnected,
    required this.online,
    required this.locked,
    this.staBlkOct = '',
  });

  final String apSsid;
  final String apIp;
  final int apClients;
  /// Firmware přidává `ap_active`; starší desky → považovat hotspot za zapnutý, pokud je `ap_ip` neprázdné.
  final bool apActive;
  final String staSsid;
  final String staIp;
  final bool staConnected;
  final bool online;
  final bool locked;
  /// CSV blokovaných 3. oktetů na desce (`sta_blk_oct`), nastaveno přes BLE z aplikace.
  final String staBlkOct;

  factory EspWifiStatus.fromJson(Map<String, dynamic> json) {
    final apIp = json['ap_ip'] as String? ?? '';
    final apActiveRaw = json['ap_active'];
    final bool apActive;
    if (apActiveRaw is bool) {
      apActive = apActiveRaw;
    } else {
      apActive = apIp.trim().isNotEmpty;
    }
    return EspWifiStatus(
      apSsid: json['ap_ssid'] as String? ?? '',
      apIp: apIp,
      apClients: (json['ap_clients'] as num?)?.toInt() ?? 0,
      apActive: apActive,
      staSsid: json['sta_ssid'] as String? ?? '',
      staIp: json['sta_ip'] as String? ?? '',
      staConnected: json['sta_connected'] as bool? ?? false,
      online: json['online'] as bool? ?? false,
      locked: json['locked'] as bool? ?? false,
      staBlkOct: json['sta_blk_oct'] as String? ?? '',
    );
  }
}
