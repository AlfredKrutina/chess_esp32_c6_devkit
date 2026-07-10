class OpeningCommonMistake {
  const OpeningCommonMistake({
    required this.wrongUci,
    required this.atPlyIndex,
    required this.hintCs,
    required this.hintEn,
  });

  final String wrongUci;
  final int atPlyIndex;
  final String hintCs;
  final String hintEn;

  String hintForLocale(String locale) =>
      locale.startsWith('cs') ? hintCs : hintEn;

  factory OpeningCommonMistake.fromJson(Map<String, dynamic> json) {
    final hint = json['hint'] as Map<String, dynamic>? ?? {};
    return OpeningCommonMistake(
      wrongUci: json['wrong_uci'] as String,
      atPlyIndex: (json['at_ply_index'] as num).toInt(),
      hintCs: hint['cs'] as String? ?? '',
      hintEn: hint['en'] as String? ?? '',
    );
  }
}

/// Lookup pedagogical hint for a wrong UCI at the current line ply.
String? openingCommonMistakeHint({
  required List<OpeningCommonMistake> mistakes,
  required int? plyIndex,
  required String? lastWrongUci,
  required String locale,
}) {
  if (plyIndex == null || lastWrongUci == null || lastWrongUci.isEmpty) {
    return null;
  }
  final normalized = lastWrongUci.toLowerCase();
  for (final m in mistakes) {
    if (m.atPlyIndex == plyIndex && m.wrongUci.toLowerCase() == normalized) {
      final hint = m.hintForLocale(locale);
      return hint.isNotEmpty ? hint : null;
    }
  }
  return null;
}
