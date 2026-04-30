import 'dart:convert';

import 'package:http/http.dart' as http;

/// Shared errors from Coach cloud backends (OpenAI / Google Generative Language API).
class CoachLlmException implements Exception {
  CoachLlmException(this.message, {this.statusCode});
  final String message;
  final int? statusCode;

  @override
  String toString() =>
      statusCode != null ? 'CoachLlmException($statusCode): $message' : 'CoachLlmException: $message';
}

abstract final class CoachOpenAiClient {
  /// OpenAI-compatible chat completions URL (…/v1/chat/completions).
  static Uri chatCompletionsUri(String apiBaseV1) {
    final t = apiBaseV1.trim().replaceAll(RegExp(r'/+$'), '');
    return Uri.parse('$t/chat/completions');
  }

  static Future<String> chatCompletionMessages({
    required String apiBaseV1,
    required String apiKey,
    required String model,
    required List<Map<String, dynamic>> messages,
    int maxTokens = 700,
    http.Client? httpClient,
  }) async {
    final client = httpClient ?? http.Client();
    try {
      final headers = <String, String>{
        'Content-Type': 'application/json',
      };
      final k = apiKey.trim();
      if (k.isNotEmpty) {
        headers['Authorization'] = 'Bearer $k';
      }
      final res = await client
          .post(
            chatCompletionsUri(apiBaseV1),
            headers: headers,
            body: jsonEncode({
              'model': model,
              'messages': messages,
              'max_tokens': maxTokens,
            }),
          )
          .timeout(const Duration(seconds: 120));
      if (res.statusCode != 200) {
        throw CoachLlmException(_truncateBody(res.body), statusCode: res.statusCode);
      }
      final map = jsonDecode(utf8.decode(res.bodyBytes));
      if (map is! Map<String, dynamic>) {
        throw CoachLlmException('OpenAI-compatible: expected JSON object');
      }
      final choices = map['choices'] as List?;
      if (choices == null || choices.isEmpty) {
        throw CoachLlmException('OpenAI-compatible: no choices in response');
      }
      final msg = (choices.first as Map)['message'] as Map?;
      final content = msg?['content'] as String?;
      if (content == null || content.trim().isEmpty) {
        throw CoachLlmException('OpenAI-compatible: empty assistant message');
      }
      return content.trim();
    } on CoachLlmException {
      rethrow;
    } on FormatException catch (e) {
      throw CoachLlmException('OpenAI-compatible: invalid JSON ($e)');
    } finally {
      if (httpClient == null) client.close();
    }
  }

  static Future<String> chatCompletion({
    required String apiKey,
    required String systemPrompt,
    required String userMessage,
    String model = 'gpt-4o-mini',
    int maxTokens = 500,
    http.Client? httpClient,
  }) {
    return chatCompletionMessages(
      apiBaseV1: 'https://api.openai.com/v1',
      apiKey: apiKey,
      model: model,
      maxTokens: maxTokens,
      httpClient: httpClient,
      messages: [
        {'role': 'system', 'content': systemPrompt},
        {'role': 'user', 'content': userMessage},
      ],
    );
  }
}

/// Google AI Studio key + Gemini/Gemma model via `generateContent` (v1beta).
abstract final class CoachGoogleGenerativeClient {
  static final _modelIdOk = RegExp(r'^[a-zA-Z0-9._\-]+$');

  static Future<String> generateContent({
    required String apiKey,
    required String modelId,
    required String systemInstruction,
    required String userText,
    int maxOutputTokens = 512,
    double temperature = 0.7,
    http.Client? httpClient,
  }) {
    return generateContentConversation(
      apiKey: apiKey,
      modelId: modelId,
      systemInstruction: systemInstruction,
      turns: [(isUser: true, text: userText)],
      maxOutputTokens: maxOutputTokens,
      temperature: temperature,
      httpClient: httpClient,
    );
  }

