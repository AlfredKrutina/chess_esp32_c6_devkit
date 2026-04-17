import '../../core/models/board_timer_state.dart';
import '../../core/models/game_snapshot.dart';

enum BoardTransport { none, wifi, ble, mock }

class BoardSessionState {
  const BoardSessionState({
    this.transport = BoardTransport.none,
    this.wifiBaseUrl,
    this.snapshot,
    this.timer,
    this.lastIfNoneMatch,
    this.pollingActive = false,
    this.webSocketActive = false,
    this.bleLabel,
    this.lastError,
    this.busy = false,
  });

  final BoardTransport transport;
  final String? wifiBaseUrl;
  final GameSnapshot? snapshot;
  final BoardTimerState? timer;
  final String? lastIfNoneMatch;
  final bool pollingActive;
  final bool webSocketActive;
  final String? bleLabel;
  final Object? lastError;
  final bool busy;

  bool get hasLiveBoard => snapshot != null;

  BoardSessionState copyWith({
    BoardTransport? transport,
    String? wifiBaseUrl,
    GameSnapshot? snapshot,
    BoardTimerState? timer,
    String? lastIfNoneMatch,
    bool clearIfNoneMatch = false,
    bool? pollingActive,
    bool? webSocketActive,
    String? bleLabel,
    Object? lastError,
    bool clearError = false,
    bool? busy,
    bool clearSnapshot = false,
    bool clearTimer = false,
  }) {
    return BoardSessionState(
      transport: transport ?? this.transport,
      wifiBaseUrl: wifiBaseUrl ?? this.wifiBaseUrl,
      snapshot: clearSnapshot ? null : (snapshot ?? this.snapshot),
      timer: clearTimer ? null : (timer ?? this.timer),
      lastIfNoneMatch:
          clearIfNoneMatch ? null : (lastIfNoneMatch ?? this.lastIfNoneMatch),
      pollingActive: pollingActive ?? this.pollingActive,
      webSocketActive: webSocketActive ?? this.webSocketActive,
      bleLabel: bleLabel ?? this.bleLabel,
      lastError: clearError ? null : (lastError ?? this.lastError),
      busy: busy ?? this.busy,
    );
  }
}
