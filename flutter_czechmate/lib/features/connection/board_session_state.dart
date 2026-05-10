import '../../core/models/board_timer_state.dart';
import '../../core/models/game_snapshot.dart';

enum BoardTransport { none, wifi, ble, mock }

class BoardSessionState {
  const BoardSessionState({
    this.transport = BoardTransport.none,
    this.wifiBaseUrl,
    this.snapshot,
    this.snapshotReceivedAt,
    this.transportStartedAt,
    this.timer,
    this.lastIfNoneMatch,
    this.pollingActive = false,
    this.webSocketActive = false,
    this.bleGattConnected = false,
    this.bleLabel,
    this.lastError,
    this.busy = false,
    this.pollSuccessCount = 0,
    this.pollFailureCount = 0,
    this.wsMessageCount = 0,
    this.lastSuccessfulPoll,
    this.bleStaIp,
    this.bleStaSsid,
    this.bleStaConnected = false,
    this.bleApBroadcasting = false,
    this.bleApSsid,
  });

  final BoardTransport transport;
  final String? wifiBaseUrl;
  final GameSnapshot? snapshot;
  final DateTime? snapshotReceivedAt;
  final DateTime? transportStartedAt;
  final BoardTimerState? timer;
  final String? lastIfNoneMatch;
  final bool pollingActive;
  final bool webSocketActive;
  final bool bleGattConnected;
  final String? bleLabel;
  final Object? lastError;
  final bool busy;

  /// Diagnostika REST poll (`AdvancedConnectionDiagnosticsView`).
  final int pollSuccessCount;
  final int pollFailureCount;
  final int wsMessageCount;
  final DateTime? lastSuccessfulPoll;

  /// Poslední STA metadata z BLE network char (notify/read).
  final String? bleStaIp;
  final String? bleStaSsid;
  final bool bleStaConnected;

  /// Hotspot desky (AP) podle posledního BLE network JSON (`ap_active`).
  final bool bleApBroadcasting;
  final String? bleApSsid;

  bool get hasLiveBoard => snapshot != null;

  BoardSessionState copyWith({
    BoardTransport? transport,
    String? wifiBaseUrl,
    GameSnapshot? snapshot,
    DateTime? snapshotReceivedAt,
    DateTime? transportStartedAt,
    BoardTimerState? timer,
    String? lastIfNoneMatch,
    bool clearIfNoneMatch = false,
    bool? pollingActive,
    bool? webSocketActive,
    bool? bleGattConnected,
    String? bleLabel,
    Object? lastError,
    bool clearError = false,
    bool? busy,
    bool clearSnapshot = false,
    bool clearSnapshotReceivedAt = false,
    bool clearTransportStartedAt = false,
    bool clearTimer = false,
    int? pollSuccessCount,
    int? pollFailureCount,
    int? wsMessageCount,
    DateTime? lastSuccessfulPoll,
    bool clearLastPoll = false,
    String? bleStaIp,
    String? bleStaSsid,
    bool? bleStaConnected,
    bool? bleApBroadcasting,
    String? bleApSsid,
    bool clearBleNetwork = false,
  }) {
    return BoardSessionState(
      transport: transport ?? this.transport,
      wifiBaseUrl: wifiBaseUrl ?? this.wifiBaseUrl,
      snapshot: clearSnapshot ? null : (snapshot ?? this.snapshot),
      snapshotReceivedAt: clearSnapshotReceivedAt
          ? null
          : (snapshotReceivedAt ?? this.snapshotReceivedAt),
      transportStartedAt: clearTransportStartedAt
          ? null
          : (transportStartedAt ?? this.transportStartedAt),
      timer: clearTimer ? null : (timer ?? this.timer),
      lastIfNoneMatch:
          clearIfNoneMatch ? null : (lastIfNoneMatch ?? this.lastIfNoneMatch),
      pollingActive: pollingActive ?? this.pollingActive,
      webSocketActive: webSocketActive ?? this.webSocketActive,
      bleGattConnected: bleGattConnected ?? this.bleGattConnected,
      bleLabel: bleLabel ?? this.bleLabel,
      lastError: clearError ? null : (lastError ?? this.lastError),
      busy: busy ?? this.busy,
      pollSuccessCount: pollSuccessCount ?? this.pollSuccessCount,
      pollFailureCount: pollFailureCount ?? this.pollFailureCount,
      wsMessageCount: wsMessageCount ?? this.wsMessageCount,
      lastSuccessfulPoll: clearLastPoll
          ? null
          : (lastSuccessfulPoll ?? this.lastSuccessfulPoll),
      bleStaIp: clearBleNetwork ? null : (bleStaIp ?? this.bleStaIp),
      bleStaSsid: clearBleNetwork ? null : (bleStaSsid ?? this.bleStaSsid),
      bleStaConnected: clearBleNetwork
          ? false
          : (bleStaConnected ?? this.bleStaConnected),
      bleApBroadcasting: clearBleNetwork
          ? false
          : (bleApBroadcasting ?? this.bleApBroadcasting),
      bleApSsid: clearBleNetwork ? null : (bleApSsid ?? this.bleApSsid),
    );
  }
}
