import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../constants/app_environment.dart';
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
    // 14: Linux ATT retry
    // 15: iOS CBATTErrorInsufficientEncryption — počkat na SMP
    // 8:  iOS CBATTErrorInsufficientAuthorization — často dokud nedoběhne šifrování / bonding
    if (e.code == 8 || e.code == 14 || e.code == 15) return true;
    final d = e.description?.toUpperCase() ?? '';
    return d.contains('UNLIKELY') ||
        d.contains('BUSY') ||
        d.contains('INSUFFICIENT ENCRYPTION') ||
        d.contains('ENCRYPTION IS INSUFFICIENT') ||
        d.contains('AUTHORIZATION IS INSUFFICIENT') ||
        d.contains('INSUFFICIENT AUTHORIZATION');
  }
  return false;
}

/// BLE snapshot notify + chunk skládání (`appendChunk` ve Swift).
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
  final _assembler = _BleChunkAssembler();

  BluetoothDevice? get device => _device;

  Future<void> disconnect() async {
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
  }

  /// [onNetworkNotify] — firmware posílá notify na network char při změně STA/IP (`ble_task_push_network_info`).
  Future<void> connect(
    BluetoothDevice device, {
    required void Function(GameSnapshot snap) onSnapshot,
    required void Function(Object error) onError,
    void Function(List<int> raw)? onNetworkNotify,
  }) async {
    await disconnect();
    _device = device;
    await device.connect(timeout: const Duration(seconds: 15));
    _connSub = device.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected) {
        onError(StateError('Bluetooth disconnected'));
      }
    });
    final services = await device.discoverServices();
    try {
      await device.requestMtu(517);
    } catch (_) {}
    /* iOS: peripheral po CONNECT zahajuje SMP; krátká pauza před prvním zápisem sníží „encryption insufficient“. */
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      await Future<void>.delayed(const Duration(milliseconds: 600));
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
      await disconnect();
      throw StateError('Missing GATT snapshot characteristic');
    }
    _snapChar = snapChar;
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
  }

  static BleNetworkInfo? tryParseNetworkJson(List<int> raw) {
    try {
      if (raw.isEmpty) return null;
      final parsed = jsonDecode(utf8.decode(raw));
      if (parsed is! Map<String, dynamic>) return null;
      final staIp = (parsed['sta_ip'] as String?)?.trim();
      final apIp = (parsed['ap_ip'] as String?)?.trim();
      final staSsid = (parsed['sta_ssid'] as String?)?.trim();
      final staConnected = parsed['sta_connected'] == true;
      final online = parsed['online'] == true;
      return BleNetworkInfo(
        staIp: (staIp == null || staIp.isEmpty) ? null : staIp,
        apIp: (apIp == null || apIp.isEmpty) ? null : apIp,
        staSsid: (staSsid == null || staSsid.isEmpty) ? null : staSsid,
        staConnected: staConnected,
        online: online,
      );
    } catch (_) {
      return null;
    }
  }

  /// Obnoví GATT po výpadku linku: UI někdy drží `bleGattConnected`, ale
  /// `device.isConnected` je false → `writeCharacteristic` hlásí fbp-code 6.
  Future<void> ensureGattReadyForCommands() async {
    final d = _device;
    if (d == null) {
      throw StateError('BLE device not set');
    }
    if (!d.isConnected) {
      await d.connect(timeout: const Duration(seconds: 20));
      if (defaultTargetPlatform == TargetPlatform.iOS) {
        await Future<void>.delayed(const Duration(milliseconds: 650));
      }
    }
    try {
      await d.requestMtu(517);
    } catch (_) {}
    final services = await d.discoverServices();
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
      throw StateError('BLE command characteristic not found');
    }
    _cmdChar = cmdChar;
    _ackChar = ackChar;
    if (netChar != null) {
      _networkChar = netChar;
    }
  }

  Future<void> _writeCmd(Map<String, dynamic> cmd) async {
    if (_cmdChar == null) throw StateError('Not connected or missing cmdChar');
    final jsonString = jsonEncode(cmd);
    final bytes = utf8.encode(jsonString);
    Object? lastError;
    final maxAttempts = defaultTargetPlatform == TargetPlatform.iOS ? 8 : 4;
    for (var attempt = 0; attempt < maxAttempts; attempt++) {
      try {
        await _cmdChar!.write(bytes, withoutResponse: false);
        if (attempt > 0 && AppEnvironment.staging) {
          debugPrint('[staging] BLE cmd OK po retry (pokus ${attempt + 1})');
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
        /* iOS 8/15: SMP často potřebuje delší prodlevu než malý JSON MOVE. */
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

  /// Parita `BLEBoardTransport.postHintHighlightDestinationOnly`.
  Future<void> postHintHighlightDestinationOnly(String toSquare) async {
    await _writeCmd({'cmd': 'hint_highlight', 'to': toSquare.toLowerCase()});
  }

  /// Parita `BLEBoardTransport.postSetupTutorial`.
  Future<void> postSetupTutorial(String action) async {
    await _writeCmd({'cmd': 'setup_tutorial', 'action': action});
  }

  Future<void> postBrightness(int percent) async {
    await _writeCmd({'cmd': 'brightness', 'percent': percent.clamp(0, 100)});
  }

  /// Parita `BLEBoardTransport.postLightCommand` / `POST /api/light/command`.
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

  /// Parita `POST /api/light/game_mode`.
  Future<void> postLightGameMode() async {
    await _writeCmd({'cmd': 'light_game_mode'});
  }

  /// BLE ekvivalent `POST /api/system/ota` — pouze HTTPS URL na `.bin`.
  Future<void> postOtaStart(String httpsFirmwareUrl) async {
    await _writeCmd({'cmd': 'ota_start', 'url': httpsFirmwareUrl.trim()});
  }

  /// Zahájení stream OTA přes BLE (`ota_ble_begin`); po ní chunky s magic `OB`.
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

  /// Stav pozastaveného / aktivního BLE stream OTA na desce (`ota_ble_status`).
  Future<OtaBleStatus?> fetchOtaBleStatus() async {
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
        const Duration(seconds: 8),
        onTimeout: () => null,
      );
    } finally {
      await sub.cancel();
    }
  }

  /// Nahraje celý `.bin` přes GATT CMD — bez Wi‑Fi (chunky `OB` + firmware bytes).
  ///
  /// Při výpadku Bluetooth deska session pozastaví (až 24 h); po znovupřipojení
  /// pokračuje ze stejného chunk indexu. [onPhase]: `paused_waiting_reconnect`, `resumed`.
  Future<void> uploadFirmwareBle(
    File bin, {
    void Function(int pct)? onProgress,
    void Function(String phase)? onPhase,
  }) async {
    final len = await bin.length();
    if (len < 32 * 1024) {
      throw StateError('Firmware file too small');
    }
    await ensureGattReadyForCommands();
    final d = _device;
    if (d == null || _cmdChar == null) {
      throw StateError('BLE cmd characteristic not ready');
    }
    try {
      await d.requestMtu(517);
    } catch (_) {}
    await Future<void>.delayed(const Duration(milliseconds: 60));
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      await Future<void>.delayed(const Duration(milliseconds: 400));
      try {
        await _writeCmd({'cmd': 'ping'});
      } catch (_) {}
      await Future<void>.delayed(const Duration(milliseconds: 280));
    }
    final mtu = d.mtuNow;
    final int payloadMax = defaultTargetPlatform == TargetPlatform.iOS
        ? ((mtu >= 40 ? mtu - 15 : 20).clamp(20, 500).clamp(20, 244)).toInt()
        : ((mtu >= 40 ? mtu - 15 : 20).clamp(20, 500)).toInt();
    final chunkTotal = (len + payloadMax - 1) ~/ payloadMax;
    if (chunkTotal > 65535) {
      throw StateError('Firmware too large for BLE chunk protocol');
    }

    const resumeTimeout = Duration(hours: 24);
    var transferBegun = false;
    final chunkGapMs = defaultTargetPlatform == TargetPlatform.iOS ? 10 : 0;

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
                await wc.write(pkt, withoutResponse: false);
                break;
              } catch (e) {
                final fbp = e is FlutterBluePlusException ? e : null;
                if (fbp?.code == 6 && attempt < 11) {
                  try {
                    await ensureGattReadyForCommands();
                  } catch (_) {}
                  await Future<void>.delayed(
                    Duration(milliseconds: 200 + attempt * 80),
                  );
                  continue;
                }
                final slowSmp =
                    fbp != null && (fbp.code == 8 || fbp.code == 15);
                if (!_isRetryableGattWrite(e) || attempt == 11) rethrow;
                await Future<void>.delayed(
                  Duration(
                    milliseconds: (slowSmp ? 240 : 40) + attempt * 90,
                  ),
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
              'BLE OTA: deska nepozastavila session (vypršení 24 h nebo abort).',
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

  /// Parita `POST /api/settings/lamp` (`auto_lamp_timeout_sec`).
  Future<void> postAutoLampTimeout(int seconds) async {
    final v = seconds.clamp(5, 7200);
    await _writeCmd({'cmd': 'settings_auto_lamp_timeout', 'seconds': v});
  }

  /// Uloží STA SSID/heslo do NVS na desce a spojí Wi‑Fi (`wifi_ble_prov` task).
  Future<void> postWifiStaConfig(String ssid, String password) async {
    await _writeCmd({
      'cmd': 'wifi_sta_config',
      'ssid': ssid.trim(),
      'password': password,
    });
  }

  /// BLE ekvivalent iOS `fetchNetworkInfo` (sta_ip/ap_ip/online).
  /// Jednorázový READ snapshotu (firmware `BLE_GATT_ACCESS_OP_READ_CHR` na snap UUID).
  Future<GameSnapshot> readSnapshot() async {
    final c = _snapChar;
    final d = _device;
    if (c == null || d == null || !d.isConnected) {
      throw StateError('BLE snapshot read: not connected');
    }
    final raw = await c.read();
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

/// Odpověď na `ota_ble_status` (cmd_ack JSON z desky).
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
  });

  final String? staIp;
  final String? apIp;
  /// SSID sítě, na kterou je deska připojená jako STA (z firmware JSON).
  final String? staSsid;
  final bool staConnected;
  final bool online;
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

  /// Vrátí kompletní JSON bajty nebo `null` (čekání na další chunk).
  List<int>? push(Uint8List data) {
    if (data.length < 4 || data[0] != 0x43 || data[1] != 0x4d) {
      return data.toList();
    }
    final part = data[2];
    final total = data[3];
    final payload = data.sublist(4);
    if (part < 1 || total < 1 || part > total) {
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
