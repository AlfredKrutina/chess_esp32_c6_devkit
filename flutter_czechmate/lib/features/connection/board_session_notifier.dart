import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/services/ble_czechmate_client.dart';
import '../../core/services/board_api_client.dart';
import '../../core/services/prefs_repository.dart';
import '../../core/services/web_socket_manager.dart';
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
  BoardSessionNotifier(this._api, this._prefs) : super(const BoardSessionState());

  final BoardApiClient _api;
  final PrefsRepository _prefs;

  Timer? _pollTimer;
  final SnapshotWebSocketClient _ws = SnapshotWebSocketClient();
  final BleCzechmateClient _ble = BleCzechmateClient();

  @override
  void dispose() {
    _stopWifiTransport();
    unawaited(_ble.disconnect());
    super.dispose();
  }

  Future<void> tryResumeFromPrefs() async {
    if (_prefs.useMockBoard) {
      await connectMock();
      return;
    }
    final url = _prefs.lastBoardBaseUrl;
    if (url != null && url.isNotEmpty) {
      await connectWifi(url);
    }
  }

  Future<void> connectMock() async {
    _stopWifiTransport();
    await _ble.disconnect();
    state = state.copyWith(busy: true, transport: BoardTransport.mock);
    try {
      final raw =
          await rootBundle.loadString('assets/snapshot_golden.json');
      final snap = GameSnapshotCodec.decodeRepairingAndNormalizing(raw);
      await _prefs.setUseMockBoard(true);
      state = state.copyWith(
        snapshot: snap,
        timer: snap.clock,
        busy: false,
        clearError: true,
        lastIfNoneMatch: snap.etagTag,
      );
    } catch (e) {
      state = state.copyWith(busy: false, lastError: e);
    }
  }

  Future<void> connectWifi(String baseUrl, {bool webSocket = true}) async {
    _stopWifiTransport();
    await _ble.disconnect();
    final trimmed = baseUrl.trim().replaceAll(RegExp(r'/$'), '');
    await _prefs.setLastBoardBaseUrl(trimmed);
    await _prefs.setUseMockBoard(false);
    state = state.copyWith(
      transport: BoardTransport.wifi,
      wifiBaseUrl: trimmed,
      busy: true,
      clearError: true,
    );
    try {
      await _pollOnce(trimmed);
      _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) {
        unawaited(_pollOnce(trimmed));
      });
      state = state.copyWith(pollingActive: true, busy: false);
      if (webSocket) {
        _ws.connect(
          trimmed,
          onSnapshot: (s) => _applySnapshot(s, trimmed),
          onError: (e) {
            if (AppEnvironment.staging) {
              debugPrint('[staging] WS error: $e');
            }
          },
          onDisconnect: () {
            state = state.copyWith(webSocketActive: false);
          },
        );
        state = state.copyWith(webSocketActive: true);
      }
    } catch (e) {
      state = state.copyWith(busy: false, lastError: e, transport: BoardTransport.none);
    }
  }

  Future<void> connectBle(BluetoothDevice device, {String? label}) async {
    _stopWifiTransport();
    await _ble.disconnect();
    state = state.copyWith(
      transport: BoardTransport.ble,
      busy: true,
      bleLabel: label ?? device.platformName,
      clearError: true,
    );
    try {
      await _ble.connect(
        device,
        onSnapshot: (s) => _applySnapshot(s, state.wifiBaseUrl ?? ''),
        onError: (e) => state = state.copyWith(lastError: e),
      );
      state = state.copyWith(busy: false);
    } catch (e) {
      state = state.copyWith(
        busy: false,
        lastError: e,
        transport: BoardTransport.none,
      );
    }
  }

  void disconnect() {
    _stopWifiTransport();
    unawaited(_ble.disconnect());
    state = const BoardSessionState();
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
    } catch (e) {
      state = state.copyWith(lastError: e);
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
    state = state.copyWith(
      snapshot: merged,
      lastIfNoneMatch: merged.etagTag ?? state.lastIfNoneMatch,
      clearError: true,
    );
    unawaited(_refreshTimer(baseForTimer, merged));
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
    final b = state.wifiBaseUrl;
    if (b == null) return;
    await _pollOnce(b);
  }

  Future<void> postRemoteMove(String from, String to, {String? promotion}) async {
    final b = state.wifiBaseUrl;
    if (b == null) {
      throw StateError('Tahy z aplikace vyžadují Wi‑Fi URL desky.');
    }
    await _api.postGameMove(b, from: from, to: to, promotion: promotion);
    await _pollOnce(b);
  }

  Future<void> postNewGame({String? fen}) async {
    final b = state.wifiBaseUrl;
    if (b == null) return;
    await _api.postNewGame(b, fen: fen);
    await _pollOnce(b);
  }

  Future<void> postTimerPause() async {
    final b = state.wifiBaseUrl;
    if (b == null) return;
    await _api.postTimerPause(b);
    await _refreshTimerHttp(b);
  }

  Future<void> postTimerResume() async {
    final b = state.wifiBaseUrl;
    if (b == null) return;
    await _api.postTimerResume(b);
    await _refreshTimerHttp(b);
  }

  Future<void> _refreshTimerHttp(String base) async {
    try {
      final t = await _api.fetchTimerState(base);
      state = state.copyWith(timer: t);
    } catch (_) {}
  }
}
