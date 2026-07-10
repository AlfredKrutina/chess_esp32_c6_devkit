// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'CzechMate';

  @override
  String get navPlay => 'Play';

  @override
  String get navAnalysis => 'Analysis';

  @override
  String get navPuzzle => 'Puzzles';

  @override
  String get navProgress => 'Training';

  @override
  String get navSettings => 'Settings';

  @override
  String get settingsLanguage => 'Language';

  @override
  String get languageSystem => 'System';

  @override
  String get languageEnglish => 'English';

  @override
  String get languageCzech => 'Czech';

  @override
  String get languageDescription =>
      'Choose app language. System follows the device; unsupported languages default to English.';

  @override
  String get onboardingWelcomeTitle => 'Welcome to CZECHMATE';

  @override
  String get onboardingSkip => 'Skip';

  @override
  String get onboardingNext => 'Next';

  @override
  String get onboardingStart => 'Start playing';

  @override
  String get onboard1Title => 'Smart chessboard';

  @override
  String get onboard1Body =>
      'Connect your CZECHMATE board over Wi‑Fi or Bluetooth and play with live sync.';

  @override
  String get onboard2Title => 'Set up the pieces';

  @override
  String get onboard2Body =>
      'Before powering on, place pieces in the starting position. The app will align with the board automatically.';

  @override
  String get onboard3Title => 'LED hints';

  @override
  String get onboard3Body =>
      'The board can highlight moves with LEDs. Hint depth is configured under Settings → Board NVS.';

  @override
  String get onboard4Title => 'Start a game';

  @override
  String get onboard4Body =>
      'On Play, tap New game and pick a time control. Flip the board (White or Black on bottom) from Game controls or Settings.';

  @override
  String get onboard5Title => 'Coach & analysis';

  @override
  String get onboard5Body =>
      'Turn on learning mode and Stockfish evaluation on Analysis for deeper insight.';

  @override
  String get netNoInternetTitle => 'No internet';

  @override
  String get netNoInternetBody =>
      'Stockfish (chess-api), Lichess, and other cloud features need internet. You can still use the board over Bluetooth offline.';

  @override
  String get netNotOnWifiTitle => 'Not on Wi‑Fi';

  @override
  String get netNotOnWifiBody =>
      'Cloud requests (chess-api, Lichess) usually work over any internet connection your device has (Wi‑Fi or cellular). To open the board’s HTTP API on a LAN IP, this device typically needs Wi‑Fi on the same network as the board. If you are only on the board’s hotspot without internet, DNS for chess-api / Lichess may fail — temporarily disable that Wi‑Fi, use another connection with internet, or dual‑stack (Wi‑Fi + cellular) if your OS allows. The app cannot force traffic off Wi‑Fi while Wi‑Fi is the default route.';

  @override
  String get errInvalidBoardUrl =>
      'Invalid board URL — hostname missing. Use http://192.168.4.1 or your board STA IP.';

  @override
  String errWifiIpThirdOctetBlocked(String host) {
    return 'Address $host is blocked by your Wi‑Fi third-octet filter. Change the list under Advanced connection, enter another URL, or adjust DHCP on your router so the board avoids that subnet.';
  }

  @override
  String get errBoardSnapshotUnreachable =>
      'Could not load the board snapshot over Wi‑Fi. Check the URL and network, or connect via Bluetooth.';

  @override
  String get errNoSavedBle =>
      'No saved Bluetooth board. Open Board discovery and pair your CZECHMATE board.';

  @override
  String get errBleHostUnsupported =>
      'Bluetooth LE is not available in this app build (Windows desktop). Connect via Wi‑Fi: use Settings → Connection and enter your board URL, or use the Android / iOS app for BLE.';

  @override
  String get discoveryDesktopBleUnavailableTitle =>
      'Bluetooth scan not available here';

  @override
  String get discoveryDesktopBleUnavailableBody =>
      'This Windows build does not ship a Bluetooth Low Energy stack for the board. Use Wi‑Fi instead: open Advanced below or Settings → Connection and enter the board HTTP URL on your LAN. For BLE pairing and scan, use the Android or iOS app.';

  @override
  String get settingsBleDesktopHint =>
      'On Windows, connect via Wi‑Fi URL (below). BLE discovery is not available in this build.';

  @override
  String get errDemoNoSnapshot => 'Demo board has no snapshot.';

  @override
  String get errConnectBeforeMoves =>
      'Connect to the board over Wi‑Fi or Bluetooth before sending moves from the app.';

  @override
  String get errSetupNeedsConnection =>
      'Setup wizard requires an active board connection (Wi‑Fi or Bluetooth).';

  @override
  String get errOtaHttps =>
      'OTA URL must start with https:// (internet) or http:// (e.g. this device on the board hotspot).';

  @override
  String get errOtaBleTransport =>
      'Firmware upload over Bluetooth requires an active Bluetooth session (connect via Bluetooth, not Wi‑Fi‑only).';

  @override
  String get errOtaBleGatt =>
      'Bluetooth GATT is not connected — reconnect to the board from Play / Board discovery.';

  @override
  String get errOtaMock => 'OTA is not available with the mock board.';

  @override
  String get errOtaConnectFirst =>
      'Connect the board over Wi‑Fi or Bluetooth, then try the update again.';

  @override
  String get errOtaBoardHttpMissingDetail =>
      'Board HTTP URL is missing (AP or STA IP). Save it under Default board URL — status polling needs it alongside Bluetooth.';

  @override
  String get errOtaNoOtaPartitions =>
      'This board has no OTA partitions (ota_0 + ota_1). Reflash with a dual‑OTA partition layout or update via USB.';

  @override
  String get errOtaStaRequiredForHttps =>
      'Connect the board to Wi‑Fi as a station (STA) so it can download firmware over HTTPS.';

  @override
  String errOtaWifiStatusCheckFailed(String error) {
    return 'Could not verify Wi‑Fi status: $error';
  }

  @override
  String get errOtaPollUnreachable =>
      'Could not read firmware update status from the board. Check the saved board HTTP URL (e.g. http://192.168.4.1).';

  @override
  String get errOtaPollTimeout =>
      'Timed out waiting for the firmware update to finish.';

  @override
  String errOtaBoardReported(String message) {
    return 'Firmware update: $message';
  }

  @override
  String get errHintsNeedConnection =>
      'Hint LEDs require a board connection. Pair via Bluetooth or connect Wi‑Fi first.';

  @override
  String get errNoActiveSession =>
      'No active board session. Connect via Wi‑Fi or Bluetooth from Board discovery.';

  @override
  String get errBrightnessNeedsBoard =>
      'Brightness control needs a connected board (Wi‑Fi or Bluetooth).';

  @override
  String get errLampNeedsBoard =>
      'Lamp control needs a connected board (Wi‑Fi or Bluetooth).';

  @override
  String get errGameLampNeedsBoard =>
      'Game lamp mode requires a connected board (Wi‑Fi or Bluetooth).';

  @override
  String get errAutoLampNeedsBoard =>
      'Auto lamp timeout requires a connected board (Wi‑Fi or Bluetooth).';

  @override
  String get moveEvalExcellentBest =>
      'Excellent move — that was the best move.';

  @override
  String get moveEvalExcellentEngine =>
      'Excellent — matches the engine best move.';

  @override
  String moveEvalWeakerNoScore(String best) {
    return 'Weaker move (no exact score). Stronger was $best.';
  }

  @override
  String moveEvalGoodLoss(int cp) {
    return 'Good move — small loss (~$cp cp).';
  }

  @override
  String moveEvalInaccuracy(int cp, String best) {
    return 'Inaccuracy — about $cp cp worse. Stronger was $best.';
  }

  @override
  String moveEvalMistake(int cp, String best) {
    return 'Mistake — about $cp cp worse. Better was $best.';
  }

  @override
  String moveEvalBlunder(int cp, String best) {
    return 'Blunder — about $cp cp worse. Much better was $best.';
  }

  @override
  String moveEvalFailed(String error) {
    return 'Evaluation failed: $error';
  }

  @override
  String get pgnEventHeader => 'CZECHMATE';

  @override
  String get pgnOpeningPrefix => 'Start';

  @override
  String get sharePgnSubject => 'Chess game (PGN)';

  @override
  String get shareSummaryNotReady =>
      'Summary isn’t ready yet — try again in a moment.';

  @override
  String shareExportFailed(String error) {
    return 'Export failed: $error';
  }

  @override
  String get sharePngEncodeError => 'Could not encode image as PNG.';

  @override
  String shareGameSummaryMeta(String result, String duration, int moves) {
    return '$result · $duration · $moves moves';
  }

  @override
  String shareGameSummaryLine(String meta) {
    return 'Game summary — CzechMate · $meta';
  }

  @override
  String get reportNoGameData => 'No game data.';

  @override
  String get reportMoveEvaluation => 'Move evaluation';

  @override
  String get reportNoStockfishData =>
      'No Stockfish data — enable move evaluation and play a game.';

  @override
  String get reportEvalPerspective =>
      'From White’s perspective (+ favors White)';

  @override
  String get reportMoveTiming => 'Move timing';

  @override
  String get reportTimingUnavailable =>
      'Move timestamps weren’t included — timing charts are unavailable.';

  @override
  String get reportElapsedTime => 'Elapsed time';

  @override
  String get reportElapsedCaption =>
      'Running total from game start by half-move';

  @override
  String get reportHalfMoveAxis => 'half-move → minutes';

  @override
  String get reportTimePerMove => 'Time per move';

  @override
  String get reportSecondsSincePrev => 'Seconds since previous move';

  @override
  String get colorWhite => 'White';

  @override
  String get colorBlack => 'Black';

  @override
  String reportMaterialCaptured(int w, int b) {
    return 'Material captured · White $w pts · Black $b pts';
  }

  @override
  String get reportAppBarTitle => 'Game summary';

  @override
  String get reportDone => 'Done';

  @override
  String get reportResultHeading => 'Result';

  @override
  String get reportShareHeading => 'Share';

  @override
  String get reportExportAdvancedTitle => 'Advanced layout';

  @override
  String get reportExportAdvancedSubtitle =>
      'Transparency, which sections appear, and their order. Shape comes from the layout thumbnails above.';

  @override
  String get reportSharePreparing => 'Preparing…';

  @override
  String get reportShareSummaryImage => 'Share…';

  @override
  String get reportCopySummaryImage => 'Copy image';

  @override
  String get reportCopySummaryBusy => 'Copying…';

  @override
  String get reportSummaryCopiedSnack => 'Copied — paste into another app.';

  @override
  String get reportCopyImageFailed => 'Couldn’t copy the image.';

  @override
  String get reportCopyImageWebHint =>
      'Image clipboard isn’t available in the browser — use Share below.';

  @override
  String get reportCopyImageDesktopFallback =>
      'Sharing instead — full image clipboard works best on phones and tablets (iPhone & Android); on desktop, use Share.';

  @override
  String get reportExportSectionOrderTitle => 'Section order (export)';

  @override
  String get reportExportSectionOrderSubtitle => '';

  @override
  String get reportExportBlockRecap => 'Recap caption';

  @override
  String get reportExportBlockBranding => 'Branding';

  @override
  String get reportExportBlockResult => 'Result & reason';

  @override
  String get reportExportBlockStats => 'Time & move stats';

  @override
  String get reportExportBlockMaterial => 'Material captured';

  @override
  String get reportExportBlockBoard => 'Board';

  @override
  String get reportExportBlockEval => 'Evaluation chart';

  @override
  String get reportExportBlockTiming => 'Timing charts';

  @override
  String get reportSharePgn => 'Share PGN file';

  @override
  String get reportExportAppearanceTitle => 'Export image';

  @override
  String get reportExportAppearanceSubtitle =>
      'Choose a layout below — the preview updates. Chart colors and Advanced options stay available.';

  @override
  String get reportExportStylePickHint =>
      'Tap a layout to match how you want to share (feed card, square, story, or wide).';

  @override
  String get reportChartPaletteChoiceHint =>
      'Preset affects eval + timing charts in Analysis and exports. Custom opens labeled rows for each chart element.';

  @override
  String get reportExportAspectRatio => 'Canvas size';

  @override
  String get reportExportAspectCard => 'Card (tall)';

  @override
  String get reportExportAspectSquare => 'Square 1:1';

  @override
  String get reportExportAspectStory => 'Story 9:16';

  @override
  String get reportExportAspectLandscape => 'Landscape 16:9';

  @override
  String get reportExportTransparentBg => 'Transparent background';

  @override
  String get reportExportTransparentHint =>
      'Only the outer frame uses alpha; board squares and charts stay on solid panels.';

  @override
  String get reportChartPaletteTitle => 'Chart colors';

  @override
  String get reportChartPaletteSubtitle =>
      'Bold presets for eval and timing charts — also used in Analysis.';

  @override
  String get reportChartPaletteTheme => 'Match app';

  @override
  String get reportChartPaletteNeon => 'Neon';

  @override
  String get reportChartPaletteSunset => 'Sunset';

  @override
  String get reportChartPaletteOcean => 'Ocean';

  @override
  String get reportChartPaletteCustom => 'Custom';

  @override
  String get reportChartPaletteEditCustom => 'Edit custom colors';

  @override
  String get reportChartPaletteCustomTitle => 'Chart colors';

  @override
  String get reportChartPaletteEvalLine => 'Evaluation line';

  @override
  String get reportChartPaletteCumulative => 'Elapsed time';

  @override
  String get reportChartPaletteBarWhite => 'White’s bars';

  @override
  String get reportChartPaletteBarBlack => 'Black’s bars';

  @override
  String get reportChartPaletteDone => 'Done';

  @override
  String get reportExportShowBranding => 'Show CzechMate header';

  @override
  String get reportExportShowStats => 'Show time & move stats';

  @override
  String get reportExportShowFinalBoard => 'Show final position';

  @override
  String get reportExportFlipBoard => 'Flip board (Black at bottom)';

  @override
  String get reportExportShowEvalChart => 'Include evaluation chart';

  @override
  String get reportExportShowCumulativeChart => 'Include elapsed-time chart';

  @override
  String get reportExportShowPerMoveChart => 'Include time-per-move bars';

  @override
  String get reportExportPresetFull => 'Full';

  @override
  String get reportExportPresetMinimal => 'Minimal';

  @override
  String get reportExportPresetStory => 'Story hero';

  @override
  String get reportExportThumbWideLabel => 'Wide';

  @override
  String get reportExportThumbWideAspect => '16∶9';

  @override
  String get reportExportShareGifRecap => 'Share move recap (GIF)';

  @override
  String get reportExportGifSubtitle =>
      'Animated recap for Stories or chats (9:16). Long games are sampled.';

  @override
  String get reportExportGifBuilding => 'Building animation…';

  @override
  String get reportExportGifShareSubject => 'Game recap (GIF)';

  @override
  String reportExportGifFailed(String error) {
    return 'GIF export failed: $error';
  }

  @override
  String reportExportRecapMove(int current, int total) {
    return 'Move $current / $total';
  }

  @override
  String get reportClock => 'Clock';

  @override
  String reportMinutesShort(String n) {
    return '$n min';
  }

  @override
  String get reportFinalFen => 'Final FEN';

  @override
  String get resultWhiteWins => 'White wins';

  @override
  String get resultBlackWins => 'Black wins';

  @override
  String get resultDraw => 'Draw';

  @override
  String get resultGameEnded => 'Game ended';

  @override
  String get endReasonUnknown => 'Unknown result';

  @override
  String get endReasonGameOver => 'Game over';

  @override
  String get endReasonCheckmate => 'Checkmate';

  @override
  String get endReasonStalemate => 'Stalemate';

  @override
  String get endReasonFinished => 'Finished';

  @override
  String get statTime => 'TIME';

  @override
  String get statMoves => 'MOVES';

  @override
  String get statWhiteAvg => 'WHITE Ø';

  @override
  String get statBlackAvg => 'BLACK Ø';

  @override
  String get durationDash => '—';

  @override
  String secondsShort(int n) {
    return '${n}s';
  }

  @override
  String minutesSeconds(int m, String s) {
    return '$m:$s';
  }

  @override
  String hoursMinutesSeconds(int h, int mm, int s) {
    return '${h}h ${mm}m ${s}s';
  }

  @override
  String get commonClose => 'Close';

  @override
  String get commonCancel => 'Cancel';

  @override
  String get commonOk => 'OK';

  @override
  String get commonSave => 'Save';

  @override
  String get commonContinue => 'Continue';

  @override
  String get commonRetry => 'Retry';

  @override
  String get commonLoading => 'Loading…';

  @override
  String get commonError => 'Error';

  @override
  String get commonYes => 'Yes';

  @override
  String get commonNo => 'No';

  @override
  String get lastErrorTitle => 'Connection issue';

  @override
  String get telemetryPrivacyTitle => 'Privacy';

  @override
  String get telemetryPrivacyBody =>
      'This build does not send analytics. Traffic goes only to your board on the local network or over Bluetooth.';

  @override
  String get telemetryIcloudTitle => 'iCloud';

  @override
  String get telemetryIcloudBody =>
      'Game data stays on device unless you export or enable sync elsewhere.';

  @override
  String get discoveryHelpConnectingTitle => 'Need help connecting?';

  @override
  String get discoveryHelpConnectingSubtitle => 'Setup steps and scanning tips';

  @override
  String get discoveryTitle => 'Find board';

  @override
  String get discoveryBlePermissionAndroid =>
      'Allow Bluetooth permissions in system settings to scan for BLE boards.';

  @override
  String get discoveryBlePermissionDenied =>
      'Bluetooth access is required to scan for the board. Allow it in system Settings, then try again.';

  @override
  String get discoveryBluetoothNotReady =>
      'Bluetooth is not ready. Turn it on in Settings, wait a few seconds, then tap Find board again.';

  @override
  String discoveryBleScanFailed(String error) {
    return 'Bluetooth scan failed: $error';
  }

  @override
  String get discoveryIntro =>
      'Bluetooth connects first. At home on the same Wi‑Fi, the app switches to faster HTTP when it can reach the board.';

  @override
  String get discoveryFlowStepsTitle => 'Three steps';

  @override
  String get discoveryFlowStep1 => 'Turn on Bluetooth on this device.';

  @override
  String get discoveryFlowStep2 =>
      'Tap Find board and pick your board from the list.';

  @override
  String get discoveryFlowStep3 =>
      'At home on the same Wi‑Fi as the board, the app switches to faster HTTP when possible; elsewhere it stays on Bluetooth.';

  @override
  String get discoveryScanCardLead =>
      'Use the button below to search nearby. To type an address instead, open Advanced.';

  @override
  String get discoveryRecoveryTitle => 'Could not connect';

  @override
  String get discoveryRecoveryBulletBt =>
      'Bluetooth is on and the board is powered on nearby.';

  @override
  String get discoveryRecoveryBulletRange =>
      'Move closer — walls and pockets sometimes weaken the signal.';

  @override
  String get discoveryRecoveryBulletHomeWifi =>
      'At home, your device and the board work best on the same Wi‑Fi for HTTP; away from home, Bluetooth alone is fine.';

  @override
  String get discoveryRecoveryBulletManual =>
      'You can enter the board address under Advanced.';

  @override
  String get discoveryRecoveryOpenAppSettings => 'Open system settings';

  @override
  String get discoveryRecoveryDismiss => 'Got it';

  @override
  String get transportDisconnected => 'Disconnected';

  @override
  String get transportDemo => 'Demo mode';

  @override
  String get transportWifi => 'Wi‑Fi';

  @override
  String get transportBluetooth => 'Bluetooth';

  @override
  String transportConnecting(String transport) {
    return '$transport · connecting…';
  }

  @override
  String get defaultBoardName => 'CZECHMATE board';

  @override
  String get discoveryWifiUrlHint => 'e.g. http://192.168.4.1';

  @override
  String get discoveryConnectWifi => 'Connect via Wi‑Fi';

  @override
  String get discoveryFindBle => 'Find board (Bluetooth)';

  @override
  String get discoveryScanning => 'Scanning…';

  @override
  String get discoveryStopScan => 'Stop scan';

  @override
  String get discoveryManualUrl => 'Manual URL entry';

  @override
  String get connectionModeAuto => 'Auto (Bluetooth then Wi‑Fi)';

  @override
  String get connectionModeWifiOnly => 'Wi‑Fi only';

  @override
  String get connectionModeBleOnly => 'Bluetooth only';

  @override
  String get preferBleOnlyTitle => 'Bluetooth only (don’t switch to Wi‑Fi)';

  @override
  String get preferBleOnlySubtitle =>
      'After BLE connect, stay on Bluetooth even if the board knows a Wi‑Fi URL.';

  @override
  String get mockBoardTileTitle => 'Demo board (no hardware)';

  @override
  String get mockBoardTileSubtitle => 'Uses bundled snapshot for UI testing.';

  @override
  String get sessionConnectedWifi => 'Connected via Wi‑Fi';

  @override
  String get sessionConnectedBle => 'Connected via Bluetooth';

  @override
  String get sessionConnectedMock => 'Demo board active';

  @override
  String get allowBluetoothSettings =>
      'Allow Bluetooth permissions in settings to scan for BLE boards.';

  @override
  String get gameAppBarTitle => 'czechmate';

  @override
  String get gameConnectionTooltip => 'Board connection';

  @override
  String get gameNewGame => 'New game';

  @override
  String get gameHintThinking => 'Thinking…';

  @override
  String get gameHint => 'Hint';

  @override
  String get gamePauseClock => 'Pause clock';

  @override
  String get gameResumeClock => 'Resume clock';

  @override
  String get gameClearHintLeds => 'Hide board hint';

  @override
  String get gameHideBoardHintTooltip => 'Hide board hint';

  @override
  String get gameNoSnapshotYet => 'No board snapshot yet.';

  @override
  String get gameFindBoard => 'Find board';

  @override
  String get gameFindBoardSubtitle =>
      'Connect Wi‑Fi or Bluetooth to load the live position.';

  @override
  String get gameControlsTitle => 'Game controls';

  @override
  String get gameHistory => 'History';

  @override
  String get gameReturnLive => 'Return to live game';

  @override
  String get gameExitPuzzleCheck => 'Exit puzzle check (keep position)';

  @override
  String get gameFindBoardTooltip => 'Find board';

  @override
  String get gameLayoutTooltip => 'Layout mode';

  @override
  String get gameLayoutStandard => 'Standard';

  @override
  String get gameLayoutBoardOnly => 'Board only';

  @override
  String get gameCoachRailShow => 'Show AI coach column';

  @override
  String get gameCoachRailHide => 'Hide AI coach column';

  @override
  String get gameControlsTooltip => 'Game controls';

  @override
  String get gameAnalysisTooltip => 'Analysis';

  @override
  String get gameSummaryTooltip => 'Game summary';

  @override
  String get gameDisconnectTooltip => 'Disconnect session';

  @override
  String get gameHidePanelTooltip => 'Hide panel';

  @override
  String gamePuzzleMovesProgress(int played, int total) {
    return 'Moves $played / $total';
  }

  @override
  String gameHintTransient(String from, String to, String evalSuffix) {
    return 'Hint: $from→$to$evalSuffix';
  }

  @override
  String gameBestMoveSnack(String from, String to) {
    return 'Recommended move: $from to $to';
  }

  @override
  String gameHintFailed(String error) {
    return 'Hint failed: $error';
  }

  @override
  String gameDemoBoardSnack(String feature) {
    return 'Demo board: $feature only works over Bluetooth or Wi‑Fi to real hardware.';
  }

  @override
  String get gameClockPauseSent => 'Clock pause sent';

  @override
  String get gameClockResumedSnackbar => 'Clock resumed';

  @override
  String get gameHintLedsClearedSnackbar => 'Hint LEDs cleared';

  @override
  String get gameMatrixGuardTitle => 'Board alignment required';

  @override
  String gameMatrixGuardPaused(String squares) {
    return 'Align pieces using board LEDs$squares. Play resumes when matched.';
  }

  @override
  String gameMatrixGuardResync(String squares) {
    return 'Board mismatch after startup$squares. Align pieces using LEDs.';
  }

  @override
  String get gameMatrixGuardForceClear => 'Resume play';

  @override
  String get gameMatrixGuardForceClearHint =>
      'Only if pieces are already aligned';

  @override
  String get gameMatrixGuardForceClearSnack =>
      'Play resumed — verify the board matches the game';

  @override
  String get gameNoSnapshotBody =>
      'Tap top-left or below to find a board (Bluetooth). Wi‑Fi and more options are under advanced settings.';

  @override
  String get gameBackToPanelTooltip => 'Back to layout with panel';

  @override
  String get gameAiCoachTitle => 'AI coach';

  @override
  String get gameMoreOptionsTooltip => 'More options';

  @override
  String statusConnectedTransport(String transport) {
    return 'Connected ($transport)';
  }

  @override
  String statusConnectingTransport(String transport) {
    return 'Connecting ($transport)…';
  }

  @override
  String statusBoardNotRespondingTransport(String transport) {
    return 'Board not responding ($transport)';
  }

  @override
  String statusBleDevLine(String name) {
    return 'BLE: $name';
  }

  @override
  String get gameDesktopPlayTitle => 'Play — desktop';

  @override
  String get gameDesktopPlaySubtitle =>
      'Tap the indicator top-left to manage connection (Wi‑Fi, Bluetooth, mock) — same as iOS.';

  @override
  String stockfishEvalFailedSnack(String msg) {
    return 'Evaluation failed: $msg';
  }

  @override
  String get firmwareDialogTitle => 'Board firmware update';

  @override
  String firmwareDialogVersions(String serverVer, String boardVer) {
    return 'Server has $serverVer, your board reports $boardVer.';
  }

  @override
  String get firmwareDialogHttpsNote =>
      'The board downloads over HTTPS (Wi‑Fi STA). CzechMate on this device only sends the link.';

  @override
  String get firmwareTurnOffReminders => 'Turn off reminders';

  @override
  String get firmwareNotNow => 'Not now';

  @override
  String get firmwareUpdateAction => 'Update';

  @override
  String get discoveryCloseTooltip => 'Close';

  @override
  String get discoveryConnectionStatusTitle => 'Connection status';

  @override
  String get discoveryScanningBoards => 'Scanning for boards…';

  @override
  String discoveryBleDevicesDev(String guid) {
    return 'Bluetooth ($guid)';
  }

  @override
  String get discoveryFoundBoards => 'Found boards';

  @override
  String get discoveryEmptyBleList =>
      'No boards found yet. Wait a few seconds with Bluetooth on and a powered-on board nearby.';

  @override
  String get discoveryAutoScanHint =>
      'Scan starts automatically unless the next connection is Wi‑Fi-only (Advanced).';

  @override
  String get discoveryAdvancedSubtitle =>
      'Wi‑Fi address, saved Bluetooth, one-shot transport';

  @override
  String get discoveryAdvancedBlockedOctetsTitle =>
      'Wi‑Fi IP filter (third octet)';

  @override
  String get discoveryAdvancedBlockedOctetsBody =>
      'The board stores this list in NVS and rejects DHCP leases whose IPv4 third octet matches (disconnect + retry). CzechMate mirrors the same filter for URLs. Values are pushed over encrypted Bluetooth when you save or connect via BLE — the firmware does not ship with a blocklist until the app sends one. Example: 88 rejects every x.x.88.x. Empty field disables blocking on both sides. Until you save anything else, the app defaults to 88.';

  @override
  String get discoveryAdvancedBlockedOctetsLabel =>
      'Blocked third octets (comma-separated)';

  @override
  String get discoveryAdvancedBlockedOctetsHint => 'e.g. 88 or 88,100';

  @override
  String get discoveryAdvancedBlockedOctetsSave => 'Save filter';

  @override
  String get discoveryAdvancedBlockedOctetsReset => 'Reset to default (88)';

  @override
  String get discoveryAdvancedBlockedOctetsSavedSnack =>
      'Wi‑Fi IP filter saved';

  @override
  String get discoveryTransportAdvancedExplanation =>
      'The app defaults to automatic routing (Bluetooth first, then Wi‑Fi when the board answers over HTTP). To force just one upcoming connection over Wi‑Fi or Bluetooth, use Settings → Board connection → Advanced — it always returns to automatic after that attempt finishes.';

  @override
  String get discoveryOpenTransportSettingsButton => 'Open connection settings';

  @override
  String get discoveryConnectionModeHeading => 'Connection mode';

  @override
  String get discoveryConnectionModeHint =>
      'Auto: Bluetooth first, then Wi‑Fi when the board is reachable. Wi‑Fi only skips Bluetooth. Bluetooth only stays on Bluetooth even if a Wi‑Fi URL is known.';

  @override
  String get discoveryBleSegmentShort => 'BLE only';

  @override
  String get discoveryReconnectSavedBoard => 'Reconnect saved board';

  @override
  String get discoveryManualAdvanced => 'Manual entry / advanced';

  @override
  String get discoveryWifiUrlLabel => 'Board address (Wi‑Fi)';

  @override
  String get discoveryEnterBoardUrlSnack =>
      'Enter the board URL or IP (e.g. http://192.168.4.1).';

  @override
  String get discoveryBoardHotspotButton => 'Board hotspot (192.168.4.1)';

  @override
  String get discoveryBoardApEnable => 'Turn on board Wi‑Fi hotspot';

  @override
  String get discoveryBoardApDisable => 'Turn off board Wi‑Fi hotspot';

  @override
  String get discoveryBoardApSubtitle =>
      'This device can join the board network for HTTP or OTA. The board ships with this hotspot off.';

  @override
  String get discoveryBoardApCommandSent =>
      'Hotspot command sent — wait a few seconds, then check Wi‑Fi networks.';

  @override
  String discoveryBoardApError(String error) {
    return 'Could not change the hotspot: $error';
  }

  @override
  String get errBoardApNeedsBle =>
      'Bluetooth must be connected to change the board hotspot.';

  @override
  String get boardWifiProvisionSheetTitle => 'Connect the board to Wi‑Fi';

  @override
  String get boardWifiProvisionSheetLead =>
      'Use the same router Wi‑Fi as this device when possible (often 2.4 GHz works best). CzechMate never reads your saved Wi‑Fi password from the system — enter it here once.';

  @override
  String get boardWifiProvisionIosSsidHint =>
      'If the current Wi‑Fi network name stays hidden (common on Apple devices until Location access is allowed for Wi‑Fi info), type the SSID manually.';

  @override
  String get boardWifiProvisionScanBoardButton => 'Scan Wi‑Fi around the board';

  @override
  String get boardWifiProvisionScanning => 'Scanning…';

  @override
  String boardWifiProvisionVisibleYes(String ssid) {
    return 'This device’s Wi‑Fi network “$ssid” appears in the board scan — you can enter the password and connect.';
  }

  @override
  String get boardWifiProvisionVisibleNo =>
      'That SSID didn’t show up in the board scan — check distance, or try a 2.4 GHz network name. You can still try; play can continue over Bluetooth.';

  @override
  String get boardWifiProvisionSurveyFailed =>
      'Board scan failed or timed out.';

  @override
  String get boardWifiProvisionDoneSnack =>
      'Wi‑Fi credentials sent — wait until the board joins the network.';

  @override
  String get boardWifiProvisionStaTimeoutHint =>
      'No STA connection yet — the board may not see that network. You can keep using Bluetooth or enable the board hotspot from Board discovery.';

  @override
  String get boardWifiProvisionFirmwareContextHint =>
      'Same Wi‑Fi setup as the sheet shown after connecting from Board discovery — one shared flow, not two different settings.';

  @override
  String get otaHttpsStaGateTitle =>
      'Internet firmware download needs router Wi‑Fi';

  @override
  String get otaHttpsStaGateBody =>
      'HTTPS updates are downloaded by the board itself. Connect the board to a home/access Wi‑Fi that has internet (2.4 GHz is usually the most compatible). Turning on the board hotspot helps this device talk to the board over HTTP, but it does not give the board internet by itself — use Bluetooth firmware upload below, or connect the board to router Wi‑Fi first.';

  @override
  String get otaHttpsStaGateBleUpload => 'Use Bluetooth firmware upload';

  @override
  String get otaHttpsStaGateHotspotBle => 'Turn on board hotspot (Bluetooth)';

  @override
  String get otaHttpsStaGateWifiTips => '2.4 GHz Wi‑Fi tips';

  @override
  String get otaHttpsStaGateCancel => 'Not now';

  @override
  String get otaHttpsWifiTipsTitle => 'Router Wi‑Fi tips';

  @override
  String get otaHttpsWifiTipsBody =>
      'Place the board in range of the router. Prefer the 2.4 GHz band if your router uses different names for 2.4 GHz and 5 GHz. After the board shows connected on your network, try the update again.';

  @override
  String get otaHttpsHotspotEnabledSnack =>
      'Hotspot command sent — join the board Wi‑Fi on this device if you need HTTP access. HTTPS download still needs the board on router Wi‑Fi with internet.';

  @override
  String get otaHttpsBleUploadHintSnack =>
      'Scroll down in Firmware update and use “Upload firmware via Bluetooth”.';

  @override
  String get connectionModeAutoShort => 'Auto';

  @override
  String get settingsOverviewTitle => 'Overview';

  @override
  String get settingsOverviewSubtitleError => 'Last error — expand for details';

  @override
  String get settingsOverviewSubtitleOk => 'Shortcuts and connection status';

  @override
  String get settingsOverviewBody =>
      'Game and board options apply to the Play tab and the Game controls panel — one set of toggles.';

  @override
  String get settingsGoToPlayTab => 'Go to Play tab';

  @override
  String get settingsConnectionTitle => 'Board connection';

  @override
  String settingsConnectionSubtitle(String tier, String transport) {
    return '$tier · $transport';
  }

  @override
  String get settingsConnectionLearnMore => 'How connecting works';

  @override
  String get settingsDisconnect => 'Disconnect';

  @override
  String get settingsAdvanced => 'Advanced';

  @override
  String get settingsAdvancedConnectionSubtitle =>
      'Default URL, connection mode, saved BLE';

  @override
  String get settingsAdvancedConnectionSubtitleV2 =>
      'Default URL, next connection override, saved BLE';

  @override
  String get settingsReconnectingBle =>
      'Reconnecting Bluetooth to saved board…';

  @override
  String get settingsReconnectSavedBleShort => 'Reconnect saved board (BLE)';

  @override
  String get settingsManualEntry => 'Manual entry';

  @override
  String get settingsSavedDefaultsTitle => 'Saved defaults';

  @override
  String get settingsSavedDefaultsBody =>
      'Used on next connection from “Find board” or after reconnecting.';

  @override
  String get settingsConnectionModeLabel => 'Connection mode';

  @override
  String get settingsConnectionModeAutoBleWifi => 'Auto (BLE → Wi‑Fi)';

  @override
  String get settingsNextConnectionTitle => 'Next connection (one-shot)';

  @override
  String get settingsNextConnectionIntro =>
      'Default is automatic. These apply only to the next connect attempt (Find board, reconnect, or resume), then the app returns to automatic so nothing stays stuck.';

  @override
  String get settingsNextConnectionWifiOnceButton => 'Next: Wi‑Fi only';

  @override
  String get settingsNextConnectionBleOnceButton => 'Next: Bluetooth only';

  @override
  String get settingsNextConnectionUseAuto => 'Use automatic';

  @override
  String get settingsNextConnectionActiveWifiOnce =>
      'Next attempt will skip Bluetooth and use saved Wi‑Fi URL only.';

  @override
  String get settingsNextConnectionActiveBleOnce =>
      'Next attempt keeps Bluetooth and will not hand off to Wi‑Fi automatically.';

  @override
  String get settingsDefaultBoardUrl => 'Default board URL';

  @override
  String get settingsInvalidUrlSnack =>
      'Invalid URL — enter a host (e.g. 192.168.4.1 or http://…)';

  @override
  String get settingsSavedSnack => 'Saved';

  @override
  String get settingsSaveUrl => 'Save URL';

  @override
  String get settingsBoardApiTokenLabel => 'Board API token';

  @override
  String get settingsBoardApiTokenSubtitle =>
      '64 hex characters from UART API_TOKEN — required for admin Wi‑Fi actions and OTA when the board enforces auth.';

  @override
  String get settingsSaveBoardApiToken => 'Save token';

  @override
  String get settingsBoardApiTokenSavedSnack => 'API token saved';

  @override
  String get settingsBoardApiTokenClearedSnack => 'API token cleared';

  @override
  String get settingsBleOnlyTitle => 'Bluetooth only';

  @override
  String get settingsBleOnlySubtitle => 'Don’t switch to Wi‑Fi after BLE';

  @override
  String get settingsWebSocketTitle => 'WebSocket snapshot';

  @override
  String get settingsWebSocketSubtitle =>
      'Reconnect Wi‑Fi session after change';

  @override
  String get settingsAppFactoryResetTitle => 'Reset app on this device';

  @override
  String get settingsAppFactoryResetSubtitle =>
      'Restores the app to its defaults and clears saved data on this device. Your chessboard is not changed.';

  @override
  String get settingsAppFactoryResetButton => 'Reset app data';

  @override
  String get settingsAppFactoryResetConfirmTitle => 'Reset now?';

  @override
  String get settingsAppFactoryResetConfirmBody =>
      'This cannot be undone. You will reconnect to the board and re-enter any API keys.';

  @override
  String get settingsAppFactoryResetConfirmAction => 'Reset';

  @override
  String get settingsAppFactoryResetDoneSnack => 'App data was cleared.';

  @override
  String get settingsAppFactoryResetRestartHint =>
      'If anything looks wrong, fully quit and reopen the app.';

  @override
  String get transportShortDemo => 'Demo';

  @override
  String get transportShortDash => '—';

  @override
  String get settingsBoardAppearanceTitle => 'Board appearance';

  @override
  String settingsBoardAppearanceSubtitle(
      String layout, String zoom, String style) {
    return '$layout · $zoom · $style';
  }

  @override
  String get settingsLayoutBoardOnlyShort => 'Board only';

  @override
  String get settingsLayoutFullUiShort => 'Full UI';

  @override
  String get settingsPlayTabGamePanel => 'Play tab & game panel';

  @override
  String get settingsLayoutLabel => 'Layout';

  @override
  String get settingsLayoutBoardSegment => 'Board';

  @override
  String get settingsLayoutBoardTooltip => 'Board only — minimal chrome';

  @override
  String get settingsLayoutFullSegment => 'Full';

  @override
  String get settingsLayoutFullTooltip => 'Standard — clocks & controls';

  @override
  String get settingsBoardZoom => 'Board zoom';

  @override
  String get settingsSquareColors => 'Square colors';

  @override
  String get settingsBoardThemeLabel => 'Theme';

  @override
  String get settingsBoardOptions => 'Board options';

  @override
  String get settingsCoordinatesTitle => 'Coordinates';

  @override
  String get settingsCoordinatesSubtitle => 'a–h and 1–8 labels';

  @override
  String get settingsPieceMotionTitle => 'Piece motion';

  @override
  String get settingsPieceMotionSubtitle => 'Animated moves on the board';

  @override
  String get settingsFlipBoardTitle => 'Flip board';

  @override
  String get settingsRemoteMovesTitle => 'Remote moves';

  @override
  String get settingsRemoteMovesSubtitle =>
      'Play from the app, not only the board';

  @override
  String get settingsLiveEvalTitle => 'Live evaluation';

  @override
  String get settingsLiveEvalSubtitle =>
      'Stockfish — fills the Analysis chart while you play';

  @override
  String get settingsResetBoardDisplay => 'Reset board display defaults';

  @override
  String get boardStyleWooden => 'Wooden (default)';

  @override
  String get boardStyleModernDark => 'Modern Dark';

  @override
  String get boardStyleIceBlue => 'Ice Blue';

  @override
  String get boardStyleForestGreen => 'Forest Green';

  @override
  String get boardStyleMarbleGray => 'Marble Gray';

  @override
  String get boardStyleMidnight => 'Midnight';

  @override
  String get boardStyleSlate => 'Slate';

  @override
  String get boardStyleCoral => 'Coral';

  @override
  String get settingsAppAppearanceTitle => 'App appearance';

  @override
  String get settingsAppearanceLight => 'Light look';

  @override
  String get settingsAppearanceDark => 'Dark look';

  @override
  String get settingsAppearanceOled => 'OLED';

  @override
  String get settingsAppearanceSystem => 'Follow system';

  @override
  String get settingsColorSchemeLabel => 'Color scheme';

  @override
  String get settingsColorSchemeHelper =>
      'Light and Dark apply immediately. System follows your device.';

  @override
  String get settingsThemeSystem => 'System';

  @override
  String get settingsThemeLight => 'Light';

  @override
  String get settingsThemeDark => 'Dark';

  @override
  String get settingsThemeOledBlack => 'OLED (true black)';

  @override
  String get settingsHapticsTitle => 'Haptic feedback';

  @override
  String get settingsSoundTitle => 'Sound effects';

  @override
  String get settingsAutoGameSummaryTitle =>
      'Auto-open game summary after game';

  @override
  String get settingsCoachAiTitle => 'Coach & AI';

  @override
  String get settingsCoachPriorityIntro =>
      'Drag to set fallback order: the app tries the first provider, then the next if it fails. Leave the list empty for offline tips only. Keys stay on this device.';

  @override
  String get settingsCoachOfflineOnly => 'Offline only';

  @override
  String get settingsCoachOpenAiOnly => 'OpenAI only';

  @override
  String get settingsCoachGoogleOnly => 'Google only';

  @override
  String get settingsCoachGroqGoogleOpenAi => 'Groq→Google→OpenAI';

  @override
  String get settingsCoachOllamaGoogle => 'Ollama→Google';

  @override
  String get settingsCoachPriorityTopLabel => 'Priority (top = first try)';

  @override
  String get settingsCoachNoCloudProviders =>
      'No cloud providers — Coach uses short on-device tips.';

  @override
  String get settingsCoachRemoveFromChain => 'Remove from chain';

  @override
  String get settingsCoachAddProvider => 'Add provider';

  @override
  String get settingsCoachExplanationLevel => 'Coach explanation level';

  @override
  String get settingsCoachExplanationLevelSubtitle =>
      '1 = Beginner, 4 = Expert';

  @override
  String get settingsCoachLevelBeginner => '1 – Beginner';

  @override
  String get settingsCoachLevelIntermediate => '2 – Intermediate';

  @override
  String get settingsCoachLevelAdvanced => '3 – Advanced';

  @override
  String get settingsCoachLevelExpert => '4 – Expert';

  @override
  String get settingsCoachCredentialsHeader =>
      'Credentials (fill what you use)';

  @override
  String get settingsCoachOpenAiProvider => 'OpenAI';

  @override
  String get settingsCoachApiKeyLabel => 'API key';

  @override
  String get settingsCoachOpenAiKeyHelper =>
      'platform.openai.com — used only for Coach.';

  @override
  String get settingsCoachModelIdLabel => 'Model id';

  @override
  String get settingsCoachOpenAiKeysButton => 'OpenAI keys';

  @override
  String get settingsCoachGoogleAiStudio => 'Google AI Studio';

  @override
  String get settingsCoachPasteGoogleKeyHint => 'Paste Google AI Studio key';

  @override
  String get settingsCoachGoogleKeyHelper =>
      'Get a key at aistudio.google.com — Gemini / Gemma.';

  @override
  String get settingsCoachCustomModel => 'Custom…';

  @override
  String get settingsCoachCustomModelId => 'Custom model id';

  @override
  String get settingsCoachCustomModelHint => 'e.g. gemini-2.5-flash';

  @override
  String get settingsCoachOpenAiKeyHint => 'sk-…';

  @override
  String get settingsCoachOpenAiFieldLabel => 'API key';

  @override
  String get settingsCoachOpenAiKeyTooltip => 'Open the OpenAI keys page';

  @override
  String get settingsCoachOpenAiModelHint => 'gpt-4o-mini';

  @override
  String get settingsCoachGroqKeyHint => 'Paste Groq API key';

  @override
  String get settingsCoachGroqModelHint => 'llama-3.3-70b-versatile';

  @override
  String get settingsCoachGroqModelHelper =>
      'Must match an enabled Groq model.';

  @override
  String get settingsCoachXaiKeyHint => 'Paste xAI API key';

  @override
  String get settingsCoachXaiModelHint => 'grok-2-latest';

  @override
  String get settingsCoachOllamaBaseHint => 'http://127.0.0.1:11434/v1';

  @override
  String get settingsCoachOllamaModelHint => 'llama3.2';

  @override
  String get settingsCoachGetGoogleKey => 'Get Google AI key';

  @override
  String get settingsCoachGroqTitle => 'Groq (OpenAI-compatible)';

  @override
  String get settingsCoachXaiTitle => 'xAI (Grok)';

  @override
  String get settingsCoachOllamaTitle => 'Ollama (local)';

  @override
  String get settingsCoachBaseUrlLabel => 'Base URL';

  @override
  String settingsCoachChainSubtitle(String chain) {
    return '$chain';
  }

  @override
  String get settingsDeveloperModeUnlockedSnack => 'Developer mode unlocked.';

  @override
  String get settingsDeveloperModeDisabledSnack => 'Developer mode turned off.';

  @override
  String get gameControlDisplaySection => 'Display';

  @override
  String get gameControlPlayModeSection => 'Play mode';

  @override
  String get gameControlSandboxLabel => 'Sandbox';

  @override
  String get gameControlMovesFromApp => 'Moves from app';

  @override
  String gameControlUndoMove(int n) {
    return 'Undo move ($n)';
  }

  @override
  String get gameControlExitSandbox => 'Exit sandbox & refresh board';

  @override
  String get gameControlLearningModeTitle => 'Learning mode';

  @override
  String get gameControlLearningModeSubtitle => 'Coach explanations in the app';

  @override
  String get gameControlExplanationLevelSettings =>
      'Explanation level (1–4) in Settings';

  @override
  String get gameControlActionsSection => 'Actions';

  @override
  String get gameControlNewGameEllipsis => 'New game…';

  @override
  String get gameControlRefreshState => 'Refresh state';

  @override
  String get gameControlPanelTitle => 'Game controls';

  @override
  String get timerUnavailable => 'Clock: off or unavailable';

  @override
  String get timerHiddenForPuzzleMode =>
      'The live game clock is hidden while you solve a puzzle or explore a position.';

  @override
  String get timerWhiteShort => 'White';

  @override
  String get timerBlackShort => 'Black';

  @override
  String get coachChatTypeFirst => 'Write a question first, then Send.';

  @override
  String get coachChatExplanationLevelLabel => 'Explanation depth';

  @override
  String get coachChatHide => 'Hide';

  @override
  String get coachChatDismiss => 'Close';

  @override
  String get manualConnTitle => 'Manual connection';

  @override
  String manualConnWifiStatusError(String error) {
    return 'Wi‑Fi status: $error';
  }

  @override
  String get manualConnEnterUrlSnack => 'Enter the board URL.';

  @override
  String get manualConnStaDisconnectedSnack =>
      'STA disconnected (command sent).';

  @override
  String get manualConnClearWifiTitle => 'Clear saved Wi‑Fi from NVS?';

  @override
  String get manualConnClearWifiBody =>
      'The ESP will forget the stored network.';

  @override
  String get manualConnNvsClearedSnack => 'Wi‑Fi credentials cleared from NVS.';

  @override
  String get manualConnSaveUrlSnack => 'URL saved to preferences.';

  @override
  String get manualConnSaveUrl => 'Save URL';

  @override
  String get manualConnTestConnection => 'Test connection (GET snapshot)';

  @override
  String get manualConnConnectSession => 'Connect session (Wi‑Fi + poll)';

  @override
  String get manualConnWifiStaNvs => 'Wi‑Fi STA & NVS';

  @override
  String get manualConnRefreshWifi => 'Refresh Wi‑Fi status';

  @override
  String get manualConnDisconnectSta => 'Disconnect ESP from STA';

  @override
  String get manualConnClearWifiNvs => 'Clear Wi‑Fi from NVS';

  @override
  String get manualConnClearConfirmAction => 'Clear';

  @override
  String get manualConnUrlLabelDev => 'Board base URL (http://…)';

  @override
  String get manualConnUrlLabelUser => 'Board address';

  @override
  String get manualConnUrlHintDev => 'http://192.168.x.x';

  @override
  String get manualConnUrlHintUser => 'Address on your LAN';

  @override
  String get manualConnBleInfoTile =>
      'You’re connected via Bluetooth. Enter the board address for network actions.';

  @override
  String get manualConnStaSectionDev =>
      'Needs HTTP reachability to the board (same LAN or board AP). BLE-only without IP won’t call these endpoints.';

  @override
  String get manualConnStaSectionUser =>
      'Use this section to manage the board’s network connection manually.';

  @override
  String get manualConnTestFailedUser => 'Could not reach the board.';

  @override
  String get haMqttTitle => 'Home Assistant & MQTT';

  @override
  String get haMqttFormLead =>
      'Enter the MQTT broker your board should use (often the same machine as Home Assistant).';

  @override
  String get haMqttSetupHelpLink => 'How do I set up MQTT and Home Assistant?';

  @override
  String get haMqttSavedSnack => 'MQTT saved to board.';

  @override
  String get haMqttHowToHa => 'Home Assistant guide';

  @override
  String get haMqttBrokerHeader => 'MQTT broker';

  @override
  String get haMqttSaveToBoard => 'Save to board';

  @override
  String get haMqttRefreshFromBoard => 'Refresh status from board';

  @override
  String get haMqttBoardStatusHeader => 'Status on board';

  @override
  String get haMqttModeTile => 'Mode';

  @override
  String get haMqttMqttTile => 'MQTT';

  @override
  String get haMqttWifiStaTile => 'Wi‑Fi STA';

  @override
  String get haMqttGuideBody =>
      '1. Home Assistant must run on the same Wi‑Fi LAN as the board (ESP connected as STA to your router).\n\n2. In Home Assistant enable an MQTT broker — commonly the “Mosquitto broker” add-on (Settings → Add-ons). Note the port (usually 1883) and any username/password from the add-on config.\n\n3. Find the IP or hostname of the machine running HA (e.g. 192.168.1.42 or homeassistant.local). The board must reach it on the LAN.\n\n4. Enter that address as “Broker host” below — it’s the MQTT server address (usually the same machine as Home Assistant). Keep port 1883 unless you changed Mosquitto.\n\n5. Saving sends settings to the board over the current link (Wi‑Fi or Bluetooth). The CzechMate app does not connect to the broker — only the ESP does.';

  @override
  String get haMqttHostFieldLabel => 'Broker host (e.g. Home Assistant IP)';

  @override
  String get haMqttPortFieldLabel => 'Port';

  @override
  String get haMqttUserFieldLabel => 'Username (optional)';

  @override
  String get haMqttPasswordFieldLabel => 'Password (optional)';

  @override
  String get haMqttFooterMock =>
      'The demo board does not support MQTT — connect a real board.';

  @override
  String get haMqttFooterConnectFirst =>
      'Connect the board (Bluetooth or Wi‑Fi) before saving the broker.';

  @override
  String get haMqttFooterBleSave =>
      'You can save over Bluetooth; refreshing status from the board uses HTTP over Wi‑Fi.';

  @override
  String get haMqttFooterTroubleshoot =>
      'If MQTT stays offline, check the HA firewall, Mosquitto credentials, and that the ESP is on the same network as the broker.';

  @override
  String get haMqttStateConnected => 'connected';

  @override
  String get haMqttStateDisconnected => 'disconnected';

  @override
  String get haMqttWifiOk => 'OK';

  @override
  String get haMqttWifiNoLink => 'no link';

  @override
  String get firmwareDailySecondConfirmBody =>
      'Start the update? The board will download firmware, write flash, and reboot. Do not interrupt power.';

  @override
  String get firmwareDailySecondConfirmTitle => 'Confirm update';

  @override
  String get profileTitle => 'Profile & Elo';

  @override
  String get profileDisplayName => 'Display name';

  @override
  String get profileNameSavedSnack => 'Name saved';

  @override
  String get profileSaveName => 'Save name';

  @override
  String get profileAvatar => 'Avatar';

  @override
  String get profileFromGallery => 'From gallery';

  @override
  String profileGalleryError(String error) {
    return 'Gallery: $error';
  }

  @override
  String get profileNameHint => 'Name shown on your profile';

  @override
  String profilePuzzleEloLine(String elo) {
    return 'Puzzle Elo: $elo';
  }

  @override
  String profileWeekStatsLine(int solved, int failed) {
    return 'Last 7 days: $solved solved · $failed wrong lines';
  }

  @override
  String get profileHeatmapTitle => 'Last 28 days';

  @override
  String get profileHeatmapCaption =>
      'Darker = more attempts; greener = higher success rate.';

  @override
  String get profileAvatarHint => 'Preset icons or a photo from your gallery.';

  @override
  String get profileEloHelpBody =>
      'Elo updates when you complete a graded puzzle with a known solution line (e.g. daily Lichess puzzles).';

  @override
  String get firmwareManifestSavedSnack => 'Manifest URL saved.';

  @override
  String get firmwareUpdateBoardTitle => 'Update board firmware?';

  @override
  String firmwareNewVersionLine(String ver, String boardVer) {
    return 'New version: $ver — on board: $boardVer.';
  }

  @override
  String get firmwareUpdateIntroBody =>
      'The board downloads the file over HTTPS. Keep power and Wi‑Fi stable.';

  @override
  String get firmwareFinalConfirmTitle => 'Final confirmation';

  @override
  String get firmwareFinalConfirmBody => 'Start OTA on the board now?';

  @override
  String get firmwareYesUpdate => 'Yes, update';

  @override
  String get firmwareDailyRemindersTitle => 'Daily update reminders';

  @override
  String get firmwareDailyRemindersSubtitle =>
      'Notify when a newer firmware is available.';

  @override
  String get firmwareSaveManifestUrl => 'Save manifest URL';

  @override
  String get firmwareCheckForUpdate => 'Check for update';

  @override
  String firmwareNewVersionChip(String ver) {
    return 'New version $ver';
  }

  @override
  String firmwareDeveloperReflashChipTitle(String ver) {
    return 'Re-flash $ver (developer)';
  }

  @override
  String firmwareDeveloperDowngradeChipTitle(String ver) {
    return 'Older build $ver (developer)';
  }

  @override
  String get firmwareDeveloperSameVersionBanner =>
      'Developer mode: manifest matches the board version — you can still download and flash the same build again (repair / verify).';

  @override
  String get firmwareDeveloperOlderManifestBanner =>
      'Developer mode: this manifest is older than the board firmware. Downgrading can break compatibility or data — only proceed if you understand the risk.';

  @override
  String get firmwareDownloadOnEspNote =>
      'Download runs on the ESP over HTTPS.';

  @override
  String get firmwareConfirmUpdateTitle => 'Confirm update';

  @override
  String get firmwareConfirmUpdateBody => 'Start firmware update on the board?';

  @override
  String get firmwareStartingOtaSnack => 'Starting OTA on the board…';

  @override
  String get firmwareDownloadToApp => 'Download to app';

  @override
  String get firmwareSendToBoard => 'Send to board';

  @override
  String get firmwareTwoStepOtaHint =>
      'Download the .bin while you have internet. Join the board hotspot, then send — the board pulls the file from this device over HTTP (no Wi‑Fi STA on the board).';

  @override
  String firmwareCachedInAppLine(String ver, String mb) {
    return 'Saved in app: v$ver (~$mb MB)';
  }

  @override
  String firmwareSavedFirmwareChipTitle(String ver) {
    return 'Saved firmware v$ver';
  }

  @override
  String get firmwareOfflineSavedFirmwareBanner =>
      'The live manifest could not be loaded. You can still flash the firmware file saved in this app.';

  @override
  String firmwareCachedDiffersFromManifestHint(
      String remoteVer, String savedVer) {
    return 'Firmware saved in the app is v$savedVer; the manifest now lists v$remoteVer. The saved file was kept — flash it whenever you are ready, or download again.';
  }

  @override
  String get firmwareDownloadSavedSnack => 'Firmware saved in the app.';

  @override
  String firmwareDownloadFailedLine(String error) {
    return 'Download failed: $error';
  }

  @override
  String get firmwareJoinHotspotForUpload =>
      'Connect to the board Wi‑Fi hotspot first — this device needs a 192.168.4.x address.';

  @override
  String get firmwareNoCachedFirmware =>
      'Download the firmware to the app first.';

  @override
  String get firmwareSendToBoardTitle => 'Upload from this device?';

  @override
  String get firmwareSendToBoardBody =>
      'The board will download the .bin over HTTP from this device. Stay on the hotspot and keep this screen open until finished.';

  @override
  String get firmwareOneStepHttpsOta =>
      'Or: board downloads via HTTPS (needs STA)';

  @override
  String get firmwareSendViaBle => 'Send via Bluetooth';

  @override
  String get firmwareSendViaBleBody =>
      'The full firmware image will be transferred over Bluetooth only (no board hotspot or STA required). Keep this device close to the board until the connection drops — the board will reboot.';

  @override
  String get firmwareBleUploadDoneSnack =>
      'Bluetooth upload finished — the board may reboot.';

  @override
  String get firmwareBleOtaReturnedFromBackgroundSnack =>
      'You returned while a Bluetooth firmware transfer was in progress. If it stalled, keep CzechMate in the foreground and this device close to the board.';

  @override
  String get firmwareOtaHttpMayLeaveAppHint =>
      'The system may briefly switch away from this app when handling an HTTPS link — that’s OK. Open CzechMate again and watch progress via the board HTTP URL.';

  @override
  String get firmwareBleOtaKeepForegroundWarning =>
      'Keep CzechMate in the foreground and avoid locking the screen until the Bluetooth transfer finishes.';

  @override
  String get firmwareBleOtaApHotspotTip =>
      'If you upload over the board hotspot, join the board Wi‑Fi first so this device can reach http://192.168.4.1.';

  @override
  String get firmwareBleOtaPausedReconnectDetail =>
      'Bluetooth transfer paused — reconnecting to the board…';

  @override
  String get firmwareBleOtaResumedTransferDetail =>
      'Connection restored — continuing firmware transfer.';

  @override
  String get firmwareOtaSlotsDisabledHint =>
      'This flash layout has no OTA slots (ota_0 + ota_1). Use USB/UART (esptool or idf.py flash) or rebuild firmware with a dual-OTA partition table (see project partitions CSV).';

  @override
  String get firmwareBoardHttpMissingDetail =>
      'Board HTTP address is missing (e.g. http://192.168.4.1). Set Default board URL in settings. Bluetooth alone does not provide an IP.';

  @override
  String get firmwareHttpsLinkExplainBody =>
      'You only send an HTTPS link — the ESP downloads the full .bin. The board must be connected to Wi‑Fi as a station (STA). You can send the start command over Wi‑Fi HTTP or Bluetooth; progress is read over HTTP to the board IP.';

  @override
  String get firmwareSettingsSecondConfirmBody =>
      'The board will write firmware and reboot. Do not cut power or lose Wi‑Fi while downloading. Continue?';

  @override
  String get firmwareOtaFinishedMaybeRebootSnack =>
      'OTA finished or connection dropped — the board may reboot.';

  @override
  String get firmwareOtaSuccessTitle => 'Firmware updated';

  @override
  String firmwareOtaSuccessBody(String installedVer, String reportedVer) {
    return 'Installed build: $installedVer.\n\nBoard version in the app: $reportedVer.\n\nIf the chess board restarted, wait until it responds again or reconnect from Settings → Connection.';
  }

  @override
  String firmwareTileTitleUpdateAvailable(String ver) {
    return 'Firmware — update available ($ver)';
  }

  @override
  String get firmwareOtaRollbackBannerTitle =>
      'Previous firmware restored after failed boot';

  @override
  String firmwareOtaRollbackBannerBodyWithAttempt(
      String current, String slot, String attempt) {
    return 'The board is running $current after the new image failed to start. Unsuccessful update ($attempt) remains in slot $slot. Install a fixed build when available.';
  }

  @override
  String firmwareOtaRollbackBannerBodyNoAttempt(String current, String slot) {
    return 'The board is running $current after the new image failed to start. The unsuccessful image is in slot $slot. Install a fixed build when available.';
  }

  @override
  String get firmwareTileTitleDefault => 'Firmware (over-the-air)';

  @override
  String get firmwareTileSubtitleIdle =>
      'Manifest + Check. Use Download / Send on hotspot, or HTTPS if the board has STA.';

  @override
  String get firmwareRemindersOnShort => 'reminders on';

  @override
  String get firmwareRemindersOffShort => 'reminders off';

  @override
  String get firmwareDailyRemindersSubtitleLong =>
      'If a newer version is on the server, ask again each day until you update or turn reminders off.';

  @override
  String get firmwareDeveloperManifestSectionTitle => 'Developer options';

  @override
  String get firmwareDeveloperManifestSectionBody =>
      'Only change the manifest URL if you maintain a custom firmware feed. The app uses the official source by default.';

  @override
  String get firmwareManifestUrlLabel => 'Manifest URL (version.json)';

  @override
  String get firmwareManifestUrlHint => 'https://…/version.json';

  @override
  String get firmwareManifestUrlHelper =>
      'Default URL is filled automatically. Clear the field and Save to use the built-in manifest only.';

  @override
  String firmwareBoardHttpVersionLine(String ver) {
    return 'Board (HTTP): $ver';
  }

  @override
  String firmwareManifestVersionLine(String ver) {
    return 'Manifest: $ver';
  }

  @override
  String firmwareWifiStaConnected(String ip) {
    return 'connected ($ip)';
  }

  @override
  String get firmwareWifiStaNotConnected => 'not connected';

  @override
  String firmwareWifiStaLine(String status) {
    return 'Wi‑Fi STA: $status';
  }

  @override
  String get firmwareNeedDefaultBoardUrlHint =>
      'Set and save Default board URL (AP or STA IP) to read the board version.';

  @override
  String firmwareOtaPercentLine(int percent) {
    return 'OTA $percent %';
  }

  @override
  String firmwareFooterTransportHint(String transport) {
    return 'Connection: $transport. HTTPS OTA needs STA; hotspot HTTP OTA works on 192.168.4.x; cached .bin can also upload over Bluetooth when GATT is connected.';
  }

  @override
  String get firmwareTransportLabelNone => 'none';

  @override
  String get firmwareTransportLabelWifi => 'wifi';

  @override
  String get firmwareTransportLabelBle => 'ble';

  @override
  String get firmwareTransportLabelMock => 'mock';

  @override
  String get errWifiSsidEmpty => 'Enter a Wi‑Fi network name (SSID).';

  @override
  String get errWifiProvNeedsBle =>
      'Connect via Bluetooth to provision Wi‑Fi on the board.';

  @override
  String get firmwareWifiBleProvStartedSnack =>
      'Wi‑Fi credentials sent — watch the board status.';

  @override
  String get firmwareOtaNoLanRouteUseBle =>
      'This device isn’t on the same LAN as the board. Use Bluetooth to upload the firmware, or join the board’s network.';

  @override
  String get firmwareOtaNoLanRouteNeedBle =>
      'Connect via Bluetooth first — without LAN access the app will send the image over BLE.';

  @override
  String get firmwareOtaPhoneNotOnLan =>
      'This device doesn’t appear to be on the board’s LAN. Join the board hotspot or the Wi‑Fi where the board has an IP.';

  @override
  String firmwareTileTitleGitBle(String ver) {
    return 'Firmware — GitHub build ($ver)';
  }

  @override
  String firmwareTileTitleDeveloperReflash(String ver) {
    return 'Firmware — re-flash ($ver)';
  }

  @override
  String firmwareTileTitleDeveloperDowngrade(String ver) {
    return 'Firmware — older manifest ($ver)';
  }

  @override
  String get firmwareTileSubtitleBleGitOnly =>
      'Bluetooth only — download the .bin to the app, then send via BLE or board hotspot HTTP.';

  @override
  String get firmwareTileSubtitleDeveloperReflash =>
      'Developer mode — manifest matches the board; open to download and flash the same build again.';

  @override
  String get firmwareTileSubtitleDeveloperDowngrade =>
      'Developer mode — manifest is older than the board; optional downgrade from this URL.';

  @override
  String get firmwareWifiBleProvisionTitle =>
      'Wi‑Fi on the board (via Bluetooth)';

  @override
  String get firmwareWifiBleProvisionSubtitle =>
      'Send SSID and password over BLE so the board can join your network as a client (STA).';

  @override
  String get firmwareWifiBleSsidLabel => 'Wi‑Fi name (SSID)';

  @override
  String get firmwareWifiBlePasswordLabel => 'Wi‑Fi password';

  @override
  String get firmwareWifiBleUsePhoneSsidButton =>
      'Use this device’s Wi‑Fi name';

  @override
  String get firmwareWifiBleSendCredentials => 'Send credentials to board';

  @override
  String firmwareBleStaOk(String ssid, String ip) {
    return 'STA: $ssid · $ip';
  }

  @override
  String get firmwareBleStaWaiting =>
      'Waiting for the board to connect to Wi‑Fi…';

  @override
  String get firmwareBleHttpOptionalHint =>
      'Board HTTP URL is optional — you can update over Bluetooth without Wi‑Fi on the board.';

  @override
  String get firmwareBleGitUnknownBoardVersion =>
      'Board version unknown over HTTP — download to the app and prefer Bluetooth upload.';

  @override
  String get firmwareSendViaBlePrimary => 'Send via Bluetooth (recommended)';

  @override
  String get firmwareSendToBoardHttpAlt => 'Send via board HTTP (hotspot)';

  @override
  String get analysisAppBarTitle => 'Analysis';

  @override
  String get analysisGameProgress => 'Game in progress';

  @override
  String get analysisGameProgressBody => 'Half-moves from the connected board.';

  @override
  String get analysisStockfishSection => 'Stockfish';

  @override
  String get analysisStockfishSubtitle =>
      'Evaluation line from the current board position.';

  @override
  String get analysisNoBoardPosition => 'No position from board';

  @override
  String get analysisNoBoardPositionBody =>
      'Connect Wi‑Fi or Bluetooth and open Play.';

  @override
  String get analysisOpenPlay => 'Open Play';

  @override
  String get analysisChartDisabledTitle => 'Chart and move evaluation are off';

  @override
  String get analysisChartDisabledSubtitle =>
      'Enable move evaluation to see quality scores.';

  @override
  String analysisChartJumpedToMove(int moveNumber) {
    return 'Opened half-move $moveNumber on Play.';
  }

  @override
  String get analysisScoresheetEmpty => 'No moves yet.';

  @override
  String get analysisEnableMoveEval => 'Enable move evaluation (Stockfish)';

  @override
  String get analysisGameOverview => 'Game overview';

  @override
  String get analysisGameOverviewBody =>
      'Material, clocks, and opening info when available.';

  @override
  String get analysisMoveQuality => 'Move quality';

  @override
  String get analysisMoveQualityBody =>
      'Average 0–100 by Stockfish grade and avg loss in cp.';

  @override
  String analysisQualitySummaryLine(String quality, String avgCp, int count) {
    return '$quality$avgCp · $count moves';
  }

  @override
  String get analysisLastMoveEvalTitle => 'Last move evaluation';

  @override
  String get analysisMoveHistoryTitle => 'Move history';

  @override
  String get analysisMoveHistoryTapHint =>
      'Tap to open chronological playback.';

  @override
  String get analysisSecondLineTitle => 'Second line';

  @override
  String get analysisSecondLineBody =>
      'Alternative continuation from the engine.';

  @override
  String get analysisCustomPositionTitle => 'Custom position';

  @override
  String get analysisCustomPositionBody => 'Analysis without a physical board.';

  @override
  String analysisDepthLabel(String n) {
    return 'Depth: $n';
  }

  @override
  String get analysisAnalyzePosition => 'Analyze position';

  @override
  String get analysisSandboxFenSnack => 'Sandbox from FEN — Play tab.';

  @override
  String get analysisPreviewInPlay => 'Preview in Play';

  @override
  String get analysisPreviewMoveTitle => 'Preview position & move';

  @override
  String get puzzleRoundExpiredSnack => 'Round time expired.';

  @override
  String get puzzleConnectBoardSnack => 'Connect the board (Wi‑Fi or BLE).';

  @override
  String get puzzleNewGameSentSnack => 'new_game command sent.';

  @override
  String get puzzleBoardRiddleTitle => 'Board puzzle';

  @override
  String get puzzleTryOnScreen => 'Try on screen';

  @override
  String get puzzleLoadToBoard => 'Load to board (new_game + FEN)';

  @override
  String get puzzleSetupWizardLabel => 'Board setup wizard';

  @override
  String get puzzleOpenLichess => 'Open on Lichess';

  @override
  String get puzzleSavePositionTitle => 'Save position';

  @override
  String get puzzleSavedSnack => 'Saved.';

  @override
  String get puzzleAddToLibrary => 'Add to library';

  @override
  String get puzzleCustomTitle => 'Custom';

  @override
  String get puzzleSetupOnBoard => 'Setup on board';

  @override
  String puzzleDailyTitle(String id) {
    return 'Daily $id';
  }

  @override
  String get puzzleDailySolveTitle => 'Daily puzzle';

  @override
  String puzzleSolutionMoves(int n) {
    return 'Solution: $n moves';
  }

  @override
  String get puzzlePoolModeTitle => 'Pool mode';

  @override
  String get puzzlePoolMixed => 'Mixed';

  @override
  String get puzzlePoolBundled => 'Bundled';

  @override
  String get puzzlePoolLibrary => 'Library';

  @override
  String get puzzleTimedRoundTitle => 'Timed round';

  @override
  String get puzzleTimedRoundBody =>
      'Random position from the selected pool, countdown in seconds.';

  @override
  String get puzzleNewRound => 'New round';

  @override
  String get puzzleStop => 'Stop';

  @override
  String puzzleRemainingSeconds(int n) {
    return 'Remaining: ${n}s';
  }

  @override
  String get puzzleSolveOnScreen => 'Solve on screen';

  @override
  String get puzzleLoadToBoardShort => 'Load to board';

  @override
  String get puzzleSetupWizardShort => 'Setup wizard';

  @override
  String get puzzleBundledOfflineTitle => 'Bundled positions (offline)';

  @override
  String get puzzleThemeMate => 'Mate';

  @override
  String get puzzleThemeFork => 'Fork';

  @override
  String get puzzleThemeEndgame => 'Endgame';

  @override
  String get puzzleThemeMixed => 'Mixed';

  @override
  String get progressAppBarTitle => 'Progress';

  @override
  String get progressSegBeginner => 'Beg.';

  @override
  String get progressSegIntermediate => 'Int.';

  @override
  String get progressSegAdvanced => 'Adv.';

  @override
  String get progressSegExpert => 'Exp.';

  @override
  String get progressTabLearn => 'Learn';

  @override
  String get progressTabStats => 'Stats';

  @override
  String get progressWelcomeTitle => 'Welcome to CzechMate';

  @override
  String get progressWelcomeBody =>
      'Track puzzles, coach, and board setup from one place.';

  @override
  String get progressProfilePuzzleEloTitle => 'Profile & puzzle Elo';

  @override
  String get progressProfilePuzzleEloSubtitle =>
      'Local nickname and puzzle rating.';

  @override
  String get progressLearnCardTitle => 'Learn';

  @override
  String get progressLearnCardSubtitle => 'LED setup wizards and AI coach';

  @override
  String get progressAiCoachTitle => 'AI coach';

  @override
  String get progressAiCoachBody =>
      'Turn on learning mode on Play. Chat uses the cloud API or a local fallback; Stockfish stays on Analysis.';

  @override
  String get progressLearningModeTile => 'Learning mode';

  @override
  String get progressCoachLevelLabel => 'Coach level';

  @override
  String get progressCoachChatButton => 'Coach chat';

  @override
  String get progressPositionPlanButton => 'Position plan';

  @override
  String get progressBoardErrorTitle => 'Connection problem';

  @override
  String get progressStartingPositionTitle => 'Starting position';

  @override
  String get progressStartingPositionBody => 'Wizard aligns pieces using LEDs.';

  @override
  String get progressStartingPositionTeachingBody =>
      'Teaching mode: LEDs show each square in order; the app shows which piece to place (same flow as iOS).';

  @override
  String get progressRunWizardStarting => 'Starting position wizard';

  @override
  String get progressAccountCardTitle => 'Account';

  @override
  String get progressAccountCardSubtitle => 'Local profile and activity';

  @override
  String get progressActiveTransportTitle => 'Active transport';

  @override
  String get progressNoSessionTitle => 'No active session yet';

  @override
  String get progressNoSessionSubtitle => 'Open Find board to connect.';

  @override
  String get progressStatsSegmentSubtitle => 'Sessions and in-app stats';

  @override
  String get progressProfileIconTooltip => 'Profile & Elo';

  @override
  String get progressLearningModePlayHint =>
      'Enable learning mode on Play → Game controls to use chat and position plan.';

  @override
  String get progressWizardConnectHint =>
      'Connect the board for LED wizards (connection chip on the Play tab).';

  @override
  String get userFacingErrBle =>
      'Bluetooth connection failed. Turn Bluetooth on, stay near the board, and try again.';

  @override
  String get userFacingErrBlePairingReset =>
      'Bluetooth pairing no longer matches. iOS / iPadOS: Settings → Bluetooth → ⓘ next to the board → Forget This Device. Android: Bluetooth settings → your board → Forget / Unpair. If the board was factory-reset, clear its bond list too, then connect again from Find board.';

  @override
  String get userFacingErrNetworkReach =>
      'Couldn\'t reach the board or server. Check Wi‑Fi, move closer, and make sure the board is on.';

  @override
  String get userFacingErrTimeout =>
      'The connection timed out. Try again when the board is powered on and in range.';

  @override
  String get userFacingErrTls =>
      'Secure connection (HTTPS) failed. Check the address and whether the board expects HTTPS.';

  @override
  String get userFacingErrWebSocket =>
      'Live updates from the board were interrupted. Use Find board to reconnect if this keeps happening.';

  @override
  String get userFacingErrGeneric =>
      'Something went wrong. Try again or reconnect using Find board.';

  @override
  String get userFacingErrDailyPuzzle =>
      'Couldn\'t load today\'s puzzle. Check internet and tap refresh.';

  @override
  String get userFacingShowTechnicalDetails => 'Show technical details';

  @override
  String get userFacingHideTechnicalDetails => 'Hide technical details';

  @override
  String get userFacingBoardWebLocked =>
      'The board\'s web dashboard is locking API access. Close it or unlock API access, then retry.';

  @override
  String get userFacingBoardApiToken =>
      'This board requires an API token. Add it under Settings → Saved defaults.';

  @override
  String get userFacingBoardBadRequest =>
      'The board rejected the request. Check move legality or required fields.';

  @override
  String get userFacingBoardUnauthorized =>
      'Access was denied. Check credentials if this board requires authentication.';

  @override
  String get userFacingBoardNotFound =>
      'That API path was not found. Your firmware may be older than this app expects.';

  @override
  String get userFacingBoardServerError =>
      'The board reported an internal error. Try again or reboot the board.';

  @override
  String get userFacingBoardUnavailable =>
      'The board is busy or unavailable. Wait a moment and retry.';

  @override
  String get userFacingBoardHttpOther =>
      'HTTP error from the board. Check the board URL and connection mode.';

  @override
  String get userFacingErrBoardUrlHostMissing =>
      'The board address is missing a host. In Settings, save a full URL such as http://192.168.4.1 on the same network as the board.';

  @override
  String get puzzleMoreActionsTooltip => 'More actions';

  @override
  String get gameClockSectionLabel => 'Clock & board LEDs';

  @override
  String get progressStatsCurrentMoves => 'Current move count';

  @override
  String get progressStatsPeakMoves => 'Peak move count seen';

  @override
  String get progressStatsPollSuccess => 'Successful HTTP polls';

  @override
  String get progressStatsPollFail => 'Failed polls';

  @override
  String get progressStatsWsMessages => 'WebSocket messages';

  @override
  String get progressShortOn => 'on';

  @override
  String get progressShortOff => 'off';

  @override
  String get progressWaitingForData => 'waiting for data';

  @override
  String get progressTransportWifi => 'Wi‑Fi';

  @override
  String get progressTransportBluetooth => 'Bluetooth';

  @override
  String progressTransportDevDetail(
      String transport, String pollState, String wsState) {
    return '$transport · polling $pollState · WS $wsState';
  }

  @override
  String progressTransportUserDetail(String transport, String syncState) {
    return '$transport · live sync $syncState';
  }

  @override
  String get progressStatsEmptySubtitle =>
      'Connect the board on Play — stats fill in once polling runs.';

  @override
  String get helpAppBarTitle => 'czechmate — help';

  @override
  String get helpConnectTitle => 'Connecting the board';

  @override
  String get helpConnectBody =>
      'Discover and pair the board over Bluetooth first. The app reaches it over HTTP on your local network—usually home Wi‑Fi, or the board hotspot (often 192.168.4.x) after you enable it from Board discovery while Bluetooth stays connected. Hotspots stay off by default. Manual URL and modes remain under Settings.';

  @override
  String get helpPlayingTitle => 'Playing a game';

  @override
  String get helpPlayingBody =>
      'Use Play for live sync, Game controls for time and flip, and Analysis for Stockfish.';

  @override
  String get helpCoachTitle => 'Coach & analysis';

  @override
  String get helpCoachBody =>
      'Enable learning mode and connect API keys in Settings for cloud coaches.';

  @override
  String get helpSettingsTitle => 'Settings & configuration';

  @override
  String get helpSettingsBody =>
      'Appearance, NVS, lamp, firmware, and MQTT live here.';

  @override
  String get coachScreenTitle => 'AI Coach';

  @override
  String get coachScreenHintTitle => 'Opening & plans';

  @override
  String get coachScreenHintBody => 'Ask about ideas for this position.';

  @override
  String coachScreenLevelLabel(String level) {
    return 'Level $level';
  }

  @override
  String get setupModeAppBar => 'Setup mode';

  @override
  String get setupModePiecePlacementTitle => 'Piece placement';

  @override
  String get setupModePiecePlacementBody =>
      'Use the wizard to align pieces with LED hints.';

  @override
  String get setupModeGoBack => 'Go back';

  @override
  String get wizardSkipStep => 'Skip step';

  @override
  String get wizardCancel => 'Cancel';

  @override
  String get connDiagTitle => 'Connection diagnostics';

  @override
  String get connDiagTransport => 'Transport';

  @override
  String get connDiagWifiBaseUrl => 'Wi‑Fi base URL';

  @override
  String get connDiagPolling => 'Polling';

  @override
  String get connDiagWebSocket => 'WebSocket';

  @override
  String get connDiagPollSuccessTitle => 'Successful snapshot GETs (incl. 304)';

  @override
  String get connDiagPollFailureTitle => 'Failed GET / poll errors';

  @override
  String get connDiagWsFramesTitle => 'WS messages (frames)';

  @override
  String get connDiagLastPollOk => 'Last successful poll';

  @override
  String get connDiagLastErrorTitle => 'Last error';

  @override
  String get connDiagActive => 'active';

  @override
  String get connDiagOff => 'off';

  @override
  String get newGameSheetTitle => 'New game';

  @override
  String get newGameSheetSubtitle => 'Pick time control and side.';

  @override
  String get newGameWhiteBottom => 'White bottom';

  @override
  String get newGameBlackBottom => 'Black bottom';

  @override
  String get newGameCustomTimeTitle => 'Custom time';

  @override
  String get newGameCustomTimeSubtitle =>
      'Minutes + increment sent to the board.';

  @override
  String newGameMinutesPerSide(String n) {
    return 'Minutes per side: $n';
  }

  @override
  String newGameIncrementSeconds(String n) {
    return 'Increment (s): $n';
  }

  @override
  String get newGameStartOnBoard => 'Start on board';

  @override
  String get newGameConnectFirstSnack =>
      'Connect a board first (Wi‑Fi or Bluetooth).';

  @override
  String get newGameStartedSnack => 'New game started on the board.';

  @override
  String newGameErrorSnack(String error) {
    return 'Error: $error';
  }

  @override
  String get newGameSheetBody => 'Choose time control and board orientation.';

  @override
  String get newGameBoardViewSection => 'Board orientation';

  @override
  String get newGameBoardViewInfoTitle => 'Board orientation';

  @override
  String get newGameWhoStartsNote => '';

  @override
  String get newGameFirmwarePresets => 'Firmware presets';

  @override
  String newGameCustomSummary(int min, int inc) {
    return '$min min + $inc s / move';
  }

  @override
  String get learnAppBarTitle => 'Chess lessons';

  @override
  String get learnBoardLedHint =>
      'Connect the board and use LED hints while practicing these lessons.';

  @override
  String learnSnackLesson(String title) {
    return 'Lesson: $title — interactive content coming soon.';
  }

  @override
  String get learnSecBasics => 'Basics';

  @override
  String get learnSecSpecial => 'Special moves';

  @override
  String get learnSecTactics => 'Tactics';

  @override
  String get learnSecStrategy => 'Strategy';

  @override
  String get learnL1Title => 'How pieces move';

  @override
  String get learnL1Desc =>
      'King, queen, rook, bishop, knight, pawn — learn each piece’s moves.';

  @override
  String get learnL2Title => 'Goals of chess';

  @override
  String get learnL2Desc =>
      'Checkmate, stalemate, draw — what end states mean.';

  @override
  String get learnL3Title => 'Piece values';

  @override
  String get learnL3Desc =>
      'Pawn=1, Knight/Bishop=3, Rook=5, Queen=9 — why it matters.';

  @override
  String get learnL4Title => 'Castling';

  @override
  String get learnL4Desc => 'Kingside and queenside — how, when, and why.';

  @override
  String get learnL5Title => 'En passant';

  @override
  String get learnL5Desc => 'The special pawn capture beginners often miss.';

  @override
  String get learnL6Title => 'Promotion';

  @override
  String get learnL6Desc =>
      'Pawn on the eighth rank — choosing the right piece.';

  @override
  String get learnL7Title => 'Fork';

  @override
  String get learnL7Desc =>
      'Attack two pieces at once — a core tactical theme.';

  @override
  String get learnL8Title => 'Pin';

  @override
  String get learnL8Desc =>
      'A piece can’t move because it would expose the king.';

  @override
  String get learnL9Title => 'Scholar’s Mate';

  @override
  String get learnL9Desc => 'Mate in four — and how to stop it.';

  @override
  String get learnL10Title => 'Control the center';

  @override
  String get learnL10Desc => 'Why e4, d4, e5, d5 matter in the opening.';

  @override
  String get learnL11Title => 'King safety';

  @override
  String get learnL11Desc =>
      'Castle early as insurance — when and how to hide the king.';

  @override
  String get learnL12Title => 'Endgame: king & pawn';

  @override
  String get learnL12Desc => 'Turn a pawn advantage into a win.';

  @override
  String get devSettingsTitle => 'Diagnostics & Developer';

  @override
  String get devStockfishFenHeader => 'Stockfish & FEN';

  @override
  String get devMoveEvalTile => 'Move evaluation (moveEvaluationEnabled)';

  @override
  String devHintDepthTile(String n) {
    return 'Hint depth (hintDepth): $n';
  }

  @override
  String get devCurrentFenLabel => 'Current FEN from board:';

  @override
  String get devNetworkTransportHeader => 'Network & transport';

  @override
  String get devBoardBaseUrlTile => 'Board base URL (ESP)';

  @override
  String get devActiveLinkTile => 'Active link status';

  @override
  String get devCoachTraceTile => 'Verbose coach logs (coach trace)';

  @override
  String get devStartConnection => 'Start connection';

  @override
  String get devStopTransport => 'Stop';

  @override
  String get devTransportStoppedSnack => 'Transport stopped.';

  @override
  String get devRefreshedFromPrefsSnack => 'Reloaded from preferences.';

  @override
  String devPingSnapshot(String result) {
    return 'Ping snapshot (RTT): $result';
  }

  @override
  String get devConnectionDiagTile => 'Connection diagnostics (REST / WS)';

  @override
  String get devDisconnectStaSnack => 'STA disconnected.';

  @override
  String get devClearWifiNvsTitle => 'Clear Wi‑Fi from NVS?';

  @override
  String get devClearWifiNvsBody => 'The ESP will forget the stored network.';

  @override
  String get devWifiNvsClearedSnack => 'Wi‑Fi NVS cleared.';

  @override
  String get devClearWifiNvsButton => 'Clear saved Wi‑Fi from NVS';

  @override
  String get devWifiConfigHeader => 'Wi‑Fi configuration on board';

  @override
  String get devWifiUnavailable =>
      'Wi‑Fi status unavailable. Tap refresh below.';

  @override
  String get devNvsToolsButton => 'Detailed tools (NVS classes)';

  @override
  String devErrorPrefix(String error) {
    return 'Error: $error';
  }

  @override
  String get devSentToEspSnack => 'Sent to ESP.';

  @override
  String get boardNvsTitle => 'Board memory (NVS)';

  @override
  String get boardNvsSavedSnack => 'Saved to board NVS.';

  @override
  String get boardNvsLedHeader => 'On-board chess hints (LED)';

  @override
  String get boardNvsEvalMode => 'Eval mode (compute best moves)';

  @override
  String get boardNvsStockfishDepth => 'Stockfish depth D1–D18';

  @override
  String get boardNvsLedBest => 'Show LED rewards (best move)';

  @override
  String get boardNvsLedGood => 'Show LED rewards (good move)';

  @override
  String get boardNvsLedCapture => 'Show LED rewards (capture)';

  @override
  String get boardNvsUartStats => 'Print hint stats to UART';

  @override
  String get boardNvsLiftBeforeBotTarget => 'Bot target LED only after lift';

  @override
  String get boardNvsTutorialMode => 'Tutorial mode enabled';

  @override
  String get boardNvsConfirmNewGameLed => 'Confirm new game with LED button';

  @override
  String get boardNvsHintLimit => 'Hint limit (0 = unlimited)';

  @override
  String get boardNvsHintTierTitle => 'Hint type (MoveHintTier)';

  @override
  String get boardNvsHintTierSubtitle =>
      'H1 = best only, H2 = top 3, H3 = full Stockfish set';

  @override
  String get boardNvsH1 => 'H1 – Best';

  @override
  String get boardNvsH2 => 'H2 – Top 3';

  @override
  String get boardNvsH3 => 'H3 – All';

  @override
  String get boardNvsOpponentHeader => 'Opponent (bot) settings';

  @override
  String get boardNvsBotMode => 'Bot mode';

  @override
  String get boardNvsPvp => 'Player vs player (off)';

  @override
  String get boardNvsPvb => 'Player vs bot';

  @override
  String get boardNvsBotStrength => 'Bot strength (level)';

  @override
  String get boardNvsBotPlaysAs => 'Bot plays as';

  @override
  String get boardNvsDemoHeader => 'Demo mode';

  @override
  String get boardNvsSaveToBoard => 'Save to board NVS';

  @override
  String boardDemoStateLine(String on) {
    return 'On-board demo: $on';
  }

  @override
  String get boardDemoOn => 'on';

  @override
  String get boardDemoOff => 'off';

  @override
  String get boardDemoEnabledConfig => 'Demo enabled (configuration)';

  @override
  String boardDemoSpeedLabel(String ms) {
    return 'Animation speed: $ms ms';
  }

  @override
  String get boardDemoSendConfig => 'Send demo config';

  @override
  String get boardDemoStart => 'Start demo';

  @override
  String get boardLedSentSnack => 'LED guidance sent.';

  @override
  String boardLedGuidanceTitle(String level) {
    return 'LED guidance (level $level / 5)';
  }

  @override
  String get boardLedSendLevel => 'Send LED level';

  @override
  String get boardGuidedCaptureTitle => 'Guided captures';

  @override
  String get boardGuidedCaptureSubtitle => 'LED highlight possible captures.';

  @override
  String get lampBoardLightTitle => 'Board light';

  @override
  String get lampDetailTitle => 'Lamp — detail';

  @override
  String get lampDetailsButton => 'Lamp details';

  @override
  String get lampHeader => 'Lamp';

  @override
  String get lampNeedsConnection =>
      'Requires board connection (Wi‑Fi or Bluetooth).';

  @override
  String get lampOnTitle => 'Lamp on';

  @override
  String lampBrightnessLabel(String n) {
    return 'Brightness $n%';
  }

  @override
  String get lampSendBrightness => 'Send brightness';

  @override
  String get lampBrightnessSentSnack => 'Brightness sent to the board.';

  @override
  String get lampColorStateSentSnack => 'Color and lamp state sent.';

  @override
  String get lampGenericCommandSnack => 'Lamp command sent.';

  @override
  String lampBlockStatusSummary(
      String mode, String r, String g, String b, String h, String t) {
    return 'Mode: $mode · RGB($r,$g,$b) · hint limit: $h · auto-off: ${t}s';
  }

  @override
  String get lampRgbHint => 'Color (R G B) — POST /api/light/command';

  @override
  String get lampSendColor => 'Send color / lamp state';

  @override
  String get lampGameModeButton => 'Game lamp mode';

  @override
  String get lampColorLabel => 'Color';

  @override
  String get lampColorfulnessLabel => 'Colorfulness';

  @override
  String get lampLedBrightnessPct => 'LED brightness (%)';

  @override
  String get lampStudioColorTitle => 'Color';

  @override
  String get lampStudioColorHint =>
      'Tap or drag — hue and saturation (center = white).';

  @override
  String get lampStudioValueTitle => 'Color lightness';

  @override
  String get lampStudioBoardPreview => 'Board preview';

  @override
  String get lampStudioPreviewGlowHint => 'Light spill preview';

  @override
  String get lampStudioPreviewLampOff => 'Lamp off';

  @override
  String get lampStudioScenes => 'Scenes';

  @override
  String get lampStudioPresetColorsTitle => 'Preset colors';

  @override
  String get lampStudioFineTuneSection => 'Fine-tune color';

  @override
  String get lampStudioApplyToBoard => 'Apply to board';

  @override
  String get lampStudioGameMode => 'Game mode';

  @override
  String get lampStudioAppliedOk => 'Color and brightness sent to the board.';

  @override
  String get lampStudioGameModeOk => 'Game lamp mode applied.';

  @override
  String get lampStudioNeedConnection =>
      'Connect the board via Wi‑Fi or Bluetooth to control the lamp.';

  @override
  String get lampStudioAutoOff => 'Auto shut-off';

  @override
  String get lampStudioSaveTimer => 'Save timer';

  @override
  String get lampStudioTimerSavedOk => 'Auto shut-off timer saved.';

  @override
  String get lampStudioSelectedSwatchLabel => 'Current color';

  @override
  String get lampStudioDeveloperRgbTitle => 'Developer · RGB values';

  @override
  String lampStudioRgbLine(int r, int g, int b) {
    return 'RGB ($r, $g, $b)';
  }

  @override
  String get lampPresetWarmWhite => 'Warm white';

  @override
  String get lampPresetCoolWhite => 'Cool white';

  @override
  String get lampPresetCalmBlue => 'Calm blue';

  @override
  String get lampPresetForestGreen => 'Forest green';

  @override
  String get lampPresetWarmth => 'Warmth';

  @override
  String get lampPresetPurpleScene => 'Purple scene';

  @override
  String get lampPresetRed => 'Red';

  @override
  String get lampPresetAmber => 'Amber';

  @override
  String get puzzleTabDaily => 'Daily';

  @override
  String get puzzleTabLibrary => 'Library';

  @override
  String get puzzleTabTraining => 'Training';

  @override
  String get puzzleBackLiveTooltip => 'Back to live game';

  @override
  String get puzzlePoolEmptySnack =>
      'No positions in this pool mode — switch to Mixed or add library items.';

  @override
  String get puzzleLibraryEmptyHint =>
      'No items yet — use the form above or Add from the daily puzzle.';

  @override
  String get puzzleLibraryRemoveTitle => 'Remove from library?';

  @override
  String puzzleLibraryRemoveBody(String title) {
    return 'Remove “$title” from your puzzle library? This cannot be undone.';
  }

  @override
  String get puzzleLibraryRemoveConfirm => 'Remove';

  @override
  String get puzzleLoadDaily => 'Load daily puzzle';

  @override
  String get puzzleRefreshDaily => 'Refresh daily puzzle';

  @override
  String get puzzleAlreadyInLibrary => 'Already in library';

  @override
  String get puzzleSavedToLibraryBanner => 'Saved to library.';

  @override
  String get puzzleLibNameLabel => 'Title';

  @override
  String get puzzleLibFenLabel => 'FEN';

  @override
  String get puzzleGoPlaySandbox => 'Sandbox — Play tab.';

  @override
  String puzzleGoPlayPuzzle(String title) {
    return 'Puzzle ($title) — Play tab.';
  }

  @override
  String get puzzleSideBlackMove => 'Black to move';

  @override
  String get puzzleSideWhiteMove => 'White to move';

  @override
  String get puzzleSideUnknown => 'Side to move: ?';

  @override
  String puzzleTrainingPoolStats(int poolN, int sessions) {
    return '$poolN positions · $sessions rounds started';
  }

  @override
  String get puzzleThemeMateIn1 => 'Mate in 1';

  @override
  String get puzzleThemeAdvancedPawn => 'Advanced pawn';

  @override
  String get puzzleThemeOpening => 'Opening';

  @override
  String get puzzleThemeKingsideAttack => 'Kingside attack';

  @override
  String get puzzleThemeMiddlegame => 'Middlegame';

  @override
  String get puzzleThemeTactic => 'Tactics';

  @override
  String get setupWizardErrMissingFen => 'FEN is missing.';

  @override
  String get setupWizardErrConnectBoard =>
      'Connect the board via Wi‑Fi or Bluetooth (demo mode does not support this tutorial).';

  @override
  String get setupWizardErrNoSteps => 'Could not build steps from this FEN.';

  @override
  String get setupWizardErrGeneric => 'Error';

  @override
  String get setupWizardErrPhysicalMismatch =>
      'The board still reports setup mode, or the physical pieces do not match — adjust pieces on the highlighted squares and try again.';

  @override
  String get setupWizardTitleStandard => 'Starting position';

  @override
  String get setupWizardTitleFromFen => 'Setup from FEN';

  @override
  String get setupWizardDoneStandard =>
      'The board confirmed the starting position.';

  @override
  String get setupWizardDoneFenLoaded =>
      'The position was loaded on the board.';

  @override
  String get setupWizardDoneNoLoad => 'Wizard finished.';

  @override
  String get setupWizardBtnDone => 'Done';

  @override
  String setupWizardStepProgress(int current, int total) {
    return 'Step $current / $total';
  }

  @override
  String setupWizardPlacePieceOn(String square) {
    return 'Place the piece on $square';
  }

  @override
  String setupWizardPieceDetail(String pieceName, String square) {
    return '$pieceName → $square';
  }

  @override
  String get setupWizardLedHint =>
      'The board LED shows the target square. Progress is confirmed by the matrix sensor or the correct piece in the snapshot.';

  @override
  String get setupWizardSkipStep => 'Skip step';

  @override
  String get boardSetupWhiteKing => 'white king';

  @override
  String get boardSetupBlackKing => 'black king';

  @override
  String get boardSetupWhiteQueen => 'white queen';

  @override
  String get boardSetupBlackQueen => 'black queen';

  @override
  String get boardSetupWhiteRook => 'white rook';

  @override
  String get boardSetupBlackRook => 'black rook';

  @override
  String get boardSetupWhiteBishop => 'white bishop';

  @override
  String get boardSetupBlackBishop => 'black bishop';

  @override
  String get boardSetupWhiteKnight => 'white knight';

  @override
  String get boardSetupBlackKnight => 'black knight';

  @override
  String get boardSetupWhitePawn => 'white pawn';

  @override
  String get boardSetupBlackPawn => 'black pawn';

  @override
  String get boardSetupPieceGeneric => 'piece';

  @override
  String get gameRemoteEmptySquareSnack => 'There is no piece on this square.';

  @override
  String get gameRemoteEmptySquareHud => 'Empty square';

  @override
  String get gameRemoteWrongTurnSnack =>
      'Pick a piece of the side that is on move.';

  @override
  String get gameRemoteWrongTurnHud => 'Not your turn';

  @override
  String get gameRemoteGameFinished => 'This game is already over.';

  @override
  String get gameRemotePositionError =>
      'Could not validate the move from the current board data.';

  @override
  String get gamePromotionPickTitle => 'Promotion — choose a piece';

  @override
  String get gamePromotionQueen => 'Queen';

  @override
  String get gamePromotionRook => 'Rook';

  @override
  String get gamePromotionBishop => 'Bishop';

  @override
  String get gamePromotionKnight => 'Knight';

  @override
  String get gamePromotionFailedSnack => 'Promotion failed';

  @override
  String get semanticsChessBoard =>
      'Chessboard, eight by eight squares. Tap to select squares.';

  @override
  String get moveHistoryEmpty => 'Move history is empty yet.';

  @override
  String get moveHistoryCurrentPosition => 'Live position — current game';

  @override
  String moveHistoryPieceSubtitle(String piece) {
    return 'Piece: $piece';
  }

  @override
  String get bundledPuzzleMateQg8Title => 'Queen mate';

  @override
  String get bundledPuzzleMateQg8Subtitle => 'White · mate in 1';

  @override
  String get bundledPuzzleBackRankTitle => 'Back rank';

  @override
  String get bundledPuzzleBackRankSubtitle => 'White · rook mate';

  @override
  String get bundledPuzzleRookCornerTitle => 'Rook vs king';

  @override
  String get bundledPuzzleRookCornerSubtitle => 'White · endgame';

  @override
  String get bundledPuzzleBlackMateTitle => 'Black to move';

  @override
  String get bundledPuzzleBlackMateSubtitle => 'Black · mate in 1';

  @override
  String get bundledPuzzlePromotionTitle => 'Pawn advance';

  @override
  String get bundledPuzzlePromotionSubtitle => 'White · promotion';

  @override
  String get bundledPuzzleTwoRooksTitle => 'Two rooks';

  @override
  String get bundledPuzzleTwoRooksSubtitle => 'White · two rooks';

  @override
  String get settingsTileSmartHomeTitle => 'Smart home (MQTT)';

  @override
  String get settingsTileSmartHomeSubtitle => 'Home Assistant & MQTT';

  @override
  String get settingsHaMqttOpenButton => 'MQTT & Home Assistant';

  @override
  String get settingsTileBoardLightTitle => 'Board light';

  @override
  String get settingsTileBoardLightSubtitle =>
      'Color, brightness, scenes, auto-off';

  @override
  String get settingsTileModulesTitle => 'Help & guides';

  @override
  String get settingsTileModulesSubtitle =>
      'Documentation and replaying the introduction';

  @override
  String get settingsNavAppTour => 'App tour (onboarding)';

  @override
  String get onboardingYourNameTitle => 'What should we call you?';

  @override
  String get onboardingYourNameSubtitle =>
      'This name appears in your profile and shared summaries.';

  @override
  String get onboardingNameHint => 'Your name';

  @override
  String get onboardingPermissionsTitle => 'Allow access';

  @override
  String get onboardingPermissionsSubtitle =>
      'You can turn features on gradually — the app works best when these are allowed.';

  @override
  String get onboardingPermPhotosTitle => 'Photos';

  @override
  String get onboardingPermPhotosBody =>
      'Pick a profile picture from your gallery.';

  @override
  String get onboardingPermPhotosAllow => 'Allow photos';

  @override
  String get onboardingPermBleTitle => 'Bluetooth';

  @override
  String get onboardingPermBleBody =>
      'Find and connect to your CzechMate board.';

  @override
  String get onboardingPermBleAllow => 'Allow Bluetooth';

  @override
  String get onboardingPermWifiTitle => 'Wi‑Fi';

  @override
  String get onboardingPermWifiBodyAndroid =>
      'Lets the app read your current network name to pre-fill Wi‑Fi setup (no rough location on Android 13+).';

  @override
  String get onboardingPermWifiBodyIos =>
      'You’ll join the board’s network in Settings when connecting — no extra step here.';

  @override
  String get onboardingPermWifiBodyDesktop =>
      'Reading the current Wi‑Fi name often isn’t available on desktop — type the SSID manually when setting up the board. Local HTTP to the board still uses your saved URL.';

  @override
  String get onboardingPermWifiAllow => 'Allow Wi‑Fi access';

  @override
  String get onboardingPermGrantedShort => 'Allowed';

  @override
  String get onboardingPermDeniedSnack =>
      'You can change this later in system Settings.';

  @override
  String get onboardingOpenSettings => 'Open Settings';

  @override
  String get onboardingPermissionsDesktopBody =>
      'Allow Bluetooth when the system asks so you can scan for the board. Use Wi‑Fi / manual URL for HTTP.';

  @override
  String get onboardingPermissionsDesktopWindowsBody =>
      'This Windows build cannot use Bluetooth Low Energy to reach the board. Connect with Wi‑Fi only: enter your board’s HTTP address under Advanced / manual connection (for example http://192.168.4.1 on the board hotspot or your home LAN IP).';

  @override
  String get settingsNavChessPuzzles => 'Chess puzzles';

  @override
  String get settingsNavProfileElo => 'Profile & puzzle Elo';

  @override
  String get settingsNavProgress => 'Progress (learning & stats)';

  @override
  String get settingsNavHelp => 'Help';

  @override
  String get settingsTileNvsDiagTitle => 'Board memory & diagnostics';

  @override
  String get settingsTileNvsDiagSubtitle => 'NVS rules, developer tools';

  @override
  String get settingsNavBoardNvs => 'Board NVS rules (LED, bot)';

  @override
  String get settingsNavDeveloperDiag => 'Developer diagnostics';

  @override
  String get settingsDiagnosticsBoardNvsLockedHint =>
      'Enable developer mode in About (tap the version row or Settings title 7×) to open raw board NVS.';

  @override
  String get settingsTileAboutTitle => 'About';

  @override
  String get settingsTileAboutSubtitle => 'Version, privacy, system';

  @override
  String get settingsAboutVersionLoading => 'Loading version…';

  @override
  String settingsAboutVersionLine(String version, String build) {
    return 'Version $version ($build) • Tap this row or “Settings” in the main settings title bar 7× (within about a second between taps) to unlock developer mode.';
  }

  @override
  String get settingsAppUpdateBannerTitle => 'App update available';

  @override
  String settingsAppUpdateBannerSubtitle(String current, String latest) {
    return 'You have $current; the latest release is $latest.';
  }

  @override
  String settingsAppUpdateBannerSubtitleRequired(
      String current, String requiredVersion) {
    return 'Your version ($current) is below the minimum supported. Update to at least $requiredVersion.';
  }

  @override
  String get settingsAppUpdateOpenDownloads => 'Open download page';

  @override
  String get settingsAboutAppUpdateLineTitle => 'New app version';

  @override
  String settingsAboutAppUpdateLineBody(String current, String latest) {
    return 'Installed: $current. Latest: $latest.';
  }

  @override
  String get settingsAboutDeveloperModeTitle => 'Developer mode';

  @override
  String get settingsAboutDeveloperModeSubtitleOn =>
      'Uncover advanced diagnostics and optional same-version OTA. Turn off to hide them again.';

  @override
  String get settingsPrivacyTitle => 'Privacy';

  @override
  String get settingsPrivacyBody =>
      'This build does not send analytics. Traffic goes only to your board on the local network or over Bluetooth.';

  @override
  String get settingsIcloudTitle => 'iCloud';

  @override
  String get settingsIcloudBody =>
      'Optional puzzle sync via iCloud (CloudKit) is planned for a future release; this build does not sync.';

  @override
  String get settingsLiveActivityTitle => 'Clock status outside the app';

  @override
  String get settingsLiveActivitySubtitle =>
      'Phones and tablets — iPhone: Live Activity (Lock Screen / Dynamic Island, iOS 16.2+). Android: ongoing notification (Android 13+ may require enabling notifications). Enable from the game screen. (Not used on desktop.)';

  @override
  String get settingsLiveActivityIosDisabledSnack =>
      'Live Activities are disabled system-wide. Enable them in Settings → czechmate → Live Activities.';

  @override
  String get settingsHomeScreenWidgetHint =>
      'Where your platform supports home-screen or desktop widgets, add CzechMate from the widget gallery for a quick snapshot when the app is in the background (alongside Live Activity on supported iPhones).';

  @override
  String get settingsWearMirrorTitle => 'Mirror clock on Wear OS';

  @override
  String get settingsWearMirrorSubtitle =>
      'Google Data Layer — same state as the ongoing notification; requires a paired Wear OS watch.';

  @override
  String get settingsWatchMirrorTitle => 'Mirror clock on Apple Watch';

  @override
  String get settingsWatchMirrorSubtitle =>
      'WatchConnectivity — same state as Live Activity; basic UI on the watch with Pause/Resume.';

  @override
  String get settingsFactoryTileTitle => 'Board factory reset';

  @override
  String get settingsFactoryTileSubtitle =>
      'Clears ESP NVS — Wi‑Fi credentials and preferences reset';

  @override
  String get settingsFactoryDialogTitle => 'Factory reset';

  @override
  String get settingsFactoryDialogBody =>
      'Erase all board NVS (Wi‑Fi, passwords, MQTT, preferences)? The device restarts; the board hotspot stays off until you turn it on from the app.';

  @override
  String get settingsFactoryErase => 'Erase';

  @override
  String get settingsFactoryCommandSent => 'Command sent.';

  @override
  String settingsFactoryError(String error) {
    return 'Error: $error';
  }

  @override
  String get settingsFactoryRunButton => 'Run board factory reset';

  @override
  String get settingsCoachOllamaUrlHelper =>
      'Enter a URL ending with /v1 (e.g. :11434/v1). Port 11434 without /v1 used to fail — the app now appends /v1 for 11434.';

  @override
  String get settingsCoachOpenAiBaseLabel => 'OpenAI API base';

  @override
  String get settingsCoachModelNameLabel => 'Model name';

  @override
  String get settingsCoachEnterGeminiModelSnack =>
      'Enter a Google AI model id (or pick a preset).';

  @override
  String get settingsCoachSavedSnack => 'Coach settings saved.';

  @override
  String get settingsCoachSaveButton => 'Save coach settings';

  @override
  String get settingsCoachGoogleKeyButton => 'Get Google AI key';

  @override
  String get settingsCoachGroqConsoleButton => 'Groq console';

  @override
  String get settingsCoachXaiConsoleButton => 'xAI console';

  @override
  String get boardNvsRefreshTooltip => 'Refresh from board';

  @override
  String get boardNvsFetchFailedFallback =>
      'Could not load data from the board. Tap ↻ to retry.';

  @override
  String boardNvsMergeHint(String depth, String eval) {
    return 'When saving, the app merges Stockfish depth ($depth) and move-eval toggle ($eval) into the NVS payload (same as iOS).';
  }

  @override
  String get boardNvsEvalOn => 'on';

  @override
  String get boardNvsEvalOff => 'off';

  @override
  String get boardNvsHintTierFootnote =>
      'H1–H3 in CzechMate is separate from on-board LED level; POST above also forwards depth/eval from app diagnostics.';

  @override
  String get boardNvsFooterMock =>
      'Demo board: NVS write does not reach real hardware.';

  @override
  String get boardNvsFooterWifiActive =>
      'Wi‑Fi session active — Save sends bot settings to ESP NVS.';

  @override
  String get boardNvsFooterBle =>
      'BLE: firmware may accept NVS/UI blob; verify status on the board web UI.';

  @override
  String get boardNvsFooterGeneric =>
      'For reliable bot NVS writes, connect the board (Wi‑Fi recommended).';

  @override
  String get boardNvsHttpBleExplain =>
      'Bluetooth is connected, but loading NVS from this screen uses HTTP. Save a valid board URL in Settings (STA IP, e.g. http://192.168.x.x), or activate a Wi‑Fi session to that IP.';

  @override
  String get boardNvsHttpWifiNoUrl =>
      'Wi‑Fi session without a valid base URL. Reconnect from Play → board manager.';

  @override
  String get boardNvsHttpMissingUrl =>
      'Missing valid board HTTP URL. In Settings fill “Default board URL” (full http://…) and try again (↻).';

  @override
  String get boardHttpMissingUrlBody =>
      'The board HTTP address is missing or invalid. In Settings, save a full URL (e.g. http://192.168.4.1 or your board STA IP). Bluetooth can still work without this URL for live play.';

  @override
  String get boardHttpFailBle =>
      'Bluetooth is connected, but the HTTP request failed (wrong URL, timeout, or web lock).';

  @override
  String get boardHttpFailWifi =>
      'Wi‑Fi session active — verify the board URL and that the board responds.';

  @override
  String get boardHttpFailMock =>
      'Demo board — HTTP to hardware does not apply.';

  @override
  String get boardHttpFailNone => 'No active connection to the board.';

  @override
  String boardHttpFailDetail(String link, String detail) {
    return '$link Detail: $detail';
  }

  @override
  String boardHttpErrGeneric(String error) {
    return 'Board communication error: $error';
  }

  @override
  String get progressLevelBeginner => 'Beginner';

  @override
  String get progressLevelIntermediate => 'Intermediate';

  @override
  String get progressLevelAdvanced => 'Advanced';

  @override
  String get progressLevelExpert => 'Expert';

  @override
  String progressLevelNumber(int n) {
    return 'Level $n';
  }

  @override
  String get progressNoSessionYet => 'No active session yet';

  @override
  String get analysisIntroEvalHint =>
      'Move evaluation and chart — chess-api or custom URL in Settings (developer).';

  @override
  String get analysisHalfMoveOrderHint =>
      'Each row is one half-move in order — white first, then black.';

  @override
  String get analysisNoBoardPositionHint =>
      'On the Play tab, connect the chessboard. Below you can analyze a custom FEN.';

  @override
  String get analysisChartDisabledBoardSubtitle =>
      'Turn on the switch below or in Settings → Board appearance.';

  @override
  String get analysisOverviewSubtitleEval =>
      'Position eval after each move (White perspective: + favors White)';

  @override
  String get analysisChartFillHintLiveEvalOn =>
      'The chart fills after moves once Stockfish evaluates (play on Play with internet).';

  @override
  String get analysisChartFillHintLiveEvalOff =>
      'Turn on automatic evaluation to see the curve here.';

  @override
  String get analysisClearEvalData => 'Clear chart and evaluations';

  @override
  String get analysisClearEvalConfirmTitle => 'Clear analysis data?';

  @override
  String get analysisClearEvalConfirmBody =>
      'This removes the evaluation chart and saved move-quality scores. You cannot undo this.';

  @override
  String get analysisClearEvalConfirmAction => 'Clear';

  @override
  String get analysisMoveQualitySideLast3 => 'Last 3 moves by side';

  @override
  String get analysisMoveQualitySideLast10 => 'Last 10 moves by side';

  @override
  String get analysisMoveQualityFullGame => 'Full game';

  @override
  String get analysisSecondLineComputing => 'Computing…';

  @override
  String get analysisSecondLineLoadButton =>
      'Load second line (from session FEN)';

  @override
  String analysisSecondLineSameMove(String from, String to) {
    return 'Both depths agree on $from→$to.';
  }

  @override
  String analysisSecondLineDualMoves(
      String f1, String t1, String f2, String t2, String suffix) {
    return '1) $f1→$t1 · 2) $f2→$t2$suffix';
  }

  @override
  String analysisSecondLineEvalApprox(String pawns) {
    return ' · eval ≈ $pawns pawns.';
  }

  @override
  String get analysisFreeAnalyzing => 'Computing…';

  @override
  String analysisBestMoveLine(
      String from, String to, String evalSuffix, String extra) {
    return 'Best move: $from–$to$evalSuffix$extra';
  }

  @override
  String analysisAvgLossCp(String cp) {
    return ' · avg loss $cp cp';
  }

  @override
  String get analysisFenFieldLabel => 'FEN';

  @override
  String get gameSandboxLoadPositionFirst =>
      'Load a position first (connect the board or use demo).';

  @override
  String get gameSandboxSelectPiece => 'Select a piece';

  @override
  String get gameSandboxWrongSide => 'Not this side to move';

  @override
  String get gameSandboxIllegalMove => 'Illegal move';

  @override
  String get gameSandboxIllegalMoveSandbox => 'Illegal move (sandbox)';

  @override
  String get gameSandboxCouldNotLoadFen => 'Could not load FEN.';

  @override
  String get gameSandboxPositionRestored => 'Position restored — try again.';

  @override
  String puzzleSuccessLineWithElo(int delta) {
    return 'Nice! Correct line · +$delta Elo';
  }

  @override
  String get puzzleSuccessLine => 'Nice! Correct line.';

  @override
  String get puzzleCelebrationTitle => 'Great job!';

  @override
  String get puzzleCelebrationBodyPlain => 'You solved the puzzle.';

  @override
  String puzzleCelebrationBodyElo(int delta) {
    return 'You solved the puzzle — rating +$delta.';
  }

  @override
  String get puzzleSaveNeedPosition =>
      'Open this puzzle on the Play tab first so the position is loaded, then save it here.';

  @override
  String get puzzleLibSavedPositionCaption =>
      'Current saved position (technical)';

  @override
  String get puzzleWrongResetting =>
      'Not the solution — resetting the position.';

  @override
  String get gameControlWhiteBottom => 'White bottom';

  @override
  String get gameControlBlackBottom => 'Black bottom';

  @override
  String get gameControlCoordsOn => 'Coords on';

  @override
  String get gameControlCoordsOff => 'Coords off';

  @override
  String get gameControlBoardSizeHint =>
      'Board size: Settings → Board appearance.';

  @override
  String get gameControlMoveHint => 'Move hint';

  @override
  String get gameControlPanelSubtitle => 'Display · Mode · Actions';

  @override
  String get gameControlLearningCloudHint =>
      'Cloud coach in the app; on-board Stockfish drives hint LEDs per NVS.';

  @override
  String get gameRemoteMovesWifi =>
      'Double-tap squares to send the move to the board over Wi‑Fi.';

  @override
  String get gameRemoteMovesBle =>
      'Double-tap squares to send the move over Bluetooth.';

  @override
  String get gameRemoteMovesMock =>
      'Demo: double-tap sends the move locally (board + clock simulation).';

  @override
  String get gameRemoteMovesNoBoard =>
      'Enabled, but no board connected. Connect via Bluetooth.';

  @override
  String get gameRemoteMovesConnect =>
      'Enabled — connect a board over Wi‑Fi or Bluetooth to send moves.';

  @override
  String get gameBoardRefreshedSnack => 'Board state refreshed';

  @override
  String get gameDemoSnapshotReloadedSnack => 'Demo snapshot reloaded';

  @override
  String get timerCenterGameOver => 'Game over';

  @override
  String get timerCenterRunning => 'Running';

  @override
  String get timerCenterStopped => 'Stopped';

  @override
  String get coachSetupBannerBody =>
      'The AI coach isn’t set up yet. Add at least one cloud provider and your API key in settings so it can answer.';

  @override
  String get coachSetupBannerAction => 'Open Coach settings';

  @override
  String coachErrorSomethingWrong(String error) {
    return 'Something went wrong: $error';
  }

  @override
  String get coachOfflineLabel => 'Offline';

  @override
  String get coachDisconnectedBanner =>
      'Board not connected — the coach isn’t receiving the live game position.';

  @override
  String get coachEmptyPromptEmbedded =>
      'Ask about plans, tactics, or specific squares.';

  @override
  String get coachEmptyPromptFullscreen =>
      'Ask about the current position or chess in general.';

  @override
  String get coachInputHintEmbedded => 'Ask the coach…';

  @override
  String get coachInputHintFullscreen => 'Ask about position or plan…';

  @override
  String get coachChainFailedBanner =>
      'Could not reach any configured AI provider (see Settings → Coach & AI).';

  @override
  String get settingsLinkDisconnected => 'Disconnected';

  @override
  String get settingsLinkConnecting => 'Connecting…';

  @override
  String get settingsLinkNoResponseYet => 'No board response yet';

  @override
  String get settingsLinkConnectedLive => 'Connected (live)';

  @override
  String get settingsCoachSubtitleOfflineTips =>
      'Short on-device tips only (no cloud)';
}
