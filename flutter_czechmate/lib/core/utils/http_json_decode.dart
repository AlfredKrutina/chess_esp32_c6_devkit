import 'dart:convert';

import 'package:archive/archive.dart';

import '../services/board_api_exception.dart';

List<int> _stripUtf8Bom(List<int> bytes) {
  if (bytes.length >= 3 &&
      bytes[0] == 0xEF &&
      bytes[1] == 0xBB &&
      bytes[2] == 0xBF) {
    return bytes.sublist(3);
  }
  return bytes;
}

/// Some servers return gzip even when clients expect plain JSON.
List<int> gunzipHttpBodyIfNeeded(List<int> bytes) {
  if (bytes.length >= 2 && bytes[0] == 0x1F && bytes[1] == 0x8B) {
    return const GZipDecoder().decodeBytes(bytes);
  }
  return bytes;
}

/// JSON object from HTTP body: optional gzip, UTF-8, optional BOM.
Map<String, dynamic> decodeHttpJsonMap(List<int> bodyBytes) {
  try {
    final inflated = gunzipHttpBodyIfNeeded(bodyBytes);
    final text = utf8.decode(_stripUtf8Bom(inflated));
    final decoded = jsonDecode(text);
    if (decoded is! Map<String, dynamic>) {
      throw BoardApiException(
        'Invalid JSON response',
        statusCode: 0,
        detail: 'Expected a JSON object',
      );
    }
    return decoded;
  } on BoardApiException {
    rethrow;
  } on FormatException catch (e) {
    throw BoardApiException(
      'Invalid JSON response',
      statusCode: 0,
      detail: e.message,
    );
  }
}

/// Safe preview for logs / errors (never throws).
String previewHttpBodyUtf8(List<int> bodyBytes, {int maxChars = 200}) {
  try {
    final inflated = gunzipHttpBodyIfNeeded(bodyBytes);
    final s = utf8.decode(_stripUtf8Bom(inflated));
    if (s.length <= maxChars) return s;
    return '${s.substring(0, maxChars)}…';
  } catch (_) {
    return '(non-text or binary body, ${bodyBytes.length} bytes)';
  }
}
