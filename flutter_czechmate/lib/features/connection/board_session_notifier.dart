import 'dart:async';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/models/board_timer_state.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/models/game_status_models.dart';
import '../../core/services/ble_czechmate_client.dart';
import '../../core/services/board_api_client.dart';
import '../../core/services/board_api_exception.dart';
import '../../core/services/mock_board_simulator.dart';
import '../../core/services/prefs_repository.dart';
import '../../core/services/web_socket_manager.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/utils/game_snapshot_codec.dart';
import 'board_session_state.dart';

final boardSessionNotifierProvider =
    StateNotifierProvider<BoardSessionNotifier, BoardSessionState>((ref) {
  return BoardSessionNotifier(
    ref.read(boardApiClientProvider),
    ref.read(prefsRepositoryProvider),
  );
});

class BoardSessionNotifier extends StateNotifier<BoardSessionState> {
  BoardSessionNotifier(this._api, this._prefs)
      : super(const BoardSessionState());

  final BoardApiClient _api;
  final PrefsRepository _prefs;

  Timer? _pollTimer;
  Timer? _mockClockTimer;
  DateTime? _mockLastTickAt;
  final SnapshotWebSocketClient _ws = SnapshotWebSocketClient();
  final BleCzechmateClient _ble = BleCzechmateClient();

  /// Po přechodu do konce partie jednou pošleme `timer_pause`, aby čas na desce nestál „v běhu“.
  bool _pauseTimerSentForFinishedGame = false;

  @override
  void dispose() {
    _stopMockClock();
    _stopWifiTransport();
    unawaited(_ble.disconnect());
    super.dispose();
  }

  Future<void> tryResumeFromPrefs() async {
    if (_prefs.useMockBoard) {
      await connectMock();
      return;
    }
    final mode = _prefs.connectionMode;
    final preferBleOnly = _prefs.preferBluetoothOnly || mode == 'ble_only';
    final wifiOnly = mode == 'wifi_only';
    final savedBle = _prefs.lastBleRemoteId;
    final hasSavedBle = savedBle != null && savedBle.isNotEmpty;
    final url = _prefs.lastBoardBaseUrl?.trim();
    final hasWifiUrl = url != null && url.isNotEmpty;
    final lastKind = _prefs.lastBoardLinkKind;

    Future<bool> tryBle() async {
      if (!hasSavedBle) return false;
      await reconnectSavedBle();
      return state.transport == BoardTransport.ble && !state.busy;
    }

    Future<bool> tryWifi() async {
      final base = url;
      if (!hasWifiUrl || base == null) return false;
      await connectWifi(base, webSocket: _prefs.useWebSocket);
      return state.transport == BoardTransport.wifi && !state.busy;
    }

    if (preferBleOnly) {
      if (await tryBle()) return;
      if (!wifiOnly) {
        await tryWifi();
      }
      return;
    }

    if (wifiOnly) {
      await tryWifi();
      return;
    }

    if (lastKind == 'bluetooth' && await tryBle()) return;
    if (await tryWifi()) return;
    await tryBle();
  }

  Future<bool> _phoneHasWifiInterface() async {
    try {
      final types = await Connectivity().checkConnectivity();
      return types.contains(ConnectivityResult.wifi);
    } catch (_) {
      return false;
    }
  }

  bool _looksLikeIPv4(String ip) {
    final p = ip.trim().split('.');
    if (p.length != 4) return false;
    for (final x in p) {
      final v = int.tryParse(x);
      if (v == null || v < 0 || v > 255) return false;
    }
    return true;
  }

