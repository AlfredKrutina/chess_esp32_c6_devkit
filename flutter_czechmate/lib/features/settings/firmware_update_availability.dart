import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
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
    this.error,
  });

  final bool loading;
  final FirmwareManifest? manifest;
  final String? boardVersion;
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

  FirmwareAvailState copyWith({
    bool? loading,
    FirmwareManifest? manifest,
    String? boardVersion,
    String? error,
    bool clearManifest = false,
    bool clearBoard = false,
    bool clearError = false,
  }) {
    return FirmwareAvailState(
      loading: loading ?? this.loading,
      manifest: clearManifest ? null : (manifest ?? this.manifest),
      boardVersion: clearBoard ? null : (boardVersion ?? this.boardVersion),
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
  Future<void> refresh({String? manifestUrlOverride}) async {
    final prefs = ref.read(prefsRepositoryProvider);
    if (prefs.useMockBoard) {
      state = const FirmwareAvailState();
      return;
    }

    final manifestUrl = (manifestUrlOverride != null &&
            manifestUrlOverride.trim().isNotEmpty)
        ? manifestUrlOverride.trim()
        : prefs.firmwareManifestUrlEffective;
    final session = ref.read(boardSessionNotifierProvider);
    final boardHttp = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
    );

    state = state.copyWith(loading: true, clearError: true);

    try {
      final api = ref.read(boardApiClientProvider);
      final manifest = await api.fetchFirmwareManifest(manifestUrl);
      String? bv;
      if (boardHttp != null && boardHttp.isNotEmpty) {
        try {
          final info = await api.fetchBoardFirmwareInfo(boardHttp);
          bv = info.version;
        } catch (_) {
          bv = null;
        }
      }
      state = FirmwareAvailState(
        loading: false,
        manifest: manifest,
        boardVersion: bv,
      );
    } catch (e) {
      state = FirmwareAvailState(
        loading: false,
        manifest: state.manifest,
        boardVersion: state.boardVersion,
        error: '$e',
      );
    }
  }
}
