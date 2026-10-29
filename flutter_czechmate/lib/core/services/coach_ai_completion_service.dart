import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

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

  /// Normalize Azure AI Foundry / Azure OpenAI base for `{base}/chat/completions`.
  ///
  /// Preferred paste shape (OpenAI v1 API, no required `api-version`):
  /// `https://<resource>.openai.azure.com/openai/v1`
  /// or `https://<resource>.services.ai.azure.com/openai/v1`.
  ///
  /// Bare host `https://<resource>.openai.azure.com` → appends `/openai/v1`.
  /// Legacy deployment roots `…/openai/deployments/<name>` are left as-is
  /// (then set optional API version in settings).
  static String normalizeAzureFoundryOpenAiBase(String raw) {
    var t = raw.trim().replaceAll(RegExp(r'/+$'), '');
    if (t.isEmpty) return t;
    final uri = Uri.tryParse(t);
    if (uri == null || !uri.hasScheme || uri.host.isEmpty) return raw.trim();
    final host = uri.host.toLowerCase();
    final isAzureHost = host.endsWith('.openai.azure.com') ||
        host.endsWith('.services.ai.azure.com') ||
        host.endsWith('.cognitiveservices.azure.com');
    if (!isAzureHost) return t;

    var path = uri.path;
    if (path.endsWith('/')) {
      path = path.substring(0, path.length - 1);
    }
    if (path.isEmpty || path == '/') {
      return uri.replace(path: '/openai/v1').toString().replaceAll(RegExp(r'/+$'), '');
    }
    if (path == '/openai') {
      return uri.replace(path: '/openai/v1').toString().replaceAll(RegExp(r'/+$'), '');
    }
    // Foundry project portal URLs are not OpenAI chat roots — use /openai/v1 on same host.
    if (path.startsWith('/api/projects/')) {
      return uri.replace(path: '/openai/v1').toString().replaceAll(RegExp(r'/+$'), '');
    }
    // Already …/openai/v1 or …/deployments/<name> — keep.
    return t;
  }

  /// URLs to try when listing what this key can call.
  /// Deployments first: that `id` is the name chat completions expect.
  static List<Uri> azureFoundryModelListUris(String normalizedBase, {String? apiVersion}) {
    var base = normalizedBase.trim().replaceAll(RegExp(r'/+$'), '');
    if (base.endsWith('/models')) {
      base = base.substring(0, base.length - '/models'.length).replaceAll(RegExp(r'/+$'), '');
    }
    final uri = Uri.tryParse(base);
    if (uri == null || !uri.hasScheme || uri.host.isEmpty) return const [];
    final port = uri.hasPort ? ':${uri.port}' : '';
    final origin = '${uri.scheme}://${uri.host}$port';
    final deployVer = (apiVersion ?? '').trim().isEmpty ? '2024-10-21' : apiVersion!.trim();
    final uris = <Uri>[
      Uri.parse('$origin/openai/deployments?api-version=${Uri.encodeQueryComponent(deployVer)}'),
      Uri.parse('$base/models'),
    ];
    final v1Models = Uri.parse('$origin/openai/v1/models');
    if (!uris.contains(v1Models)) uris.add(v1Models);
    final catalog = Uri.parse('$origin/openai/models?api-version=${Uri.encodeQueryComponent(deployVer)}');
    if (!uris.contains(catalog)) uris.add(catalog);
    return uris;
  }

  /// Model / deployment ids for an Azure AI Foundry or Azure OpenAI key.
  static Future<List<String>> listAzureFoundryModels({
    required String baseUrl,
    required String apiKey,
    String? apiVersion,
    http.Client? httpClient,
  }) async {
    final base = normalizeAzureFoundryOpenAiBase(baseUrl);
    if (base.isEmpty) {
      throw CoachLlmException('Azure AI Foundry: empty base URL');
    }
    if (apiKey.trim().isEmpty) {
      throw CoachLlmException('Azure AI Foundry: no API key');
    }
    final uris = azureFoundryModelListUris(base, apiVersion: apiVersion);
    if (uris.isEmpty) {
      throw CoachLlmException('Azure AI Foundry: base URL is not a valid address');
    }
    final client = httpClient ?? http.Client();
    Object? lastError;
    try {
      for (final uri in uris) {
        final isDeployments = uri.path.endsWith('/deployments');
        try {
          final ids = await CoachOpenAiClient.listModelIds(
            uri: uri,
            apiKey: apiKey,
            client: client,
          );
          if (ids == null) continue;
          if (ids.isEmpty && !isDeployments) continue;
          return ids;
        } on CoachLlmException catch (e) {
          if (e.statusCode == 401 || e.statusCode == 403) rethrow;
          lastError = e;
        }
      }
    } finally {
      if (httpClient == null) client.close();
    }
    if (lastError != null) throw lastError;
    return const [];
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
      case CoachAiProviderId.azureFoundry:
        final key = prefs.coachAzureFoundryApiKey?.trim();
        if (key == null || key.isEmpty) {
          throw CoachLlmException('Azure AI Foundry: no API key');
        }
        final baseRaw = prefs.coachAzureFoundryBaseUrl.trim();
        if (baseRaw.isEmpty) {
          throw CoachLlmException('Azure AI Foundry: empty base URL');
        }
        final base = normalizeAzureFoundryOpenAiBase(baseRaw);
        final deployment = prefs.coachAzureFoundryDeployment.trim();
        if (deployment.isEmpty) {
          throw CoachLlmException('Azure AI Foundry: empty deployment / model name');
        }
        if (prefs.coachTraceLogsEnabled && base != baseRaw.replaceAll(RegExp(r'/+$'), '')) {
          debugPrint(
            '[staging][coach] Azure Foundry apiBase normalized: $baseRaw → $base',
          );
        }
        final apiVer = prefs.coachAzureFoundryApiVersion.trim();
        return CoachOpenAiClient.chatCompletionMessages(
          apiBaseV1: base,
          apiKey: key,
          model: deployment,
          authStyle: CoachOpenAiAuthStyle.azureApiKey,
          apiVersion: apiVer.isEmpty ? null : apiVer,
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
        case CoachAiProviderId.azureFoundry:
          if ((prefs.coachAzureFoundryApiKey ?? '').trim().isNotEmpty &&
              prefs.coachAzureFoundryBaseUrl.trim().isNotEmpty &&
              prefs.coachAzureFoundryDeployment.trim().isNotEmpty) {
            return true;
          }
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
