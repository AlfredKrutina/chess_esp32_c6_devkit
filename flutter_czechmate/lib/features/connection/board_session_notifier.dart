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
import '../../core/localization/locale_bridge.dart';
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
import '../../core/utils/wifi_ip_third_octet_blocklist.dart';
import '../../core/utils/game_snapshot_codec.dart';
import '../../l10n/app_localizations.dart';
import 'board_session_state.dart';

final boardSessionNotifierProvider =
    StateNotifierProvider<BoardSessionNotifier, BoardSessionState>((ref) {
  return BoardSessionNotifier(
    ref.read(boardApiClientProvider),
    ref.read(prefsRepositoryProvider),
    ref,
  );
});

class BoardSessionNotifier extends StateNotifier<BoardSessionState> {
  BoardSessionNotifier(this._api, this._prefs, this._ref)
      : super(const BoardSessionState());

  final BoardApiClient _api;
  final PrefsRepository _prefs;
  final Ref _ref;

  AppLocalizations get _strings => appStringsForPrefs(_prefs);

  Timer? _pollTimer;
  Timer? _mockClockTimer;
  DateTime? _mockLastTickAt;
  Timer? _bleHandoffTimer;
  bool _bleHandoffPollInFlight = false;
  int _bleHandoffPollCount = 0;
  /// Po čerstvém BLE spojení nejdřív nepřepínat na Wi‑Fi (`connectWifi` by GATT zabilo).
  DateTime? _bleAutoHandoffNotBefore;
  final SnapshotWebSocketClient _ws = SnapshotWebSocketClient();
  final BleCzechmateClient _ble = BleCzechmateClient();

  bool _tryResumeFromPrefsInFlight = false;

  /// Po přechodu do konce partie jednou pošleme `timer_pause`, aby čas na desce nestál „v běhu“.
  bool _pauseTimerSentForFinishedGame = false;

  Future<void> _setupTutorialMutexTail = Future<void>.value();
  DateTime? _lastPostHintDestBleAt;
  String? _lastPostHintDestBleSquare;

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

