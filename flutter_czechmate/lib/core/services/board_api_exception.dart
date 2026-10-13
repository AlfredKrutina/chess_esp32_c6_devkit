class BoardApiException implements Exception {
  BoardApiException(this.message, {this.statusCode, this.detail});

  final String message;
  final int? statusCode;
  final String? detail;

  factory BoardApiException.webLocked() =>
      BoardApiException('Web locked', statusCode: 403);

  factory BoardApiException.apiTokenRequired() => BoardApiException(
        'Board API token required',
        statusCode: 403,
        detail: 'api_token_required',
      );

  factory BoardApiException.boardBusy() => BoardApiException(
        'Board is connected to another device',
        statusCode: 403,
        detail: 'board_busy',
      );

  factory BoardApiException.notInPairingMode() => BoardApiException(
        'Board is not in pairing mode',
        statusCode: 403,
        detail: 'not_pairing',
      );

  bool get isWebLocked => statusCode == 403 && message == 'Web locked';

  bool get isApiTokenRequired =>
      statusCode == 403 && detail == 'api_token_required';

  bool get isBoardBusy => statusCode == 403 && detail == 'board_busy';

  bool get isNotInPairingMode => statusCode == 403 && detail == 'not_pairing';

  @override
  String toString() =>
      'BoardApiException($statusCode): $message${detail != null ? ' — $detail' : ''}';
}
