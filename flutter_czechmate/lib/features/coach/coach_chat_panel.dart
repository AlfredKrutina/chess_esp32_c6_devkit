import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../connection/board_session_notifier.dart';
import 'coach_chat_notifier.dart';

/// Shared AI coach chat — same conversation as the full-screen [CoachScreen].
class CoachChatPanel extends ConsumerStatefulWidget {
  const CoachChatPanel({
    super.key,
    this.embedded = false,
    this.showDisconnectedBanner = true,
  });

  /// When true (game side rail), use compact banners instead of [MaterialBanner].
  final bool embedded;

  final bool showDisconnectedBanner;

  @override
  ConsumerState<CoachChatPanel> createState() => _CoachChatPanelState();
}

class _CoachChatPanelState extends ConsumerState<CoachChatPanel> {
  final _ctrl = TextEditingController();
  final _scrollCtrl = ScrollController();

  @override
  void initState() {
    super.initState();
    _ctrl.addListener(() => setState(() {}));
  }

  @override
  void dispose() {
    _ctrl.dispose();
    _scrollCtrl.dispose();
    super.dispose();
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!_scrollCtrl.hasClients) return;
        _scrollCtrl.jumpTo(_scrollCtrl.position.maxScrollExtent);
      });
    });
  }

  Future<void> _send({bool fromShortcut = false}) async {
    final q = _ctrl.text.trim();
    if (q.isEmpty) {
      if (!mounted || fromShortcut) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.coachChatTypeFirst)),
      );
      return;
    }
    _ctrl.clear();
    await ref.read(coachChatProvider.notifier).submit(q);
    if (mounted) _scrollToBottom();
  }

  /// Cmd+Enter (macOS) / Ctrl+Enter — odeslání bez nového řádku.
  void _sendViaKeyboardShortcut() {
    final chat = ref.read(coachChatProvider);
    if (chat.busy || _ctrl.text.trim().isEmpty) return;
    _send(fromShortcut: true);
  }

  @override
  Widget build(BuildContext context) {
    ref.listen<CoachChatState>(coachChatProvider, (prev, next) {
      if (prev?.messages.length != next.messages.length) {
        _scrollToBottom();
      }
    });

    final prefs = ref.watch(prefsRepositoryProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final chat = ref.watch(coachChatProvider);
    final notifier = ref.read(coachChatProvider.notifier);
    final showSetup = notifier.shouldShowSetupBanner(prefs);
    final l10n = context.l10n;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (showSetup)
          widget.embedded
              ? Padding(
                  padding: const EdgeInsets.fromLTRB(10, 10, 10, 0),
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: Theme.of(context).colorScheme.tertiaryContainer.withValues(alpha: 0.35),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: Theme.of(context).colorScheme.outline.withValues(alpha: 0.35),
                      ),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            CoachChatNotifier.setupBannerText(l10n),
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                          Align(
                            alignment: Alignment.centerRight,
                            child: TextButton(
                              onPressed: notifier.dismissSetupBanner,
                              child: Text(l10n.coachChatHide),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                )
              : MaterialBanner(
                  content: Text(CoachChatNotifier.setupBannerText(l10n)),
                  actions: [
                    TextButton(
                      onPressed: notifier.dismissSetupBanner,
                      child: Text(l10n.coachChatDismiss),
                    ),
                  ],
                ),
        if (widget.showDisconnectedBanner && session.snapshot == null)
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
            color: Colors.orange.withValues(alpha: 0.12),
            child: Row(
              children: [
                const Icon(Icons.warning_amber, color: Colors.orange, size: 18),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    l10n.coachDisconnectedBanner,
                    style: const TextStyle(fontSize: 12),
                  ),
                ),
              ],
            ),
          ),
        if (chat.busy)
          const LinearProgressIndicator(minHeight: 2),
        Expanded(
          child: chat.messages.isEmpty
              ? Center(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(
                          Icons.school_outlined,
                          size: widget.embedded ? 48 : 64,
                          color: Theme.of(context).colorScheme.primary.withValues(alpha: 0.45),
                        ),
                        const SizedBox(height: 12),
                        Text(
                          widget.embedded
                              ? l10n.coachEmptyPromptEmbedded
                              : l10n.coachEmptyPromptFullscreen,
                          textAlign: TextAlign.center,
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      ],
                    ),
                  ),
                )
              : LayoutBuilder(
                  builder: (context, constraints) {
                    final bubbleMax = widget.embedded
                        ? (constraints.maxWidth - 16).clamp(180.0, 340.0)
                        : (MediaQuery.sizeOf(context).width * 0.8).clamp(200.0, 560.0);
                    return ListView.builder(
                      key: ValueKey<int>(chat.messages.length),
                      controller: _scrollCtrl,
                      padding: const EdgeInsets.all(10),
                      itemCount: chat.messages.length,
                      itemBuilder: (_, i) => CoachChatBubble(
                        msg: chat.messages[i],
                        maxWidth: bubbleMax,
                      ),
                    );
                  },
                ),
        ),
        Padding(
          padding: EdgeInsets.only(
            left: 8,
            right: 8,
            bottom: MediaQuery.of(context).viewInsets.bottom + 8,
            top: 6,
          ),
          child: CallbackShortcuts(
            bindings: {
              const SingleActivator(LogicalKeyboardKey.enter, meta: true):
                  _sendViaKeyboardShortcut,
              const SingleActivator(LogicalKeyboardKey.enter, control: true):
                  _sendViaKeyboardShortcut,
              const SingleActivator(LogicalKeyboardKey.numpadEnter, meta: true):
                  _sendViaKeyboardShortcut,
              const SingleActivator(LogicalKeyboardKey.numpadEnter, control: true):
                  _sendViaKeyboardShortcut,
            },
            child: Focus(
              skipTraversal: false,
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.end,
                children: [
                  Expanded(
                    child: TextField(
                      controller: _ctrl,
                      decoration: InputDecoration(
                        hintText: widget.embedded
                            ? l10n.coachInputHintEmbedded
                            : l10n.coachInputHintFullscreen,
                        border: const OutlineInputBorder(),
                        isDense: true,
                      ),
                      maxLines: 3,
                      minLines: 1,
                      textInputAction: TextInputAction.newline,
                      onSubmitted: (_) => _send(),
                    ),
                  ),
                  const SizedBox(width: 8),
                  FilledButton(
                    onPressed: (chat.busy || _ctrl.text.trim().isEmpty)
                        ? null
                        : () => _send(),
                    child: chat.busy
                        ? const SizedBox(
                            width: 22,
                            height: 22,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.send),
                  ),
                ],
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class CoachChatBubble extends StatelessWidget {
  const CoachChatBubble({
    super.key,
    required this.msg,
    required this.maxWidth,
  });

  final CoachChatMessage msg;
  final double maxWidth;

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final isUser = msg.isUser;
    return Align(
      alignment: isUser ? Alignment.centerRight : Alignment.centerLeft,
      child: Container(
        margin: const EdgeInsets.only(bottom: 8),
        constraints: BoxConstraints(maxWidth: maxWidth),
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
        decoration: BoxDecoration(
          color: msg.isError
              ? cs.errorContainer
              : isUser
                  ? cs.primaryContainer
                  : cs.surfaceContainerHigh,
          borderRadius: BorderRadius.only(
            topLeft: const Radius.circular(16),
            topRight: const Radius.circular(16),
            bottomLeft: Radius.circular(isUser ? 16 : 4),
            bottomRight: Radius.circular(isUser ? 4 : 16),
          ),
        ),
        child: SelectableText(
          msg.text,
          style: TextStyle(
            color: isUser ? cs.onPrimaryContainer : cs.onSurface,
          ),
        ),
      ),
    );
  }
}