  /// Telefon má IPv4 na subnetu AP desky (typicky `192.168.4.x` při hotspotu).
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
    _bleHandoffTimer =
        Timer.periodic(const Duration(milliseconds: 2500), (_) {
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

  Future<void> tryResumeFromPrefs() async {
    if (_tryResumeFromPrefsInFlight) return;
    _tryResumeFromPrefsInFlight = true;
    try {
      await _tryResumeFromPrefsBody();
    } finally {
      _tryResumeFromPrefsInFlight = false;
    }
  }

  Future<void> _tryResumeFromPrefsBody() async {
    if (_prefs.useMockBoard) {
      await connectMock();
      return;
    }
    await _runResumeTransportAttemptsOnce();
    if (_prefs.useMockBoard) return;
    if (state.busy || _resumeHasWorkingTransport()) return;

    final savedBle = _prefs.lastBleRemoteId;
    final hasSavedBle = savedBle != null && savedBle.isNotEmpty;
    final url = _prefs.lastBoardBaseUrl?.trim();
    final hasWifiUrl = url != null && url.isNotEmpty;
    if (!hasSavedBle && !hasWifiUrl) return;

    if (AppEnvironment.staging || kDebugMode) {
      debugPrint(
        '[staging] resume: druhý pokus po prodlevě (stack BT/Wi‑Fi po probuzení)',
      );
    }
    await Future<void>.delayed(const Duration(milliseconds: 750));
    if (state.busy || _resumeHasWorkingTransport()) return;
    await _runResumeTransportAttemptsOnce();
  }

  /// Wifi aktivní polling nebo BLE GATT — bez „mrtvé“ větve po resume.
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

  Future<void> _runResumeTransportAttemptsOnce() async {
    final mode = _prefs.effectiveConnectionMode;
    final preferBleOnly = mode == 'ble_only';
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
      if (!wifiOnly &&
          !await _phoneHasWifiInterface() &&
          _savedBoardUrlLooksLikePrivateLan(base)) {
        if (AppEnvironment.staging) {
          debugPrint(
            '[staging] resume: skip Wi‑Fi attempt (no Wi‑Fi link, saved URL is private LAN)',
          );
        }
        return false;
      }
      await connectWifi(base, webSocket: _prefs.useWebSocket);
      return state.transport == BoardTransport.wifi && !state.busy;
    }

    bool bleReadyAfterWifi() =>
        state.transport == BoardTransport.ble &&
        state.bleGattConnected &&
        !state.busy;

    if (preferBleOnly) {
      if (await tryBle()) return;
      if (!wifiOnly) {
        await tryWifi();
        if (state.transport == BoardTransport.wifi && !state.busy) return;
        if (bleReadyAfterWifi()) return;
      }
      return;
    }

    if (wifiOnly) {
      await tryWifi();
      if (state.transport == BoardTransport.wifi && !state.busy) return;
      if (bleReadyAfterWifi()) return;
      await tryBle();
      return;
    }

    if (lastKind == 'bluetooth' && await tryBle()) return;
    await tryWifi();
    if (state.transport == BoardTransport.wifi && !state.busy) return;
    if (bleReadyAfterWifi()) return;
    await tryBle();
  }

  /// Rychlý GET snapshotu před `connectWifi` z BLE handoffu — bez úspěchu nesmíme zrušit funkční GATT.
  Future<bool> _probeBoardHttpBeforeBleHandoff(String normalized) async {
    try {
      await _api
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

  /// Uloží `http://<sta_ip>` při online STA, nebo `http://<ap_ip>` jen když je telefon na subnetu AP
  /// (jinak by domácí Wi‑Fi uložila 192.168.4.1 a HTTP by timeoutovalo).
  Future<void> _persistBoardHttpUrlFromBle(BleNetworkInfo? info) async {
    if (info == null) return;

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
          '[staging] BLE→Wi‑Fi handoff přeskočen (cooldown po novém BLE)',
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
    if (state.transport == BoardTransport.wifi &&
        state.wifiBaseUrl == normalized) {
      return;
    }

    if (!await _probeBoardHttpBeforeBleHandoff(normalized)) {
      if (AppEnvironment.staging) {
        debugPrint(
          '[staging] BLE→Wi‑Fi handoff skipped (HTTP unreachable): $normalized',
        );
      }
      return;
    }

    if (AppEnvironment.staging) {
      debugPrint('[staging] BLE→Wi‑Fi handoff: $normalized');
    }
    await connectWifi(normalized, webSocket: _prefs.useWebSocket);
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

  Future<void> connectWifi(String baseUrl, {bool? webSocket}) async {
    final useWs = webSocket ?? _prefs.useWebSocket;
    connDebugLog('BoardSession.connectWifi', 'raw="$baseUrl" ws=$useWs');
    try {
      final normalized = normalizeBoardHttpBaseUrl(baseUrl);
      if (normalized == null) {
        connDebugLog('BoardSession.connectWifi', 'zamítnuto: neplatná URL');
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
          'zamítnuto: blokovaný 3. oktet host=$host',
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
      await _ble.disconnect();
      await _prefs.setLastBoardBaseUrl(normalized);
      await _prefs.setUseMockBoard(false);
      state = state.copyWith(
        transport: BoardTransport.wifi,
        wifiBaseUrl: normalized,
        transportStartedAt: DateTime.now(),
        busy: true,
        bleGattConnected: false,
        clearBleNetwork: true,
        clearError: true,
      );
      try {
        await _prefs.setLastBoardLinkKind('wifi');
        // Spolehlivěji než delta pollFailureCount (ta může „viset“ z dřívější session).
        final pollsOkBefore = state.pollSuccessCount;
        await _pollOnce(normalized);
        if (state.pollSuccessCount <= pollsOkBefore) {
          connDebugLog(
            'BoardSession.connectWifi',
            'první poll selhal → výjimka (pollSuccessCount)',
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
        if (useWs) {
          _ws.connect(
            normalized,
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
        state = state.copyWith(
          busy: false,
          lastError: e,
          transport: BoardTransport.none,
          bleGattConnected: false,
          clearBleNetwork: true,
          clearSnapshot: true,
          clearSnapshotReceivedAt: true,
          clearTransportStartedAt: true,
          clearTimer: true,
        );
        await _bleFallbackAfterWifiFailure();
      }
    } finally {
      _clearNextConnectionTransportOnce();
    }
  }

  /// Po selhání HTTP pollingu zkusí uložené BLE (stejná deska podle `lastBleRemoteId`).
  Future<void> _bleFallbackAfterWifiFailure() async {
    final id = _prefs.lastBleRemoteId;
    if (id == null || id.isEmpty) return;
    connDebugLog(
      'BoardSession Wi‑Fi→BLE fallback',
      'zkusím uložené BLE remoteId=$id',
    );
    await Future<void>.delayed(const Duration(milliseconds: 350));
    await reconnectSavedBle();
  }

  Future<void> connectBle(BluetoothDevice device, {String? label}) async {
    connDebugLog(
      'BoardSession.connectBle start',
      'remoteId=${device.remoteId.str} label="${label ?? device.platformName}"',
    );
    _stopBleHandoffTimer();
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
        onError: (e) {
          connDebugLog(
            'BoardSession BLE stream/error',
            connBleErrorDetail(e),
          );
          state = state.copyWith(
            busy: false,
            lastError: e,
            bleGattConnected: false,
            clearBleNetwork: true,
          );
        },
        onNetworkNotify: (raw) => unawaited(_onBleNetworkPayload(raw)),
      );
      await _prefs.setLastBleRemoteId(device.remoteId.str);
      await _prefs.setLastBleDisplayName(label ?? device.platformName);
      await _prefs.setUseMockBoard(false);
      await _prefs.setLastBoardLinkKind('bluetooth');
      state = state.copyWith(busy: false, bleGattConnected: true);
      connDebugLog(
        'BoardSession.connectBle OK',
        'GATT navázáno → sync síťových metadat',
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
        clearSnapshot: true,
        clearSnapshotReceivedAt: true,
        clearTransportStartedAt: true,
        clearTimer: true,
      );
    } finally {
      _clearNextConnectionTransportOnce();
    }
  }

  /// Znovu připojit podle `czechmate.lastBleRemoteId` (bez skenu).
  /// Odešle na desku NVS filtr DHCP (`wifi_sta_ip_block`) podle nastavení v aplikaci.
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

  Future<void> reconnectSavedBle() async {
    final id = _prefs.lastBleRemoteId;
    if (id == null || id.isEmpty) {
      connDebugLog('reconnectSavedBle', 'chyba: žádné uložené remoteId');
      state = state.copyWith(lastError: StateError(_strings.errNoSavedBle));
      return;
    }
    connDebugLog(
      'reconnectSavedBle',
      'remoteId=$id name="${_prefs.lastBleDisplayName ?? ''}"',
    );
    final dev = BluetoothDevice(remoteId: DeviceIdentifier(id));
    await connectBle(dev, label: _prefs.lastBleDisplayName);
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

  /// Jako [disconnect], ale počká na ukončení BLE — vhodné před novým připojením ze skenu.
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
    final f = from.trim().toLowerCase();
    final t = to.trim().toLowerCase();
    if (f.isEmpty || t.isEmpty || f == t) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] postRemoteMove ignored (empty or same square)');
      }
      return;
    }
    if (state.transport == BoardTransport.mock) {
      final snap = state.snapshot;
      if (snap == null) {
        throw StateError(_strings.errDemoNoSnapshot);
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
    throw StateError(_strings.errConnectBeforeMoves);
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
    final prev = _setupTutorialMutexTail;
    final done = Completer<void>();
    _setupTutorialMutexTail = done.future;
    await prev.catchError((_) {});
    try {
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
      throw StateError(_strings.errSetupNeedsConnection);
    } finally {
      if (!done.isCompleted) done.complete();
    }
  }

  /// Uloží SSID/heslo do NVS na desce a spustí STA připojení (výsledek přijde přes network notify).
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

  /// Scan okolí na desce (`wifi_survey`, šifrovaný odkaz) — výsledek přes cmd_ack.
  Future<BleWifiSurveyResult> fetchWifiSurveyOverBle() async {
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      throw StateError(_strings.errWifiProvNeedsBle);
    }
    await _ble.ensureGattReadyForCommands();
    return _ble.fetchWifiSurvey();
  }

  /// Zapnutí/vypnutí hotspotu desky přes BLE (`wifi_ap_set`). Vyžaduje šifrované spojení.
  Future<void> setBoardHotspotEnabled(bool enabled) async {
    if (state.transport != BoardTransport.ble || !state.bleGattConnected) {
      throw StateError(_strings.errBoardApNeedsBle);
    }
    await _ble.postWifiApSet(enabled);
    await Future<void>.delayed(const Duration(milliseconds: 800));
    await _syncBleNetworkMetadata();
  }

  /// Stream `.bin` přes BLE (bez hotspotu / STA). Telefon musí mít soubor stažený v apce.
  Future<void> uploadFirmwareOtaBle(
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

  /// OTA: HTTPS URL (STA na internetu) nebo `http://…` na LAN (telefon na hotspotu).
  ///
  /// [httpBoardBaseUrl] — musí být základ URL desky (`http://192.168.4.1`), stejný jako
  /// pro `fetchBoardFirmwareInfo`; přepíše `wifiBaseUrl` session, pokud je předán.
  ///
  /// [preferHttpOtaStart] — `true` při phone-host OTA: start přes HTTP na desku i v BLE režimu
  /// (telefon je na stejné síti jako HTTP API desky, např. hotspot `192.168.4.x`).
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
      await _api.postBoardOtaStart(boardBase, url: u);
      return;
    }
    if (state.transport == BoardTransport.wifi) {
      final boardBase = normalizeBoardHttpBaseUrl(httpBoardBaseUrl ?? state.wifiBaseUrl);
      if (boardBase == null || boardBase.isEmpty) {
        throw StateError(_strings.errOtaConnectFirst);
      }
      await _api.postBoardOtaStart(boardBase, url: u);
      return;
    }
    if (state.transport == BoardTransport.ble) {
      await _ble.postOtaStart(u);
      return;
    }
    throw StateError(_strings.errOtaConnectFirst);
  }

  /// LED jen na cílovém poli (`hint_highlight` s `to`).
  Future<void> postHintDestination(String square) async {
    final sq = square.trim().toLowerCase();
    if (state.transport == BoardTransport.wifi && state.wifiBaseUrl != null) {
      await _api.postHintHighlightDestinationOnly(state.wifiBaseUrl!, sq);
      return;
    }
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
      await _ble.postHintHighlightDestinationOnly(sq);
      return;
    }
    throw StateError(_strings.errHintsNeedConnection);
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
    throw StateError(_strings.errNoActiveSession);
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
    throw StateError(_strings.errBrightnessNeedsBoard);
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
    throw StateError(_strings.errLampNeedsBoard);
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
    throw StateError(_strings.errGameLampNeedsBoard);
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
    throw StateError(_strings.errAutoLampNeedsBoard);
  }
}
