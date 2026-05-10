import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../constants/app_environment.dart';
import '../constants/firmware_defaults.dart';
import '../utils/wifi_ip_third_octet_blocklist.dart';
import '../models/chart_palette.dart';
import '../models/coach_ai_provider.dart';
import '../models/coach_llm_backend.dart';
import '../../features/game/report/game_export_block_id.dart';

class PrefsRepository {
  PrefsRepository(this._p);

  final SharedPreferences _p;

  static const keyBaseUrl = 'czechmate.boardBaseURL';
  /// 64 hex znaků — Bearer pro HTTP admin na desce (UART `API_TOKEN`).
  static const keyBoardApiToken = 'czechmate.boardApiToken';
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
  /// Jednorázově vynutí `wifi_only` / `ble_only` pro příští pokus o připojení; po dokončení
  /// `connectWifi` / `connectBle` / `connectMock` se smaže v `BoardSessionNotifier`.
  static const keyNextConnectionTransportOnce =
      'czechmate.nextConnectionTransportOnce';
  /// CSV třetích oktetů IPv4 k zamítnutí pro Wi‑Fi URL (`88` → `x.x.88.x`). Klíč chybí → výchozí `{88}`.
  static const keyWifiBlockedThirdOctets = 'czechmate.wifiBlockedThirdOctets';
  static const keyLastBleId = 'czechmate.lastBleRemoteId';
  static const keyPreferBluetoothOnly = 'czechmate.preferBluetoothOnly';
  static const keyHintDepth = 'czechmate.hintDepth';
  static const keyMoveHintTier = 'czechmate.moveHintTier';
  static const keyMoveEval = 'czechmate.moveEvaluationEnabled';
  static const keyAppearance = 'czechmate.appearance';
  /// `system` | `en` | `cs`
  static const keyUiLanguage = 'czechmate.uiLanguage';
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
  /// Absolutní cesta k uloženému `.bin` v ApplicationSupport (`firmware_cache/`).
  static const keyFirmwareCachedBinPath = 'czechmate.firmwareCachedBinPath';
  static const keyFirmwareCachedVersion = 'czechmate.firmwareCachedVersion';
  /// HTTPS URL `.bin` z manifestu v době stažení (volitelné HTTPS OTA z nastavení).
  static const keyFirmwareCachedBinSourceUrl =
      'czechmate.firmwareCachedBinSourceUrl';
  /// Verze z `app_update.json`, pro kterou uživatel zavřel banner „nová appka“.
  static const keyAppUpdateBannerDismissedLatest =
      'czechmate.appUpdateBannerDismissedLatest';
  static const keyPuzzleElo = 'czechmate.profile.puzzleElo';
  static const keyProfileDisplayName = 'czechmate.profile.displayName';
  static const keyProfileAvatarSpec = 'czechmate.profile.avatarSpec';
  static const keyActivityJson = 'czechmate.profile.activityJson';
  static const keyExportShareBlockOrder = 'czechmate.exportShareBlockOrderJson';
  static const keyChartPalettePreset = 'czechmate.chartPalettePreset';
  static const keyChartPaletteCustom = 'czechmate.chartPaletteCustomJson';

