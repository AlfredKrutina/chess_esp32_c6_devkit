import 'package:flutter/foundation.dart';

import '../models/coach_ai_provider.dart';
import 'coach_llm_clients.dart';
import 'prefs_repository.dart';

class CoachAiTurn {
  const CoachAiTurn({required this.fromUser, required this.text});
  final bool fromUser;
  final String text;
}

class CoachAiResult {
  CoachAiResult.cloud(this.text, this.providerId) : isOffline = false;
  CoachAiResult.offline(this.text) : providerId = null, isOffline = true;

  final String text;
  final CoachAiProviderId? providerId;
  final bool isOffline;
}

/// Runs the coach prompt against [PrefsRepository.coachAiPriority] until one provider succeeds.
abstract final class CoachAiCompletionService {
  static const openAiApiBase = 'https://api.openai.com/v1';
  static const groqApiBase = 'https://api.groq.com/openai/v1';
  static const xaiApiBase = 'https://api.x.ai/v1';

  /// [CoachOpenAiClient] expects `{base}/chat/completions` where base ends with `/v1`.
  /// Many users set `http://host:11434` — Ollama needs `/v1`.
  static String normalizeOllamaOpenAiBase(String raw) {
    var t = raw.trim().replaceAll(RegExp(r'/+$'), '');
    if (t.isEmpty) return t;
    final uri = Uri.tryParse(t);
    if (uri == null || !uri.hasScheme || uri.host.isEmpty) return raw.trim();
    final p = uri.path;
    if (p.endsWith('/v1')) return t;
    final rootish = p.isEmpty || p == '/';
    if (!rootish) return t;
    // Default Ollama listen port — OpenAI-compatible routes live under /v1/…
    if (uri.port == 11434) {
      return '$t/v1';
    }
    return t;
  }

  /// Cap assistant/user pairs so payloads stay reasonable on mobile networks.
  static const maxDialogMessages = 24;

  static List<CoachAiTurn> truncateDialog(List<CoachAiTurn> full) {
    var slice =
        full.length <= maxDialogMessages ? full : full.sublist(full.length - maxDialogMessages);
    while (slice.isNotEmpty && !slice.first.fromUser) {
      slice = slice.sublist(1);
    }
    return slice;
  }

  static Future<CoachAiResult> completeWithFallback({
    required PrefsRepository prefs,
    required String systemPrompt,
    required List<CoachAiTurn> priorTurns,
    required String latestUserMessage,
  }) async {
    final trace = prefs.coachTraceLogsEnabled;
    final dialog = truncateDialog([
      ...priorTurns,
      CoachAiTurn(fromUser: true, text: latestUserMessage),
    ]);

    final chain = prefs.coachAiPriority;
    if (chain.isEmpty) {
      return CoachAiResult.offline(_offlineNoChainMessage);
    }

    Object? lastError;
    for (final id in chain) {
      try {
        final text = await _tryProvider(
          prefs: prefs,
          id: id,
          systemPrompt: systemPrompt,
          dialog: dialog,
        );
        if (text.isNotEmpty) return CoachAiResult.cloud(text, id);
      } catch (e, st) {
        lastError = e;
        if (trace) {
          debugPrint('[staging][coach] provider=${id.storageValue} failed: $e');
          debugPrint('$st');
        }
      }
    }

    if (trace && lastError != null) {
      debugPrint('[staging][coach] chain exhausted; lastError=$lastError');
    }
    return CoachAiResult.offline(_offlineAllFailedMessage);
  }

  static const _offlineNoChainMessage =
      'Cloud coach is disabled (empty provider list). Add providers under Settings → Coach & AI, or keep using offline tips from the placeholder coach.';

  static const _offlineAllFailedMessage =
      'Every provider in your priority list failed or is missing credentials. Check keys, model names, and Ollama URL under Settings → Coach & AI.';

