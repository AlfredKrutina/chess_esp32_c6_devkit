import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

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

bool _isRetryableGattWrite(Object e) {
  if (e is FlutterBluePlusException) {
    if (e.code == 14) return true;
    final d = e.description?.toUpperCase() ?? '';
    return d.contains('UNLIKELY') || d.contains('BUSY');
  }
  return false;
}

/// BLE snapshot notify + chunk skládání (`appendChunk` ve Swift).
class BleCzechmateClient {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _snapChar;
  BluetoothCharacteristic? _cmdChar;
  BluetoothCharacteristic? _networkChar;
  StreamSubscription<List<int>>? _valueSub;
  StreamSubscription<List<int>>? _ackSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  final _assembler = _BleChunkAssembler();

  BluetoothDevice? get device => _device;

  Future<void> disconnect() async {
    await _valueSub?.cancel();
    await _ackSub?.cancel();
    await _connSub?.cancel();
    _valueSub = null;
    _ackSub = null;
    _connSub = null;
    _snapChar = null;
    _cmdChar = null;
    _networkChar = null;
    if (_device != null && _device!.isConnected) {
      await _device!.disconnect();
    }
    _device = null;
    _assembler.reset();
  }

  Future<void> connect(
    BluetoothDevice device, {
    required void Function(GameSnapshot snap) onSnapshot,
    required void Function(Object error) onError,
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
  }

  Future<void> _writeCmd(Map<String, dynamic> cmd) async {
    if (_cmdChar == null) throw StateError('Not connected or missing cmdChar');
    final jsonString = jsonEncode(cmd);
    final bytes = utf8.encode(jsonString);
    Object? lastError;
    for (var attempt = 0; attempt < 4; attempt++) {
      try {
        await _cmdChar!.write(bytes, withoutResponse: false);
        if (attempt > 0 && AppEnvironment.staging) {
          debugPrint('[staging] BLE cmd OK po retry (pokus ${attempt + 1})');
        }
        return;
      } catch (e) {
        lastError = e;
        if (!_isRetryableGattWrite(e) || attempt == 3) rethrow;
        await Future<void>.delayed(Duration(milliseconds: 60 + attempt * 100));
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

  /// Parita `POST /api/settings/lamp` (`auto_lamp_timeout_sec`).
  Future<void> postAutoLampTimeout(int seconds) async {
    final v = seconds.clamp(5, 7200);
    await _writeCmd({'cmd': 'settings_auto_lamp_timeout', 'seconds': v});
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
      if (raw.isEmpty) return null;
      final parsed = jsonDecode(utf8.decode(raw));
      if (parsed is! Map<String, dynamic>) return null;
      final staIp = (parsed['sta_ip'] as String?)?.trim();
      final apIp = (parsed['ap_ip'] as String?)?.trim();
      final online = parsed['online'] == true;
      return BleNetworkInfo(
        staIp: (staIp == null || staIp.isEmpty) ? null : staIp,
        apIp: (apIp == null || apIp.isEmpty) ? null : apIp,
        online: online,
      );
    } catch (_) {
      return null;
    }
  }
}

class BleNetworkInfo {
  const BleNetworkInfo({
    required this.staIp,
    required this.apIp,
    required this.online,
  });

  final String? staIp;
  final String? apIp;
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
