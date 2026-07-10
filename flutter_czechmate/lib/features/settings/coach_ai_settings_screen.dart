import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/models/coach_ai_provider.dart';
import '../../l10n/app_localizations.dart';

/// Presets for Google AI Studio (`generativelanguage.googleapis.com`).
const _kGemmaPresetModels = [
  'gemini-2.5-flash',
  'gemini-2.5-flash-lite',
  'gemma-2-9b-it',
  'gemma-2-27b-it',
  'gemma-3-27b-it',
];

String coachAiNavSubtitle(
    AppLocalizations l10n, List<CoachAiProviderId> chain) {
  if (chain.isEmpty) return l10n.settingsCoachSubtitleOfflineTips;
  return chain.map((e) => e.shortLabel).join(', ');
}

/// Coach / AI credentials and provider priority — full-screen settings route.
class CoachAiSettingsScreen extends ConsumerStatefulWidget {
  const CoachAiSettingsScreen({super.key});

  @override
  ConsumerState<CoachAiSettingsScreen> createState() =>
      _CoachAiSettingsScreenState();
}

class _CoachAiSettingsScreenState extends ConsumerState<CoachAiSettingsScreen> {
  late final TextEditingController _coachKey;
  late final TextEditingController _coachGeminiKey;
  late final TextEditingController _coachGemmaModelCustom;
  late final TextEditingController _coachOpenAiModel;
  late final TextEditingController _coachGroqKey;
  late final TextEditingController _coachGroqModel;
  late final TextEditingController _coachXaiKey;
  late final TextEditingController _coachXaiModel;
  late final TextEditingController _coachOllamaBase;
  late final TextEditingController _coachOllamaModel;
  List<CoachAiProviderId> _coachPriority = [];
  String _gemmaPresetChoice = _kGemmaPresetModels.first;

  @override
  void initState() {
    super.initState();
    final p = ref.read(prefsRepositoryProvider);
    _coachKey = TextEditingController(text: p.coachApiKey ?? '');
    _coachGeminiKey = TextEditingController(text: p.coachGeminiApiKey ?? '');
    _coachPriority = List<CoachAiProviderId>.from(p.coachAiPriority);
    _coachOpenAiModel = TextEditingController(text: p.coachOpenAiModel);
    _coachGroqKey = TextEditingController(text: p.coachGroqApiKey ?? '');
    _coachGroqModel = TextEditingController(text: p.coachGroqModel);
    _coachXaiKey = TextEditingController(text: p.coachXaiApiKey ?? '');
    _coachXaiModel = TextEditingController(text: p.coachXaiModel);
    _coachOllamaBase = TextEditingController(text: p.coachOllamaBaseUrl);
    _coachOllamaModel = TextEditingController(text: p.coachOllamaModel);
    final savedModel = p.coachGemmaModelId;
    if (_kGemmaPresetModels.contains(savedModel)) {
      _gemmaPresetChoice = savedModel;
      _coachGemmaModelCustom = TextEditingController(text: savedModel);
    } else {
      _gemmaPresetChoice = '__custom__';
      _coachGemmaModelCustom = TextEditingController(text: savedModel);
    }
  }

  @override
  void dispose() {
    _coachKey.dispose();
    _coachGeminiKey.dispose();
    _coachGemmaModelCustom.dispose();
    _coachOpenAiModel.dispose();
    _coachGroqKey.dispose();
    _coachGroqModel.dispose();
    _coachXaiKey.dispose();
    _coachXaiModel.dispose();
    _coachOllamaBase.dispose();
    _coachOllamaModel.dispose();
    super.dispose();
  }

  Future<void> _openCoachDocUrl(Uri uri) async {
    if (await canLaunchUrl(uri)) {
      await launchUrl(uri, mode: LaunchMode.externalApplication);
    }
  }

  String _resolvedGemmaModelId() {
    if (_gemmaPresetChoice != '__custom__') return _gemmaPresetChoice;
    return _coachGemmaModelCustom.text.trim();
  }

