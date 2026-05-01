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

/// Skip BOM so `{` is found for plain JSON fast path.
bool _looksLikePlainJson(List<int> bytes) {
  var i = 0;
  if (bytes.length >= 3 &&
      bytes[0] == 0xEF &&
      bytes[1] == 0xBB &&
      bytes[2] == 0xBF) {
    i = 3;
  }
  while (i < bytes.length) {
    final b = bytes[i];
    if (b == 0x20 || b == 0x09 || b == 0x0A || b == 0x0D) {
      i++;
      continue;
    }
    return b == 0x7B || b == 0x5B;
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

String _normalizeJsonText(String raw) {
  var s = raw;
  if (s.isNotEmpty && s.codeUnitAt(0) == 0xFEFF) {
    s = s.substring(1);
  }
  s = s.trim();
  // Common XSSI / anti-sniff prefixes (not on raw GitHub, but proxies/CDNs sometimes add junk).
  if (s.startsWith(")]}',\n")) {
    s = s.substring(")]}',\n".length);
  } else if (s.startsWith(")]}',\r\n")) {
    s = s.substring(")]}',\r\n".length);
  }
  return s.trim();
}

String _shortPreview(String s, [int max = 160]) {
  final t = s.trim();
  if (t.length <= max) return t;
  return '${t.substring(0, max)}…';
}

/// First top-level `{ ... }` balancing quotes and escapes (handles junk before/after JSON).
String? _extractFirstJsonObject(String text) {
  final start = text.indexOf('{');
  if (start < 0) return null;
  var depth = 0;
  var inString = false;
  var escape = false;
  for (var i = start; i < text.length; i++) {
    final c = text[i];
    if (inString) {
      if (escape) {
        escape = false;
      } else if (c == r'\') {
        escape = true;
      } else if (c == '"') {
        inString = false;
      }
      continue;
    }
    if (c == '"') {
      inString = true;
      continue;
    }
    if (c == '{') {
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0) {
        return text.substring(start, i + 1);
      }
    }
  }
  return null;
}

dynamic _jsonDecodeLenient(String text) {
  try {
    return jsonDecode(text);
  } on FormatException catch (_) {
    // Trailing garbage after a valid object (e.g. `\0`, debug text) breaks full-string decode.
    final slice = _extractFirstJsonObject(text);
    if (slice != null) {
      try {
        return jsonDecode(slice);
      } on FormatException catch (_) {}
    }
    rethrow;
  }
}

/// JSON object from HTTP body: optional gzip/zlib, UTF-8 (+ BOM), lenient UTF-8 fallback.
Map<String, dynamic> decodeHttpJsonMap(List<int> bodyBytes) {
  try {
    final inflated = decompressHttpBodyIfNeeded(bodyBytes);
    final textRaw = _utf8TextLenient(_stripUtf8Bom(inflated));
    final text = _normalizeJsonText(textRaw);

    if (text.isEmpty) {
      throw BoardApiException(
        'Invalid JSON response',
        statusCode: 0,
        detail: 'Empty body after decoding',
      );
    }
    final t = text.trimLeft();
    if (t.startsWith('<')) {
      throw BoardApiException(
        'Invalid JSON response',
        statusCode: 0,
        detail:
            'Response looks like HTML/XML, not JSON (wrong URL or captive portal?). '
            'Preview: ${_shortPreview(text)}',
      );
    }

    final decoded = _jsonDecodeLenient(text);
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
      detail:
          '${e.message}. Preview: ${_shortPreview(_utf8TextLenient(_stripUtf8Bom(decompressHttpBodyIfNeeded(bodyBytes))))}',
    );
  }
}

/// Safe preview for logs / errors (never throws).
String previewHttpBodyUtf8(List<int> bodyBytes, {int maxChars = 200}) {
  try {
    final inflated = decompressHttpBodyIfNeeded(bodyBytes);
    final s = _normalizeJsonText(_utf8TextLenient(_stripUtf8Bom(inflated)));
    if (s.length <= maxChars) return s;
    return '${s.substring(0, maxChars)}…';
  } catch (_) {
    return '(non-text or binary body, ${bodyBytes.length} bytes)';
  }
}
