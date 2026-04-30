import 'package:flutter/material.dart';

class BoardStyleColors {
  final Color light;
  final Color dark;
  final Color selected;
  final Color lastMove;
  final Color error;

  const BoardStyleColors({
    required this.light,
    required this.dark,
    required this.selected,
    required this.lastMove,
    required this.error,
  });

  static BoardStyleColors fromRaw(String raw) {
    const valid = Color(0x664CAF50);
    const last = Color(0x66FFEB3B);
    const err = Color(0x99F44336);

    switch (raw) {
      case 'modernDark':
        return const BoardStyleColors(light: Color(0xFFE0E0E0), dark: Color(0xFF616161), selected: valid, lastMove: last, error: err);
      case 'iceBlue':
        return const BoardStyleColors(light: Color(0xFFE3F2FD), dark: Color(0xFF90CAF9), selected: valid, lastMove: last, error: err);
      case 'forestGreen':
        return const BoardStyleColors(light: Color(0xFFF1F8E9), dark: Color(0xFF81C784), selected: valid, lastMove: last, error: err);
      case 'marbleGray':
        return const BoardStyleColors(light: Color(0xFFEEEEEE), dark: Color(0xFFBDBDBD), selected: valid, lastMove: last, error: err);
      case 'midnight':
        return const BoardStyleColors(light: Color(0xFF9E9E9E), dark: Color(0xFF212121), selected: valid, lastMove: last, error: err);
      case 'coral':
        return const BoardStyleColors(light: Color(0xFFFFEBEE), dark: Color(0xFFE57373), selected: valid, lastMove: last, error: err);
      case 'slate':
        return const BoardStyleColors(light: Color(0xFFECEFF1), dark: Color(0xFF78909C), selected: valid, lastMove: last, error: err);
      case 'wooden':
      default:
        // default wooden fallback
        return const BoardStyleColors(
          light: Color(0xFFF0D9B5), 
          dark: Color(0xFFB58863), 
          selected: valid, 
          lastMove: last, 
          error: err
        );
    }
  }
}
