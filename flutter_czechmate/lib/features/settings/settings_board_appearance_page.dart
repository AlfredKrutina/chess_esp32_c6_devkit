import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/constants/board_styles.dart';
import '../../core/localization/context_l10n.dart';
import '../game/state/game_ui_notifier.dart';

class SettingsBoardAppearancePage extends ConsumerWidget {
  const SettingsBoardAppearancePage({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ui = ref.watch(gameUiNotifierProvider);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final style = offeredBoardStyle(ui.boardStyleRaw);

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsBoardScreenTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
        children: [
          Text(
            l10n.settingsSquareColors,
            style: theme.textTheme.labelLarge
                ?.copyWith(fontWeight: FontWeight.w600),
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: SegmentedButton<String>(
              showSelectedIcon: false,
              segments: [
                ButtonSegment(
                  value: 'wooden',
                  label: Text(l10n.settingsBoardStyleWood),
                  icon: const _Swatch(style: 'wooden'),
                ),
                ButtonSegment(
                  value: 'slate',
                  label: Text(l10n.settingsBoardStyleSlate),
                  icon: const _Swatch(style: 'slate'),
                ),
              ],
              selected: {style},
              onSelectionChanged: (s) {
                if (s.isNotEmpty) uiN.setBoardStyle(s.first);
              },
            ),
          ),
          const SizedBox(height: 12),
          Text(
            l10n.settingsBoardCoordinatesNote,
            style: theme.textTheme.bodySmall
                ?.copyWith(color: cs.onSurfaceVariant),
          ),
          const SizedBox(height: 16),
          const Divider(),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.moreSwapSides),
            subtitle:
                Text(ui.boardFlipped ? l10n.moreBlackBottom : l10n.moreWhiteBottom),
            value: ui.boardFlipped,
            onChanged: (_) => uiN.toggleFlip(),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsPieceMotionTitle),
            subtitle: Text(l10n.settingsPieceMotionSubtitle),
            value: ui.moveAnimationsEnabled,
            onChanged: (_) => uiN.toggleMoveAnimations(),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsRemoteMovesTitle),
            subtitle: Text(l10n.settingsRemoteMovesSubtitle),
            value: ui.remoteMovesEnabled,
            onChanged: (_) => uiN.toggleRemoteMoves(),
          ),
        ],
      ),
    );
  }
}

class _Swatch extends StatelessWidget {
  const _Swatch({required this.style});

  final String style;

  @override
  Widget build(BuildContext context) {
    final c = BoardStyleColors.fromRaw(style);
    return ExcludeSemantics(
      child: ClipRRect(
        borderRadius: BorderRadius.circular(3),
        child: SizedBox(
          width: 18,
          height: 18,
          child: Column(
            children: [
              Expanded(
                child: Row(children: [
                  Expanded(child: ColoredBox(color: c.light)),
                  Expanded(child: ColoredBox(color: c.dark)),
                ]),
              ),
              Expanded(
                child: Row(children: [
                  Expanded(child: ColoredBox(color: c.dark)),
                  Expanded(child: ColoredBox(color: c.light)),
                ]),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
