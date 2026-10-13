import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';

import '../../../core/localization/context_l10n.dart';
import '../../../core/models/board_timer_state.dart';
import '../../../core/theme/app_motion.dart';
import '../../../core/theme/app_theme.dart';
import '../../../l10n/app_localizations.dart';

enum ClockState { idle, onMove, lowTime, flagged, paused, offline, noTimeControl }

/// What one clock face shows. Pure data so the rules can be tested without widgets.
@immutable
class ClockFace {
  const ClockFace({
    required this.white,
    required this.state,
    required this.remainingMs,
    required this.ticking,
  });

  final bool white;
  final ClockState state;

  /// Last value reported by the board; null without a time control.
  final int? remainingMs;

  /// True when this side's time is running on the board, so the face may count
  /// down locally between board updates.
  final bool ticking;

  @override
  bool operator ==(Object other) =>
      other is ClockFace &&
      other.white == white &&
      other.state == state &&
      other.remainingMs == remainingMs &&
      other.ticking == ticking;

  @override
  int get hashCode => Object.hash(white, state, remainingMs, ticking);
}

/// Low-time threshold: 10 % of the initial time, kept between 10 s and 60 s.
int lowTimeThresholdMs(BoardTimerState t) {
  final initial = t.config.initialTimeMs;
  if (initial <= 0) return 20000;
  return (initial ~/ 10).clamp(10000, 60000);
}

/// Board is the source of truth. The flag is shown only when the board says
/// `time_expired`; a face that counts to zero locally never flags itself.
ClockFace resolveClockFace({
  required bool white,
  required BoardTimerState? timer,
  required bool connected,
  required bool gameFinished,
  required bool whiteToMoveFallback,
}) {
  if (timer == null || !timer.isTimeControlEnabled) {
    final onMove = !gameFinished && whiteToMoveFallback == white;
    return ClockFace(
      white: white,
      state: !connected
          ? ClockState.offline
          : (onMove ? ClockState.onMove : ClockState.noTimeControl),
      remainingMs: null,
      ticking: false,
    );
  }
  final ms = math.max(0, white ? timer.whiteTimeMs : timer.blackTimeMs);
  final onMove = timer.isWhiteTurn == white;
  if (timer.timeExpired) {
    final flaggedSide = ms <= 0 || onMove;
    return ClockFace(
      white: white,
      state: flaggedSide ? ClockState.flagged : ClockState.idle,
      remainingMs: ms,
      ticking: false,
    );
  }
  if (!connected) {
    return ClockFace(
      white: white,
      state: ClockState.offline,
      remainingMs: ms,
      ticking: false,
    );
  }
  if (gameFinished) {
    return ClockFace(
      white: white,
      state: ClockState.idle,
      remainingMs: ms,
      ticking: false,
    );
  }
  if (timer.gamePaused) {
    return ClockFace(
      white: white,
      state: ClockState.paused,
      remainingMs: ms,
      ticking: false,
    );
  }
  if (!onMove) {
    return ClockFace(
      white: white,
      state: ClockState.idle,
      remainingMs: ms,
      ticking: false,
    );
  }
  return ClockFace(
    white: white,
    state: ms <= lowTimeThresholdMs(timer)
        ? ClockState.lowTime
        : ClockState.onMove,
    remainingMs: ms,
    ticking: timer.timerRunning,
  );
}

String formatClockMs(int ms) {
  final clamped = math.max(0, ms);
  if (clamped < 10000) {
    final tenths = clamped ~/ 100;
    return '${tenths ~/ 10}.${tenths % 10}';
  }
  final totalSeconds = clamped ~/ 1000;
  final h = totalSeconds ~/ 3600;
  final m = (totalSeconds % 3600) ~/ 60;
  final s = (totalSeconds % 60).toString().padLeft(2, '0');
  if (h > 0) return '$h:${m.toString().padLeft(2, '0')}:$s';
  return '$m:$s';
}

String clockStateLabel(AppLocalizations l10n, ClockState state) {
  return switch (state) {
    ClockState.idle => l10n.clockWaiting,
    ClockState.onMove => l10n.clockOnMove,
    ClockState.lowTime => l10n.clockLowTime,
    ClockState.flagged => l10n.clockFlagged,
    ClockState.paused => l10n.clockPaused,
    ClockState.offline => l10n.clockOffline,
    ClockState.noTimeControl => l10n.clockNoTimeControl,
  };
}

/// Large, read-only clock plate: piece disc, optional state icon and the time.
/// State is carried by colour; the only visible word is "time is up".
/// It is intentionally not a button: the clock is pressed on the physical board.
class PlayerClock extends StatefulWidget {
  const PlayerClock({
    super.key,
    required this.face,
    this.compact = false,
  });

  final ClockFace face;

  /// Shorter plate for tight landscape layouts.
  final bool compact;

  @override
  State<PlayerClock> createState() => _PlayerClockState();
}

