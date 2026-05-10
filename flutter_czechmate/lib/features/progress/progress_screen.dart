import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/widgets/session_error_panel.dart';
import '../../l10n/app_localizations.dart';
import '../coach/coach_screen.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/state/game_ui_notifier.dart';
import '../profile/user_profile_screen.dart';
import '../setup/board_setup_wizard_screen.dart';

bool _supportsRemoteBoardCommands(BoardSessionState s) {
  if (s.transport == BoardTransport.wifi && s.wifiBaseUrl != null) return true;
  if (s.transport == BoardTransport.ble) return true;
  return false;
}

String _transportLabel(AppLocalizations l, BoardSessionState session) {
  return switch (session.transport) {
    BoardTransport.none => l.settingsLinkDisconnected,
    BoardTransport.mock => l.transportShortDemo,
    BoardTransport.wifi => l.progressTransportWifi,
    BoardTransport.ble => l.progressTransportBluetooth,
  };
}

String _coachLevelTooltip(AppLocalizations l, int level) {
  return switch (level) {
    1 => l.progressLevelBeginner,
    2 => l.progressLevelIntermediate,
    3 => l.progressLevelAdvanced,
    4 => l.progressLevelExpert,
    _ => l.progressLevelNumber(level),
  };
}

/// True when four labeled segments would fit without wrapping (níže jen ✓).
bool _coachLevelSegmentedLabelsFit(
    BuildContext context, double rowWidth, List<String> labels) {
  if (rowWidth <= 0) return false;
  final style = Theme.of(context).textTheme.labelLarge ??
      const TextStyle(fontSize: 14, fontWeight: FontWeight.w500);
  final scaler = MediaQuery.textScalerOf(context);
  var maxLabel = 0.0;
  for (final l in labels) {
    final tp = TextPainter(
      text: TextSpan(text: l, style: style),
      textDirection: TextDirection.ltr,
      textScaler: scaler,
    )..layout(maxWidth: double.infinity);
    maxLabel = math.max(maxLabel, tp.width);
  }
  const insetPaddingDividerBudget = 28.0;
  return (rowWidth / 4) >= maxLabel + insetPaddingDividerBudget;
}

/// Na úzkém displeji bez popisků (jen ✓ na aktivním), jinak klasický SegmentedButton.
class _CoachLevelRow extends ConsumerWidget {
  const _CoachLevelRow();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final cs = Theme.of(context).colorScheme;
    final level = prefs.coachUserLevel.clamp(1, 4);
    final l10n = context.l10n;
    final segLabels = [
      l10n.progressSegBeginner,
      l10n.progressSegIntermediate,
      l10n.progressSegAdvanced,
      l10n.progressSegExpert,
    ];

    Future<void> apply(int v) async {
      await ref.read(prefsRepositoryProvider).setCoachUserLevel(v);
      ref.invalidate(prefsRepositoryProvider);
    }

