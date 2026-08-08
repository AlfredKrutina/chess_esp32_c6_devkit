import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../constants/app_environment.dart';
import '../debug/connection_debug_log.dart';
import '../models/game_snapshot.dart';
import '../utils/game_snapshot_codec.dart';

/// GATT UUID z `CZECHMATEBLEUUIDs.swift` / `ble_task.c`.
final Guid czechmateServiceGuid = Guid('A0B40001-9267-4AB6-BDCC-E8336F8A8D9E');
final Guid snapshotCharacteristicGuid =
    Guid('A0B40002-9267-4AB6-BDCC-E8336F8A8D9E');
final Guid commandCharacteristicGuid =
    Guid('A0B40003-9267-4AB6-BDCC-E8336F8A8D9E');
final Guid networkCharacteristicGuid =
    Guid('A0B40004-9267-4AB6-BDCC-E8336F8A8D9E');
final Guid commandAckCharacteristicGuid =
    Guid('A0B40005-9267-4AB6-BDCC-E8336F8A8D9E');

bool _uuidMatch(Guid a, Guid b) =>
    a.toString().toLowerCase() == b.toString().toLowerCase();

bool _isBleTransportDropped(Object e) {
  final msg = '$e'.toLowerCase();
  if (e is FlutterBluePlusException && e.code == 6) return true;
  return msg.contains('disconnect') ||
      msg.contains('not connected') ||
      msg.contains('gone');
}

bool _isRetryableGattWrite(Object e) {
  if (e is FlutterBluePlusException) {
    // iOS CoreBluetooth CBATTError (raw hodnoty z FBP):
    // 5 insufficientAuthentication, 8 insufficientAuthorization (OTA before SMP),
    // — also often "prepare queue" for long entries;
    // 15 insufficientEncryption
    // 14: Linux ATT retry
    if (e.code == 5 ||
        e.code == 8 ||
        e.code == 14 ||
        e.code == 15) {
      return true;
    }
    final d = e.description?.toUpperCase() ?? '';
    return d.contains('UNLIKELY') ||
        d.contains('BUSY') ||
        d.contains('PREPARE QUEUE') ||
        d.contains('INSUFFICIENT ENCRYPTION') ||
        d.contains('ENCRYPTION IS INSUFFICIENT') ||
        d.contains('INSUFFICIENT AUTHENTICATION') ||
        d.contains('AUTHORIZATION IS INSUFFICIENT') ||
        d.contains('INSUFFICIENT AUTHORIZATION');
  }
  return false;
}

/// BLE OTA diagnostics (iOS code 8 often insufficientAuthorization / prepare queue).
void _logBleOtaChunkWriteFailure(
  Object e, {
  required int chunkIndex,
  required int attempt,
  required bool willRethrow,
}) {
  if (!AppEnvironment.staging && !(kDebugMode && willRethrow)) {
    return;
  }
  final sb = StringBuffer(
    'BLE OTA GATT write chunk=$chunkIndex attempt=${attempt + 1}'
    '${willRethrow ? ' (giving up)' : ''}: ',
  );
  if (e is FlutterBluePlusException) {
    sb.write('FBP code=${e.code} description=${e.description}; ');
  }
  sb.write('$e');
  sb.write(
    ' | ESP: look in UART for "OTA BLE chunk rejected: link not encrypted" → bonding/encryption.',
  );
  debugPrint(sb.toString());
}

/// BLE snapshot notify + chunk adding (`appendChunk` in Swift).
class BleCzechmateClient {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _snapChar;
  BluetoothCharacteristic? _cmdChar;
  BluetoothCharacteristic? _ackChar;
  BluetoothCharacteristic? _networkChar;
  StreamSubscription<List<int>>? _valueSub;
  StreamSubscription<List<int>>? _ackSub;
  StreamSubscription<List<int>>? _netSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  /// Catching `disconnected` only after GATT setup is complete — otherwise iOS/SMP will often strip the UI before `discoverServices`.
  void Function(Object)? _onDisconnectError;
  final _assembler = _BleChunkAssembler();

  /// One GATT write queue — less "prepare queue full" and less congestion on fast hints.
  Future<void> _cmdWriteTail = Future<void>.value();

  static const Duration _kMinGapBetweenBleCmdWrites =
      Duration(milliseconds: 100);

  BluetoothDevice? get device => _device;

  Future<void> disconnect() async {
    final prev = _device?.remoteId.str;
    connDebugLog('BLE stack disconnect()', prev ?? '(no device)');
    _onDisconnectError = null;
    await _valueSub?.cancel();
    await _ackSub?.cancel();
    await _netSub?.cancel();
    await _connSub?.cancel();
    _valueSub = null;
    _ackSub = null;
    _netSub = null;
    _connSub = null;
    _snapChar = null;
    _cmdChar = null;
    _ackChar = null;
    _networkChar = null;
    if (_device != null && _device!.isConnected) {
      await _device!.disconnect();
    }
    _device = null;
    _assembler.reset();
    _cmdWriteTail = Future<void>.value();
  }