class _PlayerClockState extends State<PlayerClock>
    with SingleTickerProviderStateMixin {
  late final Ticker _ticker;
  Duration _elapsed = Duration.zero;

  @override
  void initState() {
    super.initState();
    _ticker = createTicker(_onTick);
    _syncTicker();
  }

  @override
  void didUpdateWidget(covariant PlayerClock oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.face != widget.face) {
      _ticker.stop();
      _elapsed = Duration.zero;
      _syncTicker();
    }
  }

  void _syncTicker() {
    if (widget.face.ticking && !_ticker.isActive) _ticker.start();
  }

  void _onTick(Duration elapsed) {
    final before = _text(_displayMs());
    _elapsed = elapsed;
    final ms = _displayMs();
    if (ms == 0) _ticker.stop();
    if (_text(ms) != before) setState(() {});
  }

  @override
  void dispose() {
    _ticker.dispose();
    super.dispose();
  }

  static String _text(int? ms) => ms == null ? '–' : formatClockMs(ms);

  int? _displayMs() {
    final ms = widget.face.remainingMs;
    if (ms == null) return null;
    if (!widget.face.ticking) return ms;
    return math.max(0, ms - _elapsed.inMilliseconds);
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final tokens = ClockTokens.of(context);
    final theme = Theme.of(context);
    final face = widget.face;
    final duration = AppMotion.resolve(context, AppMotion.stateChange);

    final (Color bg, Color fg) = switch (face.state) {
      ClockState.onMove => (tokens.activeBackground, tokens.activeForeground),
      ClockState.lowTime => (tokens.lowBackground, tokens.lowForeground),
      ClockState.flagged => (tokens.expiredBackground, tokens.expiredForeground),
      _ => (tokens.idleBackground, tokens.idleForeground),
    };
    final emphasised = face.state == ClockState.onMove ||
        face.state == ClockState.lowTime ||
        face.state == ClockState.flagged;
    final iconColor = emphasised ? fg : tokens.mutedForeground;

    final ms = _displayMs();
    final timeText = _text(ms);
    final side = face.white ? l10n.clockWhite : l10n.clockBlack;
    final stateLabel = clockStateLabel(l10n, face.state);

    final stateIcon = switch (face.state) {
      ClockState.paused => Icons.pause_rounded,
      ClockState.offline => Icons.link_off_rounded,
      ClockState.flagged => Icons.flag_rounded,
      _ => null,
    };

    final timeStyle = TextStyle(
      fontSize: widget.compact ? 56 : 64,
      fontWeight: FontWeight.w600,
      height: 1.0,
      letterSpacing: -1,
      color: fg,
      fontFeatures: const [FontFeature.tabularFigures()],
    );

    return Semantics(
      container: true,
      label: l10n.clockSemantics(side, timeText, stateLabel),
      excludeSemantics: true,
      child: AnimatedContainer(
        duration: duration,
        curve: AppMotion.standardCurve,
        alignment: Alignment.center,
        constraints: BoxConstraints(minHeight: widget.compact ? 88 : 112),
        padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
        decoration: BoxDecoration(
          color: bg,
          borderRadius: BorderRadius.circular(AppTheme.radius + 4),
        ),
        child: Row(
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisSize: MainAxisSize.min,
                children: [
                  Row(
                    children: [
                      AnimatedContainer(
                        duration: duration,
                        curve: AppMotion.standardCurve,
                        width: 16,
                        height: 16,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: face.white
                              ? const Color(0xFFF4F1E8)
                              : const Color(0xFF15171B),
                          border: Border.all(color: iconColor, width: 1.5),
                        ),
                      ),
                      const SizedBox(width: 10),
                      AnimatedSwitcher(
                        duration: duration,
                        switchInCurve: AppMotion.standardCurve,
                        switchOutCurve: AppMotion.reverseCurve,
                        child: stateIcon == null
                            ? const SizedBox(key: ValueKey('none'), width: 18)
                            : Icon(
                                stateIcon,
                                key: ValueKey(stateIcon),
                                size: 18,
                                color: iconColor,
                              ),
                      ),
                    ],
                  ),
                  AnimatedSize(
                    duration: duration,
                    curve: AppMotion.standardCurve,
                    alignment: Alignment.topLeft,
                    child: face.state == ClockState.flagged
                        ? Padding(
                            padding: const EdgeInsets.only(top: 6),
                            child: Text(
                              stateLabel,
                              maxLines: 2,
                              overflow: TextOverflow.ellipsis,
                              style: theme.textTheme.titleSmall?.copyWith(
                                color: fg,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                          )
                        : const SizedBox.shrink(),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 12),
            Flexible(
              flex: 3,
              child: Align(
                alignment: Alignment.centerRight,
                child: FittedBox(
                  fit: BoxFit.scaleDown,
                  child: AnimatedDefaultTextStyle(
                    duration: duration,
                    curve: AppMotion.standardCurve,
                    style: timeStyle,
                    child: Text(timeText, maxLines: 1),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
