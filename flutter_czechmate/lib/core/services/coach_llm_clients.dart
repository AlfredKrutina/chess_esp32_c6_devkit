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

/// How [CoachOpenAiClient] sends the API key.
enum CoachOpenAiAuthStyle {
  /// `Authorization: Bearer <key>` — OpenAI, Groq, xAI, Ollama, many local proxies.
  bearer,

  /// `api-key: <key>` — Azure OpenAI / Microsoft Azure AI Foundry key auth.
  azureApiKey,
}

abstract final class CoachOpenAiClient {
  /// OpenAI-compatible chat completions URL (…/v1/chat/completions).
  ///
  /// Optional [apiVersion] becomes `?api-version=` (legacy Azure deployment URLs).
  static Uri chatCompletionsUri(String apiBaseV1, {String? apiVersion}) {
    final t = apiBaseV1.trim().replaceAll(RegExp(r'/+$'), '');
    final uri = Uri.parse('$t/chat/completions');
    final ver = apiVersion?.trim();
    if (ver == null || ver.isEmpty) return uri;
    final q = Map<String, String>.from(uri.queryParameters);
    q['api-version'] = ver;
    return uri.replace(queryParameters: q);
  }

  static Future<String> chatCompletionMessages({
    required String apiBaseV1,
    required String apiKey,
    required String model,
    required List<Map<String, dynamic>> messages,
    int maxTokens = 700,
    CoachOpenAiAuthStyle authStyle = CoachOpenAiAuthStyle.bearer,
    String? apiVersion,
    http.Client? httpClient,
  }) async {
    final client = httpClient ?? http.Client();
    try {
      final headers = <String, String>{
        'Content-Type': 'application/json',
      };
      final k = apiKey.trim();
      if (k.isNotEmpty) {
        switch (authStyle) {
          case CoachOpenAiAuthStyle.bearer:
            headers['Authorization'] = 'Bearer $k';
          case CoachOpenAiAuthStyle.azureApiKey:
            headers['api-key'] = k;
        }
      }
      final res = await client
          .post(
            chatCompletionsUri(apiBaseV1, apiVersion: apiVersion),
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

  /// GET a models or deployments list. `null` means HTTP 404 (try another URL).
  ///
  /// Does not close [client].
  static Future<List<String>?> listModelIds({
    required Uri uri,
    required String apiKey,
    required http.Client client,
    CoachOpenAiAuthStyle authStyle = CoachOpenAiAuthStyle.azureApiKey,
  }) async {
    final headers = <String, String>{'Accept': 'application/json'};
    final k = apiKey.trim();
    if (k.isNotEmpty) {
      switch (authStyle) {
        case CoachOpenAiAuthStyle.bearer:
          headers['Authorization'] = 'Bearer $k';
        case CoachOpenAiAuthStyle.azureApiKey:
          headers['api-key'] = k;
      }
    }
    final res = await client.get(uri, headers: headers).timeout(const Duration(seconds: 25));
    if (res.statusCode == 404) return null;
    if (res.statusCode != 200) {
      throw CoachLlmException(_truncateBody(res.body), statusCode: res.statusCode);
    }
    try {
      return parseModelIds(utf8.decode(res.bodyBytes));
    } on CoachLlmException {
      rethrow;
    } on FormatException catch (e) {
      throw CoachLlmException('OpenAI-compatible: invalid JSON ($e)');
    }
  }

  /// Ids from an OpenAI `{data:[{id}]}` list or an Azure deployments payload.
  static List<String> parseModelIds(String body) {
    final decoded = jsonDecode(body);
    final List<dynamic> raw;
    if (decoded is Map) {
      final data = decoded['data'] ?? decoded['value'] ?? decoded['models'];
      if (data is! List) return const [];
      raw = data;
    } else if (decoded is List) {
      raw = decoded;
    } else {
      throw CoachLlmException('OpenAI-compatible: expected a model list');
    }
    final ids = <String>[];
    final seen = <String>{};
    for (final item in raw) {
      if (item is String) {
        _addModelId(ids, seen, item);
        continue;
      }
      if (item is! Map) continue;
      if (!_usableListedModel(item)) continue;
      final id = _listedModelId(item);
      if (id != null) _addModelId(ids, seen, id);
    }
    ids.sort((a, b) => a.toLowerCase().compareTo(b.toLowerCase()));
    return _preferChatModelIds(ids);
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

void _addModelId(List<String> ids, Set<String> seen, String raw) {
  final id = raw.trim();
  if (id.isEmpty || !seen.add(id)) return;
  ids.add(id);
}

String? _listedModelId(Map item) {
  for (final key in ['id', 'name']) {
    final v = item[key];
    if (v is String && v.trim().isNotEmpty) return v.trim();
  }
  final model = item['model'];
  if (model is String && model.trim().isNotEmpty) return model.trim();
  if (model is Map) {
    final name = model['name'] ?? model['id'];
    if (name is String && name.trim().isNotEmpty) return name.trim();
  }
  return null;
}

bool _usableListedModel(Map item) {
  const dead = {'failed', 'canceled', 'cancelled', 'deleting', 'disabled'};
  final status = '${item['status'] ?? ''}'.trim().toLowerCase();
  if (dead.contains(status)) return false;
  final props = item['properties'];
  if (props is Map) {
    final provisioning = '${props['provisioningState'] ?? ''}'.trim().toLowerCase();
    if (dead.contains(provisioning)) return false;
  }
  final caps = item['capabilities'];
  if (caps is Map && caps['chat_completion'] == false && caps['completion'] == false) {
    return false;
  }
  return true;
}

final _nonChatModelId = RegExp(
  r'embed|whisper|tts|dall-e|gpt-image|image-generation',
  caseSensitive: false,
);

List<String> _preferChatModelIds(List<String> ids) {
  final chat = ids.where((id) => !_nonChatModelId.hasMatch(id)).toList();
  return chat.isEmpty ? ids : chat;
}

String _truncateBody(String s, [int max = 400]) {
  final t = s.trim();
  if (t.length <= max) return t.isEmpty ? '(empty body)' : t;
  return '${t.substring(0, max)}…';
}
