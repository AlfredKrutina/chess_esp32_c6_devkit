// Parita `ChessTimeControlPresets.swift` / `time_control_type_t` (0…14).

enum ChessTimeControlPreset {
  noTimeLimit,
  bullet1_0,
  bullet2_1,
  blitz3_0,
  blitz3_2,
  blitz5_0,
  rapid10_0,
  rapid15_10;

  static ChessTimeControlPreset? tryParseName(String raw) {
    for (final v in ChessTimeControlPreset.values) {
      if (v.name == raw) return v;
    }
    return null;
  }
}

extension ChessTimeControlPresetX on ChessTimeControlPreset {
  /// Index do `TIME_CONTROLS[]` ve firmware.
  int get firmwareType {
    switch (this) {
      case ChessTimeControlPreset.noTimeLimit:
        return 0;
      case ChessTimeControlPreset.bullet1_0:
        return 1;
      case ChessTimeControlPreset.bullet2_1:
        return 3;
      case ChessTimeControlPreset.blitz3_0:
        return 4;
      case ChessTimeControlPreset.blitz3_2:
        return 5;
      case ChessTimeControlPreset.blitz5_0:
        return 6;
      case ChessTimeControlPreset.rapid10_0:
        return 8;
      case ChessTimeControlPreset.rapid15_10:
        return 10;
    }
  }

  String get title {
    switch (this) {
      case ChessTimeControlPreset.noTimeLimit:
        return 'No time limit';
      case ChessTimeControlPreset.bullet1_0:
        return '1 min';
      case ChessTimeControlPreset.bullet2_1:
        return '2 | 1';
      case ChessTimeControlPreset.blitz3_0:
        return '3 min';
      case ChessTimeControlPreset.blitz3_2:
        return '3 | 2';
      case ChessTimeControlPreset.blitz5_0:
        return '5 min';
      case ChessTimeControlPreset.rapid10_0:
        return '10 min';
      case ChessTimeControlPreset.rapid15_10:
        return '15 | 10';
    }
  }

  String get subtitle {
    switch (this) {
      case ChessTimeControlPreset.noTimeLimit:
        return 'No clock';
      case ChessTimeControlPreset.bullet1_0:
      case ChessTimeControlPreset.bullet2_1:
        return 'Bullet';
      case ChessTimeControlPreset.blitz3_0:
      case ChessTimeControlPreset.blitz3_2:
      case ChessTimeControlPreset.blitz5_0:
        return 'Blitz';
      case ChessTimeControlPreset.rapid10_0:
      case ChessTimeControlPreset.rapid15_10:
        return 'Rapid';
    }
  }
}
