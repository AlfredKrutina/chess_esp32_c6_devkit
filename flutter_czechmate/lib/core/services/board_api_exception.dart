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

  bool get isWebLocked => statusCode == 403 && message == 'Web locked';

  bool get isApiTokenRequired =>
      statusCode == 403 && detail == 'api_token_required';

  @override
  String toString() =>
      'BoardApiException($statusCode): $message${detail != null ? ' — $detail' : ''}';
}
