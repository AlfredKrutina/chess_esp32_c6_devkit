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

/// After skipping ASCII whitespace, body looks like plain JSON text.
bool _looksLikePlainJson(List<int> bytes) {
  var i = 0;
  while (i < bytes.length) {
    final b = bytes[i];
    if (b == 0x20 || b == 0x09 || b == 0x0A || b == 0x0D) {
      i++;
      continue;
    }
    return b == 0x7B || b == 0x5B; // { [
  }
  return false;
}

/// Decompress when magic matches; plain JSON is left untouched.
List<int> decompressHttpBodyIfNeeded(List<int> bytes) {
  if (bytes.isEmpty) return bytes;
  if (_looksLikePlainJson(bytes)) return bytes;

  if (bytes.length >= 2 && bytes[0] == 0x1F && bytes[1] == 0x8B) {
    try {
      return const GZipDecoder().decodeBytes(bytes);
    } catch (_) {}
  }

  // zlib wrapper (some proxies / stacks use deflate+zlib instead of gzip).
  if (bytes.length >= 2 && bytes[0] == 0x78) {
    final b1 = bytes[1];
    if (b1 == 0x01 || b1 == 0x9C || b1 == 0xDA) {
      try {
        return const ZLibDecoder().decodeBytes(bytes);
      } catch (_) {}
    }
  }

  return bytes;
}

String _utf8TextLenient(List<int> bytes) {
  try {
    return utf8.decode(bytes);
  } on FormatException {
    return utf8.decode(bytes, allowMalformed: true);
  }
}

/// JSON object from HTTP body: optional gzip/zlib, UTF-8 (+ BOM), lenient UTF-8 fallback.
Map<String, dynamic> decodeHttpJsonMap(List<int> bodyBytes) {
  try {
    final inflated = decompressHttpBodyIfNeeded(bodyBytes);
    final text = _utf8TextLenient(_stripUtf8Bom(inflated));
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
    final inflated = decompressHttpBodyIfNeeded(bodyBytes);
    final s = _utf8TextLenient(_stripUtf8Bom(inflated));
    if (s.length <= maxChars) return s;
    return '${s.substring(0, maxChars)}…';
  } catch (_) {
    return '(non-text or binary body, ${bodyBytes.length} bytes)';
  }
}
