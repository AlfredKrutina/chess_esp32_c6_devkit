import 'dart:convert';

import 'package:czechmate/core/protocol/board_command_ack.dart';
import 'package:czechmate/core/protocol/board_command_payload.dart';
import 'package:czechmate/core/protocol/saved_board_reconnect.dart';
import 'package:czechmate/core/services/board_api_client.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';

void main() {
  test('HTTP and BLE promotion letters match and are lowercase', () {
    for (final raw in ['Q', 'q', 'R', 'n', 'BISHOP', 'knight']) {
      final http = BoardCommandPayload.httpMove(
        from: 'E7',
        to: 'E8',
        promotion: raw,
      );
      final ble = BoardCommandPayload.bleMove(
        from: 'e7',
        to: 'e8',
        promotion: raw,
      );
      expect(http['promotion'], ble['promotion']);
      expect(http['promotion'], isIn(['q', 'r', 'b', 'n']));
      expect(http['from'], 'e7');
      expect(http['to'], 'e8');
      expect(ble['cmd'], 'move');
    }
    expect(
      BoardCommandPayload.httpMove(from: 'a2', to: 'a4')['promotion'],
      isNull,
    );
    expect(BoardCommandPayload.blePromotion('Q')['choice'], 'q');
    expect(
      BoardCommandPayload.httpPromote(choice: 'N', square: 'E8')['choice'],
      'n',
    );
  });

  test('BoardApiClient posts the shared lowercase promotion', () async {
    http.Request? seen;
    final api = BoardApiClient(
      httpClient: MockClient((request) async {
        seen = request;
        return http.Response('{"success":true}', 200);
      }),
    );
    await api.postGameMove(
      'http://192.168.4.1',
      from: 'E7',
      to: 'E8',
      promotion: 'Q',
    );
    expect(seen, isNotNull);
    expect(seen!.url.path, '/api/move');
    final body = jsonDecode(seen!.body) as Map<String, dynamic>;
    expect(body['from'], 'e7');
    expect(body['to'], 'e8');
    expect(body['promotion'], 'q');
    expect(
      BoardCommandPayload.bleMove(from: 'E7', to: 'E8', promotion: 'Q')['promotion'],
      body['promotion'],
    );
  });

  test('timer custom pair and lamp timeout use firmware keys', () {
    expect(
      BoardCommandPayload.timerConfig(
        type: 14,
        customMinutes: 10,
        customIncrement: 5,
      ),
      {'type': 14, 'custom_minutes': 10, 'custom_increment': 5},
    );
    expect(
      BoardCommandPayload.bleTimerConfig(type: 6)['cmd'],
      'timer_config',
    );
    expect(BoardCommandPayload.httpBrightness(40)['percent'], 40);
    expect(BoardCommandPayload.bleBrightness(40)['percent'], 40);
    expect(BoardCommandPayload.autoLampTimeout(3)['seconds'], 5);
    expect(
      BoardCommandPayload.bleAutoLampTimeout(3)['cmd'],
      'settings_auto_lamp_timeout',
    );
    expect(BoardCommandPayload.ledGuidanceLevel(9)['level'], 5);
    expect(BoardCommandPayload.ledGuidanceLevel(0).containsKey('led_guidance_level'), isFalse);
    expect(BoardCommandPayload.guidedCaptureHints(true), {'enabled': true});
    expect(
      BoardCommandPayload.bleLedGuidanceLevel(3)['cmd'],
      'settings_led_guidance',
    );
    expect(
      BoardCommandPayload.bleGuidedCaptureHints(false)['cmd'],
      'settings_guided_hints',
    );
  });

  test('saved board reconnects without user input', () {
    final phone = planSavedBoardReconnect(
      bleHostSupported: true,
      lanPathAvailable: true,
      mode: 'auto',
      identity: const SavedBoardIdentity(
        bleRemoteId: 'AA:BB:CC:DD:EE:FF',
        httpBaseUrl: 'http://192.168.1.40',
        mdnsHost: 'czechmate-aabbcc',
        lastLinkKind: 'bluetooth',
      ),
      urlLooksLikePrivateLan: (url) => url.contains('192.168.'),
    );
    expect(phone.steps.map((s) => s.kind).toList(), [
      SavedReconnectKind.ble,
      SavedReconnectKind.wifi,
      SavedReconnectKind.wifi,
    ]);
    expect(phone.steps[1].url, 'http://192.168.1.40');
    expect(phone.steps[2].url, 'http://czechmate-aabbcc.local');
  });

  test('Windows stays on Wi-Fi and does not invent BLE', () {
    final desktop = planSavedBoardReconnect(
      bleHostSupported: false,
      lanPathAvailable: false,
      mode: 'auto',
      identity: const SavedBoardIdentity(
        bleRemoteId: 'AA:BB:CC:DD:EE:FF',
        httpBaseUrl: 'http://192.168.1.40',
        mdnsHost: 'CzechMate-AABBCC.local',
        lastLinkKind: 'wifi',
      ),
      urlLooksLikePrivateLan: (url) => url.contains('192.168.'),
    );
    expect(desktop.steps.every((s) => s.kind == SavedReconnectKind.wifi), isTrue);
    expect(desktop.steps.map((s) => s.url).toList(), [
      'http://192.168.1.40',
      'http://czechmate-aabbcc.local',
    ]);
  });

  test('phone on cellular does not time out on a private LAN URL', () {
    final cellular = planSavedBoardReconnect(
      bleHostSupported: true,
      lanPathAvailable: false,
      mode: 'auto',
      identity: const SavedBoardIdentity(
        bleRemoteId: 'saved',
        httpBaseUrl: 'http://10.0.0.8',
        mdnsHost: 'czechmate-010203',
        lastLinkKind: 'bluetooth',
      ),
      urlLooksLikePrivateLan: (url) => url.contains('10.0.0.'),
    );
    expect(cellular.steps, hasLength(1));
    expect(cellular.steps.single.kind, SavedReconnectKind.ble);
  });

  test('no saved identity means the user still has to pair', () {
    final plan = planSavedBoardReconnect(
      bleHostSupported: true,
      lanPathAvailable: true,
      mode: 'auto',
      identity: const SavedBoardIdentity(),
      urlLooksLikePrivateLan: (_) => false,
    );
    expect(plan.isEmpty, isTrue);
  });

  test('HTTP and BLE share one command id and brightness key', () {
    final http = BoardCommandPayload.stamp(
      BoardCommandPayload.httpMove(from: 'e2', to: 'e4'),
      id: 7,
      epoch: 2,
    );
    final ble = BoardCommandPayload.stamp(
      BoardCommandPayload.bleMove(from: 'e2', to: 'e4'),
      id: 7,
      epoch: 2,
    );
    expect(http['id'], 7);
    expect(http['req'], ble['req']);
    expect(ble['id'], 7);
    expect(http['epoch'], ble['epoch']);
    expect(BoardCommandPayload.httpBrightness(40)['percent'], 40);
    expect(BoardCommandPayload.bleBrightness(40)['percent'], 40);
    final puzzle = BoardCommandPayload.stamp(
      BoardCommandPayload.puzzle(action: 'start', id: 3),
      id: 9,
    );
    expect(puzzle['id'], 3);
    expect(puzzle['req'], 9);
  });

  test('ack matcher drops a foreign id', () {
    expect(
      BoardCommandAck.matches(<String, dynamic>{'id': 4, 'ok': true}, 4),
      isTrue,
    );
    expect(
      BoardCommandAck.matches(<String, dynamic>{'id': 9, 'ok': true}, 4),
      isFalse,
    );
    expect(BoardCommandAck.matches(<String, dynamic>{'ok': true}, 4), isFalse);
    expect(
      BoardCommandAck.matches(<String, dynamic>{'id': 0, 'ok': true}, 4),
      isFalse,
    );
  });

  test('mdns host is only the firmware form', () {
    expect(normalizeMdnsHost('czechmate-aabbcc'), 'czechmate-aabbcc');
    expect(normalizeMdnsHost('CzechMate-AABBCC.local'), 'czechmate-aabbcc');
    expect(normalizeMdnsHost('not-a-board'), isNull);
    expect(normalizeMdnsHost(null), isNull);
  });
}
