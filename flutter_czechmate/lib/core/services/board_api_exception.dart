class BoardApiException implements Exception {
  BoardApiException(this.message, {this.statusCode, this.detail});

  final String message;
  final int? statusCode;
  final String? detail;

  factory BoardApiException.webLocked() =>
      BoardApiException('Web locked', statusCode: 403);

  @override
  String toString() =>
      'BoardApiException($statusCode): $message${detail != null ? ' — $detail' : ''}';
}
