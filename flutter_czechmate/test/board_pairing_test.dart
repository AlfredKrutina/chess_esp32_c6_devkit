import 'dart:convert';

import 'package:czechmate/core/protocol/board_pairing.dart';
import 'package:czechmate/core/services/board_api_client.dart';
import 'package:czechmate/core/services/board_api_exception.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';

void main() {
  final token = 'ab' * 32;

  test('advertisement status decides pairing, busy, and a known reconnect', () {
    expect(
      boardAirStatusFromManufacturer({
        czechmateMfgCompanyId: [czechmateMfgMagic, 1],
      }),
      BoardAirStatus.pairing,
    );
    expect(
      boardAirStatusFromManufacturer({
        czechmateMfgCompanyId: [czechmateMfgMagic, 2],
      }),
      BoardAirStatus.busy,
    );
    expect(
      boardAirStatusFromManufacturer({
        czechmateMfgCompanyId: [czechmateMfgMagic, 0],
      }),
      BoardAirStatus.idle,
    );
    expect(boardAirStatusFromManufacturer({}), BoardAirStatus.unknown);

    expect(
      decideBoardJoin(knownDevice: false, air: BoardAirStatus.idle),
      BoardJoin.notInPairingMode,
    );
    expect(
      decideBoardJoin(knownDevice: false, air: BoardAirStatus.pairing),
      BoardJoin.pairNew,
    );
    expect(
      decideBoardJoin(knownDevice: true, air: BoardAirStatus.idle),
      BoardJoin.reconnectKnown,
    );
    expect(
      decideBoardJoin(knownDevice: true, air: BoardAirStatus.busy),
      BoardJoin.occupied,
    );
    expect(
      decideBoardJoin(knownDevice: false, air: BoardAirStatus.unknown),
      BoardJoin.tryConnect,
    );
  });

  test('cmd_ack paired notify is the command token', () {
    final raw = utf8.encode(
      '{"channel":"cmd_ack","ok":true,"code":"paired","api_token":"$token"}',
    );
    expect(commandTokenFromAck(raw), token);
    expect(
      commandTokenFromAck(utf8.encode('{"code":"ok","api_token":"$token"}')),
      isNull,
    );
  });

  test('a move sends the pairing token and a busy board stays a busy board', () async {
    http.Request? seen;
    final api = BoardApiClient(
      resolveBoardApiBearerToken: () => 'ff' * 32,
      resolveBoardCommandToken: () => token,
      httpClient: MockClient((request) async {
        seen = request;
        if (request.url.path == '/api/pair') {
          return http.Response(
            '{"success":false,"error":"not_pairing"}',
            403,
          );
        }
        return http.Response(
          '{"success":false,"error":"board_busy"}',
          403,
        );
      }),
    );
    await expectLater(
      api.postGameMove('http://192.168.4.1', from: 'e2', to: 'e4'),
      throwsA(isA<BoardApiException>().having((e) => e.isBoardBusy, 'busy', isTrue)),
    );
    expect(seen!.headers['Authorization'], 'Bearer $token');

    await expectLater(
      api.postPair('http://192.168.4.1'),
      throwsA(
        isA<BoardApiException>().having(
          (e) => e.isNotInPairingMode,
          'window',
          isTrue,
        ),
      ),
    );
  });
}
