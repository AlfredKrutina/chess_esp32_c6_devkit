import 'dart:convert';

import 'package:flutter/services.dart';

class OpeningLine {
  const OpeningLine({
    required this.id,
    required this.eco,
    required this.nameCs,
    required this.nameEn,
    required this.side,
    required this.difficulty,
    required this.startFen,
    required this.lineUci,
    required this.playerPlyIndices,
    this.checkpointPlyIndices = const [],
    this.ideaCs,
    this.ideaEn,
  });

  final String id;
  final String eco;
  final String nameCs;
  final String nameEn;
  final String side;
  final int difficulty;
  final String startFen;
  final List<String> lineUci;
  final List<int> playerPlyIndices;
  final List<int> checkpointPlyIndices;
  final String? ideaCs;
  final String? ideaEn;

  String nameForLocale(String locale) =>
      locale.startsWith('cs') ? nameCs : nameEn;

  Map<String, dynamic> toStartPayload({String mode = 'learn'}) => {
        'action': 'start',
        'line_id': id,
        'mode': mode,
        'side': side,
        'start_fen': startFen,
        'line_uci': lineUci,
        'player_ply_indices': playerPlyIndices,
        if (checkpointPlyIndices.isNotEmpty)
          'checkpoint_ply_indices': checkpointPlyIndices,
      };

  factory OpeningLine.fromJson(Map<String, dynamic> json) {
    final name = json['name'] as Map<String, dynamic>? ?? {};
    final idea = json['idea'] as Map<String, dynamic>?;
    return OpeningLine(
      id: json['id'] as String,
      eco: json['eco'] as String? ?? '',
      nameCs: name['cs'] as String? ?? json['id'] as String,
      nameEn: name['en'] as String? ?? json['id'] as String,
      side: json['side'] as String? ?? 'white',
      difficulty: (json['difficulty'] as num?)?.toInt() ?? 1,
      startFen: json['start_fen'] as String,
      lineUci: (json['line_uci'] as List<dynamic>).cast<String>(),
      playerPlyIndices:
          (json['player_ply_indices'] as List<dynamic>).cast<int>(),
      checkpointPlyIndices:
          (json['checkpoint_ply_indices'] as List<dynamic>? ?? []).cast<int>(),
      ideaCs: idea?['cs'] as String?,
      ideaEn: idea?['en'] as String?,
    );
  }
}

class OpeningCatalogRepository {
  static const assetPath = 'assets/data/openings_catalog.json';

  Future<List<OpeningLine>> loadAll() async {
    final raw = await rootBundle.loadString(assetPath);
    final map = jsonDecode(raw) as Map<String, dynamic>;
    final list = map['openings'] as List<dynamic>;
    return list
        .map((e) => OpeningLine.fromJson(e as Map<String, dynamic>))
        .toList();
  }

  Future<OpeningLine?> findById(String id) async {
    final all = await loadAll();
    for (final line in all) {
      if (line.id == id) return line;
    }
    return null;
  }
}
