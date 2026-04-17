class GameUiState {
  const GameUiState({
    this.boardFlipped = false,
    this.showCoordinates = true,
    this.sandboxMode = false,
    this.remoteMovesEnabled = false,
    this.selectedSquare,
    this.sandboxFenOverride,
    this.sandboxMessage,
    this.pendingPromotionFrom,
    this.pendingPromotionTo,
  });

  final bool boardFlipped;
  final bool showCoordinates;
  final bool sandboxMode;
  final bool remoteMovesEnabled;
  final String? selectedSquare;
  final String? sandboxFenOverride;
  final String? sandboxMessage;
  final String? pendingPromotionFrom;
  final String? pendingPromotionTo;

  GameUiState copyWith({
    bool? boardFlipped,
    bool? showCoordinates,
    bool? sandboxMode,
    bool? remoteMovesEnabled,
    String? selectedSquare,
    String? sandboxFenOverride,
    String? sandboxMessage,
    String? pendingPromotionFrom,
    String? pendingPromotionTo,
    bool clearSelection = false,
    bool clearSandboxFen = false,
    bool clearPromotion = false,
    bool clearMessage = false,
  }) {
    return GameUiState(
      boardFlipped: boardFlipped ?? this.boardFlipped,
      showCoordinates: showCoordinates ?? this.showCoordinates,
      sandboxMode: sandboxMode ?? this.sandboxMode,
      remoteMovesEnabled: remoteMovesEnabled ?? this.remoteMovesEnabled,
      selectedSquare:
          clearSelection ? null : (selectedSquare ?? this.selectedSquare),
      sandboxFenOverride:
          clearSandboxFen ? null : (sandboxFenOverride ?? this.sandboxFenOverride),
      sandboxMessage:
          clearMessage ? null : (sandboxMessage ?? this.sandboxMessage),
      pendingPromotionFrom:
          clearPromotion ? null : (pendingPromotionFrom ?? this.pendingPromotionFrom),
      pendingPromotionTo:
          clearPromotion ? null : (pendingPromotionTo ?? this.pendingPromotionTo),
    );
  }
}
