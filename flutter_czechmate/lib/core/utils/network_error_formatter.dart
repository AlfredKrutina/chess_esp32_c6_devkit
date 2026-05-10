import '../services/board_api_exception.dart';

/// English-only HTTP / network summaries (legacy helpers, logs, or tests).
///
/// For Flutter UI copy, prefer [userFacingErrorSummary] in
/// `user_facing_error_message.dart` so CS/EN stay aligned with ARB.
class NetworkErrorFormatter {
  static String format(Object error) {
    if (error is BoardApiException) return _formatApiException(error);
    final s = error.toString();
    if (s.contains('SocketException') ||
        s.contains('Failed host lookup') ||
        s.contains('Connection refused')) {
      return 'Cannot reach the server. Check internet (for cloud) or that this device '
          'and the board are on the same LAN. If you are only on the board’s Wi‑Fi hotspot '
          'without internet, turn Wi‑Fi off or use another connection with internet for cloud features.';
    }
    if (s.contains('TimeoutException') || s.contains('timed out')) {
      return 'The board did not respond in time. Move closer, check power, and try again.';
    }
    if (s.contains('HandshakeException')) {
      return 'Secure connection (TLS) failed. Check that the URL uses https:// if required.';
    }
    if (s.contains('No host specified') ||
        (s.contains('Invalid argument') && s.contains('URI'))) {
      return 'Board URL is missing a host. In Settings, save a full address such as '
          'http://192.168.4.1 (same network as the board).';
    }
    return 'Something went wrong: $s';
  }

  static String _formatApiException(BoardApiException e) {
    if (e.isWebLocked) {
      return 'The board web UI has locked HTTP/API access (403). '
          'Close the board’s web dashboard or unlock API access, then retry.';
    }
    if (e.isApiTokenRequired) {
      return 'The board requires an API token for this action (403). '
          'In Settings → Saved defaults, paste the 64‑character hex token from '
          'UART command API_TOKEN, then save.';
    }
    switch (e.statusCode) {
      case 400:
        return 'Invalid request${e.detail != null ? ': ${e.detail}' : ''}. '
            'Check move legality or JSON fields.';
      case 401:
        return 'Access denied (401). Check credentials if the board requires auth.';
      case 404:
        return 'Endpoint not found (404). Your firmware may be older than this app expects.';
      case 500:
        return 'Board reported an internal error (500). Try again or reboot the board.';
      case 503:
        return 'Board is busy or unavailable (503). Wait a moment and retry.';
      case 304:
        return '';
      default:
        if (e.statusCode != null) {
          return 'HTTP ${e.statusCode}${e.detail != null ? ': ${e.detail}' : ''}.';
        }
        return e.message;
    }
  }
}
