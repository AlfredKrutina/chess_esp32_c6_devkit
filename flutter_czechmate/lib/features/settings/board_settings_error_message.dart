import '../../core/services/board_api_exception.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../l10n/app_localizations.dart';
import '../connection/board_session_state.dart';

/// Explains HTTP failures without implying “board disconnected” when BLE still works.
String boardHttpSettingsUserMessage(
  AppLocalizations l,
  Object e,
  BoardSessionState session,
) {
  final s = e.toString();
  if (s.contains('No host specified') ||
      (s.contains('Invalid argument') && s.contains('URI'))) {
    return l.boardHttpMissingUrlBody;
  }
  if (e is BoardApiException) {
    final link = switch (session.transport) {
      BoardTransport.ble => l.boardHttpFailBle,
      BoardTransport.wifi => l.boardHttpFailWifi,
      BoardTransport.mock => l.boardHttpFailMock,
      BoardTransport.none => l.boardHttpFailNone,
    };
    final detail = userFacingErrorSummary(l, e);
    return detail.isEmpty ? link : l.boardHttpFailDetail(link, detail);
  }
  return l.boardHttpErrGeneric(userFacingErrorSummary(l, e));
}
