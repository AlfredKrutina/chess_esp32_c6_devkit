import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../../l10n/app_localizations.dart';
import '../services/board_api_exception.dart';

/// Maps exceptions and low-level errors to short, localized copy for UI (no stack traces).
String userFacingErrorSummary(AppLocalizations l, Object? error) {
  if (error == null) return '';
  if (error is StateError) return error.message;

  if (error is BoardApiException) {
    if (error.isWebLocked) return l.userFacingBoardWebLocked;
    if (error.isApiTokenRequired) return l.userFacingBoardApiToken;
    switch (error.statusCode) {
      case 400:
        return error.detail != null && error.detail!.isNotEmpty
            ? '${l.userFacingBoardBadRequest} (${error.detail})'
            : l.userFacingBoardBadRequest;
      case 401:
        return l.userFacingBoardUnauthorized;
      case 404:
        return l.userFacingBoardNotFound;
      case 500:
        return l.userFacingBoardServerError;
      case 503:
        return l.userFacingBoardUnavailable;
      case 304:
        return '';
      default:
        if (error.statusCode != null) {
          final d = error.detail;
          if (d != null && d.isNotEmpty) {
            return '${l.userFacingBoardHttpOther} (${error.statusCode}: $d)';
          }
          return '${l.userFacingBoardHttpOther} (${error.statusCode})';
        }
        return error.message;
    }
  }

  if (error is FlutterBluePlusException) {
    final desc = (error.description ?? '').toLowerCase();
    // iOS CoreBluetooth / stack: bond invalidated (firmware reset, cleared NVS, …).
    if (desc.contains('removed pairing') ||
        desc.contains('pairing information')) {
      return l.userFacingErrBlePairingReset;
    }
    return l.userFacingErrBle;
  }

  final s = error.toString();

  if (s.contains('FlutterBluePlusException') ||
      s.contains('flutter_blue_plus') ||
      s.contains('Bluetooth')) {
    return l.userFacingErrBle;
  }
  if (s.contains('WebSocketChannelException') ||
      s.contains('WebSocket') ||
      s.contains('websocket')) {
    return l.userFacingErrWebSocket;
  }
  if (s.contains('SocketException') ||
      s.contains('Failed host lookup') ||
      s.contains('Connection refused') ||
      s.contains('Network is unreachable') ||
      s.contains('ClientException')) {
    return l.userFacingErrNetworkReach;
  }
  if (s.contains('TimeoutException') || s.contains('timed out')) {
    return l.userFacingErrTimeout;
  }
  if (s.contains('HandshakeException')) {
    return l.userFacingErrTls;
  }
  if (s.contains('HttpException')) {
    return l.userFacingErrNetworkReach;
  }
  if (s.contains('No host specified') ||
      (s.contains('Invalid argument') && s.contains('URI'))) {
    return l.userFacingErrBoardUrlHostMissing;
  }
  if (s.contains('CERTIFICATE_VERIFY_FAILED') ||
      s.contains('CertificateException')) {
    return l.userFacingErrTls;
  }
  if (s.contains('PlatformException') &&
      (s.contains('bluetooth') || s.contains('Bluetooth'))) {
    return l.userFacingErrBle;
  }

  return l.userFacingErrGeneric;
}
