import 'package:flutter/material.dart';

import '../localization/context_l10n.dart';
import '../utils/user_facing_error_message.dart';

/// Red banner with readable copy; optional technical text for developer mode.
class SessionErrorPanel extends StatefulWidget {
  const SessionErrorPanel({
    super.key,
    required this.error,
    required this.title,
    required this.devMode,
    this.compact = false,
  });

  final Object error;
  final String title;
  final bool devMode;

  /// Narrow strip for overlays — avoids pushing large panels through layout.
  final bool compact;

  @override
  State<SessionErrorPanel> createState() => _SessionErrorPanelState();
}

class _SessionErrorPanelState extends State<SessionErrorPanel> {
  bool _showTechnical = false;

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final summary = userFacingErrorSummary(l10n, widget.error);
    final technical = widget.error.toString();
    final hasExtraTechnical =
        widget.devMode && technical.trim().isNotEmpty && technical != summary;

    if (widget.compact) {
      return Material(
        color: cs.errorContainer,
        elevation: 3,
        shadowColor: Colors.black38,
        borderRadius: BorderRadius.circular(10),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Icon(Icons.warning_amber_rounded,
                  color: cs.onErrorContainer, size: 18),
              const SizedBox(width: 8),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      widget.title,
                      style: TextStyle(
                        color: cs.onErrorContainer,
                        fontWeight: FontWeight.w600,
                        fontSize: 13,
                      ),
                    ),
                    if (summary.isNotEmpty)
                      Text(
                        summary,
                        maxLines: 4,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: cs.onErrorContainer,
                              height: 1.25,
                              fontSize: 12.5,
                            ),
                      ),
                    if (hasExtraTechnical)
                      TextButton(
                        style: TextButton.styleFrom(
                          padding: EdgeInsets.zero,
                          minimumSize: Size.zero,
                          tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                          foregroundColor: cs.onErrorContainer,
                        ),
                        onPressed: () =>
                            setState(() => _showTechnical = !_showTechnical),
                        child: Text(
                          _showTechnical
                              ? l10n.userFacingHideTechnicalDetails
                              : l10n.userFacingShowTechnicalDetails,
                          style: const TextStyle(fontSize: 12),
                        ),
                      ),
                    if (hasExtraTechnical && _showTechnical)
                      SelectableText(
                        technical,
                        style: TextStyle(
                          fontFamily: 'monospace',
                          fontSize: 10,
                          height: 1.3,
                          color: cs.onErrorContainer,
                        ),
                      ),
                  ],
                ),
              ),
            ],
          ),
        ),
      );
    }

    return Material(
      color: cs.errorContainer,
      borderRadius: BorderRadius.circular(12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Padding(
              padding: const EdgeInsets.only(top: 2),
              child: Icon(
                Icons.warning_amber_rounded,
                color: cs.onErrorContainer,
                size: 22,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    widget.title,
                    style: TextStyle(
                      color: cs.onErrorContainer,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  if (summary.isNotEmpty) ...[
                    const SizedBox(height: 6),
                    Text(
                      summary,
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                            color: cs.onErrorContainer,
                            height: 1.35,
                          ),
                    ),
                  ],
                  if (hasExtraTechnical) ...[
                    const SizedBox(height: 8),
                    TextButton(
                      style: TextButton.styleFrom(
                        padding: EdgeInsets.zero,
                        minimumSize: Size.zero,
                        tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                        foregroundColor: cs.onErrorContainer,
                      ),
                      onPressed: () =>
                          setState(() => _showTechnical = !_showTechnical),
                      child: Text(
                        _showTechnical
                            ? l10n.userFacingHideTechnicalDetails
                            : l10n.userFacingShowTechnicalDetails,
                      ),
                    ),
                    if (_showTechnical)
                      Padding(
                        padding: const EdgeInsets.only(top: 6),
                        child: SelectableText(
                          technical,
                          style: TextStyle(
                            fontFamily: 'monospace',
                            fontSize: 11,
                            height: 1.35,
                            color: cs.onErrorContainer,
                          ),
                        ),
                      ),
                  ],
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
