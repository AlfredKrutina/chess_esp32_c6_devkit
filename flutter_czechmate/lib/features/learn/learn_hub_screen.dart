import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/theme/app_theme.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/widgets/quiet_list.dart';
import '../coach/coach_screen.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../opening/opening_catalog_screen.dart';
import '../profile/user_profile_screen.dart';
import '../puzzle/puzzle_screen.dart';
import '../setup/board_setup_wizard_screen.dart';

/// Train tab: the daily puzzle first, then everything else that teaches.
class TrainScreen extends ConsumerWidget {
  const TrainScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final session = ref.watch(boardSessionNotifierProvider);
    final boardCommands = (session.transport == BoardTransport.wifi &&
            session.wifiBaseUrl != null) ||
        session.transport == BoardTransport.ble;

    void push(Widget page) {
      Navigator.of(context).push<void>(
        MaterialPageRoute<void>(builder: (_) => page),
      );
    }

    return Scaffold(
      body: SafeArea(
        bottom: false,
        child: desktopFormDetailBody(
          ListView(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 32),
            children: [
              PressableScale(
                child: Material(
                  color: cs.surfaceContainer,
                  borderRadius: BorderRadius.circular(AppTheme.radius + 4),
                  clipBehavior: Clip.antiAlias,
                  child: InkWell(
                    onTap: () => push(const PuzzleScreen()),
                    child: Padding(
                      padding: const EdgeInsets.all(20),
                      child: Row(
                        children: [
                          Container(
                            width: 56,
                            height: 56,
                            decoration: BoxDecoration(
                              color: cs.primary.withValues(alpha: 0.16),
                              borderRadius:
                                  BorderRadius.circular(AppTheme.radius),
                            ),
                            child: Icon(Icons.extension_rounded,
                                color: cs.primary, size: 28),
                          ),
                          const SizedBox(width: 16),
                          Expanded(
                            child: Text(
                              l10n.trainDaily,
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                              style: theme.textTheme.titleLarge,
                            ),
                          ),
                          Icon(Icons.chevron_right_rounded,
                              color: cs.onSurfaceVariant),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
              QuietGroup(
                children: [
                  QuietRow(
                    icon: Icons.collections_bookmark_outlined,
                    title: l10n.trainLibrary,
                    onTap: () => push(const PuzzleScreen(initialTab: 1)),
                  ),
                  QuietRow(
                    icon: Icons.timer_outlined,
                    title: l10n.trainTimed,
                    onTap: () => push(const PuzzleScreen(initialTab: 2)),
                  ),
                ],
              ),
              QuietGroup(
                children: [
                  QuietRow(
                    icon: Icons.menu_book_outlined,
                    title: l10n.trainOpenings,
                    onTap: () => push(const OpeningCatalogScreen()),
                  ),
                  QuietRow(
                    icon: Icons.chat_bubble_outline_rounded,
                    title: l10n.trainCoach,
                    onTap: () => push(const CoachScreen()),
                  ),
                  QuietRow(
                    icon: Icons.auto_fix_high_outlined,
                    title: l10n.trainSetup,
                    onTap: boardCommands
                        ? () => push(
                              const BoardSetupWizardScreen(
                                kind: BoardSetupWizardKind.standardStart,
                              ),
                            )
                        : null,
                  ),
                  QuietRow(
                    icon: Icons.insights_outlined,
                    title: l10n.trainProgress,
                    onTap: () => push(const UserProfileScreen()),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}
