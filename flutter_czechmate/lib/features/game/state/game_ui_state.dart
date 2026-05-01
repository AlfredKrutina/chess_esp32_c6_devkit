import '../../../core/models/puzzle_challenge_state.dart';

/// Phase 3.3 / 3.6+ — Full GameUiState with transient messages, invalid-move flash,
/// layout mode, sandbox undo stack.
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
    this.historyReviewMoveIndex,
    this.boardStyleRaw = 'wooden',
    this.boardZoomStorage = 1.0,
    this.moveAnimationsEnabled = true,
    // Phase 3.3 — transient board message (auto-dismiss 4s)
    this.transientBoardMessage,
    // Phase 3.3 — invalid move flash pair
    this.invalidFlashFrom,
    this.invalidFlashTo,
    // Hint overlay on board
    this.hintFrom,
    this.hintTo,
    // Phase 4F — game layout mode
    this.layoutMode = 'standard',
    // Phase 3 — sandbox undo stack
    this.sandboxUndoStack = const [],
    this.sandboxUndoCount = 0,
    // Phase 4F — move evaluation & hint depth (app-side, merged into NVS on POST)
    this.moveEvaluationEnabled = false,
    this.hintDepth = 10,
    // Phase 4F — control panel expanded state
    this.isControlPanelExpanded = true,
    // Phase 5 — learning mode
    this.learningMode = false,
    this.puzzleChallenge,
    this.puzzleBoardTint = 0,
    this.puzzleBoardTintGreen = false,
    this.puzzleSnackText,
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
  final int? historyReviewMoveIndex;
  final String boardStyleRaw;
  final double boardZoomStorage;
  final bool moveAnimationsEnabled;
  final String? transientBoardMessage;
  final String? invalidFlashFrom;
  final String? invalidFlashTo;
  final String? hintFrom;
  final String? hintTo;
  final String layoutMode;
  final List<String> sandboxUndoStack; // list of FEN snapshots for undo
  final int sandboxUndoCount;
  final bool moveEvaluationEnabled;
  final int hintDepth;
  final bool isControlPanelExpanded;
  final bool learningMode;
  final PuzzleChallengeState? puzzleChallenge;
  /// 0 = žádný překryv; 0–1 síla barevného pulzu (puzzle feedback).
  final double puzzleBoardTint;
  final bool puzzleBoardTintGreen;
  /// Jednorázová zpráva pro SnackBar (puzzle úspěch/neúspěch), po zobrazení se vyčistí.
  final String? puzzleSnackText;

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
    int? historyReviewMoveIndex,
    String? boardStyleRaw,
    double? boardZoomStorage,
    bool? moveAnimationsEnabled,
    String? transientBoardMessage,
    String? invalidFlashFrom,
    String? invalidFlashTo,
    String? hintFrom,
    String? hintTo,
    String? layoutMode,
    List<String>? sandboxUndoStack,
    int? sandboxUndoCount,
    bool? moveEvaluationEnabled,
    int? hintDepth,
    bool? isControlPanelExpanded,
    bool? learningMode,
    PuzzleChallengeState? puzzleChallenge,
    bool clearPuzzleChallenge = false,
    double? puzzleBoardTint,
    bool? puzzleBoardTintGreen,
    bool clearPuzzleTint = false,
    String? puzzleSnackText,
    bool clearPuzzleSnack = false,
    bool clearSelection = false,
    bool clearSandboxFen = false,
    bool clearPromotion = false,
    bool clearMessage = false,
    bool clearHistoryIndex = false,
    bool clearTransient = false,
    bool clearInvalidFlash = false,
    bool clearHint = false,
  }) {
    return GameUiState(
      boardFlipped: boardFlipped ?? this.boardFlipped,
      showCoordinates: showCoordinates ?? this.showCoordinates,
      sandboxMode: sandboxMode ?? this.sandboxMode,
      remoteMovesEnabled: remoteMovesEnabled ?? this.remoteMovesEnabled,
      selectedSquare:
          clearSelection ? null : (selectedSquare ?? this.selectedSquare),
      sandboxFenOverride: clearSandboxFen
          ? null
          : (sandboxFenOverride ?? this.sandboxFenOverride),
      sandboxMessage:
          clearMessage ? null : (sandboxMessage ?? this.sandboxMessage),
      pendingPromotionFrom: clearPromotion
          ? null
          : (pendingPromotionFrom ?? this.pendingPromotionFrom),
      pendingPromotionTo: clearPromotion
          ? null
          : (pendingPromotionTo ?? this.pendingPromotionTo),
      historyReviewMoveIndex: clearHistoryIndex
          ? null
          : (historyReviewMoveIndex ?? this.historyReviewMoveIndex),
      boardStyleRaw: boardStyleRaw ?? this.boardStyleRaw,
      boardZoomStorage: boardZoomStorage ?? this.boardZoomStorage,
      moveAnimationsEnabled:
          moveAnimationsEnabled ?? this.moveAnimationsEnabled,
      transientBoardMessage: clearTransient
          ? null
          : (transientBoardMessage ?? this.transientBoardMessage),
      invalidFlashFrom: clearInvalidFlash
          ? null
          : (invalidFlashFrom ?? this.invalidFlashFrom),
      invalidFlashTo:
          clearInvalidFlash ? null : (invalidFlashTo ?? this.invalidFlashTo),
      hintFrom: clearHint ? null : (hintFrom ?? this.hintFrom),
      hintTo: clearHint ? null : (hintTo ?? this.hintTo),
      layoutMode: layoutMode ?? this.layoutMode,
      sandboxUndoStack: sandboxUndoStack ?? this.sandboxUndoStack,
      sandboxUndoCount: sandboxUndoCount ?? this.sandboxUndoCount,
      moveEvaluationEnabled:
          moveEvaluationEnabled ?? this.moveEvaluationEnabled,
      hintDepth: hintDepth ?? this.hintDepth,
      isControlPanelExpanded:
          isControlPanelExpanded ?? this.isControlPanelExpanded,
      learningMode: learningMode ?? this.learningMode,
      puzzleChallenge: clearPuzzleChallenge
          ? null
          : (puzzleChallenge ?? this.puzzleChallenge),
      puzzleBoardTint: clearPuzzleTint ? 0 : (puzzleBoardTint ?? this.puzzleBoardTint),
      puzzleBoardTintGreen:
          puzzleBoardTintGreen ?? this.puzzleBoardTintGreen,
      puzzleSnackText: clearPuzzleSnack
          ? null
          : (puzzleSnackText ?? this.puzzleSnackText),
    );
  }
}
