import 'package:flutter/material.dart';

import '../theme/app_theme.dart';

/// One rounded group on the graphite surface. No cards inside: rows are
/// separated by hairlines. The optional title is small and muted.
class QuietGroup extends StatelessWidget {
  const QuietGroup({super.key, this.title, required this.children});

  final String? title;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    return Padding(
      padding: const EdgeInsets.only(top: 20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (title != null)
            Semantics(
              header: true,
              child: Padding(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
                child: Text(
                  title!,
                  style: theme.textTheme.labelLarge?.copyWith(
                    color: cs.onSurfaceVariant,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
            ),
          Material(
            color: cs.surfaceContainer,
            borderRadius: BorderRadius.circular(AppTheme.radius + 4),
            clipBehavior: Clip.antiAlias,
            child: Column(
              children: [
                for (var i = 0; i < children.length; i++) ...[
                  if (i > 0) const Divider(indent: 56),
                  children[i],
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// Single-line row: icon, title, optional muted value on the right, chevron.
class QuietRow extends StatelessWidget {
  const QuietRow({
    super.key,
    required this.icon,
    required this.title,
    this.value,
    this.onTap,
    this.destructive = false,
    this.badge = false,
    this.trailing,
  });

  final IconData icon;
  final String title;
  final String? value;
  final VoidCallback? onTap;
  final bool destructive;
  final bool badge;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final enabled = onTap != null || trailing != null;
    final fg = destructive ? cs.error : cs.onSurface;
    final iconColor = destructive ? cs.error : cs.onSurfaceVariant;
    return ListTile(
      enabled: enabled,
      minTileHeight: 52,
      leading: Icon(icon, color: enabled ? iconColor : null),
      title: Text(
        title,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: theme.textTheme.bodyLarge?.copyWith(
          color: enabled ? fg : cs.onSurface.withValues(alpha: 0.38),
        ),
      ),
      trailing: trailing ??
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (value != null)
                ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 140),
                  child: Text(
                    value!,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.bodyMedium
                        ?.copyWith(color: cs.onSurfaceVariant),
                  ),
                ),
              if (badge) ...[
                const SizedBox(width: 8),
                Container(
                  width: 8,
                  height: 8,
                  decoration:
                      BoxDecoration(color: cs.error, shape: BoxShape.circle),
                ),
              ],
              if (!destructive && onTap != null) ...[
                const SizedBox(width: 4),
                Icon(Icons.chevron_right_rounded, color: cs.onSurfaceVariant),
              ],
            ],
          ),
      onTap: onTap,
    );
  }
}
