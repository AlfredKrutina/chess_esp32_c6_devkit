import '../../core/services/board_api_exception.dart';
import '../connection/board_session_state.dart';

/// Explains HTTP failures without implying “board disconnected” when BLE still works.
String boardHttpSettingsUserMessage(Object e, BoardSessionState session) {
  final s = e.toString();
  if (s.contains('No host specified') ||
      (s.contains('Invalid argument') && s.contains('URI'))) {
    return 'The board HTTP address is missing or invalid. In Settings, save a full URL '
        '(e.g. http://192.168.4.1 or your board’s STA IP). Bluetooth can still work '
        'without this URL for live play.';
  }
  if (e is BoardApiException) {
    final link = switch (session.transport) {
      BoardTransport.ble =>
        'Bluetooth is connected, but the HTTP request failed (wrong URL, timeout, or web lock).',
      BoardTransport.wifi =>
        'Wi‑Fi session active — verify the board URL and that the board responds.',
      BoardTransport.mock => 'Demo board — HTTP to hardware does not apply.',
      BoardTransport.none => 'No active connection to the board.',
    };
    return '$link Detail: $e';
  }
  return 'Board communication error: $e';
}