  String? get lastBoardBaseUrl => _p.getString(keyBaseUrl);
  Future<void> setLastBoardBaseUrl(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyBaseUrl);
    } else {
      await _p.setString(keyBaseUrl, v);
    }
  }

  /// Aktivní množina blokovaných 3. oktetů; klíč v prefs chybí → `{88}`.
  Set<int> get wifiBlockedThirdOctets {
    if (!_p.containsKey(keyWifiBlockedThirdOctets)) {
      return {88};
    }
    final raw = _p.getString(keyWifiBlockedThirdOctets) ?? '';
    return parseWifiBlockedThirdOctetsCsv(raw);
  }

  /// Text do pole v Pokročilém připojení (prázdný řetězec = uživatel vypnul filtr).
  String wifiBlockedThirdOctetsEditingText() {
    if (!_p.containsKey(keyWifiBlockedThirdOctets)) {
      return '88';
    }
    return _p.getString(keyWifiBlockedThirdOctets) ?? '';
  }

  /// Uloží CSV z UI; prázdný řetězec = žádné blokování (klíč zůstane s prázdnou hodnotou).
  Future<void> setWifiBlockedThirdOctetsFromUi(String csv) async {
    final t = csv.trim();
    if (!_p.containsKey(keyWifiBlockedThirdOctets) && t == '88') {
      return;
    }
    await _p.setString(keyWifiBlockedThirdOctets, t);
  }

  /// Odstraní klíč → opět výchozí blokace oktetu 88.
  Future<void> clearWifiBlockedThirdOctetsToDefault() async {
    await _p.remove(keyWifiBlockedThirdOctets);
  }

  /// CSV pro BLE příkaz `wifi_sta_ip_block` na desce — dokud uživatel nic neuložil, `88`.
  String wifiStaIpBlockCsvForBle() {
    if (!_p.containsKey(keyWifiBlockedThirdOctets)) {
      return '88';
    }
    return _p.getString(keyWifiBlockedThirdOctets) ?? '';
  }

  String? get boardApiToken => _p.getString(keyBoardApiToken);
  Future<void> setBoardApiToken(String? v) async {
    final t = v?.trim();
    if (t == null || t.isEmpty) {
      await _p.remove(keyBoardApiToken);
    } else {
      await _p.setString(keyBoardApiToken, t);
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

  /// Uložený základní režim (`auto` po [migrateLegacyConnectionModeToAutoIfNeeded]).
  ///
  /// Pro rozhodování při připojení používej [effectiveConnectionMode] — zohlední
  /// jednorázový přepis [nextConnectionTransportOnce].
  ///
  /// Starý příznak [keyPreferBluetoothOnly] se bere v úvahu jen pokud není uložen
  /// platný řetězec v [keyConnectionMode] — pak se mapuje na `ble_only`.
  String get connectionMode {
    final raw = _p.getString(keyConnectionMode)?.trim();
    if (raw != null &&
        (raw == 'wifi_only' || raw == 'ble_only' || raw == 'auto')) {
      return raw;
    }
    if (_p.getBool(keyPreferBluetoothOnly) ?? false) {
      return 'ble_only';
    }
    return 'auto';
  }

  /// `wifi_only` | `ble_only` | null — jen příští pokus o připojení (pak se smaže).
  String? get nextConnectionTransportOnce {
    final v = _p.getString(keyNextConnectionTransportOnce)?.trim();
    if (v == 'wifi_only' || v == 'ble_only') return v;
    return null;
  }

  Future<void> setNextConnectionTransportOnce(String? mode) async {
    final m = mode?.trim();
    if (m == null || m.isEmpty) {
      await _p.remove(keyNextConnectionTransportOnce);
      return;
    }
    if (m == 'wifi_only' || m == 'ble_only') {
      await _p.setString(keyNextConnectionTransportOnce, m);
    } else {
      await _p.remove(keyNextConnectionTransportOnce);
    }
  }

  /// Reálný režim pro připojení: jednorázový přepis má přednost, jinak vždy `auto`.
  String get effectiveConnectionMode =>
      nextConnectionTransportOnce ?? 'auto';

  /// Staré trvalé `wifi_only` / `ble_only` převede na `auto`; uživatele tak nic „nezasekne“.
  Future<void> migrateLegacyConnectionModeToAutoIfNeeded() async {
    final raw = _p.getString(keyConnectionMode)?.trim();
    final legacyBleFlag = _p.getBool(keyPreferBluetoothOnly) ?? false;
    final restricted = raw == 'wifi_only' ||
        raw == 'ble_only' ||
        (raw == null && legacyBleFlag);
    if (!restricted) return;
    await _p.setString(keyConnectionMode, 'auto');
    await _p.setBool(keyPreferBluetoothOnly, false);
  }

  Future<void> setConnectionMode(String mode) async {
    final m = mode == 'wifi_only' || mode == 'ble_only' || mode == 'auto'
        ? mode
        : 'auto';
    final prev = connectionMode;
    await _p.setString(keyConnectionMode, m);
    await _p.setBool(keyPreferBluetoothOnly, false);
    if (prev != m && AppEnvironment.staging) {
      debugPrint('[staging] connectionMode: $prev → $m');
    }
  }

  String? get lastBleRemoteId => _p.getString(keyLastBleId);
  Future<void> setLastBleRemoteId(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyLastBleId);
    } else {
      await _p.setString(keyLastBleId, v);
    }
  }

  /// Jednorázové vynucení jen BLE pro příští připojení.
  bool get preferBluetoothOnly => effectiveConnectionMode == 'ble_only';

  Future<void> setPreferBluetoothOnly(bool v) async {
    if (v) {
      await setNextConnectionTransportOnce('ble_only');
    } else {
      await setNextConnectionTransportOnce(null);
    }
  }

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

  /// `system` | `en` | `cs`
  String get uiLanguage => _p.getString(keyUiLanguage) ?? 'system';
  Future<void> setUiLanguage(String v) => _p.setString(keyUiLanguage, v);

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
      return;
    }
    final n = normalizeFirmwareManifestUrl(v.trim());
    if (n == kDefaultFirmwareManifestUrl) {
      await _p.remove(keyFirmwareManifestUrl);
    } else {
      await _p.setString(keyFirmwareManifestUrl, n);
    }
  }

  /// Prázdná prefs → výchozí manifest z [kDefaultFirmwareManifestUrl].
  /// Uložená hodnota se normalizuje (oprava zkrácených GitHub Pages URL).
  String get firmwareManifestUrlEffective {
    final u = firmwareManifestUrl?.trim();
    if (u == null || u.isEmpty) return kDefaultFirmwareManifestUrl;
    return normalizeFirmwareManifestUrl(u);
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

  String? get firmwareCachedBinPath => _p.getString(keyFirmwareCachedBinPath);

  String? get firmwareCachedVersion => _p.getString(keyFirmwareCachedVersion);

  String? get firmwareCachedBinSourceUrl =>
      _p.getString(keyFirmwareCachedBinSourceUrl);

  Future<void> setFirmwareCachedBin({
    required String absolutePath,
    required String version,
    String? httpsBinSourceUrl,
  }) async {
    await _p.setString(keyFirmwareCachedBinPath, absolutePath);
    await _p.setString(keyFirmwareCachedVersion, version);
    final u = httpsBinSourceUrl?.trim();
    if (u != null && u.isNotEmpty) {
      await _p.setString(keyFirmwareCachedBinSourceUrl, u);
    } else {
      await _p.remove(keyFirmwareCachedBinSourceUrl);
    }
  }

  Future<void> clearFirmwareCachedBin() async {
    await _p.remove(keyFirmwareCachedBinPath);
    await _p.remove(keyFirmwareCachedVersion);
    await _p.remove(keyFirmwareCachedBinSourceUrl);
  }

  String? get appUpdateBannerDismissedLatest =>
      _p.getString(keyAppUpdateBannerDismissedLatest);

  Future<void> setAppUpdateBannerDismissedLatest(String version) async {
    final t = version.trim();
    if (t.isEmpty) {
      await _p.remove(keyAppUpdateBannerDismissedLatest);
    } else {
      await _p.setString(keyAppUpdateBannerDismissedLatest, t);
    }
  }

  /// Puzzle-only Elo (oddělené od případného online ratingu).
  int get puzzleElo => _p.getInt(keyPuzzleElo) ?? 1200;

  Future<void> setPuzzleElo(int v) =>
      _p.setInt(keyPuzzleElo, v.clamp(100, 4000));

  /// Očekávané body za výhru nad „soupeřem“ = obtížnost puzzlu.
  Future<int> applyPuzzleSolveElo({int? puzzleRating}) async {
    final user = puzzleElo.toDouble();
    final opp = (puzzleRating ?? 1500).clamp(400, 3500).toDouble();
    final expected = 1.0 / (1.0 + math.pow(10, (opp - user) / 400.0));
    const k = 24.0;
    var delta = (k * (1.0 - expected)).round();
    delta = delta.clamp(5, 48);
    await setPuzzleElo(puzzleElo + delta);
    return delta;
  }

  String get profileDisplayName =>
      _p.getString(keyProfileDisplayName)?.trim().isNotEmpty == true
          ? _p.getString(keyProfileDisplayName)!.trim()
          : 'Player';

  /// Uložené jméno z profilu, nebo `null` když uživatel nic nezadal (UI pak použije výchozí „Player“).
  String? get profileDisplayNameStoredOrNull {
    final s = _p.getString(keyProfileDisplayName)?.trim();
    if (s == null || s.isEmpty) return null;
    return s;
  }

  Future<void> setProfileDisplayName(String value) async {
    final t = value.trim();
    if (t.isEmpty) {
      await _p.remove(keyProfileDisplayName);
    } else {
      await _p.setString(keyProfileDisplayName, t);
    }
  }

  /// `default:0` … `default:4` nebo `file:<absolutní cesta>`.
  String get profileAvatarSpec =>
      _p.getString(keyProfileAvatarSpec) ?? 'default:0';

  Future<void> setProfileAvatarSpec(String spec) =>
      _p.setString(keyProfileAvatarSpec, spec);

  /// Mapa `YYYY-MM-DD` → `{s: vyřešeno, f: špatně}`.
  Future<void> recordPuzzleActivity({required bool solved}) async {
    final day = DateTime.now().toIso8601String().split('T').first;
    final map = <String, dynamic>{};
    final raw = _p.getString(keyActivityJson);
    if (raw != null && raw.trim().isNotEmpty) {
      try {
        final dec = jsonDecode(raw);
        if (dec is Map<String, dynamic>) {
          map.addAll(dec);
        }
      } catch (_) {}
    }
    final prev = map[day];
    var sCount = 0;
    var fCount = 0;
    if (prev is Map) {
      sCount = (prev['s'] as num?)?.toInt() ?? 0;
      fCount = (prev['f'] as num?)?.toInt() ?? 0;
    }
    if (solved) {
      sCount++;
    } else {
      fCount++;
    }
    map[day] = {'s': sCount, 'f': fCount};
    await _p.setString(keyActivityJson, jsonEncode(map));
  }

  Map<String, Map<String, int>> puzzleActivityByDay() {
    final raw = _p.getString(keyActivityJson);
    if (raw == null || raw.trim().isEmpty) return {};
    try {
      final dec = jsonDecode(raw);
      if (dec is! Map<String, dynamic>) return {};
      final out = <String, Map<String, int>>{};
      dec.forEach((k, v) {
        if (v is Map) {
          out[k] = {
            's': (v['s'] as num?)?.toInt() ?? 0,
            'f': (v['f'] as num?)?.toInt() ?? 0,
          };
        }
      });
      return out;
    } catch (_) {
      return {};
    }
  }

  ({int solved7d, int failed7d}) puzzleStatsLastDays(int days) {
    final map = puzzleActivityByDay();
    var s = 0;
    var f = 0;
    final now = DateTime.now();
    for (var i = 0; i < days; i++) {
      final d = DateTime(now.year, now.month, now.day)
          .subtract(Duration(days: i));
      final key =
          '${d.year.toString().padLeft(4, '0')}-${d.month.toString().padLeft(2, '0')}-${d.day.toString().padLeft(2, '0')}';
      final e = map[key];
      if (e != null) {
        s += e['s'] ?? 0;
        f += e['f'] ?? 0;
      }
    }
    return (solved7d: s, failed7d: f);
  }

  /// Order of blocks on the game summary PNG/GIF export canvas.
  List<GameExportBlockId> get exportShareBlockOrder {
    final raw = _p.getString(keyExportShareBlockOrder);
    if (raw == null || raw.isEmpty) {
      return normalizeGameExportBlockOrder(null);
    }
    try {
      final list = jsonDecode(raw) as List<dynamic>;
      final ids = <GameExportBlockId>[];
      for (final e in list) {
        final id = gameExportBlockIdFromStorage('$e');
        if (id != null && !ids.contains(id)) ids.add(id);
      }
      return normalizeGameExportBlockOrder(ids);
    } catch (_) {
      return normalizeGameExportBlockOrder(null);
    }
  }

  Future<void> setExportShareBlockOrder(List<GameExportBlockId> order) async {
    final normalized = normalizeGameExportBlockOrder(order);
    await _p.setString(
      keyExportShareBlockOrder,
      jsonEncode(normalized.map((e) => e.name).toList()),
    );
  }

  ChartPalettePreset get chartPalettePreset =>
      ChartPalettePreset.fromStorage(_p.getString(keyChartPalettePreset));

  Future<void> setChartPalettePreset(ChartPalettePreset v) async {
    await _p.setString(keyChartPalettePreset, v.name);
  }

  ChartPaletteColors? get chartPaletteCustom {
    final raw = _p.getString(keyChartPaletteCustom);
    if (raw == null || raw.isEmpty) return null;
    try {
      final dec = jsonDecode(raw);
      if (dec is! Map<String, dynamic>) return null;
      return ChartPaletteColors.fromJson(dec);
    } catch (_) {
      return null;
    }
  }

  Future<void> setChartPaletteCustom(ChartPaletteColors colors) async {
    await _p.setString(keyChartPaletteCustom, jsonEncode(colors.toJson()));
  }

  ChartPaletteColors resolvedChartPalette(ColorScheme colorScheme) {
    final preset = chartPalettePreset;
    if (preset == ChartPalettePreset.custom) {
      return chartPaletteCustom ?? ChartPaletteColors.neon;
    }
    return ChartPaletteColors.forPreset(preset, colorScheme);
  }

  /// Smaže lokální data aplikace: `SharedPreferences`, cache staženého firmwaru
  /// (`ApplicationSupport/…/firmware_cache`). **Nemění** firmware ani nastavení na desce.
  Future<void> factoryResetApplicationLocalState() async {
    final legacyBin = firmwareCachedBinPath;
    try {
      final support = await getApplicationSupportDirectory();
      final firmwareDir = Directory('${support.path}/firmware_cache');
      if (await firmwareDir.exists()) {
        await for (final entity in firmwareDir.list()) {
          if (entity is File) {
            try {
              await entity.delete();
            } catch (_) {}
          }
        }
      }
    } catch (_) {}
    if (legacyBin != null && legacyBin.isNotEmpty) {
      try {
        final f = File(legacyBin);
        if (await f.exists()) await f.delete();
      } catch (_) {}
    }
    await _p.clear();
  }
}
