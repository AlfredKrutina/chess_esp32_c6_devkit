import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
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

String _transportLabel(BoardSessionState session) {
  return switch (session.transport) {
    BoardTransport.none => 'Disconnected',
    BoardTransport.mock => 'Demo mode',
    BoardTransport.wifi => 'Wi‑Fi',
    BoardTransport.ble => 'Bluetooth',
  };
}

String _coachLevelTooltip(int level) {
  return switch (level) {
    1 => 'Beginner — začátečník',
    2 => 'Intermediate — středně pokročilý',
    3 => 'Advanced — pokročilý',
    4 => 'Expert — expert',
    _ => 'Úroveň $level',
  };
}

/// True when four labeled segments would fit without wrapping (níže jen ✓).
bool _coachLevelSegmentedLabelsFit(BuildContext context, double rowWidth) {
  if (rowWidth <= 0) return false;
  final style = Theme.of(context).textTheme.labelLarge ??
      const TextStyle(fontSize: 14, fontWeight: FontWeight.w500);
  final scaler = MediaQuery.textScalerOf(context);
  const labels = ['Beg.', 'Int.', 'Adv.', 'Exp.'];
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

    Future<void> apply(int v) async {
      await ref.read(prefsRepositoryProvider).setCoachUserLevel(v);
      ref.invalidate(prefsRepositoryProvider);
    }

    return LayoutBuilder(
      builder: (context, bc) {
        final compact =
            !_coachLevelSegmentedLabelsFit(context, bc.maxWidth);
        if (!compact) {
          return SegmentedButton<int>(
            segments: const [
              ButtonSegment(value: 1, label: Text('Beg.')),
              ButtonSegment(value: 2, label: Text('Int.')),
              ButtonSegment(value: 3, label: Text('Adv.')),
              ButtonSegment(value: 4, label: Text('Exp.')),
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
                    message: _coachLevelTooltip(i),
                    child: Semantics(
                      button: true,
                      label: _coachLevelTooltip(i),
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
    final prefs = ref.watch(prefsRepositoryProvider);
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: Theme.of(context).colorScheme.surfaceContainerLowest,
      appBar: AppBar(
        title: const Text('Progress'),
        centerTitle: false,
        actions: [
          IconButton(
            tooltip: 'Profile & Elo',
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
                    segments: const [
                      ButtonSegment(
                          value: 0,
                          label: Text('Learn'),
                          icon: Icon(Icons.school, size: 18)),
                      ButtonSegment(
                          value: 1,
                          label: Text('Stats'),
                          icon: Icon(Icons.bar_chart, size: 18)),
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
                      ? 'Board wizards and AI coach'
                      : 'Sessions and in-app stats',
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: cs.onSurfaceVariant),
                  textAlign: TextAlign.center,
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 10),
            child: Material(
              color: cs.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(14),
              clipBehavior: Clip.antiAlias,
              child: ListTile(
                leading: CircleAvatar(
                  backgroundColor: cs.primaryContainer,
                  child: Icon(Icons.person_rounded,
                      color: cs.onPrimaryContainer),
                ),
                title: const Text('Profile & puzzle Elo'),
                subtitle: Text(
                  '${prefs.profileDisplayName} · Elo ${prefs.puzzleElo}',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                trailing: const Icon(Icons.chevron_right),
                onTap: () => Navigator.push<void>(
                  context,
                  MaterialPageRoute<void>(
                    builder: (_) => const UserProfileScreen(),
                  ),
                ),
              ),
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
    final cs = Theme.of(context).colorScheme;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const _SectionHeader(
            title: 'Learn',
            subtitle: 'LED setup wizards and AI coach',
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
                          Text('AI coach',
                              style: Theme.of(context).textTheme.titleMedium),
                          Text(
                            'Turn on learning mode on Play. Chat uses the cloud API or a local fallback; Stockfish stays on Analysis.',
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
                  title: const Text('Learning mode'),
                  value: ui.learningMode,
                  onChanged: (v) => uiN.setLearningMode(v),
                ),
                Text('Coach level',
                    style: Theme.of(context).textTheme.labelLarge),
                const SizedBox(height: 6),
                const _CoachLevelRow(),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: FilledButton.icon(
                        onPressed: ui.learningMode
                            ? () => Navigator.of(context).push(
                                  MaterialPageRoute<void>(
                                      builder: (_) => const CoachScreen()),
                                )
                            : null,
                        icon: const Icon(Icons.chat_bubble_outline),
                        label: const Text('Coach chat'),
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
                        label: const Text('Position plan'),
                      ),
                    ),
                  ],
                ),
                if (!ui.learningMode)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Text(
                      'Enable learning mode on Play → Game controls to use chat and position plan.',
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
          Card(
            color: cs.errorContainer,
            child: ListTile(
              leading: Icon(Icons.error_outline, color: cs.onErrorContainer),
              title: Text('Board error',
                  style: TextStyle(
                      color: cs.onErrorContainer, fontWeight: FontWeight.w600)),
              subtitle: Text('${session.lastError}',
                  style: TextStyle(color: cs.onErrorContainer)),
            ),
          ),
        Card(
          elevation: 0,
          color: cs.surfaceContainerHigh,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('Starting position',
                    style: Theme.of(context).textTheme.titleSmall),
                const SizedBox(height: 6),
                Text(
                  'Teaching mode: LEDs show each square in order; the app shows which piece to place (same flow as iOS).',
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: cs.onSurfaceVariant),
                ),
                const SizedBox(height: 12),
                FilledButton.icon(
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
                  icon: const Icon(Icons.grid_3x3),
                  label: const Text('Run wizard — starting position'),
                ),
                if (!_supportsRemoteBoardCommands(session))
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Text(
                      'Connect the board for LED wizards (connection chip on the Play tab).',
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
    final peak = prefs.statsPeakMoveCount;
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const _SectionHeader(
            title: 'Account',
            subtitle: 'Local profile and activity',
            icon: Icons.analytics_outlined),
        Card(
          elevation: 0,
          color: cs.surfaceContainerHigh,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                _LabeledRow('Current move count',
                    '${session.snapshot?.status.moveCount ?? 0}'),
                const Divider(),
                _LabeledRow('Peak move count seen', '$peak'),
                const Divider(),
                _LabeledRow(
                    'Successful HTTP polls', '${session.pollSuccessCount}'),
                const Divider(),
                _LabeledRow('Failed polls', '${session.pollFailureCount}'),
                const Divider(),
                _LabeledRow('WebSocket messages', '${session.wsMessageCount}'),
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
              title: const Text('Active transport'),
              subtitle: Text(
                devMode
                    ? '${_transportLabel(session)} · polling ${session.pollingActive ? "on" : "off"} · WS ${session.webSocketActive ? "on" : "off"}'
                    : '${_transportLabel(session)} · live sync ${session.snapshot != null ? "active" : "waiting for data"}',
              ),
            ),
          )
        else
          Card(
            elevation: 0,
            color: cs.surfaceContainerHighest,
            child: const ListTile(
              leading: Icon(Icons.info_outline),
              title: Text('No active session yet'),
              subtitle: Text(
                  'Connect the board on Play — stats fill in once polling runs.'),
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
