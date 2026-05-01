import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

import '../constants/firmware_defaults.dart';
import '../models/coach_ai_provider.dart';
import '../models/coach_llm_backend.dart';

class PrefsRepository {
  PrefsRepository(this._p);

  final SharedPreferences _p;

  static const keyBaseUrl = 'czechmate.boardBaseURL';
  static const keyMock = 'czechmate.useMockBoard';
  static const keyCoachKey = 'czechmate.coachApiKey';
  static const keyCoachLlmBackend = 'czechmate.coach.llmBackend';
  static const keyCoachGeminiKey = 'czechmate.coach.geminiApiKey';
  static const keyCoachGemmaModel = 'czechmate.coach.gemmaModelId';
  static const keyCoachAiPriority = 'czechmate.coach.aiPriorityJson';
  static const keyCoachOpenAiModel = 'czechmate.coach.openAiModel';
  static const keyCoachGroqKey = 'czechmate.coach.groqApiKey';
  static const keyCoachGroqModel = 'czechmate.coach.groqModel';
  static const keyCoachXaiKey = 'czechmate.coach.xaiApiKey';
  static const keyCoachXaiModel = 'czechmate.coach.xaiModel';
  static const keyCoachOllamaBase = 'czechmate.coach.ollamaBaseUrl';
  static const keyCoachOllamaModel = 'czechmate.coach.ollamaModel';
  static const keyConnectionMode = 'czechmate.connectionMode';
  static const keyLastBleId = 'czechmate.lastBleRemoteId';
  static const keyPreferBluetoothOnly = 'czechmate.preferBluetoothOnly';
  static const keyHintDepth = 'czechmate.hintDepth';
  static const keyMoveHintTier = 'czechmate.moveHintTier';
  static const keyMoveEval = 'czechmate.moveEvaluationEnabled';
  static const keyAppearance = 'czechmate.appearance';
  static const keyHaptics = 'czechmate.hapticsEnabled';
  static const keySound = 'czechmate.soundEffectsEnabled';
  static const keyEndgameAutoOpen = 'czechmate.endgameReportAutoOpen';
  static const keyCoachLevel = 'czechmate.coach.userLevel';
  static const keyDevMode = 'czechmate.developerModeUnlocked';
  static const keyLayoutMode = 'czechmate.game.layoutMode';
  static const keyDesktopCoachRailVisible = 'czechmate.game.desktopCoachRailVisible';
  static const keyBoardZoom = 'czechmate.game.boardZoomStorage';
  static const keyBoardFlipped = 'czechmate.boardFlipped';
  static const keyShowCoords = 'czechmate.showBoardCoordinates';
  static const keyMoveAnims = 'czechmate.moveAnimationsEnabled';
  static const keyRemoteMoves = 'czechmate.remoteMovesEnabled';
  static const keyBoardStyle = 'czechmate.boardStyleRaw';
  static const keyLedGuidance = 'czechmate.ledGuidanceLevel';
  static const keyGuidedCapturePref = 'czechmate.guidedCapturePref';
  /// Parita `AppDebugLog.coachTraceLogsDefaultsKey` na iOS.
  static const keyCoachTraceLogs = 'czechmate.coachTraceLogsEnabled';
  static const keyUseWebSocket = 'czechmate.useWebSocket';
  static const keyStockfishBase = 'czechmate.stockfishApiBaseUrl';
  static const keyLearningMode = 'czechmate.learningModeEnabled';
  static const keyLastBleName = 'czechmate.lastBLEDeviceDisplayName';
  static const keyLastLinkKind = 'czechmate.lastBoardLinkKind';
  static const keyLiveActivity = 'czechmate.liveActivityEnabled';
  static const keyWatchCompanionMirror = 'czechmate.watchCompanionMirrorEnabled';
  /// Wear OS Data Layer mirror z Android telefonu (`WearDataLayerMirror`).
  static const keyWearDataLayerMirror = 'czechmate.wearDataLayerMirrorEnabled';
  static const keyPuzzleLibraryJson = 'czechmate.puzzleLibraryJson';
  static const keyPuzzleTrainingPool = 'czechmate.puzzleTrainingPoolMode';
  static const keyPuzzleTrainingSessions = 'czechmate.puzzleTrainingSessionsStarted';
  static const keyStatsPeakMoveCount = 'czechmate.statsPeakMoveCount';
  /// `p:<presetEnumName>` nebo `c:<minutes>:<incrementSec>` — parita iOS `lastNewGameTimeSelection`.
  static const keyLastNewGameTimeControl = 'czechmate.lastNewGameTimeControlV1';
  /// Raw URL na hostovaný `version.json` (GitHub Pages / raw.githubusercontent.com).
  static const keyFirmwareManifestUrl = 'czechmate.firmwareManifestUrl';
  static const keyFirmwareUpdateReminders = 'czechmate.firmwareUpdateRemindersEnabled';
  /// ISO den `YYYY-MM-DD` — naposledy uživatel ťukl „Teď ne“ u nabídky aktualizace.
  static const keyFirmwareReminderDismissDay = 'czechmate.firmwareReminderDismissDay';