  void _attachDisconnectListener() {
    _connSub?.cancel();
    _connSub = null;
    final d = _device;
    final handler = _onDisconnectError;
    if (d == null || handler == null) {
      return;
    }
    _connSub = d.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected) {
        connDebugLog(
          'BLE connectionState → disconnected',
          'remoteId=${d.remoteId.str}',
        );
        handler(StateError('Bluetooth disconnected'));
      }
    });
  }

  Future<List<BluetoothService>> _discoverServicesRobust(
      BluetoothDevice device) async {
    if (defaultTargetPlatform != TargetPlatform.iOS) {
      connDebugLog('discoverServices', 'single attempt (non-iOS)');
      return device.discoverServices();
    }
    Object? lastErr;
    for (var attempt = 0; attempt < 4; attempt++) {
      try {
        if (!device.isConnected) {
          throw StateError('Bluetooth disconnected');
        }
        final list = await device.discoverServices();
        connDebugLog(
          'discoverServices OK',
          'iOS attempt=${attempt + 1} services=${list.length}',
        );
        return list;
      } catch (e) {
        lastErr = e;
        connDebugLog(
          'discoverServices error',
          'iOS attempt=${attempt + 1}/4 ${connBleErrorDetail(e)}',
        );
        if (attempt == 3) {
          rethrow;
        }
        await Future<void>.delayed(
          Duration(milliseconds: 320 + attempt * 220),
        );
      }
    }
    throw lastErr ?? StateError('discoverServices failed');
  }

  /// [onNetworkNotify] — firmware sends notifications to network char when STA/IP changes (`ble_task_push_network_info`).
  Future<void> connect(
    BluetoothDevice device, {
    required void Function(GameSnapshot snap) onSnapshot,
    required void Function(Object error) onError,
    void Function(List<int> raw)? onNetworkNotify,
  }) async {
    connDebugLog(
      'GATT session start',
      'remoteId=${device.remoteId.str} platformName="${device.platformName}"',
    );
    await disconnect();
    _device = device;
    _onDisconnectError = onError;
    try {
      connDebugLog('GAP device.connect()', 'timeout=15s');
      await device.connect(timeout: const Duration(seconds: 15));
      connDebugLog(
        'GAP device.connect done',
        'isConnected=${device.isConnected}',
      );
      /* iOS: short delay for link settle; SMP on the board is deferred (~1.8 s),
       * so GATT discovery can start earlier than the previous 1350 ms + SMP together. */
      if (defaultTargetPlatform == TargetPlatform.iOS) {
        connDebugLog('iOS delay before discoverServices', '600ms');
        await Future<void>.delayed(const Duration(milliseconds: 600));
      }
      final services = await _discoverServicesRobust(device);
      try {
        await device.requestMtu(517);
        connDebugLog('requestMtu', '517');
      } catch (e) {
        connDebugLog('requestMtu skipped', connBleErrorDetail(e));
      }
      BluetoothCharacteristic? snapChar;
      BluetoothCharacteristic? ackChar;
      for (final svc in services) {
        if (!_uuidMatch(svc.uuid, czechmateServiceGuid)) continue;
        for (final c in svc.characteristics) {
          if (_uuidMatch(c.uuid, snapshotCharacteristicGuid)) snapChar = c;
          if (_uuidMatch(c.uuid, commandCharacteristicGuid)) _cmdChar = c;
          if (_uuidMatch(c.uuid, networkCharacteristicGuid)) _networkChar = c;
          if (_uuidMatch(c.uuid, commandAckCharacteristicGuid)) ackChar = c;
        }
      }
      _ackChar = ackChar;
      if (snapChar == null) {
        connDebugLog(
          'GATT session FAILED',
          'missing snapshot characteristic (service UUID not found?)',
        );
        await disconnect();
        throw StateError('Missing GATT snapshot characteristic');
      }
      _snapChar = snapChar;
      _assembler.reset();
      /* READ before enabling notify — otherwise CM chunks hit an empty assembler
       * in parallel with READ and can corrupt the sequence behind the scenes.
       * iOS often returns only a JSON slice from READ (~MTU) → FormatException; the first
       * valid state still arrives via notify. */
      if (defaultTargetPlatform != TargetPlatform.iOS) {
        try {
          final initialSnap = await readSnapshot();
          onSnapshot(initialSnap);
          connDebugLog('readSnapshot after connect', 'OK');
        } catch (e) {
          connDebugLog(
            'readSnapshot after connect failed → relying on notify',
            connBleErrorDetail(e),
          );
        }
      } else {
        connDebugLog(
          'readSnapshot after connect',
          'skipped on iOS (shortened READ → notify only)',
        );
      }
      await snapChar.setNotifyValue(true);
      _valueSub = snapChar.onValueReceived.listen(
        (List<int> raw) {
          try {
            final complete = _assembler.push(Uint8List.fromList(raw));
            if (complete == null) return;
            onSnapshot(GameSnapshotCodec.decodeBytes(complete));
          } catch (e) {
            onError(e);
          }
        },
        onError: onError,
      );
      if (ackChar != null) {
        await ackChar.setNotifyValue(true);
        _ackSub = ackChar.onValueReceived.listen((raw) {
          // Here we could handle cmd_ack logic (not strictly required if we just blindly send over BLE)
        });
      }
      final netChar = _networkChar;
      if (netChar != null &&
          onNetworkNotify != null &&
          (netChar.properties.notify || netChar.properties.indicate)) {
        await netChar.setNotifyValue(true);
        _netSub = netChar.onValueReceived.listen(
          onNetworkNotify,
          onError: onError,
        );
      }
      _attachDisconnectListener();
      connDebugLog(
        'GATT session ready',
        'network notify=${netChar != null && onNetworkNotify != null}',
      );
    } catch (e, st) {
      connDebugLog('GATT session FAILED', connBleErrorDetail(e));
      if (kDebugMode || AppEnvironment.staging) {
        debugPrint('[czechmate][conn] stackTrace: $st');
      }
      await disconnect();
      rethrow;
    }
  }

  static BleNetworkInfo? tryParseNetworkJson(List<int> raw) {
    try {
      if (raw.isEmpty) return null;
      final parsed = jsonDecode(utf8.decode(raw));
      if (parsed is! Map<String, dynamic>) return null;
      final staIp = (parsed['sta_ip'] as String?)?.trim();
      final apIpRaw = (parsed['ap_ip'] as String?)?.trim() ?? '';
      final staSsid = (parsed['sta_ssid'] as String?)?.trim();
      final staConnected = parsed['sta_connected'] == true;
      final online = parsed['online'] == true;
      final apActiveRaw = parsed['ap_active'];
      final bool apActive;
      if (apActiveRaw is bool) {
        apActive = apActiveRaw;
      } else {
        apActive = apIpRaw.isNotEmpty;
      }
      final apIp = apActive && apIpRaw.isNotEmpty ? apIpRaw : (apActive ? '192.168.4.1' : null);
      final apSsid = (parsed['ap_ssid'] as String?)?.trim();
      return BleNetworkInfo(
        staIp: (staIp == null || staIp.isEmpty) ? null : staIp,
        apIp: apIp,
        staSsid: (staSsid == null || staSsid.isEmpty) ? null : staSsid,
        staConnected: staConnected,
        online: online,
        apActive: apActive,
        apSsid: (apSsid == null || apSsid.isEmpty) ? null : apSsid,
      );
    } catch (_) {
      return null;
    }
  }

  /// Restores GATT after a line drop: UI sometimes keeps `bleGattConnected`, but
  /// `device.isConnected` is false → `writeCharacteristic` reports fbp-code 6.
  Future<void> ensureGattReadyForCommands() async {
    final d = _device;
    if (d == null) {
      connDebugLog('ensureGattReady', 'error: device null');
      throw StateError('BLE device not set');
    }
    connDebugLog(
      'ensureGattReady start',
      'remoteId=${d.remoteId.str} isConnected=${d.isConnected}',
    );
    var iosDelayAfterLink = false;
    if (!d.isConnected) {
      connDebugLog('ensureGattReady', 'device.connect 20s');
      await d.connect(timeout: const Duration(seconds: 20));
      iosDelayAfterLink = defaultTargetPlatform == TargetPlatform.iOS;
    }
    if (iosDelayAfterLink) {
      await Future<void>.delayed(const Duration(milliseconds: 900));
    }
    final services = await _discoverServicesRobust(d);
    try {
      await d.requestMtu(517);
    } catch (_) {}
    BluetoothCharacteristic? cmdChar;
    BluetoothCharacteristic? netChar;
    BluetoothCharacteristic? ackChar;
    for (final svc in services) {
      if (!_uuidMatch(svc.uuid, czechmateServiceGuid)) continue;
      for (final c in svc.characteristics) {
        if (_uuidMatch(c.uuid, commandCharacteristicGuid)) cmdChar = c;
        if (_uuidMatch(c.uuid, networkCharacteristicGuid)) netChar = c;
        if (_uuidMatch(c.uuid, commandAckCharacteristicGuid)) ackChar = c;
      }
    }
    if (cmdChar == null) {
      connDebugLog('ensureGattReady FAIL', 'cmd characteristic not found');
      throw StateError('BLE command characteristic not found');
    }
    _cmdChar = cmdChar;
    _ackChar = ackChar;
    if (netChar != null) {
      _networkChar = netChar;
    }
    _attachDisconnectListener();
    connDebugLog('ensureGattReady OK', 'cmd+net reattached');
  }

  /// OTA and other sensitive BLE commands require active link encryption (SMP) on the board.
  /// iOS often sends a write early → CBATTError 8 (insufficientAuthorization); Bond will help Android.
  Future<void> prepareEncryptedBleLink() async {
    await ensureGattReadyForCommands();
    final d = _device;
    if (d == null) {
      throw StateError('BLE device not set');
    }

    if (defaultTargetPlatform == TargetPlatform.android) {
      try {
        await d.createBond(timeout: 75);
      } catch (_) {}
      await Future<void>.delayed(const Duration(milliseconds: 450));
      return;
    }

    if (defaultTargetPlatform != TargetPlatform.iOS) {
      await Future<void>.delayed(const Duration(milliseconds: 280));
      return;
    }

    Object? lastErr;
    for (var i = 0; i < 42; i++) {
      try {
        final st = await fetchOtaBleStatus(
          responseTimeout: const Duration(seconds: 4),
        );
        if (st != null) {
          return;
        }
        lastErr = StateError('ota_ble_status: no cmd_ack');
      } catch (e) {
        lastErr = e;
        final fbp = e is FlutterBluePlusException ? e : null;
        if (!_isRetryableGattWrite(e) &&
            !(fbp != null && fbp.code == 6) &&
            i > 10) {
          rethrow;
        }
      }
      await Future<void>.delayed(Duration(milliseconds: 260 + i * 22));
    }
    throw lastErr ??
        StateError(
          'Bluetooth encryption not ready. Accept any pairing prompt for the '
          'board, wait a few seconds, then retry OTA.',
        );
  }

  Future<void> _writeCmd(Map<String, dynamic> cmd) async {
    if (_cmdChar == null) throw StateError('Not connected or missing cmdChar');
    final bytes = utf8.encode(jsonEncode(cmd));
    final prev = _cmdWriteTail;
    final done = Completer<void>();
    _cmdWriteTail = done.future;
    await prev.catchError((_) {});
    try {
      await Future<void>.delayed(_kMinGapBetweenBleCmdWrites);
      await _writeCmdBytesWithRetries(bytes);
    } finally {
      if (!done.isCompleted) done.complete();
    }
  }

  Future<void> _writeCmdBytesWithRetries(List<int> bytes) async {
    Object? lastError;
    final maxAttempts = defaultTargetPlatform == TargetPlatform.iOS ? 8 : 4;
    for (var attempt = 0; attempt < maxAttempts; attempt++) {
      try {
        await _cmdChar!.write(bytes, withoutResponse: false);
        if (attempt > 0 && AppEnvironment.staging) {
          debugPrint('[staging] BLE cmd OK after retry (attempt ${attempt + 1})');
        }
        return;
      } catch (e) {
        lastError = e;
        final fbp = e is FlutterBluePlusException ? e : null;
        if (fbp?.code == 6 && attempt < maxAttempts - 1) {
          try {
            await ensureGattReadyForCommands();
          } catch (_) {}
          await Future<void>.delayed(
            Duration(milliseconds: 260 + attempt * 130),
          );
          continue;
        }
        if (!_isRetryableGattWrite(e) || attempt == maxAttempts - 1) rethrow;
        /* iOS 5/15 encryption/auth; 8 prepare queue — longer delays than for short JSON. */
        final baseMs =
            defaultTargetPlatform == TargetPlatform.iOS ? 180 : 60;
        await Future<void>.delayed(
          Duration(milliseconds: baseMs + attempt * 120),
        );
      }
    }
    throw lastError ?? StateError('BLE write failed');
  }

  Future<void> postMove(String from, String to, {String? promotion}) async {
    final cmd = {
      'cmd': 'move',
      'from': from.toLowerCase(),
      'to': to.toLowerCase()
    };
    if (promotion != null) cmd['promotion'] = promotion.toLowerCase();
    await _writeCmd(cmd);
  }

  Future<void> postNewGame({String? fen}) async {
    final cmd = <String, dynamic>{'cmd': 'new_game'};
    if (fen != null && fen.trim().isNotEmpty) cmd['fen'] = fen.trim();
    await _writeCmd(cmd);
  }

  Future<void> postUndo() async => await _writeCmd({'cmd': 'undo'});

  Future<void> postPromotion(String choice) async {
    await _writeCmd({'cmd': 'promotion', 'choice': choice.toLowerCase()});
  }

  Future<void> postTimerConfig(int type,
      {int? customMinutes, int? customIncrement}) async {
    final cmd = <String, dynamic>{'cmd': 'timer_config', 'type': type};
    if (type == 14) {
      if (customMinutes != null) cmd['custom_minutes'] = customMinutes;
      if (customIncrement != null) cmd['custom_increment'] = customIncrement;
    }
    await _writeCmd(cmd);
  }

  Future<void> postTimerPause() async =>
      await _writeCmd({'cmd': 'timer_pause'});
  Future<void> postTimerResume() async =>
      await _writeCmd({'cmd': 'timer_resume'});
  Future<void> postTimerReset() async =>
      await _writeCmd({'cmd': 'timer_reset'});

  Future<void> postHintHighlight(String from, String to) async {
    await _writeCmd({
      'cmd': 'hint_highlight',
      'from': from.toLowerCase(),
      'to': to.toLowerCase()
    });
  }

  Future<void> postHintClear() async => await _writeCmd({'cmd': 'hint_clear'});

  Future<void> postGuardClear() async => await _writeCmd({'cmd': 'guard_clear'});

  /// Parity `BLEBoardTransport.postHintHighlightDestinationOnly`.
  Future<void> postHintHighlightDestinationOnly(String toSquare) async {
    await _writeCmd({'cmd': 'hint_highlight', 'to': toSquare.toLowerCase()});
  }

  /// Parity `BLEBoardTransport.postSetupTutorial`.
  Future<void> postSetupTutorial(String action) async {
    await _writeCmd({'cmd': 'setup_tutorial', 'action': action});
  }

  /// Parity `POST /api/game/opening` — start | cancel | hint | checkpoint_ack.
  Future<void> postOpening(Map<String, dynamic> body) async {
    final cmd = <String, dynamic>{'cmd': 'opening'};
    cmd.addAll(body);
    await _writeCmd(cmd);
  }

  Future<void> postBrightness(int percent) async {
    await _writeCmd({'cmd': 'brightness', 'percent': percent.clamp(0, 100)});
  }

  /// Parity `BLEBoardTransport.postLightCommand` / `POST /api/light/command`.
  Future<void> postLightCommand({
    required bool state,
    required int r,
    required int g,
    required int b,
  }) async {
    final rs = r.clamp(0, 255);
    final gs = g.clamp(0, 255);
    final bs = b.clamp(0, 255);
    await _writeCmd({
      'cmd': 'light_command',
      'state': state,
      'r': rs,
      'g': gs,
      'b': bs,
    });
  }

  /// Parity `POST /api/light/game_mode`.
  Future<void> postLightGameMode() async {
    await _writeCmd({'cmd': 'light_game_mode'});
  }

  /// BLE ekvivalent `POST /api/system/ota` — pouze HTTPS URL na `.bin`.
  Future<void> postOtaStart(String httpsFirmwareUrl) async {
    await prepareEncryptedBleLink();
    await _writeCmd({'cmd': 'ota_start', 'url': httpsFirmwareUrl.trim()});
  }

  /// Begin OTA stream over BLE (`ota_ble_begin`); after her chunky with magic `OB`.
  Future<void> postOtaBleBegin(int sizeBytes) async {
    await _writeCmd({'cmd': 'ota_ble_begin', 'size': sizeBytes});
  }

  Future<void> postOtaBleAbort() async {
    await _writeCmd({'cmd': 'ota_ble_abort'});
  }

  Future<void> _waitUntilBleConnected(Duration timeout) async {
    final d = _device;
    if (d == null) {
      throw StateError('BLE device not set');
    }
    if (d.isConnected) return;
    await d.connectionState
        .where((s) => s == BluetoothConnectionState.connected)
        .first
        .timeout(timeout);
  }

  /// Status of suspended / active BLE stream OTA on the board (`ota_ble_status`).
  Future<OtaBleStatus?> fetchOtaBleStatus({
    Duration responseTimeout = const Duration(seconds: 8),
  }) async {
    await ensureGattReadyForCommands();
    final ack = _ackChar;
    if (ack == null) return null;
    await ack.setNotifyValue(true);
    final completer = Completer<OtaBleStatus?>();
    late final StreamSubscription<List<int>> sub;
    sub = ack.onValueReceived.listen(
      (raw) {
        try {
          final dynamic parsed = jsonDecode(utf8.decode(raw));
          if (parsed is! Map) return;
          final map = Map<String, dynamic>.from(parsed);
          if (map['cmd'] != 'ota_ble_status') return;
          if (!completer.isCompleted) {
            completer.complete(OtaBleStatus.fromJson(map));
          }
        } catch (_) {}
      },
      onError: (_) {
        if (!completer.isCompleted) completer.complete(null);
      },
    );
    try {
      await _writeCmd({'cmd': 'ota_ble_status'});
      return await completer.future.timeout(
        responseTimeout,
        onTimeout: () => null,
      );
    } finally {
      await sub.cancel();
    }
  }

  /// Uploads the entire `.bin` via GATT CMD — without Wi‑Fi (`OB` chunks + firmware bytes).
  ///
  /// In the event of a Bluetooth failure, the board pauses the session (up to 24 h); after reconnecting
  /// continues from the same chunk index. [onPhase]: `paused_waiting_reconnect`, `resumed`.
  Future<void> uploadFirmwareBle(
    File bin, {
    void Function(int pct)? onProgress,
    void Function(String phase)? onPhase,
  }) async {
    final len = await bin.length();
    if (len < 32 * 1024) {
      throw StateError('Firmware file too small');
    }
    await prepareEncryptedBleLink();
    final d = _device;
    if (d == null || _cmdChar == null) {
      throw StateError('BLE cmd characteristic not ready');
    }
    try {
      await d.requestMtu(517);
    } catch (_) {}
    await Future<void>.delayed(const Duration(milliseconds: 60));
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      await Future<void>.delayed(const Duration(milliseconds: 220));
    }
    final mtu = d.mtuNow;
    /*
     * OB packet = 6 B header + payload; one GATT write must fit in one ATT value
     * of length ≤ (mtu − 3). Otherwise iOS splits Prepare Write → queue (apple-code 8, “authorization”).
     * Requirement: 6 + payload ≤ mtu − 3  ⇒  payload ≤ mtu − 9; −1 B reserve ⇒ mtu − 10.
     */
    final int singlePduPayload = mtu >= 27
        ? (mtu - 10).clamp(12, 400).toInt()
        : (mtu - 9).clamp(8, 400).toInt();
    final int payloadMax = defaultTargetPlatform == TargetPlatform.iOS
        ? singlePduPayload.clamp(12, 236).toInt()
        : singlePduPayload.clamp(12, 500).toInt();
    final chunkTotal = (len + payloadMax - 1) ~/ payloadMax;
    if (chunkTotal > 65535) {
      throw StateError('Firmware too large for BLE chunk protocol');
    }

    const resumeTimeout = Duration(hours: 24);
    var transferBegun = false;
    final chunkGapMs = defaultTargetPlatform == TargetPlatform.iOS ? 55 : 0;

    Future<void> negotiateMtuLight() async {
      try {
        await d.requestMtu(517);
      } catch (_) {}
      await Future<void>.delayed(const Duration(milliseconds: 40));
      if (defaultTargetPlatform == TargetPlatform.iOS) {
        await Future<void>.delayed(const Duration(milliseconds: 200));
      }
    }

    RandomAccessFile? raf;
    try {
      raf = await bin.open();
      var offset = 0;
      var idx = 0;

      while (offset < len) {
        try {
          if (!transferBegun) {
            await postOtaBleBegin(len);
            transferBegun = true;
            if (defaultTargetPlatform == TargetPlatform.iOS) {
              await Future<void>.delayed(const Duration(milliseconds: 140));
            }
          }

          while (offset < len) {
            final take =
                payloadMax < len - offset ? payloadMax : len - offset;
            final chunk = await raf.read(take);
            if (chunk.length != take) {
              throw StateError('Unexpected EOF reading firmware');
            }
            final pkt = Uint8List(6 + chunk.length);
            pkt[0] = 0x4f;
            pkt[1] = 0x42;
            pkt[2] = idx & 0xff;
            pkt[3] = (idx >> 8) & 0xff;
            pkt[4] = chunkTotal & 0xff;
            pkt[5] = (chunkTotal >> 8) & 0xff;
            pkt.setRange(6, 6 + chunk.length, chunk);

            for (var attempt = 0; attempt < 12; attempt++) {
              final wc = _cmdChar;
              if (wc == null) {
                throw StateError('BLE cmd characteristic not ready');
              }
              try {
                await wc.write(
                  pkt,
                  withoutResponse: false,
                  /* Single-PDU writes — no long Prepare Write on iOS. */
                  allowLongWrite: false,
                  timeout: defaultTargetPlatform == TargetPlatform.iOS ? 45 : 15,
                );
                break;
              } catch (e) {
                final fbp = e is FlutterBluePlusException ? e : null;
                final willRethrow =
                    !_isRetryableGattWrite(e) || attempt == 11;
                _logBleOtaChunkWriteFailure(
                  e,
                  chunkIndex: idx,
                  attempt: attempt,
                  willRethrow: willRethrow,
                );
                if (fbp?.code == 6 && attempt < 11) {
                  try {
                    await ensureGattReadyForCommands();
                  } catch (_) {}
                  await Future<void>.delayed(
                    Duration(milliseconds: 200 + attempt * 80),
                  );
                  continue;
                }
                if (willRethrow) {
                  rethrow;
                }
                final ios = defaultTargetPlatform == TargetPlatform.iOS;
                final code = fbp?.code ?? -1;
                /* 8 ≈ prepareQueueFull — longer pause; 5/15 ≈ auth/encryption — SMP. */
                final int baseMs;
                final int stepMs;
                if (code == 8) {
                  baseMs = ios ? 480 : 140;
                  stepMs = ios ? 200 : 100;
                } else if (code == 15 || code == 5) {
                  baseMs = ios ? 360 : 120;
                  stepMs = ios ? 160 : 90;
                } else {
                  baseMs = ios ? 100 : 40;
                  stepMs = ios ? 110 : 90;
                }
                await Future<void>.delayed(
                  Duration(milliseconds: baseMs + attempt * stepMs),
                );
              }
            }

            offset += take;
            idx++;
            onProgress?.call((offset * 100 ~/ len).clamp(0, 99));
            if (chunkGapMs > 0) {
              await Future<void>.delayed(Duration(milliseconds: chunkGapMs));
            }
          }
        } catch (e) {
          if (!transferBegun || !_isBleTransportDropped(e)) {
            try {
              await postOtaBleAbort();
            } catch (_) {}
            rethrow;
          }
          onPhase?.call('paused_waiting_reconnect');
          await _waitUntilBleConnected(resumeTimeout);
          await negotiateMtuLight();
          await ensureGattReadyForCommands();
          final st = await fetchOtaBleStatus();
          if (st == null || !st.session || !st.paused) {
            try {
              await postOtaBleAbort();
            } catch (_) {}
            throw StateError(
              'BLE OTA: board did not pause session (24h expiry or abort).',
            );
          }
          offset = st.bytes;
          idx = st.nextChunk;
          await raf.setPosition(offset);
          onPhase?.call('resumed');
        }
      }
    } finally {
      await raf?.close();
    }
  }

  /// Parity `POST /api/settings/lamp` (`auto_lamp_timeout_sec`).
  Future<void> postAutoLampTimeout(int seconds) async {
    final v = seconds.clamp(5, 7200);
    await _writeCmd({'cmd': 'settings_auto_lamp_timeout', 'seconds': v});
  }

  /// Saves STA SSID/password to NVS on board and connects Wi‑Fi (`wifi_ble_prov` task).
  Future<void> postWifiStaConfig(String ssid, String password) async {
    await _writeCmd({
      'cmd': 'wifi_sta_config',
      'ssid': ssid.trim(),
      'password': password,
    });
  }

  /// NVS on board + when DHCP reject IP with blocked 3rd octet (`wifi_sta_ip_block`, encrypted BLE).
  Future<void> postWifiStaIpBlock(String thirdOctetsCsv) async {
    await ensureGattReadyForCommands();
    await _writeCmd({
      'cmd': 'wifi_sta_ip_block',
      'third_octets': thirdOctetsCsv.trim(),
    });
  }

  /// Active scan of the surroundings on the board; the response comes via `cmd_ack` notify (`wifi_survey`).
  Future<BleWifiSurveyResult> fetchWifiSurvey({
    Duration timeout = const Duration(seconds: 18),
  }) async {
    await ensureGattReadyForCommands();
    final ack = _ackChar;
    if (ack == null) {
      throw StateError('BLE wifi_survey: missing cmd_ack characteristic');
    }
    final completer = Completer<BleWifiSurveyResult>();
    final acc = <int>[];
    late final StreamSubscription<List<int>> sub;
    late final Timer timer;

    void completeOnce(BleWifiSurveyResult r) {
      timer.cancel();
      if (!completer.isCompleted) {
        completer.complete(r);
      }
    }

    sub = ack.onValueReceived.listen(
      (raw) {
        acc.addAll(raw);
        final r = BleWifiSurveyResult.tryParse(acc);
        if (r != null) {
          unawaited(sub.cancel());
          completeOnce(r);
        }
      },
      onError: (Object e) {
        unawaited(sub.cancel());
        if (!completer.isCompleted) {
          completer.completeError(e);
        }
      },
    );

    timer = Timer(timeout, () {
      unawaited(sub.cancel());
      if (!completer.isCompleted) {
        completer.complete(
          const BleWifiSurveyResult(
            ok: false,
            networks: [],
            message: 'timeout',
          ),
        );
      }
    });

    await _writeCmd({'cmd': 'wifi_survey'});
    try {
      return await completer.future;
    } finally {
      timer.cancel();
      await sub.cancel();
    }
  }

  /// Enable/disable Wi‑Fi hotspot on the board (`wifi_ap_set`, encrypted BLE).
  Future<void> postWifiApSet(bool enabled) async {
    await _writeCmd({
      'cmd': 'wifi_ap_set',
      'enabled': enabled,
    });
  }

  /// BLE ekvivalent iOS `fetchNetworkInfo` (sta_ip/ap_ip/online).
  /// One-time READ snapshot (firmware `BLE_GATT_ACCESS_OP_READ_CHR` on snap UUID).
  Future<GameSnapshot> readSnapshot() async {
    final c = _snapChar;
    final d = _device;
    if (c == null || d == null || !d.isConnected) {
      throw StateError('BLE snapshot read: not connected');
    }
    final raw = await c.read().timeout(
          const Duration(seconds: 28),
          onTimeout: () => throw TimeoutException('BLE snapshot read'),
        );
    if (raw.isEmpty) {
      throw StateError('BLE snapshot read: empty response');
    }
    return GameSnapshotCodec.decodeBytes(raw);
  }

  Future<BleNetworkInfo?> fetchNetworkInfo() async {
    final c = _networkChar;
    if (c == null) return null;
    try {
      final raw = await c.read();
      return tryParseNetworkJson(raw);
    } catch (_) {
      return null;
    }
  }
}

