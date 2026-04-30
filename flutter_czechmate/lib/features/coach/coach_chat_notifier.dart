import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/constants/app_environment.dart';
import '../../core/models/coach_ai_provider.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/services/coach_ai_completion_service.dart';
import '../../core/services/prefs_repository.dart';
import '../../core/utils/fen_from_board.dart';
import '../connection/board_session_notifier.dart';
import 'coach_manager.dart';

class CoachChatMessage {
  const CoachChatMessage({
    required this.text,
    required this.isUser,
    this.isError = false,
  });

  final String text;
  final bool isUser;
  final bool isError;
}

class CoachChatState {
  const CoachChatState({
    this.messages = const [],
    this.busy = false,
    this.lastSuccessProvider,
    this.hideSetupBanner = false,
  });

  final List<CoachChatMessage> messages;
  final bool busy;
  final CoachAiProviderId? lastSuccessProvider;
  final bool hideSetupBanner;

  CoachChatState copyWith({
    List<CoachChatMessage>? messages,
    bool? busy,
    CoachAiProviderId? lastSuccessProvider,
    bool? hideSetupBanner,
    bool clearLastSuccessProvider = false,
  }) {
    return CoachChatState(
      messages: messages ?? this.messages,
      busy: busy ?? this.busy,
      lastSuccessProvider: clearLastSuccessProvider
          ? null
          : (lastSuccessProvider ?? this.lastSuccessProvider),
      hideSetupBanner: hideSetupBanner ?? this.hideSetupBanner,
    );
  }
}

class CoachChatNotifier extends StateNotifier<CoachChatState> {
  CoachChatNotifier(this._ref) : super(const CoachChatState());

  final Ref _ref;

  static const _levelPrompts = {
    1:
        'You are a friendly chess coach for absolute beginners. Explain simply, avoid jargon.',
    2:
        'You are a chess coach for intermediate players. Explain tactical ideas briefly and clearly.',
    3:
        'You are a chess coach for experienced players. Focus on strategy, structure, and planning.',
    4:
        'You are a chess coach at IM/GM level. Analyze deeply, mention concrete lines and tempo.',
  };

  static const _sessionContinuityHint =
      'This is a continuing chat session: keep answers consistent with earlier turns. '
      'If a board position (FEN) appears in your instructions and differs from an older message, '
      'always treat the latest FEN as the current position.';

  void dismissSetupBanner() {
    state = state.copyWith(hideSetupBanner: true);
  }

  List<CoachAiTurn> _priorTurnsBeforeLatest() {
    final msgs = state.messages;
    if (msgs.length < 2) return [];
    final out = <CoachAiTurn>[];
    for (var i = 0; i < msgs.length - 1; i++) {
      final m = msgs[i];
      if (m.isError) continue;
      out.add(CoachAiTurn(fromUser: m.isUser, text: m.text));
    }
    return out;
  }

  Future<String> _offlineCoachReply({
    required GameSnapshot? snap,
    required int level,
  }) async {
    final manager = _ref.read(coachManagerProvider.notifier);
    if (snap != null) await manager.requestHint(fenFromSnapshot(snap));
    final hint = _ref.read(coachManagerProvider.notifier).lastHint;
    return hint ?? _localFallback(level);
  }

  String _localFallback(int level) {
    switch (level) {
      case 1:
        return 'Try to castle early for king safety and develop pieces toward the center. '
            'In Settings → Coach & AI, add providers to the priority list and API keys for cloud coaching.';
      case 2:
        return 'Look for tactical themes — forks, pins, skewers. '
            'Configure providers under Settings → Coach & AI for analysis of this exact position.';
      case 3:
        return 'Focus on weak squares and pawn structure. '
            'Add cloud providers under Settings → Coach & AI for deeper analysis.';
      default:
        return 'Try to calculate lines three moves ahead. '
            'Use Settings → Coach & AI to set provider priority (OpenAI, Google, Groq, xAI, Ollama).';
    }
  }

  Future<void> submit(String rawQuestion) async {
    final q = rawQuestion.trim();
    if (q.isEmpty) return;

    final snap = _ref.read(boardSessionNotifierProvider).snapshot;
    final prefs = _ref.read(prefsRepositoryProvider);
    final level = prefs.coachUserLevel;

    if (prefs.coachTraceLogsEnabled) {
      debugPrint(
        '[staging][coach] ask chain=${prefs.coachAiPriority.map((e) => e.storageValue).join('>')} level=$level qLen=${q.length}',
      );
    }

    state = state.copyWith(
      messages: [...state.messages, CoachChatMessage(text: q, isUser: true)],
      busy: true,
    );

    try {
      final fenLine =
          snap != null ? '\nCurrent position FEN: ${fenFromSnapshot(snap)}' : '';
      final systemPrompt =
          '${_levelPrompts[level] ?? _levelPrompts[2]!}\n$_sessionContinuityHint\n'
          'Reply in clear English.$fenLine';

      final prior = _priorTurnsBeforeLatest();
      final chain = prefs.coachAiPriority;

      late final String reply;
      if (chain.isEmpty) {
        reply = await _offlineCoachReply(snap: snap, level: level);
        state = state.copyWith(clearLastSuccessProvider: true);
      } else {
        final result = await CoachAiCompletionService.completeWithFallback(
          prefs: prefs,
          systemPrompt: systemPrompt,
          priorTurns: prior,
          latestUserMessage: q,
        );
        if (result.isOffline) {
          if (AppEnvironment.staging) {
            debugPrint('[staging][coach] fallback to offline tips after chain failure');
          }
          final offline = await _offlineCoachReply(snap: snap, level: level);
          reply =
              'Could not reach any configured AI provider (see Settings → Coach & AI).\n\n$offline';
          state = state.copyWith(clearLastSuccessProvider: true);
        } else {
          reply = result.text;
          state = state.copyWith(lastSuccessProvider: result.providerId);
        }
      }

      state = state.copyWith(
        messages: [...state.messages, CoachChatMessage(text: reply, isUser: false)],
        busy: false,
      );
    } catch (e) {
      state = state.copyWith(
        messages: [
          ...state.messages,
          CoachChatMessage(text: 'Something went wrong: $e', isUser: false, isError: true),
        ],
        busy: false,
      );
    }
  }

  String providerChipLabel(PrefsRepository prefs) {
    if (prefs.coachAiPriority.isEmpty) return 'Offline';
    if (state.lastSuccessProvider != null) {
      return state.lastSuccessProvider!.shortLabel;
    }
    return prefs.coachAiPriority.map((e) => e.shortLabel).join(' → ');
  }

  bool shouldShowSetupBanner(PrefsRepository prefs) {
    if (state.hideSetupBanner) return false;
    if (prefs.coachAiPriority.isEmpty) return false;
    return !CoachAiCompletionService.anyProviderConfigured(prefs);
  }

  static String setupBannerText(PrefsRepository prefs) {
    return 'Your coach priority list is non-empty, but no provider has credentials yet. '
        'Open Settings → Coach & AI and fill API keys (or Ollama URL) for at least one listed provider.';
  }
}

final coachChatProvider =
    StateNotifierProvider<CoachChatNotifier, CoachChatState>((ref) {
  return CoachChatNotifier(ref);
});
