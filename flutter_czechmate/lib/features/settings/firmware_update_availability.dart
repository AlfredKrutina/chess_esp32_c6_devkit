import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/constants/firmware_defaults.dart';
import '../../core/models/board_firmware_models.dart';
import '../../core/utils/board_http_base_url.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Manifest vs board check status (no auto mesh on build — calls [refresh]).
class FirmwareAvailState {
  const FirmwareAvailState({
    this.loading = false,
    this.manifest,
    this.boardVersion,
    this.boardOtaSupported,
    this.otaLastBootFailed,
    this.otaFailedFirmwareVersion,
    this.otaFailedSlot,
    this.error,
  });

  final bool loading;
  final FirmwareManifest? manifest;
  final String? boardVersion;

  /// Null = unknown (no HTTP info yet or older firmware without `ota_supported`).
  final bool? boardOtaSupported;

  /// The board reports `ota_last_boot_failed` from `GET /api/system/firmware`.
  final bool? otaLastBootFailed;
  final String? otaFailedFirmwareVersion;
  final String? otaFailedSlot;

  final String? error;

  bool get hasBoardVersion =>
      boardVersion != null && boardVersion!.trim().isNotEmpty;

  bool get updateAvailable {
    final m = manifest;
    final b = boardVersion;
    if (m == null || !hasBoardVersion) {
      return false;
    }
    return compareSemverLoose(m.version, b!) > 0;
  }

  /// Same semver as manifest (plate and manifest known).
  bool get sameSemverAsManifest {
    final m = manifest;
    final b = boardVersion;
    if (m == null || !hasBoardVersion) {
      return false;
    }
    return compareSemverLoose(m.version, b!) == 0;
  }

  /// The manifest from the source is older than the reported board version (both known).
  bool get manifestOlderThanBoard {
    final m = manifest;
    final b = boardVersion;
    if (m == null || !hasBoardVersion) {
      return false;
    }
    return compareSemverLoose(m.version, b!) < 0;
  }

  /// The Git manifest is valid and either newer than the board or we don't know the board version (BLE only / no HTTP).
  bool get showBleGitFirmwareActions {
    final m = manifest;
    if (m == null || m.version.trim().isEmpty || m.url.trim().isEmpty) {
      return false;
    }
    if (updateAvailable) {
      return true;
    }
    return !hasBoardVersion;
  }

  /// OTA actions from the manifest (including developer re-flash of the same or older version).
  bool showOtaFromGitWithDeveloper(bool developerUnlocked) {
    if (showBleGitFirmwareActions) {
      return true;
    }
    final m = manifest;
    if (m == null || m.version.trim().isEmpty || m.url.trim().isEmpty) {
      return false;
    }
    return developerUnlocked &&
        hasBoardVersion &&
        (sameSemverAsManifest || manifestOlderThanBoard);
  }

  FirmwareAvailState copyWith({
    bool? loading,
    FirmwareManifest? manifest,
    String? boardVersion,
    bool? boardOtaSupported,
    bool? otaLastBootFailed,
    String? otaFailedFirmwareVersion,
    String? otaFailedSlot,
    String? error,
    bool clearManifest = false,
    bool clearBoard = false,
    bool clearBoardOtaSupported = false,
    bool clearOtaRollback = false,
    bool clearError = false,
  }) {
    return FirmwareAvailState(
      loading: loading ?? this.loading,
      manifest: clearManifest ? null : (manifest ?? this.manifest),
      boardVersion: clearBoard ? null : (boardVersion ?? this.boardVersion),
      boardOtaSupported: clearBoardOtaSupported
          ? null
          : (boardOtaSupported ?? this.boardOtaSupported),
      otaLastBootFailed: clearOtaRollback
          ? null
          : (otaLastBootFailed ?? this.otaLastBootFailed),
      otaFailedFirmwareVersion: clearOtaRollback
          ? null
          : (otaFailedFirmwareVersion ?? this.otaFailedFirmwareVersion),
      otaFailedSlot: clearOtaRollback
          ? null
          : (otaFailedSlot ?? this.otaFailedSlot),
      error: clearError ? null : (error ?? this.error),
    );
  }
}

final firmwareUpdateAvailabilityProvider =
    NotifierProvider<FirmwareUpdateAvailabilityNotifier, FirmwareAvailState>(
  FirmwareUpdateAvailabilityNotifier.new,
);

