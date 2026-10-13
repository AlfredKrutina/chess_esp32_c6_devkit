import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/constants/app_environment.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/services/board_api_exception.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../../l10n/app_localizations.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../state/game_ui_notifier.dart';

/// Colour on move as firmware `side` (`white` / `black`), or null when the
/// board would refuse a resignation (no live game, puzzle, opening trainer,
/// setup tutorial, or the local sandbox is shown instead of the board).
String? resignableSide(BoardSessionState session, {required bool sandbox}) {
  if (sandbox) return null;
  switch (session.transport) {
    case BoardTransport.wifi:
    case BoardTransport.ble:
    case BoardTransport.mock:
      break;
    case BoardTransport.none:
      return null;
  }
  final st = session.snapshot?.status;
  if (st == null || st.isGameFinished) return null;
  switch (st.gameState.toLowerCase()) {
    case 'active':
    case 'playing':
    case 'paused':
    case 'promotion':
      break;
    default:
      return null;
  }
  if (st.puzzle?.active == true || st.puzzle?.setupActive == true) {
    return null;
  }
  if (st.openingTraining?.active == true) return null;
  if (st.boardSetupTutorial == true) return null;
  return st.currentPlayer.toLowerCase().startsWith('b') ? 'black' : 'white';
}

/// Asks "Resign for White/Black?" and sends the command only on explicit
/// confirmation. Returns true when the board accepted the command.
Future<bool> confirmAndResign(BuildContext context, WidgetRef ref) async {
  final session = ref.read(boardSessionNotifierProvider);
  final sandbox = ref.read(gameUiNotifierProvider).sandboxMode;
  final side = resignableSide(session, sandbox: sandbox);
  if (side == null) return false;
  final notifier = ref.read(boardSessionNotifierProvider.notifier);
  final ok = await showResignConfirmDialog(
    context,
    white: side == 'white',
    onConfirm: () => notifier.postResign(side),
  );
  if (ok == true && context.mounted) {
  }
  return ok == true;
}

/// Destructive confirmation. "Cancel" holds the initial focus, so Enter or a
/// stray tap on the default action never ends the game.
Future<bool?> showResignConfirmDialog(
  BuildContext context, {
  required bool white,
  required Future<void> Function() onConfirm,
}) {
  return showDialog<bool>(
    context: context,
    builder: (_) => _ResignConfirmDialog(white: white, onConfirm: onConfirm),
  );
}

class _ResignConfirmDialog extends StatefulWidget {
  const _ResignConfirmDialog({required this.white, required this.onConfirm});

  final bool white;
  final Future<void> Function() onConfirm;

  @override
  State<_ResignConfirmDialog> createState() => _ResignConfirmDialogState();
}

class _ResignConfirmDialogState extends State<_ResignConfirmDialog> {
  bool _busy = false;
  String? _error;

  Future<void> _confirm(AppLocalizations l10n) async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      await widget.onConfirm();
      if (!mounted) return;
      Navigator.of(context).pop(true);
    } catch (e) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] resign failed: $e');
      }
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = e is BoardApiException && e.statusCode == 409
            ? l10n.gameResignRefused
            : l10n.gameResignFailed(userFacingErrorSummary(l10n, e));
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    return PopScope(
      canPop: !_busy,
      child: AlertDialog(
        icon: Icon(Icons.flag_outlined, color: cs.error),
        title: Text(
          widget.white ? l10n.uiResignTitleWhite : l10n.uiResignTitleBlack,
        ),
        content: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 420),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                widget.white ? l10n.uiResignBodyWhite : l10n.uiResignBodyBlack,
              ),
              if (_error != null) ...[
                const SizedBox(height: 16),
                Semantics(
                  liveRegion: true,
                  child: Container(
                    width: double.infinity,
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: cs.errorContainer,
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Text(
                      _error!,
                      style: theme.textTheme.bodyMedium
                          ?.copyWith(color: cs.onErrorContainer),
                    ),
                  ),
                ),
              ],
            ],
          ),
        ),
        actions: [
          TextButton(
            autofocus: true,
            onPressed: _busy ? null : () => Navigator.of(context).pop(false),
            child: Text(l10n.commonCancel),
          ),
          FilledButton.icon(
            style: FilledButton.styleFrom(
              backgroundColor: cs.error,
              foregroundColor: cs.onError,
            ),
            onPressed: _busy ? null : () => _confirm(l10n),
            icon: _busy
                ? SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(
                      strokeWidth: 2,
                      color: cs.onSurface,
                    ),
                  )
                : const Icon(Icons.flag_outlined),
            label: Text(_busy ? l10n.gameResignSending : l10n.gameResign),
          ),
        ],
      ),
    );
  }
}

/// Destructive outlined button for Game controls. Disabled with a tooltip
/// when no game runs on the board.
class ResignButton extends ConsumerWidget {
  const ResignButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    final sandbox =
        ref.watch(gameUiNotifierProvider.select((s) => s.sandboxMode));
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final enabled = resignableSide(session, sandbox: sandbox) != null;
    final button = OutlinedButton.icon(
      style: OutlinedButton.styleFrom(
        foregroundColor: cs.error,
        side: BorderSide(
          color: enabled ? cs.error : cs.onSurface.withValues(alpha: 0.12),
        ),
      ),
      onPressed: enabled ? () => confirmAndResign(context, ref) : null,
      icon: const Icon(Icons.flag_outlined),
      label: Text(l10n.gameResign),
    );
    if (enabled) return button;
    return Tooltip(message: l10n.gameResignUnavailable, child: button);
  }
}