  String? get lastBoardBaseUrl => _p.getString(keyBaseUrl);
  Future<void> setLastBoardBaseUrl(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyBaseUrl);
    } else {
      await _p.setString(keyBaseUrl, v);
    }
  }

  bool get useMockBoard => _p.getBool(keyMock) ?? false;
  Future<void> setUseMockBoard(bool v) => _p.setBool(keyMock, v);

  String? get coachApiKey => _p.getString(keyCoachKey);
  Future<void> setCoachApiKey(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyCoachKey);
    } else {
      await _p.setString(keyCoachKey, v);
    }
  }

  /// Defaults to OpenAI for existing installs (same as prior single-key behavior).
  CoachLlmBackend get coachLlmBackend =>
      CoachLlmBackend.fromStorage(_p.getString(keyCoachLlmBackend));

  Future<void> setCoachLlmBackend(CoachLlmBackend v) async {
    await _p.setString(keyCoachLlmBackend, v.storageValue);
  }

  String? get coachGeminiApiKey => _p.getString(keyCoachGeminiKey);

  Future<void> setCoachGeminiApiKey(String? v) async {
    if (v == null || v.trim().isEmpty) {
      await _p.remove(keyCoachGeminiKey);
    } else {
      await _p.setString(keyCoachGeminiKey, v.trim());
    }
  }

  /// Model resource id for Google Generative Language API (no `models/` prefix).
  /// Default [gemini-2.5-flash] matches typical Google AI Studio free-tier availability.
  String get coachGemmaModelId => _p.getString(keyCoachGemmaModel) ?? 'gemini-2.5-flash';

  Future<void> setCoachGemmaModelId(String v) async {
    final t = v.trim();
    if (t.isEmpty) {
      await _p.remove(keyCoachGemmaModel);
    } else {
      await _p.setString(keyCoachGemmaModel, t);
    }
  }

  /// Ordered fallback chain. Empty → coach behaves as offline-only (built-in tips).
  /// When [keyCoachAiPriority] was never set, migrates from [coachLlmBackend].
  List<CoachAiProviderId> get coachAiPriority {
    final raw = _p.getString(keyCoachAiPriority);
    if (raw != null && raw.isNotEmpty) {
      try {
        final dec = jsonDecode(raw);
        if (dec is List) {
          final out = <CoachAiProviderId>[];
          for (final e in dec) {
            final id = CoachAiProviderId.tryParse('$e');
            if (id != null && !out.contains(id)) out.add(id);
          }
          return out;
        }
      } catch (_) {}
    }
    switch (coachLlmBackend) {
      case CoachLlmBackend.offline:
        return [];
      case CoachLlmBackend.openai:
        return [CoachAiProviderId.openai];
      case CoachLlmBackend.googleGemma:
        return [CoachAiProviderId.googleGemini];
    }
  }

  Future<void> setCoachAiPriority(List<CoachAiProviderId> order) async {
    final unique = <CoachAiProviderId>[];
    for (final id in order) {
      if (!unique.contains(id)) unique.add(id);
    }
    await _p.setString(
      keyCoachAiPriority,
      jsonEncode(unique.map((e) => e.storageValue).toList()),
    );
  }

  /// Keeps older code paths coherent with the first cloud provider in the chain.
  Future<void> syncCoachLlmBackendFromPriority() async {
    final p = coachAiPriority;
    if (p.isEmpty) {
      await setCoachLlmBackend(CoachLlmBackend.offline);
      return;
    }
    if (p.first == CoachAiProviderId.googleGemini) {
      await setCoachLlmBackend(CoachLlmBackend.googleGemma);
      return;
    }
    await setCoachLlmBackend(CoachLlmBackend.openai);
  }

  String get coachOpenAiModel => _p.getString(keyCoachOpenAiModel) ?? 'gpt-4o-mini';

  Future<void> setCoachOpenAiModel(String v) async {
    final t = v.trim();
    if (t.isEmpty) {
      await _p.remove(keyCoachOpenAiModel);
    } else {
      await _p.setString(keyCoachOpenAiModel, t);
    }
  }

  String? get coachGroqApiKey => _p.getString(keyCoachGroqKey);

  Future<void> setCoachGroqApiKey(String? v) async {
    if (v == null || v.trim().isEmpty) {
      await _p.remove(keyCoachGroqKey);
    } else {
      await _p.setString(keyCoachGroqKey, v.trim());
    }
  }

  String get coachGroqModel => _p.getString(keyCoachGroqModel) ?? 'llama-3.3-70b-versatile';

  Future<void> setCoachGroqModel(String v) async {
    final t = v.trim();
    if (t.isEmpty) {
      await _p.remove(keyCoachGroqModel);
    } else {
      await _p.setString(keyCoachGroqModel, t);
    }
  }

  String? get coachXaiApiKey => _p.getString(keyCoachXaiKey);

  Future<void> setCoachXaiApiKey(String? v) async {
    if (v == null || v.trim().isEmpty) {
      await _p.remove(keyCoachXaiKey);
    } else {
      await _p.setString(keyCoachXaiKey, v.trim());
    }
  }

  String get coachXaiModel => _p.getString(keyCoachXaiModel) ?? 'grok-2-latest';

  Future<void> setCoachXaiModel(String v) async {
    final t = v.trim();
    if (t.isEmpty) {
      await _p.remove(keyCoachXaiModel);
    } else {
      await _p.setString(keyCoachXaiModel, t);
    }
  }

  /// OpenAI-compatible base, e.g. `http://127.0.0.1:11434/v1` for Ollama.
  String get coachOllamaBaseUrl =>
      _p.getString(keyCoachOllamaBase) ?? 'http://127.0.0.1:11434/v1';

  Future<void> setCoachOllamaBaseUrl(String v) async {
    final t = v.trim();
    if (t.isEmpty) {
      await _p.remove(keyCoachOllamaBase);
    } else {
      await _p.setString(keyCoachOllamaBase, t);
    }
  }

  String get coachOllamaModel => _p.getString(keyCoachOllamaModel) ?? 'llama3.2';

  Future<void> setCoachOllamaModel(String v) async {
    final t = v.trim();
    if (t.isEmpty) {
      await _p.remove(keyCoachOllamaModel);
    } else {
      await _p.setString(keyCoachOllamaModel, t);
    }
  }

  String get connectionMode => _p.getString(keyConnectionMode) ?? 'auto';
  Future<void> setConnectionMode(String mode) => _p.setString(keyConnectionMode, mode);

  String? get lastBleRemoteId => _p.getString(keyLastBleId);
  Future<void> setLastBleRemoteId(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyLastBleId);
    } else {
      await _p.setString(keyLastBleId, v);
    }
  }

  bool get preferBluetoothOnly => _p.getBool(keyPreferBluetoothOnly) ?? false;
  Future<void> setPreferBluetoothOnly(bool v) => _p.setBool(keyPreferBluetoothOnly, v);

  // New parity getters/setters
  bool get boardFlipped => _p.getBool(keyBoardFlipped) ?? false;
  Future<void> setBoardFlipped(bool v) => _p.setBool(keyBoardFlipped, v);

  bool get showCoordinates => _p.getBool(keyShowCoords) ?? true;
  Future<void> setShowCoordinates(bool v) => _p.setBool(keyShowCoords, v);

  bool get moveAnimationsEnabled => _p.getBool(keyMoveAnims) ?? true;
  Future<void> setMoveAnimationsEnabled(bool v) => _p.setBool(keyMoveAnims, v);

  bool get remoteMovesEnabled => _p.getBool(keyRemoteMoves) ?? false;
  Future<void> setRemoteMovesEnabled(bool v) => _p.setBool(keyRemoteMoves, v);

  /// Only `standard` and `boardOnly`; legacy `wide` maps to `standard`.
  String get layoutMode {
    final raw = _p.getString(keyLayoutMode) ?? 'standard';
    return raw == 'wide' ? 'standard' : raw;
  }

  Future<void> setLayoutMode(String v) async {
    final normalized = v == 'wide' ? 'standard' : v;
    await _p.setString(keyLayoutMode, normalized);
  }

  /// Desktop Play — zda je zobrazený pravý sloupec „AI trenér“ (tam kde je dost šířky).
  bool get desktopCoachRailVisible =>
      _p.getBool(keyDesktopCoachRailVisible) ?? true;

  Future<void> setDesktopCoachRailVisible(bool v) =>
      _p.setBool(keyDesktopCoachRailVisible, v);

  double get boardZoomStorage => _p.getDouble(keyBoardZoom) ?? 1.0;
  Future<void> setBoardZoomStorage(double v) => _p.setDouble(keyBoardZoom, v);

  String get boardStyleRaw => _p.getString(keyBoardStyle) ?? 'wooden';
  Future<void> setBoardStyleRaw(String v) => _p.setString(keyBoardStyle, v);

  String get appearance => _p.getString(keyAppearance) ?? 'system';
  Future<void> setAppearance(String v) => _p.setString(keyAppearance, v);

  bool get hapticsEnabled => _p.getBool(keyHaptics) ?? true;
  Future<void> setHapticsEnabled(bool v) => _p.setBool(keyHaptics, v);

  bool get soundEffectsEnabled => _p.getBool(keySound) ?? true;
  Future<void> setSoundEffectsEnabled(bool v) => _p.setBool(keySound, v);

  bool get endgameReportAutoOpen => _p.getBool(keyEndgameAutoOpen) ?? true;
  Future<void> setEndgameReportAutoOpen(bool v) => _p.setBool(keyEndgameAutoOpen, v);

  int get coachUserLevel => _p.getInt(keyCoachLevel) ?? 2;
  Future<void> setCoachUserLevel(int v) => _p.setInt(keyCoachLevel, v);

  bool get developerModeUnlocked => _p.getBool(keyDevMode) ?? false;
  Future<void> setDeveloperModeUnlocked(bool v) => _p.setBool(keyDevMode, v);

  int get hintDepth => _p.getInt(keyHintDepth) ?? 10;
  Future<void> setHintDepth(int v) => _p.setInt(keyHintDepth, v.clamp(1, 18));

  int get moveHintTier => _p.getInt(keyMoveHintTier) ?? 2;
  Future<void> setMoveHintTier(int v) => _p.setInt(keyMoveHintTier, v);

  bool get moveEvaluationEnabled => _p.getBool(keyMoveEval) ?? false;
  Future<void> setMoveEvaluationEnabled(bool v) => _p.setBool(keyMoveEval, v);

  bool get useWebSocket => _p.getBool(keyUseWebSocket) ?? true;
  Future<void> setUseWebSocket(bool v) => _p.setBool(keyUseWebSocket, v);

  bool get coachTraceLogsEnabled => _p.getBool(keyCoachTraceLogs) ?? false;
  Future<void> setCoachTraceLogsEnabled(bool v) => _p.setBool(keyCoachTraceLogs, v);

  /// Primary backend URL (chess-api JSON POST). If it fails, the app tries stockfish.online (GET) then Lichess cloud-eval (GET, cached only).
  String get stockfishApiBaseUrl => _p.getString(keyStockfishBase) ?? 'https://chess-api.com/v1';
  Future<void> setStockfishApiBaseUrl(String? v) async {
    if (v == null || v.trim().isEmpty) {
      await _p.remove(keyStockfishBase);
    } else {
      await _p.setString(keyStockfishBase, v.trim());
    }
  }

  bool get learningModeEnabled => _p.getBool(keyLearningMode) ?? false;
  Future<void> setLearningModeEnabled(bool v) => _p.setBool(keyLearningMode, v);

  String? get lastBleDisplayName => _p.getString(keyLastBleName);
  Future<void> setLastBleDisplayName(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyLastBleName);
    } else {
      await _p.setString(keyLastBleName, v);
    }
  }

  String get lastBoardLinkKind => _p.getString(keyLastLinkKind) ?? 'none';
  Future<void> setLastBoardLinkKind(String v) => _p.setString(keyLastLinkKind, v);

  bool get liveActivityEnabled => _p.getBool(keyLiveActivity) ?? false;
  Future<void> setLiveActivityEnabled(bool v) => _p.setBool(keyLiveActivity, v);

  bool get watchCompanionMirrorEnabled =>
      _p.getBool(keyWatchCompanionMirror) ?? false;
  Future<void> setWatchCompanionMirrorEnabled(bool v) =>
      _p.setBool(keyWatchCompanionMirror, v);

  bool get wearDataLayerMirrorEnabled =>
      _p.getBool(keyWearDataLayerMirror) ?? false;
  Future<void> setWearDataLayerMirrorEnabled(bool v) =>
      _p.setBool(keyWearDataLayerMirror, v);

  /// JSON pole uložených puzzlů `[{id,fen,title,solution,themes,rating}]`.
  String? get puzzleLibraryJson => _p.getString(keyPuzzleLibraryJson);
  Future<void> setPuzzleLibraryJson(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyPuzzleLibraryJson);
    } else {
      await _p.setString(keyPuzzleLibraryJson, v);
    }
  }

  /// `mixed` | `bundledOnly` | `libraryOnly` — parita iOS `PuzzleTrainingPoolMode`.
  String get puzzleTrainingPoolMode => _p.getString(keyPuzzleTrainingPool) ?? 'mixed';
  Future<void> setPuzzleTrainingPoolMode(String v) => _p.setString(keyPuzzleTrainingPool, v);

  int get puzzleTrainingSessionsStarted => _p.getInt(keyPuzzleTrainingSessions) ?? 0;
  Future<void> incrementPuzzleTrainingSessions() async {
    await _p.setInt(keyPuzzleTrainingSessions, puzzleTrainingSessionsStarted + 1);
  }

  /// Max. pozorovaný `move_count` z desky (parita `StatsRecorder.peakMoveCount` na iOS).
  int get statsPeakMoveCount => _p.getInt(keyStatsPeakMoveCount) ?? 0;
  Future<void> setStatsPeakMoveCountIfHigher(int moveCount) async {
    if (moveCount > statsPeakMoveCount) {
      await _p.setInt(keyStatsPeakMoveCount, moveCount);
    }
  }

  String? get lastNewGameTimeControl => _p.getString(keyLastNewGameTimeControl);
  Future<void> setLastNewGameTimeControl(String v) =>
      _p.setString(keyLastNewGameTimeControl, v);

  String? get firmwareManifestUrl => _p.getString(keyFirmwareManifestUrl);
  Future<void> setFirmwareManifestUrl(String? v) async {
    if (v == null || v.trim().isEmpty) {
      await _p.remove(keyFirmwareManifestUrl);
    } else {
      await _p.setString(keyFirmwareManifestUrl, v.trim());
    }
  }

  /// Prázdná prefs → výchozí manifest z [kDefaultFirmwareManifestUrl].
  String get firmwareManifestUrlEffective {
    final u = firmwareManifestUrl;
    if (u != null && u.trim().isNotEmpty) {
      return u.trim();
    }
    return kDefaultFirmwareManifestUrl;
  }

  bool get firmwareUpdateRemindersEnabled =>
      _p.getBool(keyFirmwareUpdateReminders) ?? true;

  Future<void> setFirmwareUpdateRemindersEnabled(bool v) =>
      _p.setBool(keyFirmwareUpdateReminders, v);

  String? get firmwareReminderDismissDay =>
      _p.getString(keyFirmwareReminderDismissDay);

  Future<void> setFirmwareReminderDismissDay(String? isoDay) async {
    if (isoDay == null || isoDay.isEmpty) {
      await _p.remove(keyFirmwareReminderDismissDay);
    } else {
      await _p.setString(keyFirmwareReminderDismissDay, isoDay);
    }
  }
}
