import 'dart:convert';

/// The board refused a command. [code] is `busy`, `stale`, or another ack code.
class BoardCommandRejected implements Exception {
  BoardCommandRejected(this.code);

  final String code;

  @override
  String toString() => 'BoardCommandRejected($code)';
}

/// GATT write returned, but no `cmd_ack` with the same id arrived.
class BoardCommandAckTimeout implements Exception {
  BoardCommandAckTimeout(this.id);

  final int id;

  @override
  String toString() => 'BoardCommandAckTimeout($id)';
}

/// Matches a `cmd_ack` to the command that asked for it.
class BoardCommandAck {
  const BoardCommandAck._();

  static int? idOf(Map<String, dynamic> json) {
    final value = json['id'];
    if (value is num && value != 0) return value.toInt();
    return null;
  }

  /// False for a missing id and for an ack that belongs to another command.
  static bool matches(Map<String, dynamic> json, int id) => idOf(json) == id;

  static Map<String, dynamic>? tryParse(List<int> raw) {
    if (raw.length < 2) return null;
    try {
      final parsed = jsonDecode(utf8.decode(raw, allowMalformed: true));
      if (parsed is Map<String, dynamic>) return parsed;
      if (parsed is Map) return Map<String, dynamic>.from(parsed);
    } catch (_) {}
    return null;
  }

  /// When the body is JSON and carries `id`, it must equal [expected].
  /// Plain text and older firmware without `id` are accepted.
  static void expectResponseId(List<int> body, int expected) {
    if (body.isEmpty) return;
    Map<String, dynamic>? map;
    try {
      final parsed = jsonDecode(utf8.decode(body, allowMalformed: true));
      if (parsed is Map<String, dynamic>) {
        map = parsed;
      } else if (parsed is Map) {
        map = Map<String, dynamic>.from(parsed);
      }
    } catch (_) {
      return;
    }
    if (map == null) return;
    final got = idOf(map);
    if (got != null && got != expected) {
      throw BoardCommandRejected('ack_mismatch');
    }
  }
}