  Future<void> _maybeAutoSwitchBleToWifi() async {
    final mode = _prefs.connectionMode;
    if (_prefs.preferBluetoothOnly || mode == 'ble_only') return;
    final info = await _ble.fetchNetworkInfo();
    final staIp = info?.staIp;
    if (info == null ||
        info.online != true ||
        staIp == null ||
        !_looksLikeIPv4(staIp)) {
      return;
    }
    if (!await _phoneHasWifiInterface()) return;
    final url = 'http://$staIp';
    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE→Wi‑Fi handoff: $url');
    }
    await connectWifi(url, webSocket: _prefs.useWebSocket);
  }

  Future<void> connectMock() async {
    _stopWifiTransport();
    await _ble.disconnect();
    state = state.copyWith(busy: true, transport: BoardTransport.mock);
    try {
      final raw = await rootBundle.loadString('assets/snapshot_golden.json');
      final snap = GameSnapshotCodec.decodeRepairingAndNormalizing(raw);
      await _prefs.setUseMockBoard(true);
      await _prefs.setLastBoardLinkKind('mock');
      final timer = _demoTimerFromSnapshot(snap);
      _pauseTimerSentForFinishedGame = false;
      state = state.copyWith(
        snapshot: snap,
        snapshotReceivedAt: DateTime.now(),
        transportStartedAt: DateTime.now(),
        timer: timer,
        busy: false,
        bleGattConnected: false,
        clearError: true,
        lastIfNoneMatch: snap.etagTag,
      );
      _startMockClock();
    } catch (e) {
      state = state.copyWith(busy: false, lastError: e);
    }
  }

  Future<void> connectWifi(String baseUrl, {bool? webSocket}) async {
    final useWs = webSocket ?? _prefs.useWebSocket;
    _stopWifiTransport();
    await _ble.disconnect();
    final normalized = normalizeBoardHttpBaseUrl(baseUrl);
    if (normalized == null) {
      state = state.copyWith(
        busy: false,
        lastError: StateError(
          'Invalid board URL — hostname missing. Use http://192.168.4.1 or your board STA IP.',
        ),
      );
      return;
    }
    await _prefs.setLastBoardBaseUrl(normalized);
    await _prefs.setUseMockBoard(false);
    state = state.copyWith(
      transport: BoardTransport.wifi,
      wifiBaseUrl: normalized,
      transportStartedAt: DateTime.now(),
      busy: true,
      bleGattConnected: false,
      clearError: true,
    );
    try {
      await _prefs.setLastBoardLinkKind('wifi');
      await _pollOnce(normalized);
      _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) {
        unawaited(_pollOnce(normalized));
      });
      state = state.copyWith(pollingActive: true, busy: false);
      if (useWs) {
        _ws.connect(
          normalized,
          onSnapshot: (s) => _applySnapshot(s, normalized),
          onError: (e) {
            if (AppEnvironment.staging) {
              debugPrint('[staging] WS error: $e');
            }
          },
          onDisconnect: () {
            state = state.copyWith(webSocketActive: false);
          },
          onMessageReceived: () {
            state = state.copyWith(wsMessageCount: state.wsMessageCount + 1);
          },
        );
        state = state.copyWith(webSocketActive: true);
      }
    } catch (e) {
      state = state.copyWith(
        busy: false,
        lastError: e,
        transport: BoardTransport.none,
        bleGattConnected: false,
        clearSnapshot: true,
        clearSnapshotReceivedAt: true,
        clearTransportStartedAt: true,
        clearTimer: true,
      );
    }
  }

  Future<void> connectBle(BluetoothDevice device, {String? label}) async {
    _stopWifiTransport();
    await _ble.disconnect();
    state = state.copyWith(
      transport: BoardTransport.ble,
      transportStartedAt: DateTime.now(),
      busy: true,
      bleGattConnected: false,
      bleLabel: label ?? device.platformName,
      clearError: true,
    );
    try {
      await _ble.connect(
        device,
        onSnapshot: (s) => _applySnapshot(s, ''),
        onError: (e) =>
            state = state.copyWith(lastError: e, bleGattConnected: false),
      );
      await _prefs.setLastBleRemoteId(device.remoteId.str);
      await _prefs.setLastBleDisplayName(label ?? device.platformName);
      await _prefs.setUseMockBoard(false);
      await _prefs.setLastBoardLinkKind('bluetooth');
      state = state.copyWith(busy: false, bleGattConnected: true);
      await _maybeAutoSwitchBleToWifi();
    } catch (e) {
      state = state.copyWith(
        busy: false,
        lastError: e,
        transport: BoardTransport.none,
        bleGattConnected: false,
        clearSnapshot: true,
        clearSnapshotReceivedAt: true,
        clearTransportStartedAt: true,
        clearTimer: true,
      );
    }
  }

  /// Znovu připojit podle `czechmate.lastBleRemoteId` (bez skenu).
  Future<void> reconnectSavedBle() async {
    final id = _prefs.lastBleRemoteId;
    if (id == null || id.isEmpty) {
      state =
          state.copyWith(lastError: StateError(
              'No saved Bluetooth board. Open Board discovery and pair your CZECHMATE board.',
            ));
      return;
    }
    final dev = BluetoothDevice(remoteId: DeviceIdentifier(id));
    await connectBle(dev, label: _prefs.lastBleDisplayName);
  }

  void disconnect() {
    _stopMockClock();
    _stopWifiTransport();
    unawaited(_ble.disconnect());
    _pauseTimerSentForFinishedGame = false;
    state = const BoardSessionState();
  }

  void _stopMockClock() {
    _mockClockTimer?.cancel();
    _mockClockTimer = null;
    _mockLastTickAt = null;
  }

  BoardTimerState? _demoTimerFromSnapshot(GameSnapshot snap) {
    final c = snap.clock;
    if (c != null && c.isTimeControlEnabled) return c;
    final wt = snap.status.whiteTime ?? 300;
    final bt = snap.status.blackTime ?? 300;
    const cfg = TimerConfigPart(
      type: 6,
      name: 'Demo',
      description: 'Simulated',
      initialTimeMs: 300000,
      incrementMs: 0,
      isFast: true,
    );
    return BoardTimerState(
      whiteTimeMs: wt * 1000,
      blackTimeMs: bt * 1000,
      timerRunning:
          snap.status.isTimerRunning && !snap.status.isGameFinished,
      isWhiteTurn: snap.status.currentPlayer.toLowerCase().startsWith('w'),
      gamePaused: false,
      timeExpired: false,
      config: cfg,
      totalMoves: snap.history.moves.length,
      avgMoveTimeMs: 0,
    );
  }

  void _startMockClock() {
    _stopMockClock();
    if (state.transport != BoardTransport.mock) return;
    final t = state.timer;
    if (t == null || !t.isTimeControlEnabled) return;
    if (state.snapshot?.status.isGameFinished == true) return;
    _mockLastTickAt = DateTime.now();
    _mockClockTimer =
        Timer.periodic(const Duration(milliseconds: 300), (_) => _mockClockTick());
  }

  void _mockClockTick() {
    if (state.transport != BoardTransport.mock) return;
    final t = state.timer;
    if (t == null || !t.isTimeControlEnabled) return;
    if (!t.timerRunning || t.gamePaused || t.timeExpired) return;
    final snap = state.snapshot;
    if (snap == null || snap.status.isGameFinished) return;

    final now = DateTime.now();
    final prev = _mockLastTickAt ?? now;
    final dt = now.difference(prev).inMilliseconds;
    _mockLastTickAt = now;
    if (dt <= 0 || dt > 5000) return;

    var w = t.whiteTimeMs;
    var b = t.blackTimeMs;
    if (t.isWhiteTurn) {
      w -= dt;
      if (w <= 0) {
        _mockApplyTimeout(loserIsWhite: true);
        return;
      }
    } else {
      b -= dt;
      if (b <= 0) {
        _mockApplyTimeout(loserIsWhite: false);
        return;
      }
    }

    state = state.copyWith(
      timer: BoardTimerState(
        whiteTimeMs: w,
        blackTimeMs: b,
        timerRunning: t.timerRunning,
        isWhiteTurn: t.isWhiteTurn,
        gamePaused: t.gamePaused,
        timeExpired: t.timeExpired,
        config: t.config,
        totalMoves: t.totalMoves,
        avgMoveTimeMs: t.avgMoveTimeMs,
      ),
    );
  }

  void _mockApplyTimeout({required bool loserIsWhite}) {
    final snap = state.snapshot;
    final t = state.timer;
    if (snap == null || t == null) return;

    final winner = loserIsWhite ? 'Black' : 'White';
    final loser = loserIsWhite ? 'White' : 'Black';
    final end = GameEndStatus(
      ended: true,
      reason: 'timeout',
      winner: winner,
      loser: loser,
    );

    final newStatus = GameStatus(
      gameState: 'finished',
      currentPlayer: snap.status.currentPlayer,
      moveCount: snap.status.moveCount,
      whiteTime: loserIsWhite ? 0 : (t.whiteTimeMs ~/ 1000),
      blackTime: loserIsWhite ? (t.blackTimeMs ~/ 1000) : 0,
      inCheck: snap.status.inCheck,
      checkmate: snap.status.checkmate,
      stalemate: snap.status.stalemate,
      webLocked: snap.status.webLocked,
      internetConnected: snap.status.internetConnected,
      brightness: snap.status.brightness,
      castlingInProgress: snap.status.castlingInProgress,
      gameEnd: end,
      pieceLifted: snap.status.pieceLifted,
      errorState: snap.status.errorState,
      restoreState: snap.status.restoreState,
      boardSetupTutorial: snap.status.boardSetupTutorial,
      puzzle: snap.status.puzzle,
      guidedCaptureHintsEnabled: snap.status.guidedCaptureHintsEnabled,
      ledGuidanceLevel: snap.status.ledGuidanceLevel,
      matrixGuardActive: snap.status.matrixGuardActive,
      matrixGuardConflicts: snap.status.matrixGuardConflicts,
      matrixGuardLiftedLow: snap.status.matrixGuardLiftedLow,
      matrixGuardLiftedHigh: snap.status.matrixGuardLiftedHigh,
      matrixGuardDroppedLow: snap.status.matrixGuardDroppedLow,
      matrixGuardDroppedHigh: snap.status.matrixGuardDroppedHigh,
      lightMode: snap.status.lightMode,
      lightState: snap.status.lightState,
      lightR: snap.status.lightR,
      lightG: snap.status.lightG,
      lightB: snap.status.lightB,
      autoLampTimeoutSec: snap.status.autoLampTimeoutSec,
      chessHintLimit: snap.status.chessHintLimit,
      matrixOccupied: snap.status.matrixOccupied,
    );

    final newSnap = GameSnapshot(
      stateVersion: (snap.stateVersion ?? 0) + 1,
      board: snap.board,
      timestamp: DateTime.now().millisecondsSinceEpoch,
      status: newStatus,
      history: snap.history,
      captured: snap.captured,
      clock: snap.clock,
      gameId: snap.gameId,
    );

    state = state.copyWith(
      snapshot: newSnap,
      timer: BoardTimerState(
        whiteTimeMs: loserIsWhite ? 0 : t.whiteTimeMs,
        blackTimeMs: loserIsWhite ? t.blackTimeMs : 0,
        timerRunning: false,
        isWhiteTurn: t.isWhiteTurn,
        gamePaused: true,
        timeExpired: true,
        config: t.config,
        totalMoves: t.totalMoves,
        avgMoveTimeMs: t.avgMoveTimeMs,
      ),
      lastIfNoneMatch: newSnap.etagTag,
    );

    if (!_pauseTimerSentForFinishedGame) {
      _pauseTimerSentForFinishedGame = true;
      unawaited(_pauseTimerAfterGameEnd());
    }
  }

  void _stopWifiTransport() {
    _pollTimer?.cancel();
    _pollTimer = null;
    _ws.disconnect();
    if (state.webSocketActive) {
      state = state.copyWith(webSocketActive: false, pollingActive: false);
    } else {
      state = state.copyWith(pollingActive: false);
    }
  }

  Future<void> _pollOnce(String base) async {
    try {
      final tag = state.lastIfNoneMatch;
      final s = await _api.fetchSnapshotIfChanged(base, ifNoneMatch: tag);
      if (s != null) _applySnapshot(s, base);
      state = state.copyWith(
        pollSuccessCount: state.pollSuccessCount + 1,
        lastSuccessfulPoll: DateTime.now(),
      );
    } catch (e) {
      state = state.copyWith(
        lastError: e,
        pollFailureCount: state.pollFailureCount + 1,
      );
    }
  }

  void _applySnapshot(GameSnapshot s, String baseForTimer) {
    final prev = state.snapshot;
    var merged = s;
    if (prev != null) {
      merged = merged.replacingStatus(
        s.status.coalescingTransientLampOff(prev.status),
      );
    }
    final nowFinished = merged.status.isGameFinished;
    final wasFinished = prev?.status.isGameFinished ?? false;
    if (!nowFinished) {
      _pauseTimerSentForFinishedGame = false;
    } else if (!wasFinished && !_pauseTimerSentForFinishedGame) {
      _pauseTimerSentForFinishedGame = true;
      unawaited(_pauseTimerAfterGameEnd());
    }

    state = state.copyWith(
      snapshot: merged,
      snapshotReceivedAt: DateTime.now(),
      lastIfNoneMatch: merged.etagTag ?? state.lastIfNoneMatch,
      clearError: true,
    );
    unawaited(_prefs.setStatsPeakMoveCountIfHigher(merged.status.moveCount));
    unawaited(_refreshTimer(baseForTimer, merged));
  }

  Future<void> _pauseTimerAfterGameEnd() async {
    try {
      await postTimerPause();
    } catch (_) {
      // Deska může čas už sama zastavit; ignoruj chybu sítě/GATT.
    }
  }

  Future<void> _refreshTimer(String base, GameSnapshot snap) async {
    if (snap.clock != null) {
      state = state.copyWith(timer: snap.clock);
      return;
    }
    if (base.isEmpty) return;
    try {
      final t = await _api.fetchTimerState(base);
      state = state.copyWith(timer: t);
    } catch (_) {}
  }

  Future<void> refreshNow() async {
    if (state.transport == BoardTransport.mock) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] mock refreshNow: skipped (game state preserved)');
      }
      return;
    }
    final b = state.wifiBaseUrl?.trim();
    if (state.transport == BoardTransport.wifi &&
        b != null &&
        b.isNotEmpty) {
      await _pollOnce(b);
      return;
    }
    if (state.transport == BoardTransport.ble && state.bleGattConnected) {
      try {
        final s = await _ble.readSnapshot();
        _applySnapshot(s, '');
        if (AppEnvironment.staging) {
          debugPrint('[staging] BLE snapshot refresh (GATT read)');
        }
      } catch (e) {
        state = state.copyWith(lastError: e);
      }
    }
  }

  Future<void> postRemoteMove(String from, String to,
      {String? promotion}) async {
    if (state.transport == BoardTransport.mock) {
      final snap = state.snapshot;
      if (snap == null) {
        throw StateError('Demo board has no snapshot.');
      }
      try {
        if (AppEnvironment.staging) {
          debugPrint(
            '[staging] mock postRemoteMove $from→$to promotion=$promotion',
          );
        }
        final r = mockApplyRemoteMove(
          snap: snap,
          timer: state.timer,
          from: from,
          to: to,
          promotion: promotion,
        );
        state = state.copyWith(
          snapshot: r.snapshot,
          timer: r.timer,
          clearError: true,
          lastIfNoneMatch: r.snapshot.etagTag,
        );
        if (r.snapshot.status.isGameFinished && !_pauseTimerSentForFinishedGame) {
          _pauseTimerSentForFinishedGame = true;
          unawaited(_pauseTimerAfterGameEnd());
        }
      } on BoardApiException {
        rethrow;
      }
      return;
    }
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final b = state.wifiBaseUrl!;
      await _api.postGameMove(b, from: from, to: to, promotion: promotion);
      await _pollOnce(b);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postMove(from, to, promotion: promotion);
      return;
    }
    throw StateError(
        'Connect to the board over Wi‑Fi or Bluetooth before sending moves from the app.',
      );
  }

  Future<void> postNewGame({String? fen}) async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final b = state.wifiBaseUrl!;
      await _api.postNewGame(b, fen: fen);
      await _pollOnce(b);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postNewGame(fen: fen);
      return;
    }
    if (state.transport == BoardTransport.mock) {
      await connectMock();
    }
  }

  /// Puzzle / vlastní FEN na desku — Wi‑Fi HTTP nebo BLE příkaz `new_game`.
  Future<void> sendPuzzleFenToBoard(String fen) async {
    final f = fen.trim();
    if (f.isEmpty) return;
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await postNewGame(fen: f);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      try {
        await _ble.postNewGame(fen: f);
      } catch (e) {
        state = state.copyWith(lastError: e);
      }
    }
  }

  Future<void> postTimerPause() async {
    if (state.transport == BoardTransport.mock) {
      final t = state.timer;
      if (t == null || !t.isTimeControlEnabled) return;
      state = state.copyWith(
        timer: BoardTimerState(
          whiteTimeMs: t.whiteTimeMs,
          blackTimeMs: t.blackTimeMs,
          timerRunning: false,
          isWhiteTurn: t.isWhiteTurn,
          gamePaused: true,
          timeExpired: t.timeExpired,
          config: t.config,
          totalMoves: t.totalMoves,
          avgMoveTimeMs: t.avgMoveTimeMs,
        ),
      );
      return;
    }
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final b = state.wifiBaseUrl!;
      await _api.postTimerPause(b);
      await _refreshTimerHttp(b);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postTimerPause();
    }
  }

  Future<void> postTimerResume() async {
    if (state.transport == BoardTransport.mock) {
      final t = state.timer;
      final snap = state.snapshot;
      if (t == null || !t.isTimeControlEnabled) return;
      final finished = snap?.status.isGameFinished ?? false;
      state = state.copyWith(
        timer: BoardTimerState(
          whiteTimeMs: t.whiteTimeMs,
          blackTimeMs: t.blackTimeMs,
          timerRunning: !finished,
          isWhiteTurn: t.isWhiteTurn,
          gamePaused: false,
          timeExpired: t.timeExpired,
          config: t.config,
          totalMoves: t.totalMoves,
          avgMoveTimeMs: t.avgMoveTimeMs,
        ),
      );
      _mockLastTickAt = DateTime.now();
      return;
    }
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final b = state.wifiBaseUrl!;
      await _api.postTimerResume(b);
      await _refreshTimerHttp(b);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postTimerResume();
    }
  }

  Future<void> _refreshTimerHttp(String base) async {
    try {
      final t = await _api.fetchTimerState(base);
      state = state.copyWith(timer: t);
    } catch (_) {}
  }

  /// `POST /api/game/setup_tutorial` nebo BLE `setup_tutorial` — `start` | `cancel` | `finish`.
  Future<void> postSetupTutorialAction(String action) async {
    final a = action.trim().toLowerCase();
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _api.postSetupTutorial(state.wifiBaseUrl!, action: a);
      if (a != 'start') await refreshNow();
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postSetupTutorial(a);
      return;
    }
    throw StateError(
        'Setup wizard requires an active board connection (Wi‑Fi or Bluetooth).',
      );
  }

  /// LED jen na cílovém poli (`hint_highlight` s `to`).
  Future<void> postHintDestination(String square) async {
    final sq = square.trim().toLowerCase();
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _api.postHintHighlightDestinationOnly(state.wifiBaseUrl!, sq);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postHintHighlightDestinationOnly(sq);
      return;
    }
    throw StateError(
      'Hint LEDs require a board connection. Pair via Bluetooth or connect Wi‑Fi first.',
    );
  }

  Future<void> postHintClearBoard() async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _api.postHintClear(state.wifiBaseUrl!);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postHintClear();
    }
  }

  /// Reed matice: Wi‑Fi `GET /api/status`, BLE poslední `snapshot.status.matrix_occupied`.
  Future<List<int>?> fetchMatrixOccupiedForWizard() async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      try {
        return await _api.fetchMatrixOccupied(state.wifiBaseUrl!);
      } catch (_) {
        return null;
      }
    }
    return state.snapshot?.status.matrixOccupied;
  }

  /// Parita iOS `startNewGameWithTimeControl` — `timer_config` pak `new_game`.
  Future<void> startNewGameWithTimeControl({
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) async {
    if (AppEnvironment.staging) {
      debugPrint(
        '[staging] startNewGameWithTimeControl type=$type min=$customMinutes inc=$customIncrement ${state.transport}',
      );
    }
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final b = state.wifiBaseUrl!;
      await _api.postTimerConfig(
        b,
        type: type,
        customMinutes: customMinutes,
        customIncrement: customIncrement,
      );
      await _api.postNewGame(b);
      await _pollOnce(b);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postTimerConfig(
        type,
        customMinutes: customMinutes,
        customIncrement: customIncrement,
      );
      await _ble.postNewGame();
      return;
    }
    if (state.transport == BoardTransport.mock) {
      final cfg = mockTimerConfigForFirmware(
        type: type,
        customMinutes: customMinutes,
        customIncrement: customIncrement,
      );
      await _mockResetGame(cfg);
      return;
    }
    throw StateError(
      'No active board session. Connect via Wi‑Fi or Bluetooth from Board discovery.',
    );
  }

  Future<void> _mockResetGame(TimerConfigPart cfg) async {
    _stopMockClock();
    final raw = await rootBundle.loadString('assets/snapshot_golden.json');
    final base = GameSnapshotCodec.decodeRepairingAndNormalizing(raw);
    final wSec =
        cfg.type == 0 ? (base.status.whiteTime ?? 300) : cfg.initialTimeMs ~/ 1000;
    final bSec =
        cfg.type == 0 ? (base.status.blackTime ?? 300) : cfg.initialTimeMs ~/ 1000;
    final snap = GameSnapshot(
      stateVersion: 1,
      board: base.board,
      timestamp: DateTime.now().millisecondsSinceEpoch,
      status: mockStatusForNewGame(base.status, whiteSec: wSec, blackSec: bSec),
      history: const GameMoveHistory(moves: []),
      captured: const CapturedPieces(whiteCaptured: [], blackCaptured: []),
      clock: null,
      gameId: base.gameId,
    );
    final timer = cfg.type == 0
        ? null
        : BoardTimerState(
            whiteTimeMs: cfg.initialTimeMs,
            blackTimeMs: cfg.initialTimeMs,
            timerRunning: true,
            isWhiteTurn: true,
            gamePaused: false,
            timeExpired: false,
            config: cfg,
            totalMoves: 0,
            avgMoveTimeMs: 0,
          );
    _pauseTimerSentForFinishedGame = false;
    state = state.copyWith(
      snapshot: snap,
      snapshotReceivedAt: DateTime.now(),
      timer: timer,
      lastIfNoneMatch: snap.etagTag,
      clearError: true,
    );
    _startMockClock();
  }

  Future<void> postBoardBrightness(int percent) async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _api.postBrightness(state.wifiBaseUrl!, percent);
      await _pollOnce(state.wifiBaseUrl!);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postBrightness(percent);
      return;
    }
    throw StateError(
      'Brightness control needs a connected board (Wi‑Fi or Bluetooth).',
    );
  }

  Future<void> postBoardLightCommand({
    required bool lampState,
    required int r,
    required int g,
    required int b,
  }) async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final base = state.wifiBaseUrl!;
      await _api.postLightCommand(
        base,
        state: lampState,
        r: r,
        g: g,
        b: b,
      );
      await _pollOnce(base);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postLightCommand(state: lampState, r: r, g: g, b: b);
      return;
    }
    throw StateError(
      'Lamp control needs a connected board (Wi‑Fi or Bluetooth).',
    );
  }

  Future<void> postBoardLightGameMode() async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      final base = state.wifiBaseUrl!;
      await _api.postLightGameMode(base);
      await _pollOnce(base);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postLightGameMode();
      return;
    }
    throw StateError(
      'Game lamp mode requires a connected board (Wi‑Fi or Bluetooth).',
    );
  }

  Future<void> postBoardAutoLampTimeout(int seconds) async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _api.postAutoLampTimeout(state.wifiBaseUrl!, seconds);
      await _pollOnce(state.wifiBaseUrl!);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postAutoLampTimeout(seconds);
      return;
    }
    throw StateError(
      'Auto lamp timeout requires a connected board (Wi‑Fi or Bluetooth).',
    );
  }
}
