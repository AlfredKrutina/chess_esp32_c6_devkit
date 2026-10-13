import 'dart:convert';

/// Manufacturer id in the board scan response (Espressif 0x02E5).
const czechmateMfgCompanyId = 0x02E5;

/// First payload byte so we ignore other Espressif advertisements.
const czechmateMfgMagic = 0xC2;

/// What the board is advertising. [unknown] means the scan response had no status byte.
enum BoardAirStatus { unknown, idle, pairing, busy }

/// How discovery should treat a tap on a CZECHMATE advertisement.
enum BoardJoin {
  reconnectKnown,
  pairNew,
  notInPairingMode,
  occupied,
  tryConnect,
}

BoardAirStatus boardAirStatusFromManufacturer(Map<int, List<int>> data) {
  final payload = data[czechmateMfgCompanyId];
  if (payload == null || payload.length < 2 || payload[0] != czechmateMfgMagic) {
    return BoardAirStatus.unknown;
  }
  final status = payload[1];
  if ((status & 2) != 0) return BoardAirStatus.busy;
  if ((status & 1) != 0) return BoardAirStatus.pairing;
  return BoardAirStatus.idle;
}

BoardJoin decideBoardJoin({
  required bool knownDevice,
  required BoardAirStatus air,
}) {
  if (air == BoardAirStatus.busy) return BoardJoin.occupied;
  if (knownDevice) return BoardJoin.reconnectKnown;
  if (air == BoardAirStatus.pairing) return BoardJoin.pairNew;
  if (air == BoardAirStatus.idle) return BoardJoin.notInPairingMode;
  return BoardJoin.tryConnect;
}

/// cmd_ack notify after a new bond: `{"code":"paired","api_token":"<64 hex>"}`.
String? commandTokenFromAck(List<int> raw) {
  try {
    final parsed = jsonDecode(utf8.decode(raw));
    if (parsed is! Map) return null;
    if (parsed['code'] != 'paired') return null;
    final tok = parsed['api_token'];
    if (tok is! String) return null;
    final text = tok.trim();
    if (!RegExp(r'^[0-9a-fA-F]{64}$').hasMatch(text)) return null;
    return text;
  } catch (_) {
    return null;
  }
}
