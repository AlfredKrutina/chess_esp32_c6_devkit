import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/constants/firmware_defaults.dart';
import '../../core/models/board_firmware_models.dart';
import '../../core/utils/board_http_base_url.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Stav kontroly manifestu vs deska (bez automatické sítě při build — volá [refresh]).
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

  /// Deska hlásí `ota_last_boot_failed` z `GET /api/system/firmware`.
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

  /// Stejná semver jako manifest (deska i manifest známé).
  bool get sameSemverAsManifest {
    final m = manifest;
    final b = boardVersion;
    if (m == null || !hasBoardVersion) {
      return false;
    }
    return compareSemverLoose(m.version, b!) == 0;
  }

  /// Manifest ze zdroje je starší než hlášená verze desky (oba známé).
  bool get manifestOlderThanBoard {
    final m = manifest;
    final b = boardVersion;
    if (m == null || !hasBoardVersion) {
      return false;
    }
    return compareSemverLoose(m.version, b!) < 0;
  }

  /// Manifest z Gitu je platný a buď je novější než deska, nebo verzi desky neznáme (jen BLE / bez HTTP).
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

  /// OTA akce z manifestu (včetně vývojářského znovu‑flash stejné nebo starší verze).
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

  /// [manifestUrlOverride] — např. text z pole v nastavení ještě před uložením do prefs.
  ///
  /// [skipBoardHttpFetch] — jen manifest z Gitu (mobil má internet, deska jen BLE bez známé HTTP URL).
  ///
  /// [afterBleOtaAssumeBoardMatchesManifest] — po úspěšném BLE stream OTA: znovu načte manifest, ale desku
  /// nečte přes HTTP (restart/spojení); verzi desky nastaví na verzi z manifestu (nahrál se ten bin).
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
