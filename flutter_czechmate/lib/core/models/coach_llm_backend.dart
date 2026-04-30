/// Where the in-app AI Coach gets its replies from.
enum CoachLlmBackend {
  /// Chat Completions API (GPT-4o mini).
  openai('openai'),

  /// Google AI Studio via Generative Language API (e.g. `gemini-2.5-flash` or `gemma-2-9b-it`).
  googleGemma('google_gemma'),

  /// Built-in tips only — no network LLM calls from Coach.
  offline('offline');

  const CoachLlmBackend(this.storageValue);

  final String storageValue;

  static CoachLlmBackend fromStorage(String? raw) {
    if (raw == null || raw.isEmpty) return CoachLlmBackend.openai;
    for (final b in CoachLlmBackend.values) {
      if (b.storageValue == raw) return b;
    }
    return CoachLlmBackend.openai;
  }

  bool get usesCloud => this != CoachLlmBackend.offline;
}
