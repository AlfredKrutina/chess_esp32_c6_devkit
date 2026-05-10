import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/widgets/app_modal_sheet.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/widgets/pressable_scale.dart';
import '../../../core/constants/app_environment.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../../core/constants/chess_time_control_presets.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../state/game_ui_notifier.dart';

/// Bottom sheet — časová kontrola + `timer_config` + `new_game`.
Future<void> showNewGameWithTimeSheet(BuildContext context) async {
  await showAppModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    showDragHandle: true,
    useSafeArea: true,
    builder: (ctx) {
      return DraggableScrollableSheet(
        initialChildSize: 0.7,
        minChildSize: 0.38,
        maxChildSize: 0.94,
        expand: false,
        builder: (_, scrollController) =>
            NewGameTimeSheet(scrollController: scrollController),
      );
    },
  );
}

class NewGameTimeSheet extends ConsumerStatefulWidget {
  const NewGameTimeSheet({super.key, required this.scrollController});

  final ScrollController scrollController;

  @override
  ConsumerState<NewGameTimeSheet> createState() => _NewGameTimeSheetState();
}

class _NewGameTimeSheetState extends ConsumerState<NewGameTimeSheet> {
  bool _useCustom = false;
  ChessTimeControlPreset _preset = ChessTimeControlPreset.blitz5_0;
  double _customMin = 10;
  double _customInc = 0;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    final raw = ref.read(prefsRepositoryProvider).lastNewGameTimeControl;
    if (raw == null || raw.isEmpty) return;
    if (raw.startsWith('p:')) {
      final name = raw.substring(2);
      final p = ChessTimeControlPreset.tryParseName(name);
      if (p != null) {
        _preset = p;
        _useCustom = false;
      }
    } else if (raw.startsWith('c:')) {
      final parts = raw.split(':');
      if (parts.length >= 3) {
        _useCustom = true;
        _customMin = (int.tryParse(parts[1]) ?? 10).toDouble().clamp(1, 180);
        _customInc = (int.tryParse(parts[2]) ?? 0).toDouble().clamp(0, 60);
      }
    }
  }

  bool _canSubmit(BoardSessionState session) {
    return session.transport == BoardTransport.wifi ||
        session.transport == BoardTransport.ble ||
        session.transport == BoardTransport.mock;
  }

  Future<void> _submit() async {
    final session = ref.read(boardSessionNotifierProvider);
    if (!_canSubmit(session)) {
      if (mounted) {
        showGlassSnackBar(
          context,
          context.l10n.newGameConnectFirstSnack,
        );
      }
      return;
    }
    setState(() => _busy = true);
    final prefs = ref.read(prefsRepositoryProvider);
    final n = ref.read(boardSessionNotifierProvider.notifier);
    try {
      if (_useCustom) {
        final m = _customMin.round().clamp(1, 180);
        final inc = _customInc.round().clamp(0, 60);
        await n.startNewGameWithTimeControl(
          type: 14,
          customMinutes: m,
          customIncrement: inc,
        );
        await prefs.setLastNewGameTimeControl('c:$m:$inc');
      } else {
        await n.startNewGameWithTimeControl(type: _preset.firmwareType);
        await prefs.setLastNewGameTimeControl('p:${_preset.name}');
      }
      if (AppEnvironment.staging) {
        debugPrint(
            '[staging] New game prefs saved: ${prefs.lastNewGameTimeControl}');
      }
      if (mounted) {
        Navigator.of(context).pop();
        showGlassSnackBar(context, context.l10n.newGameStartedSnack);
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        showGlassSnackBar(
          context,
          l10n.newGameErrorSnack(userFacingErrorSummary(l10n, e)),
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Widget _section(
    BuildContext context, {
    required Widget child,
    String? title,
  }) {
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;
    return Card(
      margin: EdgeInsets.zero,
      elevation: 0,
      color: cs.surfaceContainerLow,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (title != null) ...[
              Text(
                title,
                style: tt.titleSmall?.copyWith(fontWeight: FontWeight.w600),
              ),
              const SizedBox(height: 12),
            ],
            child,
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final session = ref.watch(boardSessionNotifierProvider);
    final boardUi = ref.watch(gameUiNotifierProvider);
    final boardUiN = ref.read(gameUiNotifierProvider.notifier);
    final kb = MediaQuery.viewInsetsOf(context).bottom;
    final canStart = _canSubmit(session);

    return Material(
      color: cs.surface,
      child: Padding(
        padding: EdgeInsets.only(bottom: kb),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(20, 4, 16, 12),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  CircleAvatar(
                    radius: 28,
                    backgroundColor: cs.primaryContainer,
                    child: Icon(
                      Icons.restart_alt_rounded,
                      size: 30,
                      color: cs.onPrimaryContainer,
                    ),
                  ),
                  const SizedBox(width: 14),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          l10n.newGameSheetTitle,
                          style: theme.textTheme.headlineSmall?.copyWith(
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                        const SizedBox(height: 4),
                        Text(
                          l10n.newGameSheetSubtitle,
                          style: theme.textTheme.bodyMedium?.copyWith(
                            color: cs.onSurfaceVariant,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
            if (!canStart)
              Padding(
                padding: const EdgeInsets.fromLTRB(20, 0, 20, 12),
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: cs.errorContainer.withValues(alpha: 0.35),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(
                      color: cs.error.withValues(alpha: 0.25),
                    ),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 14,
                      vertical: 10,
                    ),
                    child: Row(
                      children: [
                        Icon(Icons.link_off_rounded, color: cs.error, size: 22),
                        const SizedBox(width: 10),
                        Expanded(
                          child: Text(
                            l10n.newGameConnectFirstSnack,
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: cs.onErrorContainer,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            Expanded(
              child: ListView(
                controller: widget.scrollController,
                padding: const EdgeInsets.fromLTRB(20, 0, 20, 16),
                children: [
                  Text(
                    l10n.newGameSheetBody,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: cs.onSurfaceVariant,
                    ),
                  ),
                  const SizedBox(height: 16),
                  _section(
                    context,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Row(
                          children: [
                            Icon(Icons.rotate_90_degrees_ccw,
                                size: 22, color: cs.primary),
                            const SizedBox(width: 8),
                            Expanded(
                              child: Text(
                                l10n.newGameBoardViewSection,
                                style: theme.textTheme.titleSmall?.copyWith(
                                  fontWeight: FontWeight.w600,
                                ),
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 10),
                        SegmentedButton<bool>(
                          segments: [
                            ButtonSegment<bool>(
                              value: false,
                              label: Text(l10n.newGameWhiteBottom),
                              icon: const Icon(Icons.contrast_outlined, size: 18),
                            ),
                            ButtonSegment<bool>(
                              value: true,
                              label: Text(l10n.newGameBlackBottom),
                              icon: const Icon(Icons.contrast, size: 18),
                            ),
                          ],
                          selected: {boardUi.boardFlipped},
                          onSelectionChanged: _busy
                              ? null
                              : (s) {
                                  if (s.isEmpty) return;
                                  final want = s.first;
                                  if (want != boardUi.boardFlipped) {
                                    boardUiN.toggleFlip();
                                  }
                                },
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 12),
                  _section(
                    context,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.newGameCustomTimeTitle),
                          subtitle: Text(
                            _useCustom
                                ? l10n.newGameCustomSummary(
                                    _customMin.round(), _customInc.round())
                                : l10n.newGameFirmwarePresets,
                          ),
                          value: _useCustom,
                          onChanged: _busy
                              ? null
                              : (v) => setState(() => _useCustom = v),
                        ),
                        if (_useCustom) ...[
                          Text(
                            l10n.newGameMinutesPerSide(
                              '${_customMin.round()}',
                            ),
                            style: theme.textTheme.labelLarge,
                          ),
                          Slider(
                            min: 1,
                            max: 180,
                            divisions: 179,
                            value: _customMin.clamp(1, 180),
                            onChanged: _busy
                                ? null
                                : (x) => setState(() => _customMin = x),
                          ),
                          Text(
                            l10n.newGameIncrementSeconds(
                              '${_customInc.round()}',
                            ),
                            style: theme.textTheme.labelLarge,
                          ),
                          Slider(
                            min: 0,
                            max: 60,
                            divisions: 60,
                            value: _customInc.clamp(0, 60),
                            onChanged: _busy
                                ? null
                                : (x) => setState(() => _customInc = x),
                          ),
                        ] else ...[
                          const SizedBox(height: 4),
                          Wrap(
                            spacing: 8,
                            runSpacing: 8,
                            children: [
                              for (final p in ChessTimeControlPreset.values)
                                ChoiceChip(
                                  showCheckmark: false,
                                  labelPadding: const EdgeInsets.symmetric(
                                    horizontal: 10,
                                    vertical: 2,
                                  ),
                                  label: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    crossAxisAlignment:
                                        CrossAxisAlignment.start,
                                    children: [
                                      Text(p.title),
                                      Text(
                                        p.subtitle,
                                        style: theme.textTheme.labelSmall
                                            ?.copyWith(
                                          color: (!_useCustom && _preset == p)
                                              ? cs.onSecondaryContainer
                                              : cs.onSurfaceVariant,
                                        ),
                                      ),
                                    ],
                                  ),
                                  selected: !_useCustom && _preset == p,
                                  onSelected: _busy
                                      ? null
                                      : (_) => setState(() {
                                            _preset = p;
                                            _useCustom = false;
                                          }),
                                ),
                            ],
                          ),
                        ],
                      ],
                    ),
                  ),
                  const SizedBox(height: 24),
                ],
              ),
            ),
            DecoratedBox(
              decoration: BoxDecoration(
                color: cs.surface,
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withValues(alpha: 0.06),
                    blurRadius: 12,
                    offset: const Offset(0, -4),
                  ),
                ],
              ),
              child: SafeArea(
                top: false,
                child: Padding(
                  padding: const EdgeInsets.fromLTRB(20, 12, 20, 16),
                  child: SizedBox(
                    width: double.infinity,
                    child: PressableScale(
                      child: FilledButton.icon(
                        onPressed:
                            (_busy || !canStart) ? null : _submit,
                        icon: _busy
                            ? SizedBox(
                                width: 22,
                                height: 22,
                                child: CircularProgressIndicator(
                                  strokeWidth: 2,
                                  color: cs.onPrimary,
                                ),
                              )
                            : const Icon(Icons.play_arrow_rounded),
                        label: Text(l10n.newGameStartOnBoard),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
