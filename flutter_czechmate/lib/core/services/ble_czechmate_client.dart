import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/game_snapshot.dart';
import '../utils/game_snapshot_codec.dart';

/// GATT UUID z `CZECHMATEBLEUUIDs.swift` / `ble_task.c`.
final Guid czechmateServiceGuid = Guid('A0B40001-9267-4AB6-BDCC-E8336F8A8D9E');
final Guid snapshotCharacteristicGuid = Guid('A0B40002-9267-4AB6-BDCC-E8336F8A8D9E');

bool _uuidMatch(Guid a, Guid b) =>
    a.toString().toLowerCase() == b.toString().toLowerCase();

/// BLE snapshot notify + chunk skládání (`appendChunk` ve Swift).
class BleCzechmateClient {
  BluetoothDevice? _device;
  StreamSubscription<List<int>>? _valueSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  final _assembler = _BleChunkAssembler();

  BluetoothDevice? get device => _device;

  Future<void> disconnect() async {
    await _valueSub?.cancel();
    await _connSub?.cancel();
    _valueSub = null;
    _connSub = null;
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
        onError(StateError('BLE odpojeno'));
      }
    });
    final services = await device.discoverServices();
    BluetoothCharacteristic? snapChar;
    for (final svc in services) {
      if (!_uuidMatch(svc.uuid, czechmateServiceGuid)) continue;
      for (final c in svc.characteristics) {
        if (_uuidMatch(c.uuid, snapshotCharacteristicGuid)) {
          snapChar = c;
          break;
        }
      }
    }
    if (snapChar == null) {
      await disconnect();
      throw StateError('Chybí GATT snapshot charakteristika');
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
  }
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
