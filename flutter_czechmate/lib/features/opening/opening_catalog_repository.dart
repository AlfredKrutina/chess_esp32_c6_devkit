import 'dart:convert';

import 'package:flutter/services.dart';

import 'opening_rationale.dart';

class OpeningCurriculum {
  const OpeningCurriculum({
    required this.id,
    required this.nameCs,
    required this.nameEn,
    required this.lineIds,
  });

  final String id;
  final String nameCs;
  final String nameEn;
  final List<String> lineIds;

  factory OpeningCurriculum.fromJson(Map<String, dynamic> json) {
    final name = json['name'] as Map<String, dynamic>? ?? {};
    return OpeningCurriculum(
      id: json['id'] as String,
      nameCs: name['cs'] as String? ?? json['id'] as String,
      nameEn: name['en'] as String? ?? json['id'] as String,
      lineIds: (json['line_ids'] as List<dynamic>).cast<String>(),
    );
  }
}

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
    this.rationale,
    this.mirrorLineId,
    this.stepCommentsCs = const {},
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
  final OpeningRationale? rationale;
  final String? mirrorLineId;
  final Map<int, String> stepCommentsCs;

  String nameForLocale(String locale) =>
      locale.startsWith('cs') ? nameCs : nameEn;

  String? commentForPlayerPly(int playerPlyIndex) {
    final ply = playerPlyIndex < playerPlyIndices.length
        ? playerPlyIndices[playerPlyIndex]
        : null;
    if (ply == null) return null;
    return stepCommentsCs[ply];
  }

  String? subtitleForLocale(String locale) {
    final fromRationale = rationale?.summaryForLocale(locale);
    if (fromRationale != null && fromRationale.isNotEmpty) {
      return fromRationale;
    }
    return locale.startsWith('cs') ? ideaCs : ideaEn;
  }

  Map<String, dynamic> toStartPayload({
    String mode = 'learn',
    String opponentMode = 'virtual',
  }) => {
        'action': 'start',
        'line_id': id,
        'mode': mode,
        'opponent_mode': opponentMode,
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
    final rationaleJson = json['rationale'] as Map<String, dynamic>?;
    final steps = json['steps'] as List<dynamic>? ?? [];
    final comments = <int, String>{};
    for (final raw in steps) {
      final step = raw as Map<String, dynamic>;
      final ply = (step['ply_index'] as num?)?.toInt();
      final comment = step['comment'] as Map<String, dynamic>?;
      if (ply != null && comment != null) {
        comments[ply] = comment['cs'] as String? ?? '';
      }
    }
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
      rationale: rationaleJson != null
          ? OpeningRationale.fromJson(rationaleJson)
          : null,
      mirrorLineId: json['mirror_line_id'] as String?,
      stepCommentsCs: comments,
    );
  }
}

class OpeningCatalogRepository {
  static const assetPath = 'assets/data/openings_catalog.json';

  Future<Map<String, dynamic>> _loadRoot() async {
    final raw = await rootBundle.loadString(assetPath);
    return jsonDecode(raw) as Map<String, dynamic>;
  }

  Future<List<OpeningLine>> loadAll() async {
    final map = await _loadRoot();
    final list = map['openings'] as List<dynamic>;
    return list
        .map((e) => OpeningLine.fromJson(e as Map<String, dynamic>))
        .toList();
  }

  Future<List<OpeningCurriculum>> loadCurricula() async {
    final map = await _loadRoot();
    final list = map['curricula'] as List<dynamic>? ?? [];
    return list
        .map((e) => OpeningCurriculum.fromJson(e as Map<String, dynamic>))
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
