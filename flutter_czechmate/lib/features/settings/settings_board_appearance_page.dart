import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/localization/context_l10n.dart';
import '../game/state/game_ui_notifier.dart';

class SettingsBoardAppearancePage extends ConsumerStatefulWidget {
  const SettingsBoardAppearancePage({super.key});

  @override
  ConsumerState<SettingsBoardAppearancePage> createState() =>
      _SettingsBoardAppearancePageState();
}

class _SettingsBoardAppearancePageState
    extends ConsumerState<SettingsBoardAppearancePage> {
  @override
  Widget build(BuildContext context) {
    final ui = ref.watch(gameUiNotifierProvider);
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsBoardAppearanceTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
        children: [
          Text(
            l10n.settingsPlayTabGamePanel,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 14),
          Text(
            l10n.settingsLayoutLabel,
            style: Theme.of(context).textTheme.labelLarge?.copyWith(
                  fontWeight: FontWeight.w600,
                ),
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: SegmentedButton<String>(
              showSelectedIcon: false,
              segments: [
                ButtonSegment<String>(
                  value: 'boardOnly',
                  label: Text(l10n.settingsLayoutBoardSegment),
                  tooltip: l10n.settingsLayoutBoardTooltip,
                ),
                ButtonSegment<String>(
                  value: 'standard',
                  label: Text(l10n.settingsLayoutFullSegment),
                  tooltip: l10n.settingsLayoutFullTooltip,
                ),
              ],
              selected: {ui.layoutMode},
              onSelectionChanged: (s) {
                if (s.isEmpty) return;
                ref
                    .read(gameUiNotifierProvider.notifier)
                    .setLayoutMode(s.first);
              },
            ),
          ),
          const SizedBox(height: 14),
          Row(
            crossAxisAlignment: CrossAxisAlignment.baseline,
            textBaseline: TextBaseline.alphabetic,
            children: [
              Text(
                l10n.settingsBoardZoom,
                style: Theme.of(context).textTheme.titleSmall,
              ),
              const Spacer(),
              Text(
                '${(ui.boardZoomStorage * 100).round()}%',
                style: Theme.of(context).textTheme.titleSmall?.copyWith(
                  color: cs.primary,
                  fontFeatures: const [FontFeature.tabularFigures()],
                ),
              ),
            ],
          ),
          Slider(
            min: 0.7,
            max: 1.5,
            divisions: 16,
            value: ui.boardZoomStorage.clamp(0.7, 1.5),
            onChanged: ref.read(gameUiNotifierProvider.notifier).setBoardZoom,
          ),
          const SizedBox(height: 4),
          Text(
            l10n.settingsSquareColors,
            style: Theme.of(context).textTheme.labelLarge?.copyWith(
                  fontWeight: FontWeight.w600,
                ),
          ),
          const SizedBox(height: 8),
          DropdownButtonFormField<String>(
            isExpanded: true,
            initialValue: ui.boardStyleRaw,
            decoration: InputDecoration(
              labelText: l10n.settingsBoardThemeLabel,
              border: const OutlineInputBorder(),
            ),
            items: [
              DropdownMenuItem(
                  value: 'wooden', child: Text(l10n.boardStyleWooden)),
              DropdownMenuItem(
                  value: 'modernDark', child: Text(l10n.boardStyleModernDark)),
              DropdownMenuItem(
                  value: 'iceBlue', child: Text(l10n.boardStyleIceBlue)),
              DropdownMenuItem(
                  value: 'forestGreen',
                  child: Text(l10n.boardStyleForestGreen)),
              DropdownMenuItem(
                  value: 'marbleGray', child: Text(l10n.boardStyleMarbleGray)),
              DropdownMenuItem(
                  value: 'midnight', child: Text(l10n.boardStyleMidnight)),
              DropdownMenuItem(
                  value: 'slate', child: Text(l10n.boardStyleSlate)),
              DropdownMenuItem(
                  value: 'coral', child: Text(l10n.boardStyleCoral)),
            ],
            onChanged: (v) {
              if (v != null) {
                ref.read(gameUiNotifierProvider.notifier).setBoardStyle(v);
              }
            },
          ),
          const SizedBox(height: 12),
          Text(
            l10n.settingsBoardOptions,
            style: Theme.of(context).textTheme.labelLarge?.copyWith(
                  fontWeight: FontWeight.w600,
                ),
          ),
          const SizedBox(height: 4),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsCoordinatesTitle),
            subtitle: Text(l10n.settingsCoordinatesSubtitle),
            value: ui.showCoordinates,
            onChanged: (_) =>
                ref.read(gameUiNotifierProvider.notifier).toggleCoordinates(),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsPieceMotionTitle),
            subtitle: Text(l10n.settingsPieceMotionSubtitle),
            value: ui.moveAnimationsEnabled,
            onChanged: (_) => ref
                .read(gameUiNotifierProvider.notifier)
                .toggleMoveAnimations(),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsFlipBoardTitle),
            value: ui.boardFlipped,
            onChanged: (_) =>
                ref.read(gameUiNotifierProvider.notifier).toggleFlipped(),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsRemoteMovesTitle),
            subtitle: Text(l10n.settingsRemoteMovesSubtitle),
            value: ui.remoteMovesEnabled,
            onChanged: (_) =>
                ref.read(gameUiNotifierProvider.notifier).toggleRemoteMoves(),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: Text(l10n.settingsLiveEvalTitle),
            subtitle: Text(l10n.settingsLiveEvalSubtitle),
            value: ui.moveEvaluationEnabled,
            onChanged: (v) => ref
                .read(gameUiNotifierProvider.notifier)
                .setMoveEvaluationEnabled(v),
          ),
          const SizedBox(height: 12),
          OutlinedButton(
            onPressed: () async {
              await ref
                  .read(gameUiNotifierProvider.notifier)
                  .resetBoardDisplayDefaults();
              if (context.mounted) setState(() {});
            },
            child: Text(l10n.settingsResetBoardDisplay),
          ),
        ],
      ),
    );
  }
}
