/// Identifies one vertical block on the PNG/GIF export canvas (user-reorderable).
enum GameExportBlockId {
  recap,
  branding,
  result,
  stats,
  material,
  board,
  eval,
  timing,
}

/// Stable default visual order.
List<GameExportBlockId> get defaultGameExportBlockOrder =>
    List<GameExportBlockId>.unmodifiable(GameExportBlockId.values);

/// Dedup + append any missing ids (forward-compatible).
List<GameExportBlockId> normalizeGameExportBlockOrder(List<GameExportBlockId>? raw) {
  final seen = <GameExportBlockId>{};
  final out = <GameExportBlockId>[];
  if (raw != null) {
    for (final id in raw) {
      if (!seen.contains(id)) {
        seen.add(id);
        out.add(id);
      }
    }
  }
  for (final id in GameExportBlockId.values) {
    if (!seen.contains(id)) out.add(id);
  }
  return out;
}

GameExportBlockId? gameExportBlockIdFromStorage(String raw) {
  for (final v in GameExportBlockId.values) {
    if (v.name == raw) return v;
  }
  return null;
}