class FirmwareUpdateAvailabilityNotifier extends Notifier<FirmwareAvailState> {
  @override
  FirmwareAvailState build() => const FirmwareAvailState();

  /// [manifestUrlOverride] — e.g. text from the field in the settings before saving to prefs.
  ///
  /// [skipBoardHttpFetch] — only manifest from Git (mobile has internet, board only BLE without known HTTP URL).
  ///
  /// [afterBleOtaAssumeBoardMatchesManifest] — after successful BLE stream OTA: reload manifest but board
  /// does not read via HTTP (restart/connection); it sets the version of the board to the version from the manifest (the bin was uploaded).
  Future<void> refresh({
    String? manifestUrlOverride,
    bool skipBoardHttpFetch = false,
    bool afterBleOtaAssumeBoardMatchesManifest = false,
  }) async {
    final prefs = ref.read(prefsRepositoryProvider);
    if (prefs.useMockBoard) {
      state = const FirmwareAvailState();
      return;
    }

    final manifestUrl =
        (manifestUrlOverride != null && manifestUrlOverride.trim().isNotEmpty)
            ? normalizeFirmwareManifestUrl(manifestUrlOverride.trim())
            : prefs.firmwareManifestUrlEffective;
    final session = ref.read(boardSessionNotifierProvider);
    final boardHttp = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
      bleStaIp: session.bleStaIp,
    );

    state = state.copyWith(loading: true, clearError: true);

    final api = ref.read(boardApiClientProvider);

    FirmwareManifest manifest;
    try {
      manifest = await api.fetchFirmwareManifest(manifestUrl);
    } catch (e) {
      state = FirmwareAvailState(
        loading: false,
        manifest: state.manifest,
        boardVersion: state.boardVersion,
        boardOtaSupported: state.boardOtaSupported,
        otaLastBootFailed: state.otaLastBootFailed,
        otaFailedFirmwareVersion: state.otaFailedFirmwareVersion,
        otaFailedSlot: state.otaFailedSlot,
        error: 'Manifest ($manifestUrl): $e',
      );
      return;
    }

    String? bv;
    bool? otaSup;
    bool? otaLbf;
    String? otaFfv;
    String? otaFs;
    String? err;
    if (!skipBoardHttpFetch && boardHttp != null && boardHttp.isNotEmpty) {
      try {
        final info = await api.fetchBoardFirmwareInfo(boardHttp);
        bv = info.version;
        otaSup = info.otaSupported;
        otaLbf = info.otaLastBootFailed;
        otaFfv = info.otaFailedFirmwareVersion;
        otaFs = info.otaFailedSlot;
      } catch (e) {
        err = 'Board HTTP ($boardHttp): $e';
      }
    }

    final String? boardVerOut;
    final bool? otaSupOut;
    final bool? otaLbfOut;
    final String? otaFfvOut;
    final String? otaFsOut;
    if (!skipBoardHttpFetch && boardHttp != null && boardHttp.isNotEmpty) {
      boardVerOut = bv;
      otaSupOut = otaSup;
      otaLbfOut = otaLbf;
      otaFfvOut = otaFfv;
      otaFsOut = otaFs;
    } else if (skipBoardHttpFetch && afterBleOtaAssumeBoardMatchesManifest) {
      boardVerOut = manifest.version;
      otaSupOut = state.boardOtaSupported;
      otaLbfOut = false;
      otaFfvOut = null;
      otaFsOut = null;
    } else if (skipBoardHttpFetch) {
      boardVerOut = state.boardVersion;
      otaSupOut = state.boardOtaSupported;
      otaLbfOut = state.otaLastBootFailed;
      otaFfvOut = state.otaFailedFirmwareVersion;
      otaFsOut = state.otaFailedSlot;
    } else {
      boardVerOut = bv;
      otaSupOut = otaSup;
      otaLbfOut = otaLbf;
      otaFfvOut = otaFfv;
      otaFsOut = otaFs;
    }

    state = FirmwareAvailState(
      loading: false,
      manifest: manifest,
      boardVersion: boardVerOut,
      boardOtaSupported: otaSupOut,
      otaLastBootFailed: otaLbfOut,
      otaFailedFirmwareVersion: otaFfvOut,
      otaFailedSlot: otaFsOut,
      error: err,
    );
  }
}
