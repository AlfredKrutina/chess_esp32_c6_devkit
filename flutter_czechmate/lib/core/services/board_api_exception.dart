class BoardApiException implements Exception {
  BoardApiException(this.message, {this.statusCode, this.detail});

  final String message;
  final int? statusCode;
  final String? detail;

  factory BoardApiException.webLocked() =>
      BoardApiException('Web locked', statusCode: 403);

  bool get isWebLocked => statusCode == 403 && message == 'Web locked';

  @override
  String toString() =>
      'BoardApiException($statusCode): $message${detail != null ? ' — $detail' : ''}';
}
