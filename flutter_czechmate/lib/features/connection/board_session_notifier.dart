import 'dart:async';
import 'dart:io';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/debug/connection_debug_log.dart';
import '../../core/platform/flutter_blue_plus_host_supported.dart';
import '../../core/localization/locale_bridge.dart';
import '../../core/models/board_timer_state.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/models/game_status_models.dart';
import '../../core/models/board_firmware_models.dart';
import '../../core/services/ble_czechmate_client.dart';
import '../../core/services/board_api_client.dart';
import '../../core/services/mock_board_simulator.dart';
import '../../core/services/prefs_repository.dart';
import '../../core/services/web_socket_manager.dart';
import '../../core/protocol/board_command_payload.dart';
import '../../core/protocol/board_command_port.dart';
import '../../core/protocol/mock_board_command_port.dart';
import '../../core/protocol/saved_board_reconnect.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/utils/wifi_ip_third_octet_blocklist.dart';
import '../../core/utils/game_snapshot_codec.dart';
import '../../l10n/app_localizations.dart';
import 'board_session_state.dart';

final boardSessionNotifierProvider =
    StateNotifierProvider<BoardSessionNotifier, BoardSessionState>((ref) {
  return BoardSessionNotifier(
    ref.read(prefsRepositoryProvider),
    ref,
  );
});

class BoardSessionNotifier extends StateNotifier<BoardSessionState> {
  BoardSessionNotifier(this._prefs, this._ref)
      : super(const BoardSessionState()) {
    _ble.onAckTimeout = () {
      unawaited(refreshNow());
    };
  }

  final PrefsRepository _prefs;
  final Ref _ref;

  /// Always current instance — must not remain after `invalidate(boardApiClientProvider)`
  /// holder for the already closed `http.Client`.
  BoardApiClient get _boardHttp => _ref.read(boardApiClientProvider);

  /// Current parallel [connectBle] (user + resume fallback) breaks GATT and CM chunks.
  Future<void> _bleConnectSerial = Future<void>.value();

  AppLocalizations get _strings => appStringsForPrefs(_prefs);

  Timer? _pollTimer;
  Timer? _mockClockTimer;
  DateTime? _mockLastTickAt;
  Timer? _bleHandoffTimer;
  bool _bleHandoffPollInFlight = false;
  int _bleHandoffPollCount = 0;

  /// Do not switch to Wi‑Fi after a fresh BLE connection (`connectWifi' would kill GATT).
  DateTime? _bleAutoHandoffNotBefore;
  final SnapshotWebSocketClient _ws = SnapshotWebSocketClient();
  final BleCzechmateClient _ble = BleCzechmateClient();

  Future<void>? _resumeInFlight;

  /// After passing to the end of the game, we send `timer_pause` once so that the time on the board does not stop "running".
  bool _pauseTimerSentForFinishedGame = false;

  Future<void> _setupTutorialMutexTail = Future<void>.value();
  DateTime? _lastPostHintDestBleAt;
  String? _lastPostHintDestBleSquare;

  bool _linkRecoveryInFlight = false;
  DateTime? _linkRecoveryNotBefore;
  int _consecutivePollFails = 0;

  /// After `load`, keep asking the board to start once the matrix matches.
  bool _puzzleAlignWatch = false;
  int _puzzleAlignStarts = 0;
  bool _puzzleStartBusy = false;

  late final MockBoardCommandPort _mockCommands = MockBoardCommandPort(
    readSnapshot: () => state.snapshot,
    readTimer: () => state.timer,
    apply: (snap, timer) {
      state = state.copyWith(
        snapshot: snap,
        timer: timer,
        clearError: true,
        lastIfNoneMatch: snap.etagTag,
      );
    },
    plainNewGame: connectMock,
    resetWithTime: _mockResetGame,
    onGameFinished: () {
      if (_pauseTimerSentForFinishedGame) return;
      _pauseTimerSentForFinishedGame = true;
      unawaited(_pauseTimerAfterGameEnd());
    },
    onTimerResumed: () {
      _mockLastTickAt = DateTime.now();
    },
    refuse: _refuseMessage,
  );

  @override
  void dispose() {
    _clearBleHandoffCooldown();
    _stopMockClock();
    _stopBleHandoffTimer();
    _stopWifiTransport();
    unawaited(_ble.disconnect());
    super.dispose();
  }

  void _stopBleHandoffTimer() {
    _bleHandoffTimer?.cancel();
    _bleHandoffTimer = null;
    _bleHandoffPollInFlight = false;
    _bleHandoffPollCount = 0;
  }

  void _clearBleHandoffCooldown() {
    _bleAutoHandoffNotBefore = null;
  }

