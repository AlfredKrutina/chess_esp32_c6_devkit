import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

/// Per-line mastery stored under `opening_progress_v1`.
class OpeningLineProgress {
  const OpeningLineProgress({
    this.stars = 0,
    this.bestDrillErrors = 99,
    this.lastCompletedAt,
  });

  final int stars;
  final int bestDrillErrors;
  final String? lastCompletedAt;

  OpeningLineProgress copyWith({
    int? stars,
    int? bestDrillErrors,
    String? lastCompletedAt,
  }) {
    return OpeningLineProgress(
      stars: stars ?? this.stars,
      bestDrillErrors: bestDrillErrors ?? this.bestDrillErrors,
      lastCompletedAt: lastCompletedAt ?? this.lastCompletedAt,
    );
  }

  factory OpeningLineProgress.fromJson(Map<String, dynamic> json) {
    return OpeningLineProgress(
      stars: (json['stars'] as num?)?.toInt() ?? 0,
      bestDrillErrors: (json['best_drill_errors'] as num?)?.toInt() ?? 99,
      lastCompletedAt: json['last_completed_at'] as String?,
    );
  }

  Map<String, dynamic> toJson() => {
        'stars': stars,
        'best_drill_errors': bestDrillErrors,
        if (lastCompletedAt != null) 'last_completed_at': lastCompletedAt,
      };
}

class OpeningProgressRepository {
  OpeningProgressRepository(this._prefs);

  static const storageKey = 'opening_progress_v1';

  final SharedPreferences _prefs;

  Map<String, OpeningLineProgress> loadAll() {
    final raw = _prefs.getString(storageKey);
    if (raw == null || raw.isEmpty) return {};
    try {
      final map = jsonDecode(raw) as Map<String, dynamic>;
      return map.map(
        (k, v) => MapEntry(
          k,
          OpeningLineProgress.fromJson(
            Map<String, dynamic>.from(v as Map),
          ),
        ),
      );
    } catch (_) {
      return {};
    }
  }

  OpeningLineProgress progressFor(String lineId) =>
      loadAll()[lineId] ?? const OpeningLineProgress();

  Future<void> _saveAll(Map<String, OpeningLineProgress> all) async {
    final encoded = jsonEncode(
      all.map((k, v) => MapEntry(k, v.toJson())),
    );
    await _prefs.setString(storageKey, encoded);
  }

  /// Updates stars per plan §4.2 after a completed lesson.
  Future<OpeningLineProgress> recordCompletion({
    required String lineId,
    required String mode,
    int drillErrors = 0,
    bool timedSuccess = true,
  }) async {
    final all = loadAll();
    final prev = all[lineId] ?? const OpeningLineProgress();
    var stars = prev.stars;
    var bestDrill = prev.bestDrillErrors;

    switch (mode) {
      case 'learn':
        if (stars < 1) stars = 1;
        break;
      case 'drill':
        if (stars < 1) stars = 1;
        if (drillErrors <= 2 && stars < 2) stars = 2;
        if (drillErrors < bestDrill) bestDrill = drillErrors;
        break;
      case 'timed':
        if (timedSuccess) {
          if (stars < 1) stars = 1;
          if (stars < 3) stars = 3;
        }
        break;
      case 'mirror':
        if (drillErrors <= 2 && stars < 2) stars = 2;
        if (stars < 4) stars = 4;
        if (drillErrors < bestDrill) bestDrill = drillErrors;
        break;
    }

    final next = prev.copyWith(
      stars: stars,
      bestDrillErrors: bestDrill,
      lastCompletedAt: DateTime.now().toUtc().toIso8601String(),
    );
    all[lineId] = next;
    await _saveAll(all);
    return next;
  }

  bool mirrorUnlocked(String lineId, {String? mirrorLineId}) {
    if (mirrorLineId == null || mirrorLineId.isEmpty) return false;
    return progressFor(lineId).stars >= 2;
  }
}