  Widget _coachFieldLabel(BuildContext context, String text) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Text(
        text,
        style: theme.textTheme.titleSmall?.copyWith(
          fontWeight: FontWeight.w600,
          color: theme.colorScheme.onSurface,
        ),
      ),
    );
  }

  InputDecoration _coachOutlineDecoration({
    String? hintText,
    String? helperText,
  }) {
    return InputDecoration(
      hintText: hintText,
      helperText: helperText,
      border: const OutlineInputBorder(),
      filled: true,
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      helperMaxLines: 4,
    );
  }

  Widget _coachExpansionFields(List<Widget> fields) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          for (var i = 0; i < fields.length; i++) ...[
            if (i > 0) const SizedBox(height: 18),
            fields[i],
          ],
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final l10n = context.l10n;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsCoachAiTitle)),
      body: desktopSettingsDetailBody(
        FocusTraversalGroup(
          policy: OrderedTraversalPolicy(),
          child: ListView(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 32),
            children: [
              Text(
                l10n.settingsCoachPriorityIntro,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 10,
                runSpacing: 10,
                children: [
                  TextButton(
                    onPressed: () => setState(() => _coachPriority = []),
                    child: Text(l10n.settingsCoachOfflineOnly),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                l10n.settingsCoachPriorityTopLabel,
                style: Theme.of(context).textTheme.labelLarge,
              ),
              const SizedBox(height: 4),
              if (_coachPriority.isEmpty)
                Padding(
                  padding: const EdgeInsets.symmetric(vertical: 8),
                  child: Text(
                    l10n.settingsCoachNoCloudProviders,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                  ),
                )
              else
                ReorderableListView(
                  shrinkWrap: true,
                  physics: const NeverScrollableScrollPhysics(),
                  onReorder: (oldIndex, newIndex) {
                    setState(() {
                      if (newIndex > oldIndex) newIndex -= 1;
                      final id = _coachPriority.removeAt(oldIndex);
                      _coachPriority.insert(newIndex, id);
                    });
                  },
                  children: [
                    ..._coachPriority.asMap().entries.map((entry) {
                      final index = entry.key;
                      final id = entry.value;
                      return ListTile(
                        key: ValueKey('${id.storageValue}_$index'),
                        contentPadding: const EdgeInsets.symmetric(
                          horizontal: 4,
                          vertical: 10,
                        ),
                        leading: const Icon(Icons.drag_handle),
                        title: Text(id.shortLabel),
                        trailing: IconButton(
                          icon: const Icon(Icons.close, size: 20),
                          onPressed: () => setState(
                            () => _coachPriority.removeAt(index),
                          ),
                          tooltip: l10n.settingsCoachRemoveFromChain,
                        ),
                      );
                    }),
                  ],
                ),
              const SizedBox(height: 8),
              Align(
                alignment: Alignment.centerLeft,
                child: Text(
                  l10n.settingsCoachAddProvider,
                  style: Theme.of(context).textTheme.labelLarge,
                ),
              ),
              const SizedBox(height: 4),
              Wrap(
                spacing: 10,
                runSpacing: 10,
                children: [
                  for (final id in CoachAiProviderId.values)
                    if (!_coachPriority.contains(id))
                      ActionChip(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 12,
                          vertical: 10,
                        ),
                        label: Text('+ ${id.shortLabel}'),
                        onPressed: () => setState(() => _coachPriority.add(id)),
                      ),
                ],
              ),
              const SizedBox(height: 12),
              Builder(
                builder: (context) {
                  final theme = Theme.of(context);
                  final cs = theme.colorScheme;
                  final level = prefs.coachUserLevel.clamp(1, 4);
                  String levelLabel(int lv) => switch (lv) {
                        1 => l10n.settingsCoachLevelBeginner,
                        2 => l10n.settingsCoachLevelIntermediate,
                        3 => l10n.settingsCoachLevelAdvanced,
                        _ => l10n.settingsCoachLevelExpert,
                      };
                  return Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      Text(
                        l10n.settingsCoachExplanationLevel,
                        style: theme.textTheme.titleSmall
                            ?.copyWith(fontWeight: FontWeight.w600),
                      ),
                      const SizedBox(height: 4),
                      Text(
                        l10n.settingsCoachExplanationLevelSubtitle,
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: cs.onSurfaceVariant,
                        ),
                      ),
                      const SizedBox(height: 10),
                      Row(
                        crossAxisAlignment: CrossAxisAlignment.center,
                        children: [
                          SizedBox(
                            width: 76,
                            child: Text(
                              l10n.settingsCoachLevelBeginner,
                              style: theme.textTheme.labelSmall?.copyWith(
                                color: cs.onSurfaceVariant,
                              ),
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          Expanded(
                            child: Slider(
                              value: level.toDouble(),
                              min: 1,
                              max: 4,
                              divisions: 3,
                              label: levelLabel(level),
                              onChanged: (x) {
                                final v = x.round().clamp(1, 4);
                                prefs.setCoachUserLevel(v).then((_) {
                                  if (mounted) setState(() {});
                                });
                              },
                            ),
                          ),
                          SizedBox(
                            width: 76,
                            child: Text(
                              l10n.settingsCoachLevelExpert,
                              textAlign: TextAlign.end,
                              style: theme.textTheme.labelSmall?.copyWith(
                                color: cs.onSurfaceVariant,
                              ),
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                        ],
                      ),
                      Center(
                        child: Text(
                          levelLabel(level),
                          style: theme.textTheme.labelMedium?.copyWith(
                            color: cs.primary,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ),
                    ],
                  );
                },
              ),
              Text(
                l10n.settingsCoachCredentialsHeader,
                style: Theme.of(context).textTheme.titleSmall,
              ),
              const SizedBox(height: 8),
              ExpansionTile(
                tilePadding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                childrenPadding: EdgeInsets.zero,
                title: Text(l10n.settingsCoachOpenAiProvider),
                initiallyExpanded:
                    _coachPriority.contains(CoachAiProviderId.openai),
                children: [
                  _coachExpansionFields([
                    TextField(
                      controller: _coachKey,
                      decoration: _coachOutlineDecoration().copyWith(
                        labelText: l10n.settingsCoachOpenAiFieldLabel,
                        hintText: l10n.settingsCoachOpenAiKeyHint,
                        suffixIcon: IconButton(
                          tooltip: l10n.settingsCoachOpenAiKeyTooltip,
                          icon: const Icon(Icons.help_outline),
                          onPressed: () => _openCoachDocUrl(
                            Uri.parse('https://platform.openai.com/api-keys'),
                          ),
                        ),
                      ),
                      obscureText: true,
                      autocorrect: false,
                      enableSuggestions: false,
                    ),
                    _coachFieldLabel(context, l10n.settingsCoachModelIdLabel),
                    TextField(
                      controller: _coachOpenAiModel,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachOpenAiModelHint,
                      ),
                    ),
                    Align(
                      alignment: Alignment.centerLeft,
                      child: TextButton.icon(
                        icon: const Icon(Icons.open_in_new, size: 18),
                        label: Text(l10n.settingsCoachOpenAiKeysButton),
                        onPressed: () => _openCoachDocUrl(
                          Uri.parse('https://platform.openai.com/api-keys'),
                        ),
                      ),
                    ),
                  ]),
                ],
              ),
              ExpansionTile(
                tilePadding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                childrenPadding: EdgeInsets.zero,
                title: Text(l10n.settingsCoachGoogleAiStudio),
                initiallyExpanded:
                    _coachPriority.contains(CoachAiProviderId.googleGemini),
                children: [
                  _coachExpansionFields([
                    _coachFieldLabel(context, l10n.settingsCoachApiKeyLabel),
                    TextField(
                      controller: _coachGeminiKey,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachPasteGoogleKeyHint,
                        helperText: l10n.settingsCoachGoogleKeyHelper,
                      ),
                      obscureText: true,
                      autocorrect: false,
                      enableSuggestions: false,
                    ),
                    _coachFieldLabel(context, l10n.settingsCoachModelIdLabel),
                    DropdownButtonFormField<String>(
                      isExpanded: true,
                      value: _gemmaPresetChoice == '__custom__'
                          ? '__custom__'
                          : (_kGemmaPresetModels.contains(_gemmaPresetChoice)
                              ? _gemmaPresetChoice
                              : '__custom__'),
                      decoration: _coachOutlineDecoration(),
                      items: [
                        ..._kGemmaPresetModels.map(
                          (id) => DropdownMenuItem(value: id, child: Text(id)),
                        ),
                        DropdownMenuItem(
                          value: '__custom__',
                          child: Text(l10n.settingsCoachCustomModel),
                        ),
                      ],
                      onChanged: (v) {
                        if (v == null) return;
                        setState(() {
                          _gemmaPresetChoice = v;
                          if (v != '__custom__') {
                            _coachGemmaModelCustom.text = v;
                          }
                        });
                      },
                    ),
                    if (_gemmaPresetChoice == '__custom__') ...[
                      _coachFieldLabel(
                          context, l10n.settingsCoachCustomModelId),
                      TextField(
                        controller: _coachGemmaModelCustom,
                        decoration: _coachOutlineDecoration(
                          hintText: l10n.settingsCoachCustomModelHint,
                        ),
                      ),
                    ],
                    Align(
                      alignment: Alignment.centerLeft,
                      child: TextButton.icon(
                        icon: const Icon(Icons.open_in_new, size: 18),
                        label: Text(l10n.settingsCoachGoogleKeyButton),
                        onPressed: () => _openCoachDocUrl(
                          Uri.parse('https://aistudio.google.com/app/apikey'),
                        ),
                      ),
                    ),
                  ]),
                ],
              ),
              ExpansionTile(
                tilePadding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                childrenPadding: EdgeInsets.zero,
                title: Text(l10n.settingsCoachGroqTitle),
                initiallyExpanded:
                    _coachPriority.contains(CoachAiProviderId.groq),
                children: [
                  _coachExpansionFields([
                    _coachFieldLabel(context, l10n.settingsCoachApiKeyLabel),
                    TextField(
                      controller: _coachGroqKey,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachGroqKeyHint,
                      ),
                      obscureText: true,
                      autocorrect: false,
                      enableSuggestions: false,
                    ),
                    _coachFieldLabel(context, l10n.settingsCoachModelIdLabel),
                    TextField(
                      controller: _coachGroqModel,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachGroqModelHint,
                        helperText: l10n.settingsCoachGroqModelHelper,
                      ),
                    ),
                    Align(
                      alignment: Alignment.centerLeft,
                      child: TextButton.icon(
                        icon: const Icon(Icons.open_in_new, size: 18),
                        label: Text(l10n.settingsCoachGroqConsoleButton),
                        onPressed: () => _openCoachDocUrl(
                          Uri.parse('https://console.groq.com/keys'),
                        ),
                      ),
                    ),
                  ]),
                ],
              ),
              ExpansionTile(
                tilePadding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                childrenPadding: EdgeInsets.zero,
                title: Text(l10n.settingsCoachXaiTitle),
                initiallyExpanded:
                    _coachPriority.contains(CoachAiProviderId.xaiGrok),
                children: [
                  _coachExpansionFields([
                    _coachFieldLabel(context, l10n.settingsCoachApiKeyLabel),
                    TextField(
                      controller: _coachXaiKey,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachXaiKeyHint,
                      ),
                      obscureText: true,
                      autocorrect: false,
                      enableSuggestions: false,
                    ),
                    _coachFieldLabel(context, l10n.settingsCoachModelIdLabel),
                    TextField(
                      controller: _coachXaiModel,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachXaiModelHint,
                      ),
                    ),
                    Align(
                      alignment: Alignment.centerLeft,
                      child: TextButton.icon(
                        icon: const Icon(Icons.open_in_new, size: 18),
                        label: Text(l10n.settingsCoachXaiConsoleButton),
                        onPressed: () => _openCoachDocUrl(
                          Uri.parse('https://console.x.ai/'),
                        ),
                      ),
                    ),
                  ]),
                ],
              ),
              ExpansionTile(
                tilePadding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                childrenPadding: EdgeInsets.zero,
                title: Text(l10n.settingsCoachOllamaTitle),
                initiallyExpanded:
                    _coachPriority.contains(CoachAiProviderId.ollama),
                children: [
                  _coachExpansionFields([
                    _coachFieldLabel(
                        context, l10n.settingsCoachOpenAiBaseLabel),
                    TextField(
                      controller: _coachOllamaBase,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachOllamaBaseHint,
                        helperText: l10n.settingsCoachOllamaUrlHelper,
                      ),
                      keyboardType: TextInputType.url,
                      autocorrect: false,
                    ),
                    _coachFieldLabel(context, l10n.settingsCoachModelNameLabel),
                    TextField(
                      controller: _coachOllamaModel,
                      decoration: _coachOutlineDecoration(
                        hintText: l10n.settingsCoachOllamaModelHint,
                      ),
                      autocorrect: false,
                    ),
                  ]),
                ],
              ),
              const SizedBox(height: 12),
              FilledButton.tonal(
                onPressed: () async {
                  await prefs.setCoachAiPriority(_coachPriority);
                  await prefs.syncCoachLlmBackendFromPriority();
                  await prefs.setCoachApiKey(_coachKey.text.trim());
                  await prefs.setCoachGeminiApiKey(_coachGeminiKey.text.trim());
                  await prefs
                      .setCoachOpenAiModel(_coachOpenAiModel.text.trim());
                  await prefs.setCoachGroqApiKey(_coachGroqKey.text.trim());
                  await prefs.setCoachGroqModel(_coachGroqModel.text.trim());
                  await prefs.setCoachXaiApiKey(_coachXaiKey.text.trim());
                  await prefs.setCoachXaiModel(_coachXaiModel.text.trim());
                  await prefs
                      .setCoachOllamaBaseUrl(_coachOllamaBase.text.trim());
                  await prefs
                      .setCoachOllamaModel(_coachOllamaModel.text.trim());
                  final mid = _resolvedGemmaModelId();
                  if (_coachPriority.contains(CoachAiProviderId.googleGemini) &&
                      mid.isEmpty) {
                    if (context.mounted) {
                      showAppSnackBar(
                        context,
                        l10n.settingsCoachEnterGeminiModelSnack,
                      );
                    }
                    return;
                  }
                  if (mid.isNotEmpty) {
                    await prefs.setCoachGemmaModelId(mid);
                  }
                  ref.invalidate(prefsRepositoryProvider);
                  if (context.mounted) {
                    showAppSnackBar(context, l10n.settingsCoachSavedSnack);
                  }
                },
                child: Text(l10n.settingsCoachSaveButton),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
