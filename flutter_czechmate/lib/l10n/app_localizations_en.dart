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
  String get navPuzzle => 'Puzzle';

  @override
  String get navProgress => 'Progress';

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
      'chess-api usually works over mobile data. To open the board’s HTTP API on a LAN IP, the phone typically needs Wi‑Fi on the same network. If you are only on the board’s hotspot without internet, DNS for chess-api / Lichess may fail — temporarily disable that Wi‑Fi, enable mobile data, or use dual‑stack (Wi‑Fi + cellular data) if your OS allows. The app cannot force traffic to cellular while Wi‑Fi is the default route.';

  @override
  String get errInvalidBoardUrl =>
      'Invalid board URL — hostname missing. Use http://192.168.4.1 or your board STA IP.';

  @override
  String get errNoSavedBle =>
      'No saved Bluetooth board. Open Board discovery and pair your CZECHMATE board.';

  @override
  String get errDemoNoSnapshot => 'Demo board has no snapshot.';

  @override
  String get errConnectBeforeMoves =>
      'Connect to the board over Wi‑Fi or Bluetooth before sending moves from the app.';

  @override
  String get errSetupNeedsConnection =>
      'Setup wizard requires an active board connection (Wi‑Fi or Bluetooth).';

  @override
  String get errOtaHttps => 'OTA requires an HTTPS URL to the .bin file.';

  @override
  String get errOtaMock => 'OTA is not available with the mock board.';

  @override
  String get errOtaConnectFirst =>
      'Connect the board over Wi‑Fi or Bluetooth, then try the update again.';

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
  String get reportShareExportHint =>
      'Export the summary card above as a PNG, then use the system share sheet.';

  @override
  String get reportSharePreparing => 'Preparing…';

  @override
  String get reportShareSummaryImage => 'Share summary image';

  @override
  String get reportSharePgn => 'Share PGN file';

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
  String get lastErrorTitle => 'Last error';

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
  String get discoveryTitle => 'Find board';

  @override
  String get discoveryBlePermissionAndroid =>
      'Allow Bluetooth permissions in system settings to scan for BLE boards.';

  @override
  String discoveryBleScanFailed(String error) {
    return 'Bluetooth scan failed: $error';
  }

  @override
  String get discoveryIntro =>
      'Tap a board in the list. The app connects over Bluetooth and may switch to Wi‑Fi (STA) when the board is online on the network and your phone has Wi‑Fi.';

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
  String get discoveryWifiUrlHint => 'Board URL (http://…)';

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
  String get gameClearHintLeds => 'Clear hint LEDs';

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
    return 'Best move: $from→$to';
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
      'The board downloads over HTTPS (Wi‑Fi STA). The phone only sends the link.';

  @override
  String get firmwareTurnOffReminders => 'Turn off reminders';

  @override
  String get firmwareNotNow => 'Not now';

  @override
  String get firmwareUpdateAction => 'Update';
}