/// One network from BLE `wifi_survey`.
class BleWifiSurveyNetwork {
  const BleWifiSurveyNetwork({required this.ssid, required this.rssi});

  final String ssid;
  final int rssi;
}

/// The result of `wifi_survey` (cmd_ack JSON from the board, may arrive in multiple notifications).
class BleWifiSurveyResult {
  const BleWifiSurveyResult({
    required this.ok,
    required this.networks,
    this.message,
  });

  final bool ok;
  final List<BleWifiSurveyNetwork> networks;
  final String? message;

  static BleWifiSurveyResult? tryParse(List<int> raw) {
    if (raw.length < 8) {
      return null;
    }
    try {
      final s = utf8.decode(raw);
      final dynamic parsed = jsonDecode(s);
      if (parsed is! Map<String, dynamic>) {
        return null;
      }
      if (parsed['channel'] != 'cmd_ack') {
        return null;
      }
      final cmd = parsed['cmd']?.toString();
      if (cmd != 'wifi_survey') {
        return null;
      }
      final success = parsed['ok'] == true;
      if (!success) {
        return BleWifiSurveyResult(
          ok: false,
          networks: const [],
          message: parsed['message']?.toString(),
        );
      }
      final nets = <BleWifiSurveyNetwork>[];
      final arr = parsed['networks'];
      if (arr is List<dynamic>) {
        for (final x in arr) {
          if (x is Map<String, dynamic>) {
            final name = x['ssid']?.toString() ?? '';
            final r = (x['rssi'] as num?)?.toInt() ?? -100;
            if (name.isNotEmpty) {
              nets.add(BleWifiSurveyNetwork(ssid: name, rssi: r));
            }
          }
        }
      }
      return BleWifiSurveyResult(ok: true, networks: nets);
    } catch (_) {
      return null;
    }
  }