    return LayoutBuilder(
      builder: (context, bc) {
        final compact =
            !_coachLevelSegmentedLabelsFit(context, bc.maxWidth, segLabels);
        if (!compact) {
          return SegmentedButton<int>(
            segments: [
              ButtonSegment(value: 1, label: Text(l10n.progressSegBeginner)),
              ButtonSegment(
                  value: 2, label: Text(l10n.progressSegIntermediate)),
              ButtonSegment(value: 3, label: Text(l10n.progressSegAdvanced)),
              ButtonSegment(value: 4, label: Text(l10n.progressSegExpert)),
            ],
            selected: {level},
            onSelectionChanged: (Set<int> s) async {
              if (s.isEmpty) {
                return;
              }
              await apply(s.first);
            },
          );
        }

        return Row(
          children: [
            for (var i = 1; i <= 4; i++)
              Expanded(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 3),
                  child: Tooltip(
                    message: _coachLevelTooltip(l10n, i),
                    child: Semantics(
                      button: true,
                      label: _coachLevelTooltip(l10n, i),
                      selected: level == i,
                      child: Material(
                        color: level == i
                            ? cs.primary
                            : cs.surfaceContainerHighest,
                        borderRadius: BorderRadius.circular(12),
                        clipBehavior: Clip.antiAlias,
                        child: InkWell(
                          onTap: () => apply(i),
                          child: SizedBox(
                            height: 44,
                            child: Center(
                              child: level == i
                                  ? Icon(
                                      Icons.check_rounded,
                                      color: cs.onPrimary,
                                      size: 22,
                                    )
                                  : const SizedBox.shrink(),
                            ),
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
              ),
          ],
        );
      },
    );
  }
}

/// Parity with iOS `ProgressTabView` — **Learn** | **Stats** segments.
class ProgressScreen extends ConsumerWidget {
  const ProgressScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final segment = ref.watch(progressSegmentProvider);
    final cs = Theme.of(context).colorScheme;
    final l10n = context.l10n;

    return Scaffold(
      backgroundColor: Theme.of(context).colorScheme.surfaceContainerLowest,
      appBar: AppBar(
        title: Text(l10n.progressAppBarTitle),
        centerTitle: false,
        actions: [
          IconButton(
            tooltip: l10n.progressProfileIconTooltip,
            icon: const Icon(Icons.person_outline),
            onPressed: () => Navigator.push<void>(
              context,
              MaterialPageRoute<void>(
                builder: (_) => const UserProfileScreen(),
              ),
            ),
          ),
        ],
      ),
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
            child: Column(
              children: [
                SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: SegmentedButton<int>(
                    segments: [
                      ButtonSegment(
                          value: 0,
                          label: Text(l10n.progressTabLearn),
                          icon: const Icon(Icons.school, size: 18)),
                      ButtonSegment(
                          value: 1,
                          label: Text(l10n.progressTabStats),
                          icon: const Icon(Icons.bar_chart, size: 18)),
                    ],
                    selected: {segment},
                    onSelectionChanged: (Set<int> s) {
                      if (s.isEmpty) return;
                      ref.read(progressSegmentProvider.notifier).state =
                          s.first;
                    },
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  segment == 0
                      ? l10n.progressLearnCardSubtitle
                      : l10n.progressStatsSegmentSubtitle,
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: cs.onSurfaceVariant),
                  textAlign: TextAlign.center,
                ),
              ],
            ),
          ),
          Expanded(
            child: segment == 0 ? _LearnBody() : _StatsBody(),
          ),
        ],
      ),
    );
  }
}