  Future<void> _delayedSyncWifiStaIpBlockToBoard() async {
    await Future<void>.delayed(const Duration(seconds: 3));
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      return;
    }
    await syncWifiStaIpBlockToBoardIfBle();
  }

  /// This device has IPv4 on the subnet of the AP board (typically `192.168.4.x` at the hotspot).
  Future<bool> _phoneIpv4OnBoardApSubnet() async {
    try {
      for (final iface in await NetworkInterface.list()) {
        for (final addr in iface.addresses) {
          if (addr.type == InternetAddressType.IPv4 &&
              addr.address.startsWith('192.168.4.')) {
            return true;
          }
        }
      }
    } catch (_) {}
    return false;
  }

  void _clearNextConnectionTransportOnce() {
    if (_prefs.nextConnectionTransportOnce == null) return;
    unawaited(_prefs.setNextConnectionTransportOnce(null).then((_) {
      _ref.read(connectionModeUiRefreshProvider.notifier).state++;
    }));
  }

  void _startBleHandoffRetry() {
    _stopBleHandoffTimer();
    if (_prefs.effectiveConnectionMode == 'ble_only') {
      return;
    }
    _bleHandoffPollCount = 0;
    _bleHandoffTimer = Timer.periodic(const Duration(milliseconds: 2500), (_) {
      unawaited(_bleHandoffPollTick());
    });
  }

  Future<void> _bleHandoffPollTick() async {
    if (_bleHandoffPollInFlight) return;
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      _stopBleHandoffTimer();
      return;
    }
    _bleHandoffPollCount++;
    if (_bleHandoffPollCount > 40) {
      _stopBleHandoffTimer();
      return;
    }
    _bleHandoffPollInFlight = true;
    try {
      await _syncBleNetworkMetadata();
    } finally {
      _bleHandoffPollInFlight = false;
    }
    if (state.transport == BoardTransport.wifi) {
      _stopBleHandoffTimer();
    }
  }

  Future<void> tryResumeFromPrefs() {
    final current = _resumeInFlight;
    if (current != null) return current;
    final run = _tryResumeFromPrefsBody();
    late final Future<void> tracked;
    tracked = run.whenComplete(() {
      if (identical(_resumeInFlight, tracked)) {
        _resumeInFlight = null;
      }
    });
    _resumeInFlight = tracked;
    return tracked;
  }

  Future<void> _tryResumeFromPrefsBody() async {
    if (_prefs.useMockBoard) {
      await connectMock();
      return;
    }
    await _runResumeTransportAttemptsOnce(preserveGame: true);
    if (_prefs.useMockBoard) return;
    if (state.busy || _resumeHasWorkingTransport()) return;

    final savedBle = _prefs.lastBleRemoteId;
    final hasSavedBle = savedBle != null && savedBle.isNotEmpty;
    final url = _prefs.lastBoardBaseUrl?.trim();
    final hasWifiUrl = url != null && url.isNotEmpty;
    if (!hasSavedBle && !hasWifiUrl) return;

    if (AppEnvironment.staging || kDebugMode) {
      debugPrint(
        '[staging] resume: second attempt after delay (BT/Wi‑Fi stack after wake)',
      );
    }
    await Future<void>.delayed(const Duration(milliseconds: 750));
    if (state.busy || _resumeHasWorkingTransport()) return;
    await _runResumeTransportAttemptsOnce(preserveGame: true);
  }

  /// Wifi active polling or BLE GATT — no "dead" branch after resume.
  bool _resumeHasWorkingTransport() {
    if (state.transport == BoardTransport.mock) return true;
    if (state.busy) return false;
    if (state.transport == BoardTransport.wifi) {
      return state.pollingActive;
    }
    if (state.transport == BoardTransport.ble) {
      return state.bleGattConnected;
    }
    return false;
  }

  Future<void> _runResumeTransportAttemptsOnce({
    bool preserveGame = false,
  }) async {
    final lan = await _lanPathAvailableForResume();
    final plan = planSavedBoardReconnect(
      bleHostSupported: isFlutterBluePlusHostSupported,
      lanPathAvailable: lan,
      mode: _prefs.effectiveConnectionMode,
      identity: SavedBoardIdentity(
        bleRemoteId: _prefs.lastBleRemoteId,
        httpBaseUrl: _prefs.lastBoardBaseUrl,
        mdnsHost: _prefs.lastMdnsHost,
        lastLinkKind: _prefs.lastBoardLinkKind,
      ),
      urlLooksLikePrivateLan: _savedBoardUrlLooksLikePrivateLan,
    );
    connDebugLog(
      'reconnect plan',
      plan.isEmpty
          ? 'empty'
          : plan.steps
              .map((s) => s.kind == SavedReconnectKind.ble
                  ? 'ble'
                  : 'wifi ${s.url}')
              .join(' -> '),
    );
    for (final step in plan.steps) {
      if (_resumeHasWorkingTransport()) return;
      switch (step.kind) {
        case SavedReconnectKind.ble:
          await reconnectSavedBle(preserveGameOnFailure: preserveGame);
          if (state.transport == BoardTransport.ble &&
              state.bleGattConnected &&
              !state.busy) {
            return;
          }
          break;
        case SavedReconnectKind.wifi:
          final url = step.url;
          if (url == null || url.isEmpty) break;
          await connectWifi(
            url,
            webSocket: _prefs.useWebSocket,
            allowTransportFallback: false,
            preserveGameOnFailure: preserveGame,
          );
          if (state.transport == BoardTransport.wifi && !state.busy) return;
          if (state.transport == BoardTransport.ble &&
              state.bleGattConnected &&
              !state.busy) {
            return;
          }
          break;
      }
    }
  }

  /// Desktop has no BLE, so the saved URL is always worth trying.
  /// Phones: Wi‑Fi or Ethernet. Cellular alone is not a LAN path.
  Future<bool> _lanPathAvailableForResume() async {
    if (!isFlutterBluePlusHostSupported) return true;
    try {
      final types = await Connectivity().checkConnectivity();
      return types.contains(ConnectivityResult.wifi) ||
          types.contains(ConnectivityResult.ethernet);
    } catch (_) {
      return false;
    }
  }

  Future<void> _rememberMdnsHost(String? raw) async {
    final host = normalizeMdnsHost(raw);
    if (host == null) return;
    if (_prefs.lastMdnsHost == host) return;
    await _prefs.setLastMdnsHost(host);
    connDebugLog('saved mdns host', host);
  }

  Future<void> _persistMdnsHostFromWifiStatus(String base) async {
    try {
      final status = await _boardHttp.fetchWiFiStatus(base);
      await _rememberMdnsHost(status.mdnsHost);
    } catch (e) {
      if (AppEnvironment.staging || kDebugMode) {
        debugPrint('[staging] mdns host read skipped: $e');
      }
    }
  }

  String _refuseMessage(BoardCommandRefuse kind) {
    return switch (kind) {
      BoardCommandRefuse.connectBeforeMoves => _strings.errConnectBeforeMoves,
      BoardCommandRefuse.demoNoSnapshot => _strings.errDemoNoSnapshot,
      BoardCommandRefuse.setupNeedsConnection =>
        _strings.errSetupNeedsConnection,
      BoardCommandRefuse.hintsNeedConnection => _strings.errHintsNeedConnection,
      BoardCommandRefuse.noActiveSession => _strings.errNoActiveSession,
      BoardCommandRefuse.brightnessNeedsBoard =>
        _strings.errBrightnessNeedsBoard,
      BoardCommandRefuse.lampNeedsBoard => _strings.errLampNeedsBoard,
      BoardCommandRefuse.gameLampNeedsBoard => _strings.errGameLampNeedsBoard,
      BoardCommandRefuse.autoLampNeedsBoard => _strings.errAutoLampNeedsBoard,
      BoardCommandRefuse.otaMock => _strings.errOtaMock,
    };
  }

  BoardCommandPort? _commandPortOrNull() {
    switch (state.transport) {
      case BoardTransport.wifi:
        final base = state.wifiBaseUrl;
        if (base == null || base.isEmpty) return null;
        return HttpBoardCommandPort(_boardHttp, base);
      case BoardTransport.ble:
        return BleBoardCommandPort(_ble);
      case BoardTransport.mock:
        return _mockCommands;
      case BoardTransport.none:
        return null;
    }
  }

  Future<void> _claimWriteSession(String transport) async {
    final epoch = BoardCommandPayload.beginEpoch();
    try {
      if (transport == 'ble') {
        await _ble.postSessionClaim(epoch: epoch, transport: 'ble');
      } else {
        final base = state.wifiBaseUrl;
        if (base == null || base.isEmpty) return;
        await _boardHttp.postSessionClaim(
          base,
          epoch: epoch,
          transport: 'wifi',
        );
      }
    } catch (e) {
      connDebugLog('session claim', '$transport $e');
    }
  }

  Future<void> _refreshAfterWifiCommand() async {
    if (state.transport != BoardTransport.wifi) return;
    final base = state.wifiBaseUrl;
    if (base == null || base.isEmpty) return;
    await _pollOnce(base);
  }

  /// Drop mid-game: reconnect the saved board and read its snapshot.
  /// Does not start a new game.
  Future<void> _recoverLiveLink(String reason) async {
    if (_linkRecoveryInFlight || _resumeInFlight != null) return;
    if (state.transport == BoardTransport.mock) return;
    if (state.busy) return;
    final hold = _linkRecoveryNotBefore;
    if (hold != null && DateTime.now().isBefore(hold)) return;
    _linkRecoveryInFlight = true;
    try {
      connDebugLog('link recovery', reason);
      await _failoverToWifiIfAvailable();
      if (_resumeHasWorkingTransport()) {
        await refreshNow();
        connDebugLog('link recovery', 'resynced on existing wifi path');
        return;
      }
      for (var attempt = 1; attempt <= 2; attempt++) {
        if (_resumeHasWorkingTransport()) break;
        connDebugLog('link recovery', 'quiet attempt $attempt');
        await _runResumeTransportAttemptsOnce(preserveGame: true);
        if (_resumeHasWorkingTransport()) {
          await refreshNow();
          connDebugLog(
            'link recovery',
            'resynced current position; no new game',
          );
          return;
        }
        await Future<void>.delayed(Duration(milliseconds: 600 * attempt));
      }
      connDebugLog('link recovery', 'gave up ($reason)');
      _linkRecoveryNotBefore = DateTime.now().add(const Duration(seconds: 20));
    } finally {
      _linkRecoveryInFlight = false;
    }
  }

  /// A quick GET of the snapshot before `connectWifi` from the BLE handoff — we must not cancel the working GATT without success.
  Future<bool> _probeBoardHttpBeforeBleHandoff(String normalized) async {
    try {
      await _boardHttp
          .fetchSnapshotIfChanged(normalized, ifNoneMatch: null)
          .timeout(const Duration(seconds: 5));
      return true;
    } catch (_) {
      return false;
    }
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

  bool _hostLooksLikePrivateLanIpv4(String host) {
    if (!_looksLikeIPv4(host)) return false;
    final p = host.trim().split('.');
    final a = int.parse(p[0]);
    final b = int.parse(p[1]);
    if (a == 10) return true;
    if (a == 172 && b >= 16 && b <= 31) return true;
    if (a == 192 && b == 168) return true;
    return false;
  }

  bool _savedBoardUrlLooksLikePrivateLan(String rawUrl) {
    final norm = normalizeBoardHttpBaseUrl(rawUrl.trim());
    if (norm == null) return false;
    final host = Uri.tryParse(norm)?.host ?? '';
    return _hostLooksLikePrivateLanIpv4(host);
  }

  /// Saves `http://<sta_ip>` when the STA is online, or `http://<ap_ip>` only when the client is on the AP subnet
  /// (otherwise the home Wi‑Fi would store 192.168.4.1 and HTTP would timeout).
  Future<void> _persistBoardHttpUrlFromBle(BleNetworkInfo? info) async {
    if (info == null) return;
    await _rememberMdnsHost(info.mdnsHost);

    String? raw;
    if (info.online == true) {
      final sta = info.staIp;
      if (sta != null &&
          _looksLikeIPv4(sta) &&
          !wifiIpv4ThirdOctetIsBlocked(sta, _prefs.wifiBlockedThirdOctets)) {
        raw = 'http://$sta';
      }
    }
    if (raw == null) {
      final ap = info.apIp?.trim();
      if (ap != null &&
          _looksLikeIPv4(ap) &&
          !wifiIpv4ThirdOctetIsBlocked(ap, _prefs.wifiBlockedThirdOctets) &&
          await _phoneIpv4OnBoardApSubnet()) {
        raw = 'http://$ap';
      }
    }
    if (raw == null) return;

    final url = normalizeBoardHttpBaseUrl(raw);
    if (url == null) return;
    final prev = _prefs.lastBoardBaseUrl?.trim();
    if (prev == url) return;
    await _prefs.setLastBoardBaseUrl(url);
    if (AppEnvironment.staging) {
      debugPrint('[staging] Cached board HTTP URL from BLE: $url');
    }
  }

  Future<void> _tryBleToWifiHandoff(BleNetworkInfo? info) async {
    if (_prefs.effectiveConnectionMode == 'ble_only') {
      return;
    }
    final handoffEarliest = _bleAutoHandoffNotBefore;
    if (handoffEarliest != null && DateTime.now().isBefore(handoffEarliest)) {
      if (AppEnvironment.staging) {
        debugPrint(
          '[staging] BLE→Wi‑Fi handoff skipped (cooldown after new BLE)',
        );
      }
      return;
    }
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      return;
    }
    if (state.busy) return;
    if (info == null) return;

    String? rawUrl;
    if (info.online == true) {
      final staIp = info.staIp;
      if (staIp != null &&
          _looksLikeIPv4(staIp) &&
          !wifiIpv4ThirdOctetIsBlocked(staIp, _prefs.wifiBlockedThirdOctets)) {
        if (!await _phoneHasWifiInterface()) return;
        rawUrl = 'http://$staIp';
      }
    }
    if (rawUrl == null) {
      final ap = info.apIp?.trim();
      if (ap != null &&
          _looksLikeIPv4(ap) &&
          !wifiIpv4ThirdOctetIsBlocked(ap, _prefs.wifiBlockedThirdOctets) &&
          await _phoneIpv4OnBoardApSubnet()) {
        rawUrl = 'http://$ap';
      }
    }
    if (rawUrl == null) return;

    final normalized = normalizeBoardHttpBaseUrl(rawUrl);
    if (normalized == null) return;
    if (state.wifiBaseUrl == normalized && state.wifiChannelActive) {
      return;
    }

    if (!await _probeBoardHttpBeforeBleHandoff(normalized)) {
      if (AppEnvironment.staging) {
        debugPrint(
          '[staging] BLE→Wi‑Fi background channel skipped (HTTP unreachable): $normalized',
        );
      }
      return;
    }

    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE→Wi‑Fi background channel open: $normalized');
    }
    await connectWifi(normalized,
        webSocket: _prefs.useWebSocket, disconnectBle: false);
  }

  void _applyBleNetworkToState(BleNetworkInfo? info) {
    if (info == null) {
      return;
    }
    state = state.copyWith(
      bleStaIp: info.staIp,
      bleStaSsid: info.staSsid,
      bleStaConnected: info.staConnected,
      bleApBroadcasting: info.apActive,
      bleApSsid: info.apSsid,
    );
  }

  Future<void> _syncBleNetworkMetadata() async {
    final info = await _ble.fetchNetworkInfo();
    _applyBleNetworkToState(info);
    await _persistBoardHttpUrlFromBle(info);
    await _tryBleToWifiHandoff(info);
  }

  Future<void> _onBleNetworkPayload(List<int> raw) async {
    final info = BleCzechmateClient.tryParseNetworkJson(raw);
    _applyBleNetworkToState(info);
    await _persistBoardHttpUrlFromBle(info);
    await _tryBleToWifiHandoff(info);
  }

  Future<void> connectMock() async {
    _clearBleHandoffCooldown();
    _stopBleHandoffTimer();
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
        clearBleNetwork: true,
        clearError: true,
        lastIfNoneMatch: snap.etagTag,
      );
      _startMockClock();
    } catch (e) {
      state = state.copyWith(busy: false, lastError: e);
    } finally {
      _clearNextConnectionTransportOnce();
    }
  }

  Future<void> connectWifi(String baseUrl,
      {bool? webSocket,
      bool disconnectBle = true,
      bool allowTransportFallback = true,
      bool preserveGameOnFailure = false}) async {
    final useWs = webSocket ?? _prefs.useWebSocket;
    connDebugLog('BoardSession.connectWifi',
        'raw="$baseUrl" ws=$useWs disconnectBle=$disconnectBle');
    try {
      final normalized = normalizeBoardHttpBaseUrl(baseUrl);
      if (normalized == null) {
        connDebugLog('BoardSession.connectWifi', 'rejected: invalid URL');
        state = state.copyWith(
          busy: false,
          lastError: StateError(_strings.errInvalidBoardUrl),
        );
        return;
      }
      final host = Uri.tryParse(normalized)?.host ?? '';
      if (wifiIpv4ThirdOctetIsBlocked(host, _prefs.wifiBlockedThirdOctets)) {
        connDebugLog(
          'BoardSession.connectWifi',
          'rejected: blocked 3rd octet host=$host',
        );
        state = state.copyWith(
          busy: false,
          lastError: StateError(_strings.errWifiIpThirdOctetBlocked(host)),
        );
        return;
      }
      _clearBleHandoffCooldown();
      _stopBleHandoffTimer();
      _stopWifiTransport();
      if (disconnectBle) {
        await _ble.disconnect();
      }
      await _prefs.setLastBoardBaseUrl(normalized);
      await _prefs.setUseMockBoard(false);
      final keepBleAsRealtime =
          !disconnectBle && state.transport == BoardTransport.ble;
      state = state.copyWith(
        transport: keepBleAsRealtime ? BoardTransport.ble : BoardTransport.wifi,
        wifiBaseUrl: normalized,
        wifiChannelActive: true,
        transportStartedAt: DateTime.now(),
        busy: true,
        bleGattConnected: keepBleAsRealtime ? state.bleGattConnected : false,
        clearBleNetwork: disconnectBle,
        clearError: true,
      );
      try {
        await _prefs.setLastBoardLinkKind('wifi');
        // More reliable than delta pollFailureCount (it can "hang" from an earlier session).
        final pollsOkBefore = state.pollSuccessCount;
        await _pollOnce(normalized);
        if (state.pollSuccessCount <= pollsOkBefore) {
          connDebugLog(
            'BoardSession.connectWifi',
            'first poll failed → exception (pollSuccessCount)',
          );
          throw state.lastError ??
              StateError(_strings.errBoardSnapshotUnreachable);
        }
        connDebugLog(
          'BoardSession.connectWifi OK',
          'polling HTTP $normalized',
        );
        _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) {
          unawaited(_pollOnce(normalized));
        });
        state = state.copyWith(pollingActive: true, busy: false);
        if (!keepBleAsRealtime) {
          unawaited(_claimWriteSession('wifi'));
        }
        unawaited(_persistMdnsHostFromWifiStatus(normalized));
        if (useWs) {
          _ws.connect(
            normalized,
            commandToken: _prefs.boardCommandToken,
            onSnapshot: (s) => _applySnapshot(s, normalized),
            onError: (e) {
              state = state.copyWith(
                lastError: e,
                webSocketActive: false,
              );
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
        connDebugLog(
          'BoardSession.connectWifi FAIL',
          '${e.runtimeType}: $e',
        );
        if (disconnectBle) {
          state = state.copyWith(
            busy: false,
            lastError: e,
            transport: BoardTransport.none,
            bleGattConnected: false,
            clearBleNetwork: true,
            clearSnapshot: !preserveGameOnFailure,
            clearSnapshotReceivedAt: !preserveGameOnFailure,
            clearTransportStartedAt: !preserveGameOnFailure,
            clearTimer: !preserveGameOnFailure,
          );
          if (allowTransportFallback) {
            await _bleFallbackAfterWifiFailure();
          }
        } else {
          // Background Wi‑Fi channel failed; BLE realtime path stays up.
          state = state.copyWith(
            busy: false,
            lastError: e,
            wifiChannelActive: false,
          );
        }
      }
    } finally {
      _clearNextConnectionTransportOnce();
    }
  }

  /// Hysteretic failover: only switch the realtime path to Wi‑Fi when BLE is
  /// actually down and a reachable Wi‑Fi channel was already open in the
  /// background. Avoids bouncing on a single slow packet.
  Future<void> _failoverToWifiIfAvailable() async {
    final base = state.wifiBaseUrl;
    if (base == null ||
        base.isEmpty ||
        !state.wifiChannelActive ||
        state.transport != BoardTransport.ble) {
      return;
    }
    connDebugLog(
      'BoardSession BLE→Wi‑Fi failover',
      'realtime path switching after BLE loss',
    );
    await Future<void>.delayed(const Duration(milliseconds: 350));
    if (state.bleGattConnected || state.transport != BoardTransport.ble) {
      // BLE recovered in the meantime — keep it.
      return;
    }
    await connectWifi(base, webSocket: _prefs.useWebSocket);
  }

  /// After HTTP polling fails, it will try stored BLE (same board according to `lastBleRemoteId`).
  Future<void> _bleFallbackAfterWifiFailure() async {
    if (!isFlutterBluePlusHostSupported) return;
    final id = _prefs.lastBleRemoteId;
    if (id == null || id.isEmpty) return;
    connDebugLog(
      'BoardSession Wi‑Fi→BLE fallback',
      'trying saved BLE remoteId=$id',
    );
    await Future<void>.delayed(const Duration(milliseconds: 350));
    await reconnectSavedBle();
  }

  Future<void> connectBle(BluetoothDevice device,
      {String? label,
      bool preserveGameOnFailure = false,
      bool requestPairToken = false}) async {
    final prev = _bleConnectSerial;
    final gate = Completer<void>();
    _bleConnectSerial = gate.future;
    await prev;
    try {
      if (!isFlutterBluePlusHostSupported) {
        state = state.copyWith(lastError: StateError(_strings.errBleHostUnsupported));
        return;
      }
      await _connectBleBody(
        device,
        label: label,
        preserveGameOnFailure: preserveGameOnFailure,
        requestPairToken: requestPairToken,
      );
    } finally {
      if (!gate.isCompleted) gate.complete();
    }
  }

  Future<void> _connectBleBody(BluetoothDevice device,
      {String? label,
      bool preserveGameOnFailure = false,
      bool requestPairToken = false}) async {
    connDebugLog(
      'BoardSession.connectBle start',
      'remoteId=${device.remoteId.str} label="${label ?? device.platformName}"',
    );
    _stopBleHandoffTimer();
    // V2: do not tear down a reachable Wi‑Fi channel; it becomes the bulk /
    // failover path while BLE handles realtime commands.
    await _ble.disconnect();
    state = state.copyWith(
      transport: BoardTransport.ble,
      transportStartedAt: DateTime.now(),
      busy: true,
      bleGattConnected: false,
      bleLabel: label ?? device.platformName,
      wifiChannelActive: false,
      clearError: true,
    );
    try {
      await _ble.connect(
        device,
        onSnapshot: (s) => _applySnapshot(s, ''),
        onError: (e) {
          connDebugLog(
            'BoardSession BLE stream/error',
            connBleErrorDetail(e),
          );
          final wasBle = state.transport == BoardTransport.ble;
          state = state.copyWith(
            busy: false,
            lastError: e,
            bleGattConnected: false,
            clearBleNetwork: true,
          );
          if (wasBle &&
              !_linkRecoveryInFlight &&
              _resumeInFlight == null) {
            unawaited(_recoverLiveLink('ble stream'));
          } else if (wasBle) {
            unawaited(_failoverToWifiIfAvailable());
          }
        },
        onNetworkNotify: (raw) => unawaited(_onBleNetworkPayload(raw)),
        requestPairToken: requestPairToken,
        onCommandToken: (token) {
          connDebugLog('pairing token saved', 'next to ${device.remoteId.str}');
          unawaited(_prefs.setBoardCommandToken(token));
        },
      );
      await _prefs.setLastBleRemoteId(device.remoteId.str);
      await _prefs.setLastBleDisplayName(label ?? device.platformName);
      await _prefs.setUseMockBoard(false);
      await _prefs.setLastBoardLinkKind('bluetooth');
      state = state.copyWith(busy: false, bleGattConnected: true);
      unawaited(_claimWriteSession('ble'));
      connDebugLog(
        'BoardSession.connectBle OK',
        'GATT attached → sync network metadata',
      );
      _bleAutoHandoffNotBefore =
          DateTime.now().add(const Duration(seconds: 22));
      await _syncBleNetworkMetadata();
      unawaited(_delayedSyncWifiStaIpBlockToBoard());
      if (state.transport == BoardTransport.ble) {
        _startBleHandoffRetry();
      }
    } catch (e) {
      connDebugLog(
        'BoardSession.connectBle FAIL',
        connBleErrorDetail(e),
      );
      _clearBleHandoffCooldown();
      _stopBleHandoffTimer();
      state = state.copyWith(
        busy: false,
        lastError: e,
        transport: BoardTransport.none,
        bleGattConnected: false,
        clearBleNetwork: true,
        clearSnapshot: !preserveGameOnFailure,
        clearSnapshotReceivedAt: !preserveGameOnFailure,
        clearTransportStartedAt: !preserveGameOnFailure,
        clearTimer: !preserveGameOnFailure,
      );
    } finally {
      _clearNextConnectionTransportOnce();
    }
  }

  /// Reconnect according to `czechmate.lastBleRemoteId` (without scan).
  /// Sends a DHCP filter (`wifi_sta_ip_block`) to the NVS board according to the settings in the application.
  Future<void> syncWifiStaIpBlockToBoardIfBle() async {
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      return;
    }
    try {
      await _ble.prepareEncryptedBleLink();
      await _ble.postWifiStaIpBlock(_prefs.wifiStaIpBlockCsvForBle());
    } catch (e) {
      if (AppEnvironment.staging || kDebugMode) {
        debugPrint('[staging] wifi_sta_ip_block sync: $e');
      }
    }
  }

  Future<void> reconnectSavedBle({bool preserveGameOnFailure = false}) async {
    if (!isFlutterBluePlusHostSupported) {
      connDebugLog('reconnectSavedBle', 'host without flutter_blue_plus');
      state = state.copyWith(lastError: StateError(_strings.errBleHostUnsupported));
      return;
    }
    final id = _prefs.lastBleRemoteId;
    if (id == null || id.isEmpty) {
      connDebugLog('reconnectSavedBle', 'error: no saved remoteId');
      state = state.copyWith(lastError: StateError(_strings.errNoSavedBle));
      return;
    }
    connDebugLog(
      'reconnectSavedBle',
      'remoteId=$id name="${_prefs.lastBleDisplayName ?? ''}"',
    );
    final dev = BluetoothDevice(remoteId: DeviceIdentifier(id));
    await connectBle(
      dev,
      label: _prefs.lastBleDisplayName,
      preserveGameOnFailure: preserveGameOnFailure,
    );
  }

  void disconnect() {
    _clearBleHandoffCooldown();
    _stopMockClock();
    _stopBleHandoffTimer();
    _stopWifiTransport();
    unawaited(_ble.disconnect());
    _pauseTimerSentForFinishedGame = false;
    _setupTutorialMutexTail = Future<void>.value();
    _lastPostHintDestBleAt = null;
    _lastPostHintDestBleSquare = null;
    state = const BoardSessionState();
  }

  /// Like [disconnect], but will wait for BLE to end — convenient before reconnecting from a scan.
  Future<void> disconnectAwaitBle() async {
    _clearBleHandoffCooldown();
    _stopMockClock();
    _stopBleHandoffTimer();
    _stopWifiTransport();
    await _ble.disconnect();
    _pauseTimerSentForFinishedGame = false;
    _setupTutorialMutexTail = Future<void>.value();
    _lastPostHintDestBleAt = null;
    _lastPostHintDestBleSquare = null;
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
      timerRunning: snap.status.isTimerRunning && !snap.status.isGameFinished,
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
    _mockClockTimer = Timer.periodic(
        const Duration(milliseconds: 300), (_) => _mockClockTick());
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
      state = state.copyWith(
          webSocketActive: false, pollingActive: false, wifiChannelActive: false);
    } else {
      state =
          state.copyWith(pollingActive: false, wifiChannelActive: false);
    }
  }

  Future<void> _pollOnce(String base) async {
    try {
      final tag = state.lastIfNoneMatch;
      final s = await _boardHttp.fetchSnapshotIfChanged(base, ifNoneMatch: tag);
      if (s != null) _applySnapshot(s, base);
      _consecutivePollFails = 0;
      state = state.copyWith(
        pollSuccessCount: state.pollSuccessCount + 1,
        lastSuccessfulPoll: DateTime.now(),
      );
    } catch (e) {
      _consecutivePollFails++;
      state = state.copyWith(
        lastError: e,
        pollFailureCount: state.pollFailureCount + 1,
      );
      if (_consecutivePollFails >= 3 &&
          state.transport == BoardTransport.wifi &&
          !_linkRecoveryInFlight &&
          _resumeInFlight == null) {
        _consecutivePollFails = 0;
        unawaited(_recoverLiveLink('http poll failed'));
      }
    }
  }

  void _applySnapshot(GameSnapshot s, String baseForTimer) {
    final prev = state.snapshot;
    final incoming = s.stateVersion;
    final held = prev?.stateVersion;
    final canCompare = incoming != null && held != null;
    if (canCompare && incoming < held) {
      return;
    }
    var merged = s;
    if (prev != null && (!canCompare || incoming == held)) {
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
    unawaited(_maybeContinuePuzzleSetup());
  }

  Future<void> _pauseTimerAfterGameEnd() async {
    try {
      await postTimerPause();
    } catch (_) {
      // The board can stop time by itself; ignore network/GATT error.
    }
  }

  Future<void> _refreshTimer(String base, GameSnapshot snap) async {
    if (snap.clock != null) {
      state = state.copyWith(timer: snap.clock);
      return;
    }
    if (base.isEmpty) return;
    try {
      final t = await _boardHttp.fetchTimerState(base);
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
    if (state.transport == BoardTransport.wifi && b != null && b.isNotEmpty) {
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
    final f = from.trim().toLowerCase();
    final t = to.trim().toLowerCase();
    if (f.isEmpty || t.isEmpty || f == t) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] postRemoteMove ignored (empty or same square)');
      }
      return;
    }
    if (state.transport == BoardTransport.mock) {
      final port = _commandPortOrNull();
      if (port == null) {
        throw StateError(_strings.errDemoNoSnapshot);
      }
      if (AppEnvironment.staging) {
        debugPrint(
          '[staging] mock postRemoteMove $from→$to promotion=$promotion',
        );
      }
      await port.postMove(from: f, to: t, promotion: promotion);
      return;
    }
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errConnectBeforeMoves);
    }
    await port.postMove(from: f, to: t, promotion: promotion);
    await _refreshAfterWifiCommand();
  }

  Future<void> postNewGame({String? fen}) async {
    final port = _commandPortOrNull();
    if (port == null) return;
    await port.postNewGame(fen: fen);
    await _refreshAfterWifiCommand();
  }

  /// Stage a puzzle on the board. Does not start a blind `new_game`.
  ///
  /// [solution] is solver-first. An empty line only places the position once
  /// the matrix matches, then leaves a normal game.
  Future<void> sendPuzzleToBoard({
    required String fen,
    List<String> solution = const [],
    String title = '',
  }) async {
    final f = fen.trim();
    if (f.isEmpty) return;
    if (state.transport != BoardTransport.wifi &&
        state.transport != BoardTransport.ble) {
      return;
    }
    final port = _commandPortOrNull();
    if (port == null) return;
    if (AppEnvironment.staging) {
      debugPrint(
        '[staging] puzzle load plies=${solution.length} transport=${state.transport}',
      );
    }
    await port.postPuzzle(BoardCommandPayload.puzzle(
      action: 'load',
      fen: f,
      title: title,
      solution: solution,
    ));
    _puzzleAlignWatch = true;
    _puzzleAlignStarts = 0;
    await Future<void>.delayed(const Duration(milliseconds: 220));
    await refreshNow();
  }

  /// Kept for callers that only have a FEN. Still goes through puzzle setup.
  Future<void> sendPuzzleFenToBoard(String fen) =>
      sendPuzzleToBoard(fen: fen);

  Future<void> startPuzzlePlay() async {
    if (_puzzleStartBusy) return;
    final port = _commandPortOrNull();
    if (port == null) return;
    _puzzleStartBusy = true;
    try {
      if (AppEnvironment.staging) {
        debugPrint('[staging] puzzle start');
      }
      await port.postPuzzle(BoardCommandPayload.puzzle(action: 'start'));
      await Future<void>.delayed(const Duration(milliseconds: 220));
      await refreshNow();
    } catch (e) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] puzzle start refused: $e');
      }
      rethrow;
    } finally {
      _puzzleStartBusy = false;
    }
  }

  Future<void> cancelPuzzle() async {
    _puzzleAlignWatch = false;
    _puzzleAlignStarts = 0;
    final port = _commandPortOrNull();
    if (port == null) return;
    if (AppEnvironment.staging) {
      debugPrint('[staging] puzzle cancel');
    }
    await port.postPuzzle(BoardCommandPayload.puzzle(action: 'cancel'));
    await Future<void>.delayed(const Duration(milliseconds: 160));
    await refreshNow();
  }

  Future<void> _maybeContinuePuzzleSetup() async {
    if (!_puzzleAlignWatch || _puzzleStartBusy) return;
    final puzzle = state.snapshot?.status.puzzle;
    if (puzzle == null) return;
    if (puzzle.active == true) {
      _puzzleAlignWatch = false;
      if (AppEnvironment.staging) {
        debugPrint('[staging] puzzle play active session=${puzzle.session}');
      }
      return;
    }
    if (puzzle.physicalMatch != true) {
      _puzzleAlignStarts = 0;
      return;
    }
    if (puzzle.setupActive == true && _puzzleAlignStarts < 8) {
      _puzzleAlignStarts++;
      try {
        await startPuzzlePlay();
      } catch (_) {
        // 409 while the matrix is still settling; the next snapshot retries.
      }
    }
  }

  Future<void> postTimerPause() async {
    final port = _commandPortOrNull();
    if (port == null) return;
    await port.postTimerPause();
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _refreshTimerHttp(state.wifiBaseUrl!);
    }
  }

  Future<void> postTimerResume() async {
    final port = _commandPortOrNull();
    if (port == null) return;
    await port.postTimerResume();
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _refreshTimerHttp(state.wifiBaseUrl!);
    }
  }

  /// Resign [side] (`white` / `black`) — the board ends the game the same way
  /// as a 10 s king lift (result, clock, snapshot). Throws when refused.
  Future<void> postResign(String side) async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errConnectBeforeMoves);
    }
    if (AppEnvironment.staging) {
      debugPrint('[staging] postResign side=$side via ${state.transport.name}');
    }
    await port.postResign(side: side);
    await _refreshAfterWifiCommand();
  }

  Future<void> _refreshTimerHttp(String base) async {
    try {
      final t = await _boardHttp.fetchTimerState(base);
      state = state.copyWith(timer: t);
    } catch (_) {}
  }

  /// `POST /api/game/setup_tutorial` or BLE `setup_tutorial` — `start` | `cancel` | `finish`.
  Future<void> postSetupTutorialAction(String action) async {
    final prev = _setupTutorialMutexTail;
    final done = Completer<void>();
    _setupTutorialMutexTail = done.future;
    await prev.catchError((_) {});
    try {
      final a = action.trim().toLowerCase();
      final port = _commandPortOrNull();
      if (port == null) {
        throw StateError(_strings.errSetupNeedsConnection);
      }
      await port.postSetupTutorial(a);
      if (a != 'start') await refreshNow();
    } finally {
      if (!done.isCompleted) done.complete();
    }
  }

  /// `POST /api/game/opening` — start | cancel | hint | checkpoint_ack (+ line payload on start).
  Future<void> postOpeningAction(Map<String, dynamic> body) async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errSetupNeedsConnection);
    }
    await port.postOpening(body);
    if (state.transport == BoardTransport.wifi) {
      await refreshNow();
    }
  }

  Future<void> postOpeningRaw(Map<String, dynamic> body) => postOpeningAction(body);

  /// It stores the SSID/password in the NVS on the board and starts the STA connection (the result will come via network notify).
  Future<void> provisionStaWifiOverBle({
    required String ssid,
    required String password,
  }) async {
    final s = ssid.trim();
    if (s.isEmpty) {
      throw StateError(_strings.errWifiSsidEmpty);
    }
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      throw StateError(_strings.errWifiProvNeedsBle);
    }
    await _ble.postWifiStaConfig(s, password);
  }

  /// Scan the surroundings on the board (`wifi_survey`, encrypted link) — result via cmd_ack.
  Future<BleWifiSurveyResult> fetchWifiSurveyOverBle() async {
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      throw StateError(_strings.errWifiProvNeedsBle);
    }
    await _ble.ensureGattReadyForCommands();
    return _ble.fetchWifiSurvey();
  }

  /// Enable/disable board hotspot via BLE (`wifi_ap_set`). Requires an encrypted connection.
  Future<void> setBoardHotspotEnabled(bool enabled) async {
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      throw StateError(_strings.errBoardApNeedsBle);
    }
    await _ble.postWifiApSet(enabled);
    await Future<void>.delayed(const Duration(milliseconds: 800));
    await _syncBleNetworkMetadata();
  }

  /// Stream `.bin` over BLE (no hotspot / STA). The application on this device must have the file downloaded.
  ///
  /// Returns the board OTA state after the reboot into the new image: `success` only once
  /// the new firmware confirmed a stable start; `rolled_back` / `aborted` / `error` otherwise.
  /// `null` when the board answered without OTA guard fields or never answered in time.
  /// [onBleOtaPhase] also receives `verifying` while waiting for that confirmation.
  Future<BoardOtaStatus?> uploadFirmwareOtaBle(
    File binFile, {
    void Function(int pct)? onProgress,
    void Function(String phase)? onBleOtaPhase,
  }) async {
    if (state.transport != BoardTransport.ble) {
      throw StateError(_strings.errOtaBleTransport);
    }
    if (!state.bleGattConnected) {
      throw StateError(_strings.errOtaBleGatt);
    }
    await WakelockPlus.enable();
    try {
      await _ble.uploadFirmwareBle(
        binFile,
        onProgress: onProgress,
        onPhase: onBleOtaPhase,
      );
      onBleOtaPhase?.call('verifying');
      return await _awaitBleOtaOutcome();
    } catch (e) {
      final msg = '$e';
      final lostLink = (e is FlutterBluePlusException && e.code == 6) ||
          msg.contains('device is not connected');
      if (lostLink) {
        state = state.copyWith(bleGattConnected: false);
      }
      rethrow;
    } finally {
      await WakelockPlus.disable();
    }
  }

  static bool _isFinalOtaState(String s) =>
      s == BoardOtaState.success ||
      s == BoardOtaState.rolledBack ||
      s == BoardOtaState.aborted ||
      s == BoardOtaState.error ||
      s == BoardOtaState.idle;

  /// Reads `ota_ble_status` over BLE, reconnecting the saved board when the link is down
  /// (e.g. right after the OTA reboot). Never takes over an active Wi‑Fi session.
  Future<OtaBleStatus?> probeOtaStatusViaBle() async {
    if (!isFlutterBluePlusHostSupported) return null;
    final id = _prefs.lastBleRemoteId;
    if (id == null || id.isEmpty) return null;
    if (!state.bleGattConnected) {
      if (state.transport == BoardTransport.wifi ||
          _linkRecoveryInFlight ||
          _resumeInFlight != null ||
          state.busy) {
        return null;
      }
      await reconnectSavedBle(preserveGameOnFailure: true);
      if (!state.bleGattConnected) return null;
    }
    return _ble.fetchOtaBleStatus();
  }

  /// After the last chunk the board verifies the image, switches the boot slot and restarts.
  /// The new image reports `verifying` until it confirms itself (stable uptime with an OTA
  /// transport up), then `success`; a failed start makes the bootloader return to the
  /// previous slot, which reports `rolled_back`.
  Future<BoardOtaStatus?> _awaitBleOtaOutcome() async {
    final deadline = DateTime.now().add(const Duration(minutes: 5));
    await Future<void>.delayed(const Duration(seconds: 6));
    while (DateTime.now().isBefore(deadline)) {
      try {
        final st = await probeOtaStatusViaBle();
        if (st != null) {
          final ota = st.ota;
          if (AppEnvironment.staging || kDebugMode) {
            debugPrint(
              '[staging] OTA outcome probe: state=${ota?.state ?? 'legacy'} '
              'reason=${ota?.reason ?? ''} awaiting=${ota?.awaitingConfirm}',
            );
          }
          if (ota == null) return null;
          if (_isFinalOtaState(ota.state)) return ota;
        }
      } catch (e) {
        if (AppEnvironment.staging || kDebugMode) {
          debugPrint('[staging] OTA outcome probe failed: $e');
        }
      }
      await Future<void>.delayed(const Duration(seconds: 3));
    }
    return null;
  }

  /// OTA: HTTPS URL (STA on the internet) or `http://…` on LAN (client on the board hotspot).
  ///
  /// [httpBoardBaseUrl] — must be the base URL of the board (`http://192.168.4.1`), same as
  /// for `fetchBoardFirmwareInfo`; overrides `wifiBaseUrl` session if passed.
  ///
  /// [preferHttpOtaStart] — `true` when hosted OTA: start via HTTP on board even in BLE mode
  /// (the client is on the same network as the HTTP API board, e.g. hotspot `192.168.4.x`).
  Future<void> requestFirmwareOta(String firmwareUrl,
      {String? httpBoardBaseUrl, bool preferHttpOtaStart = false}) async {
    final u = firmwareUrl.trim();
    if (!u.startsWith('https://') && !u.startsWith('http://')) {
      throw StateError(_strings.errOtaHttps);
    }
    if (state.transport == BoardTransport.mock) {
      throw StateError(_strings.errOtaMock);
    }
    if (preferHttpOtaStart) {
      final boardBase = normalizeBoardHttpBaseUrl(httpBoardBaseUrl);
      if (boardBase == null || boardBase.isEmpty) {
        throw StateError(_strings.errOtaBoardHttpMissingDetail);
      }
      await HttpBoardCommandPort(_boardHttp, boardBase).postOtaStart(u);
      return;
    }
    // V2: use the background Wi‑Fi channel for bulk OTA whenever it is open,
    // even if the realtime path is still BLE.
    final boardBase = normalizeBoardHttpBaseUrl(
        httpBoardBaseUrl ?? (state.wifiChannelActive ? state.wifiBaseUrl : null));
    if (boardBase != null && boardBase.isNotEmpty) {
      await HttpBoardCommandPort(_boardHttp, boardBase).postOtaStart(u);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await BleBoardCommandPort(_ble).postOtaStart(u);
      return;
    }
    throw StateError(_strings.errOtaConnectFirst);
  }

  /// LED only on target field (`hint_highlight` with `to`).
  Future<void> postHintDestination(String square) async {
    final sq = square.trim().toLowerCase();
    if (state.transport == BoardTransport.ble) {
      final now = DateTime.now();
      if (_lastPostHintDestBleSquare == sq &&
          _lastPostHintDestBleAt != null &&
          now.difference(_lastPostHintDestBleAt!) <
              const Duration(milliseconds: 450)) {
        return;
      }
      _lastPostHintDestBleSquare = sq;
      _lastPostHintDestBleAt = now;
    }
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errHintsNeedConnection);
    }
    await port.postHintDestination(sq);
  }

  Future<void> postHintClearBoard() async {
    final port = _commandPortOrNull();
    if (port == null) return;
    await port.postHintClear();
  }

  /// Emergency cancel matrix guard (board must be physically aligned).
  Future<void> postGuardClear() async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errHintsNeedConnection);
    }
    await port.postGuardClear();
  }

  /// Reed matrix: Wi‑Fi `GET /api/status`, BLE last `snapshot.status.matrix_occupied`.
  Future<List<int>?> fetchMatrixOccupiedForWizard() async {
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      try {
        return await _boardHttp.fetchMatrixOccupied(state.wifiBaseUrl!);
      } catch (_) {
        return null;
      }
    }
    return state.snapshot?.status.matrixOccupied;
  }

  /// Parity iOS `startNewGameWithTimeControl` — `timer_config` pak `new_game`.
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
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errNoActiveSession);
    }
    await port.postTimerConfig(
      type: type,
      customMinutes: customMinutes,
      customIncrement: customIncrement,
    );
    await port.postNewGame();
    await _refreshAfterWifiCommand();
  }

  Future<void> _mockResetGame(TimerConfigPart cfg) async {
    _stopMockClock();
    final raw = await rootBundle.loadString('assets/snapshot_golden.json');
    final base = GameSnapshotCodec.decodeRepairingAndNormalizing(raw);
    final wSec = cfg.type == 0
        ? (base.status.whiteTime ?? 300)
        : cfg.initialTimeMs ~/ 1000;
    final bSec = cfg.type == 0
        ? (base.status.blackTime ?? 300)
        : cfg.initialTimeMs ~/ 1000;
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
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errBrightnessNeedsBoard);
    }
    await port.postBrightness(percent);
    await _refreshAfterWifiCommand();
  }

  Future<void> postHallRest() async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errBrightnessNeedsBoard);
    }
    await port.postHallRest();
  }

  Future<void> postBoardLightCommand({
    required bool lampState,
    required int r,
    required int g,
    required int b,
  }) async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errLampNeedsBoard);
    }
    await port.postLightCommand(lampState: lampState, r: r, g: g, b: b);
    await _refreshAfterWifiCommand();
  }

  Future<void> postBoardLightPattern({
    required bool on,
    required int pen,
    required int ar,
    required int ag,
    required int ab,
    required int br,
    required int bg,
    required int bb,
    bool clear = false,
  }) async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errLampNeedsBoard);
    }
    await port.postLightPattern(
      on: on,
      pen: pen,
      ar: ar,
      ag: ag,
      ab: ab,
      br: br,
      bg: bg,
      bb: bb,
      clear: clear,
    );
    await _refreshAfterWifiCommand();
  }

  Future<void> postBoardLightGameMode() async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errGameLampNeedsBoard);
    }
    await port.postLightGameMode();
    await _refreshAfterWifiCommand();
  }

  Future<void> postBoardAutoLampTimeout(int seconds) async {
    final port = _commandPortOrNull();
    if (port == null) {
      throw StateError(_strings.errAutoLampNeedsBoard);
    }
    await port.postAutoLampTimeout(seconds);
    await _refreshAfterWifiCommand();
  }
}