  bool hasSsid(String? ssid) {
    if (ssid == null || ssid.isEmpty) {
      return false;
    }
    for (final n in networks) {
      if (n.ssid == ssid) {
        return true;
      }
    }
    return false;
  }
}

/// Reply to `ota_ble_status` (cmd_ack JSON from board).
class OtaBleStatus {
  const OtaBleStatus({
    required this.session,
    required this.paused,
    required this.activeRx,
    required this.bytes,
    required this.total,
    required this.nextChunk,
    required this.percent,
  });

  factory OtaBleStatus.fromJson(Map<String, dynamic> m) {
    return OtaBleStatus(
      session: m['session'] == true,
      paused: m['paused'] == true,
      activeRx: m['active_rx'] == true,
      bytes: (m['bytes'] as num?)?.toInt() ?? 0,
      total: (m['total'] as num?)?.toInt() ?? 0,
      nextChunk: (m['next_chunk'] as num?)?.toInt() ?? 0,
      percent: (m['percent'] as num?)?.toInt() ?? 0,
    );
  }

  final bool session;
  final bool paused;
  final bool activeRx;
  final int bytes;
  final int total;
  final int nextChunk;
  final int percent;
}

class BleNetworkInfo {
  const BleNetworkInfo({
    required this.staIp,
    required this.apIp,
    required this.online,
    this.staSsid,
    this.staConnected = false,
    this.apActive = false,
    this.apSsid,
  });