  /// Multiturn: each turn is user or model (assistant). Must start with a user turn.
  static Future<String> generateContentConversation({
    required String apiKey,
    required String modelId,
    required String systemInstruction,
    required List<({bool isUser, String text})> turns,
    int maxOutputTokens = 700,
    double temperature = 0.7,
    http.Client? httpClient,
  }) async {
    final mid = modelId.trim().replaceFirst(RegExp(r'^models/'), '');
    if (mid.isEmpty) throw CoachLlmException('Google AI: model id is empty');
    if (!_modelIdOk.hasMatch(mid)) {
      throw CoachLlmException(
        'Google AI: unsupported model id (use letters, numbers, dots, underscore, hyphen only)',
      );
    }
    if (turns.isEmpty) {
      throw CoachLlmException('Google AI: no conversation turns');
    }

    final client = httpClient ?? http.Client();
    try {
      final uri = Uri.parse(
        'https://generativelanguage.googleapis.com/v1beta/models/$mid:generateContent',
      ).replace(queryParameters: {'key': apiKey.trim()});

      final contents = <Map<String, dynamic>>[
        for (final t in turns)
          {
            'role': t.isUser ? 'user' : 'model',
            'parts': [
              {'text': t.text},
            ],
          },
      ];

      final body = <String, dynamic>{
        'systemInstruction': {
          'parts': [
            {'text': systemInstruction},
          ],
        },
        'contents': contents,
        'generationConfig': {
          'temperature': temperature,
          'maxOutputTokens': maxOutputTokens,
        },
      };

      final res = await client
          .post(
            uri,
            headers: const {'Content-Type': 'application/json'},
            body: jsonEncode(body),
          )
          .timeout(const Duration(seconds: 120));

      final bytes = res.bodyBytes;
      Map<String, dynamic>? map;
      try {
        final decoded = jsonDecode(utf8.decode(bytes));
        map = decoded is Map<String, dynamic> ? decoded : null;
      } catch (_) {
        map = null;
      }

      if (res.statusCode != 200) {
        final msg = _googleErrorMessage(map) ?? _truncateBody(utf8.decode(bytes));
        throw CoachLlmException(msg, statusCode: res.statusCode);
      }
      if (map == null) throw CoachLlmException('Google AI: invalid JSON');

      final block = map['promptFeedback'] as Map<String, dynamic>?;
      final br = block?['blockReason'];
      if (br != null && '$br'.isNotEmpty) {
        throw CoachLlmException('Google AI: blocked ($br)');
      }

      final candidates = map['candidates'] as List?;
      if (candidates == null || candidates.isEmpty) {
        throw CoachLlmException('Google AI: no candidates (model may have refused output)');
      }
      final first = candidates.first;
      if (first is! Map<String, dynamic>) {
        throw CoachLlmException('Google AI: invalid candidate');
      }
      final finish = first['finishReason'] as String?;
      if (finish == 'SAFETY') {
        throw CoachLlmException('Google AI: finishReason=SAFETY');
      }
      final content = first['content'] as Map<String, dynamic>?;
      final parts = content?['parts'] as List?;
      if (parts == null || parts.isEmpty) {
        throw CoachLlmException('Google AI: empty content parts');
      }
      final buf = StringBuffer();
      for (final p in parts) {
        if (p is Map && p['text'] is String) buf.write(p['text'] as String);
      }
      final text = buf.toString().trim();
      if (text.isEmpty) throw CoachLlmException('Google AI: no text in response');
      return text;
    } on CoachLlmException {
      rethrow;
    } on FormatException catch (e) {
      throw CoachLlmException('Google AI: invalid JSON ($e)');
    } finally {
      if (httpClient == null) client.close();
    }
  }

  static String? _googleErrorMessage(Map<String, dynamic>? map) {
    final err = map?['error'];
    if (err is Map && err['message'] is String) return err['message'] as String;
    return null;
  }
}

String _truncateBody(String s, [int max = 400]) {
  final t = s.trim();
  if (t.length <= max) return t.isEmpty ? '(empty body)' : t;
  return '${t.substring(0, max)}…';
}
