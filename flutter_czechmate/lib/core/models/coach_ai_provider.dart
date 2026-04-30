/// Cloud / local providers for the AI Coach fallback chain.
enum CoachAiProviderId {
  groq('groq'),
  googleGemini('google_gemini'),
  openai('openai'),
  xaiGrok('xai_grok'),
  ollama('ollama');

  const CoachAiProviderId(this.storageValue);

  final String storageValue;

  static CoachAiProviderId? tryParse(String? raw) {
    if (raw == null || raw.isEmpty) return null;
    for (final v in CoachAiProviderId.values) {
      if (v.storageValue == raw) return v;
    }
    return null;
  }

  String get shortLabel => switch (this) {
        CoachAiProviderId.groq => 'Groq',
        CoachAiProviderId.googleGemini => 'Google AI',
        CoachAiProviderId.openai => 'OpenAI',
        CoachAiProviderId.xaiGrok => 'xAI Grok',
        CoachAiProviderId.ollama => 'Ollama',
      };
}