  final String? staIp;
  final String? apIp;
  /// SSID of the network to which the board is connected as a STA (from firmware JSON).
  final String? staSsid;
  final bool staConnected;
  final bool online;
  /// Hotspot plates (AP) are currently broadcasting.
  final bool apActive;
  /// Hotspot SSID from network notify (optional).
  final String? apSsid;
}

class _BleChunkAssembler {
  Uint8List _buf = Uint8List(0);
  int _part = 0;
  int _total = 0;

  void reset() {
    _buf = Uint8List(0);
    _part = 0;
    _total = 0;
  }

  /// Returns complete JSON bytes or `null` (waiting for next chunk).
  List<int>? push(Uint8List data) {
    if (data.isEmpty) {
      return null;
    }
    /* READ char returns raw JSON (`{`…); notify uses CM header + chunks. */
    final bool cmChunk =
        data.length >= 4 && data[0] == 0x43 && data[1] == 0x4d;
    if (!cmChunk) {
      if (data[0] == 0x7b) {
        return data.toList();
      }
      if (AppEnvironment.staging || kDebugMode) {
        debugPrint(
          'BLE snapshot: unexpected frame without CM (len=${data.length}), ignoring',
        );
      }
      return null;
    }
    final part = data[2];
    final total = data[3];
    final payload = data.sublist(4);
    if (part < 1 || total < 1 || part > total) {
      if (AppEnvironment.staging || kDebugMode) {
        debugPrint(
          'BLE snapshot chunk: invalid part/total ($part/$total), reset',
        );
      }
      reset();
      return null;
    }
    if (part == 1) {
      _buf = Uint8List.fromList(payload);
      _part = 1;
      _total = total;
      if (total == 1) {
        final out = _buf.toList();
        reset();
        return out;
      }
      return null;
    }
    if (_total != total || _part != part - 1) {
      if (AppEnvironment.staging || kDebugMode) {
        debugPrint(
          'BLE snapshot chunk: discontinuous sequence (expected part ${_part + 1}, '
          'total=$_total; got part=$part total=$total), reset',
        );
      }
      reset();
      return null;
    }
    final merged = Uint8List(_buf.length + payload.length);
    merged.setAll(0, _buf);
    merged.setAll(_buf.length, payload);
    _buf = merged;
    _part = part;
    if (part == total) {
      final out = _buf.toList();
      reset();
      return out;
    }
    return null;
  }
}