  static Future<String> _tryProvider({
    required PrefsRepository prefs,
    required CoachAiProviderId id,
    required String systemPrompt,
    required List<CoachAiTurn> dialog,
  }) async {
    switch (id) {
      case CoachAiProviderId.openai:
        final key = prefs.coachApiKey?.trim();
        if (key == null || key.isEmpty) {
          throw CoachLlmException('OpenAI: no API key');
        }
        return CoachOpenAiClient.chatCompletionMessages(
          apiBaseV1: openAiApiBase,
          apiKey: key,
          model: prefs.coachOpenAiModel,
          messages: _openAiMessages(systemPrompt, dialog),
        );
      case CoachAiProviderId.groq:
        final key = prefs.coachGroqApiKey?.trim();
        if (key == null || key.isEmpty) {
          throw CoachLlmException('Groq: no API key');
        }
        return CoachOpenAiClient.chatCompletionMessages(
          apiBaseV1: groqApiBase,
          apiKey: key,
          model: prefs.coachGroqModel,
          messages: _openAiMessages(systemPrompt, dialog),
        );
      case CoachAiProviderId.xaiGrok:
        final key = prefs.coachXaiApiKey?.trim();
        if (key == null || key.isEmpty) {
          throw CoachLlmException('xAI Grok: no API key');
        }
        return CoachOpenAiClient.chatCompletionMessages(
          apiBaseV1: xaiApiBase,
          apiKey: key,
          model: prefs.coachXaiModel,
          messages: _openAiMessages(systemPrompt, dialog),
        );
      case CoachAiProviderId.ollama:
        final baseRaw = prefs.coachOllamaBaseUrl.trim();
        if (baseRaw.isEmpty) throw CoachLlmException('Ollama: empty base URL');
        final base = normalizeOllamaOpenAiBase(baseRaw);
        if (prefs.coachTraceLogsEnabled &&
            base != baseRaw.replaceAll(RegExp(r'/+$'), '')) {
          debugPrint('[staging][coach] Ollama apiBase normalized: $baseRaw → $base');
        }
        return CoachOpenAiClient.chatCompletionMessages(
          apiBaseV1: base,
          apiKey: '',
          model: prefs.coachOllamaModel,
          messages: _openAiMessages(systemPrompt, dialog),
        );
      case CoachAiProviderId.googleGemini:
        final key = prefs.coachGeminiApiKey?.trim();
        if (key == null || key.isEmpty) {
          throw CoachLlmException('Google AI: no API key');
        }
        return CoachGoogleGenerativeClient.generateContentConversation(
          apiKey: key,
          modelId: prefs.coachGemmaModelId,
          systemInstruction: systemPrompt,
          turns: [
            for (final t in dialog) (isUser: t.fromUser, text: t.text),
          ],
        );
    }
  }

  static List<Map<String, dynamic>> _openAiMessages(
    String systemPrompt,
    List<CoachAiTurn> dialog,
  ) {
    final out = <Map<String, dynamic>>[
      {'role': 'system', 'content': systemPrompt},
    ];
    for (final t in dialog) {
      out.add({
        'role': t.fromUser ? 'user' : 'assistant',
        'content': t.text,
      });
    }
    return out;
  }

  /// True if any provider in [prefs.coachAiPriority] has the minimum config to attempt a call.
  static bool anyProviderConfigured(PrefsRepository prefs) {
    for (final id in prefs.coachAiPriority) {
      switch (id) {
        case CoachAiProviderId.openai:
          if ((prefs.coachApiKey ?? '').trim().isNotEmpty) return true;
        case CoachAiProviderId.googleGemini:
          if ((prefs.coachGeminiApiKey ?? '').trim().isNotEmpty) return true;
        case CoachAiProviderId.groq:
          if ((prefs.coachGroqApiKey ?? '').trim().isNotEmpty) return true;
        case CoachAiProviderId.xaiGrok:
          if ((prefs.coachXaiApiKey ?? '').trim().isNotEmpty) return true;
        case CoachAiProviderId.ollama:
          if (prefs.coachOllamaBaseUrl.trim().isNotEmpty &&
              prefs.coachOllamaModel.trim().isNotEmpty) {
            return true;
          }
      }
    }
    return false;
  }
}
