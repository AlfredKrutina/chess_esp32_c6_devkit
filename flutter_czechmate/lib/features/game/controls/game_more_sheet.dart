import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_navigation.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/theme/app_motion.dart';
import '../../../core/utils/board_action_feedback.dart';
import '../../../core/widgets/app_modal_sheet.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../history/move_history_view.dart';
import '../report/game_end_report_screen.dart';
import '../state/game_ui_notifier.dart';
import 'new_game_time_sheet.dart';
import 'resign_action.dart';

/// Second layer of the game screen. [hostContext] stays mounted after the sheet
/// closes, so follow-up dialogs and routes open from the game screen.
Future<void> showGameMoreSheet(BuildContext hostContext) {
  return showAppModalBottomSheet<void>(
    context: hostContext,
    isScrollControlled: true,
    showDragHandle: true,
    useSafeArea: true,
    builder: (sheetContext) => DraggableScrollableSheet(
      initialChildSize: 0.6,
      minChildSize: 0.36,
      maxChildSize: 0.94,
      expand: false,
      builder: (_, scroll) =>
          _GameMoreSheet(hostContext: hostContext, scrollController: scroll),
    ),
  );
}

class _GameMoreSheet extends ConsumerWidget {
  const _GameMoreSheet({
    required this.hostContext,
    required this.scrollController,
  });

  final BuildContext hostContext;
  final ScrollController scrollController;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    final sessionN = ref.read(boardSessionNotifierProvider.notifier);
    final hasBoard = session.transport != BoardTransport.none;
    final physical = session.transport == BoardTransport.wifi ||
        session.transport == BoardTransport.ble;
    final finished = session.snapshot?.status.isGameFinished == true;
    final canResign = resignableSide(session, sandbox: ui.sandboxMode) != null;

    void closeThen(void Function() action) {
      Navigator.of(context).pop();
      action();
    }

    Widget sectionLabel(String text) => Padding(
          padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
          child: Text(
            text,
            style: theme.textTheme.labelLarge?.copyWith(
              color: cs.onSurfaceVariant,
              fontWeight: FontWeight.w600,
            ),
          ),
        );

    return ListView(
      controller: scrollController,
      padding: const EdgeInsets.fromLTRB(8, 0, 8, 24),
      children: [
        sectionLabel(l10n.moreMoves),
        const Padding(
          padding: EdgeInsets.symmetric(horizontal: 8),
          child: MoveHistoryView(),
        ),
        const SizedBox(height: 8),
        if (hasBoard)
          ListTile(
            leading: const Icon(Icons.restart_alt_rounded),
            title: Text(l10n.gameNewGame),
            onTap: () => closeThen(() => showNewGameWithTimeSheet(hostContext)),
          ),
        ListTile(
          leading: const Icon(Icons.show_chart_rounded),
          title: Text(l10n.moreAnalysis),
          onTap: () => closeThen(() => pushAnalysisRoute(hostContext)),
        ),
        if (finished)
          ListTile(
            leading: const Icon(Icons.summarize_outlined),
            title: Text(l10n.moreSummary),
            onTap: () => closeThen(() {
              Navigator.of(hostContext).push(MaterialPageRoute<void>(
                builder: (_) => const GameEndReportScreen(),
              ));
            }),
          ),
        ListTile(
          leading: const Icon(Icons.swap_vert_rounded),
          title: Text(l10n.uiFlipBoard),
          trailing: AnimatedRotation(
            turns: ui.boardFlipped ? 0.5 : 0,
            duration: AppMotion.resolve(context, AppMotion.stateChange),
            curve: AppMotion.standardCurve,
            child: Icon(Icons.south_rounded, color: cs.onSurfaceVariant),
          ),
          onTap: uiN.toggleFlip,
        ),
        if (physical)
          ListTile(
            leading: const Icon(Icons.lightbulb_outline),
            title: Text(l10n.uiClearHint),
            onTap: () => closeThen(() {
              runBoardCommandWithSnackBar(
                hostContext,
                sessionN.postHintClearBoard,
              );
            }),
          ),
        const Padding(
          padding: EdgeInsets.symmetric(vertical: 8),
          child: Divider(),
        ),
        if (hasBoard)
          ListTile(
            enabled: canResign,
            leading: Icon(
              Icons.flag_outlined,
              color: canResign ? cs.error : null,
            ),
            title: Text(
              l10n.gameResign,
              style: canResign ? TextStyle(color: cs.error) : null,
            ),
            onTap: () => closeThen(() => confirmAndResign(hostContext, ref)),
          ),
        if (hasBoard)
          ListTile(
            leading: const Icon(Icons.link_off_rounded),
            title: Text(l10n.uiDisconnect),
            onTap: () => closeThen(sessionN.disconnect),
          ),
      ],
    );
  }
}
