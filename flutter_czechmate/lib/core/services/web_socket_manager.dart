import 'dart:async';
import 'dart:typed_data';

import 'package:web_socket_channel/web_socket_channel.dart';

import '../models/game_snapshot.dart';
import '../utils/game_snapshot_codec.dart';

/// `ws://host` / `wss://host` — `ChessboardWebSocketClient.swift` (JSON = snapshot).
class SnapshotWebSocketClient {
  WebSocketChannel? _channel;
  StreamSubscription<dynamic>? _sub;

  static Uri websocketUriFromBase(String baseHttpUrl) {
    final u = Uri.parse(baseHttpUrl.trim());
    final scheme = u.scheme.toLowerCase() == 'https' ? 'wss' : 'ws';
    return Uri(
      scheme: scheme,
      host: u.host,
      port: u.hasPort ? u.port : null,
      path: '/ws',
    );
  }

  bool get isConnected => _channel != null;

  void connect(
    String baseHttpUrl, {
    required void Function(GameSnapshot snap) onSnapshot,
    required void Function(Object error) onError,
    required void Function() onDisconnect,
    void Function()? onMessageReceived,
  }) {
    disconnect();
    final uri = websocketUriFromBase(baseHttpUrl);
    final ch = WebSocketChannel.connect(uri);
    _channel = ch;
    _sub = ch.stream.listen(
      (dynamic data) {
        onMessageReceived?.call();
        try {
          if (data is String) {
            onSnapshot(GameSnapshotCodec.decodeRepairingAndNormalizing(data));
          } else if (data is List<int>) {
            onSnapshot(GameSnapshotCodec.decodeBytes(data));
          } else if (data is Uint8List) {
            onSnapshot(GameSnapshotCodec.decodeBytes(data));
          }
        } catch (e) {
          onError(e);
        }
      },
      onError: (Object e, _) {
        onError(e);
        onDisconnect();
      },
      onDone: onDisconnect,
      cancelOnError: true,
    );
  }

  void disconnect() {
    _sub?.cancel();
    _sub = null;
    _channel?.sink.close();
    _channel = null;
  }
}
