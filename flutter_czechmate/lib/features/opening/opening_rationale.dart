import 'package:flutter/material.dart';

import 'opening_catalog_repository.dart';

class OpeningRationale {
  const OpeningRationale({
    this.summaryCs,
    this.summaryEn,
    required this.whyThisLineCs,
    required this.whyThisLineEn,
    required this.insteadOfCs,
    required this.insteadOfEn,
    this.whenToPlayCs,
    this.whenToPlayEn,
    this.relatedLineIds = const [],
  });

  final String? summaryCs;
  final String? summaryEn;
  final String whyThisLineCs;
  final String whyThisLineEn;
  final String insteadOfCs;
  final String insteadOfEn;
  final String? whenToPlayCs;
  final String? whenToPlayEn;
  final List<String> relatedLineIds;

  bool get hasSummary =>
      (summaryCs != null && summaryCs!.isNotEmpty) ||
      (summaryEn != null && summaryEn!.isNotEmpty);

  String? summaryForLocale(String locale) {
    final cs = summaryCs;
    final en = summaryEn;
    if (locale.startsWith('cs')) {
      return cs?.isNotEmpty == true ? cs : en;
    }
    return en?.isNotEmpty == true ? en : cs;
  }

  String whyThisLineForLocale(String locale) =>
      locale.startsWith('cs') ? whyThisLineCs : whyThisLineEn;

  String insteadOfForLocale(String locale) =>
      locale.startsWith('cs') ? insteadOfCs : insteadOfEn;

  String? whenToPlayForLocale(String locale) {
    final cs = whenToPlayCs;
    final en = whenToPlayEn;
    if (locale.startsWith('cs')) {
      return cs?.isNotEmpty == true ? cs : en;
    }
    return en?.isNotEmpty == true ? en : cs;
  }

  factory OpeningRationale.fromJson(Map<String, dynamic> json) {
    Map<String, dynamic>? loc(String key) =>
        json[key] as Map<String, dynamic>?;

    final summary = loc('summary');
    final why = loc('why_this_line') ?? const {};
    final instead = loc('instead_of') ?? const {};
    final when = loc('when_to_play');

    return OpeningRationale(
      summaryCs: summary?['cs'] as String?,
      summaryEn: summary?['en'] as String?,
      whyThisLineCs: why['cs'] as String? ?? '',
      whyThisLineEn: why['en'] as String? ?? '',
      insteadOfCs: instead['cs'] as String? ?? '',
      insteadOfEn: instead['en'] as String? ?? '',
      whenToPlayCs: when?['cs'] as String?,
      whenToPlayEn: when?['en'] as String?,
      relatedLineIds:
          (json['related_line_ids'] as List<dynamic>? ?? []).cast<String>(),
    );
  }
}

class OpeningRationalePanel extends StatelessWidget {
  const OpeningRationalePanel({
    super.key,
    required this.rationale,
    required this.locale,
    this.linesById = const {},
    this.compact = false,
    this.onRelatedTap,
  });

  final OpeningRationale rationale;
  final String locale;
  final Map<String, OpeningLine> linesById;
  final bool compact;
  final void Function(String lineId)? onRelatedTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final cs = locale.startsWith('cs');
    final when = rationale.whenToPlayForLocale(locale);

    return Card(
      margin: EdgeInsets.zero,
      color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.45),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  Icons.lightbulb_outline,
                  size: 18,
                  color: theme.colorScheme.primary,
                ),
                const SizedBox(width: 6),
                Text(
                  cs ? 'Proč tato varianta?' : 'Why this line?',
                  style: theme.textTheme.titleSmall?.copyWith(
                    color: theme.colorScheme.primary,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            _section(
              context,
              cs ? 'Proč učíme tuto linii' : 'Why we teach this line',
              rationale.whyThisLineForLocale(locale),
            ),
            if (!compact) ...[
              const SizedBox(height: 8),
              _section(
                context,
                cs ? 'Místo čeho' : 'Instead of',
                rationale.insteadOfForLocale(locale),
              ),
              if (when != null && when.isNotEmpty) ...[
                const SizedBox(height: 8),
                _section(
                  context,
                  cs ? 'Kdy hrát' : 'When to play',
                  when,
                ),
              ],
              if (rationale.relatedLineIds.isNotEmpty) ...[
                const SizedBox(height: 8),
                Text(
                  cs ? 'Související linie' : 'Related lines',
                  style: theme.textTheme.labelMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 4),
                Wrap(
                  spacing: 6,
                  runSpacing: 4,
                  children: [
                    for (final id in rationale.relatedLineIds)
                      _relatedChip(context, id),
                  ],
                ),
              ],
            ],
          ],
        ),
      ),
    );
  }

  Widget _relatedChip(BuildContext context, String lineId) {
    final line = linesById[lineId];
    final label = line?.nameForLocale(locale) ?? lineId;
    if (onRelatedTap == null) {
      return Chip(
        label: Text(label),
        visualDensity: VisualDensity.compact,
        materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
      );
    }
    return ActionChip(
      label: Text(label),
      visualDensity: VisualDensity.compact,
      materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
      onPressed: () => onRelatedTap!(lineId),
    );
  }

  Widget _section(BuildContext context, String title, String body) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          title,
          style: theme.textTheme.labelMedium?.copyWith(
            fontWeight: FontWeight.w600,
          ),
        ),
        const SizedBox(height: 2),
        Text(body, style: theme.textTheme.bodySmall),
      ],
    );
  }
}
