import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';

/// Full label for dropdown / menus (matches Settings wording).
String coachLevelDropdownItemLabel(AppLocalizations l, int level) {
  return switch (level.clamp(1, 4)) {
    1 => l.settingsCoachLevelBeginner,
    2 => l.settingsCoachLevelIntermediate,
    3 => l.settingsCoachLevelAdvanced,
    _ => l.settingsCoachLevelExpert,
  };
}

String coachLevelTooltip(AppLocalizations l, int level) {
  return switch (level) {
    1 => l.progressLevelBeginner,
    2 => l.progressLevelIntermediate,
    3 => l.progressLevelAdvanced,
    4 => l.progressLevelExpert,
    _ => l.progressLevelNumber(level),
  };
}

bool _coachLevelSegmentedLabelsFit(
  BuildContext context,
  double rowWidth,
  List<String> labels,
) {
  if (rowWidth <= 0) return false;
  final style = Theme.of(context).textTheme.labelLarge ??
      const TextStyle(fontSize: 14, fontWeight: FontWeight.w500);
  final scaler = MediaQuery.textScalerOf(context);
  var maxLabel = 0.0;
  for (final label in labels) {
    final tp = TextPainter(
      text: TextSpan(text: label, style: style),
      textDirection: TextDirection.ltr,
      textScaler: scaler,
    )..layout(maxWidth: double.infinity);
    maxLabel = math.max(maxLabel, tp.width);
  }
  const insetPaddingDividerBudget = 28.0;
  return (rowWidth / 4) >= maxLabel + insetPaddingDividerBudget;
}

/// Compact messenger-style level picker for coach chat composer.
class CoachLevelDropdown extends ConsumerWidget {
  const CoachLevelDropdown({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final level = prefs.coachUserLevel.clamp(1, 4);
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;

    return Material(
      color: cs.surfaceContainerHighest.withValues(alpha: 0.65),
      borderRadius: BorderRadius.circular(12),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10),
        child: DropdownButtonHideUnderline(
          child: DropdownButton<int>(
            value: level,
            isExpanded: true,
            borderRadius: BorderRadius.circular(12),
            items: [
              for (var i = 1; i <= 4; i++)
                DropdownMenuItem<int>(
                  value: i,
                  child: Text(coachLevelDropdownItemLabel(l10n, i)),
                ),
            ],
            onChanged: (v) async {
              if (v == null) return;
              await ref.read(prefsRepositoryProvider).setCoachUserLevel(v);
              ref.invalidate(prefsRepositoryProvider);
            },
          ),
        ),
      ),
    );
  }
}

/// Segmented coach explanation level (1–4). Shared by Progress learn steps.
class CoachExplanationLevelBar extends ConsumerWidget {
  const CoachExplanationLevelBar({super.key});

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
                value: 2,
                label: Text(l10n.progressSegIntermediate),
              ),
              ButtonSegment(value: 3, label: Text(l10n.progressSegAdvanced)),
              ButtonSegment(value: 4, label: Text(l10n.progressSegExpert)),
            ],
            selected: {level},
            onSelectionChanged: (Set<int> s) async {
              if (s.isEmpty) return;
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
                    message: coachLevelTooltip(l10n, i),
                    child: Semantics(
                      button: true,
                      label: coachLevelTooltip(l10n, i),
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
                                  : Text(
                                      '$i',
                                      style: Theme.of(context)
                                          .textTheme
                                          .labelLarge
                                          ?.copyWith(
                                            color: cs.onSurfaceVariant,
                                            fontWeight: FontWeight.w600,
                                          ),
                                    ),
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