class _LearnBody extends ConsumerWidget {
  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;
    final cs = Theme.of(context).colorScheme;
    final l10n = context.l10n;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _SectionHeader(
            title: l10n.progressLearnCardTitle,
            subtitle: l10n.progressLearnCardSubtitle,
            icon: Icons.school),
        Card(
          elevation: 0,
          color: cs.surfaceContainerHigh,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    CircleAvatar(
                      backgroundColor: cs.primaryContainer,
                      child: Icon(Icons.workspace_premium, color: cs.primary),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(l10n.progressAiCoachTitle,
                              style: Theme.of(context).textTheme.titleMedium),
                          Text(
                            l10n.progressAiCoachBody,
                            style: Theme.of(context)
                                .textTheme
                                .bodySmall
                                ?.copyWith(color: cs.onSurfaceVariant),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                SwitchListTile(
                  contentPadding: EdgeInsets.zero,
                  title: Text(l10n.progressLearningModeTile),
                  value: ui.learningMode,
                  onChanged: (v) => uiN.setLearningMode(v),
                ),
                Text(l10n.progressCoachLevelLabel,
                    style: Theme.of(context).textTheme.labelLarge),
                const SizedBox(height: 6),
                const _CoachLevelRow(),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: PressableScale(
                        child: FilledButton.icon(
                          onPressed: ui.learningMode
                              ? () => Navigator.of(context).push(
                                    MaterialPageRoute<void>(
                                        builder: (_) => const CoachScreen()),
                                  )
                              : null,
                          icon: const Icon(Icons.chat_bubble_outline),
                          label: Text(l10n.progressCoachChatButton),
                        ),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: OutlinedButton.icon(
                        onPressed: (ui.learningMode && session.snapshot != null)
                            ? () => Navigator.of(context).push(
                                  MaterialPageRoute<void>(
                                      builder: (_) => const CoachScreen()),
                                )
                            : null,
                        icon: const Icon(Icons.center_focus_strong_outlined),
                        label: Text(l10n.progressPositionPlanButton),
                      ),
                    ),
                  ],
                ),
                if (!ui.learningMode)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Text(
                      l10n.progressLearningModePlayHint,
                      style: Theme.of(context)
                          .textTheme
                          .bodySmall
                          ?.copyWith(color: cs.outline),
                    ),
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 16),
        if (session.lastError != null)
          SessionErrorPanel(
            error: session.lastError!,
            title: l10n.progressBoardErrorTitle,
            devMode: devMode,
          ),
        Card(
          elevation: 0,
          color: cs.surfaceContainerHigh,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(l10n.progressStartingPositionTitle,
                    style: Theme.of(context).textTheme.titleSmall),
                const SizedBox(height: 6),
                Text(
                  l10n.progressStartingPositionTeachingBody,
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: cs.onSurfaceVariant),
                ),
                const SizedBox(height: 12),
                PressableScale(
                  child: FilledButton.icon(
                    onPressed: _supportsRemoteBoardCommands(session)
                        ? () {
                            Navigator.of(context).push<void>(
                              MaterialPageRoute<void>(
                                fullscreenDialog: true,
                                builder: (_) => const BoardSetupWizardScreen(
                                    kind: BoardSetupWizardKind.standardStart),
                              ),
                            );
                          }
                        : null,
                    icon: const Icon(Icons.auto_fix_high),
                    label: Text(l10n.progressRunWizardStarting),
                  ),
                ),
                if (!_supportsRemoteBoardCommands(session))
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Text(
                      l10n.progressWizardConnectHint,
                      style: Theme.of(context)
                          .textTheme
                          .bodySmall
                          ?.copyWith(color: cs.outline),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _StatsBody extends ConsumerWidget {
  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final cs = Theme.of(context).colorScheme;
    final l10n = context.l10n;
    final peak = prefs.statsPeakMoveCount;
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _SectionHeader(
            title: l10n.progressAccountCardTitle,
            subtitle: l10n.progressAccountCardSubtitle,
            icon: Icons.analytics_outlined),
        Card(
          elevation: 0,
          color: cs.surfaceContainerHigh,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                _LabeledRow(l10n.progressStatsCurrentMoves,
                    '${session.snapshot?.status.moveCount ?? 0}'),
                const Divider(),
                _LabeledRow(l10n.progressStatsPeakMoves, '$peak'),
                const Divider(),
                _LabeledRow(l10n.progressStatsPollSuccess,
                    '${session.pollSuccessCount}'),
                const Divider(),
                _LabeledRow(
                    l10n.progressStatsPollFail, '${session.pollFailureCount}'),
                const Divider(),
                _LabeledRow(
                    l10n.progressStatsWsMessages, '${session.wsMessageCount}'),
              ],
            ),
          ),
        ),
        const SizedBox(height: 16),
        if (session.pollingActive || session.transport != BoardTransport.none)
          Card(
            elevation: 0,
            color: cs.surfaceContainerHigh,
            child: ListTile(
              leading: const Icon(Icons.link),
              title: Text(l10n.progressActiveTransportTitle),
              subtitle: Text(
                devMode
                    ? l10n.progressTransportDevDetail(
                        _transportLabel(l10n, session),
                        session.pollingActive
                            ? l10n.progressShortOn
                            : l10n.progressShortOff,
                        session.webSocketActive
                            ? l10n.progressShortOn
                            : l10n.progressShortOff,
                      )
                    : l10n.progressTransportUserDetail(
                        _transportLabel(l10n, session),
                        session.snapshot != null
                            ? l10n.connDiagActive
                            : l10n.progressWaitingForData,
                      ),
              ),
            ),
          )
        else
          Card(
            elevation: 0,
            color: cs.surfaceContainerHighest,
            child: ListTile(
              leading: const Icon(Icons.info_outline),
              title: Text(l10n.progressNoSessionTitle),
              subtitle: Text(l10n.progressStatsEmptySubtitle),
            ),
          ),
      ],
    );
  }
}

class _SectionHeader extends StatelessWidget {
  const _SectionHeader({
    required this.title,
    required this.subtitle,
    required this.icon,
  });
  final String title;
  final String subtitle;
  final IconData icon;

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: cs.primary, size: 28),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title,
                    style: Theme.of(context)
                        .textTheme
                        .titleMedium
                        ?.copyWith(fontWeight: FontWeight.w600)),
                Text(subtitle,
                    style: Theme.of(context)
                        .textTheme
                        .bodySmall
                        ?.copyWith(color: cs.onSurfaceVariant)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _LabeledRow extends StatelessWidget {
  const _LabeledRow(this.label, this.value);
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Expanded(
            child: Text(label, style: Theme.of(context).textTheme.bodyMedium)),
        Text(value,
            style: Theme.of(context)
                .textTheme
                .bodyLarge
                ?.copyWith(fontWeight: FontWeight.w600)),
      ],
    );
  }
}
